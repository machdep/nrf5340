/*-
 * Copyright (c) 2019-2020 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/console.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/thread.h>
#include <sys/ringbuf.h>
#include <sys/sem.h>

#include <arm/arm/nvic.h>
#include <arm/nordicsemi/nrf5340_net_core.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/log.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/conn.h>
#include <bluetooth/driver.h>

#include <nrfxlib/ble_controller/include/ble_controller.h>
#include <nrfxlib/ble_controller/include/ble_controller_hci.h>

#include "ble.h"

#define	USEC_TO_TICKS(n)	(n)

#define	BLE_CONTROLLER_PROCESS_IRQn	ID_EGU0
#define	BLE_MAX_CONN_EVENT_LEN_DEFAULT	7500

#define	MAX_TX_PKT_SIZE BLE_CONTROLLER_DEFAULT_TX_PACKET_SIZE
#define	MAX_RX_PKT_SIZE BLE_CONTROLLER_DEFAULT_RX_PACKET_SIZE
#define	BLE_SLAVE_COUNT		1
#define	BLE_MASTER_COUNT	1

#define MASTER_MEM_SIZE (BLE_CONTROLLER_MEM_PER_MASTER_LINK(	\
	MAX_TX_PKT_SIZE,					\
	MAX_RX_PKT_SIZE,					\
	BLE_CONTROLLER_DEFAULT_TX_PACKET_COUNT,			\
	BLE_CONTROLLER_DEFAULT_RX_PACKET_COUNT)			\
	+ BLE_CONTROLLER_MEM_MASTER_LINKS_SHARED)

#define SLAVE_MEM_SIZE (BLE_CONTROLLER_MEM_PER_SLAVE_LINK(	\
	MAX_TX_PKT_SIZE,					\
	MAX_RX_PKT_SIZE,					\
	BLE_CONTROLLER_DEFAULT_TX_PACKET_COUNT,			\
	BLE_CONTROLLER_DEFAULT_RX_PACKET_COUNT)			\
	+ BLE_CONTROLLER_MEM_SLAVE_LINKS_SHARED)

#define MEMPOOL_SIZE ((BLE_SLAVE_COUNT * SLAVE_MEM_SIZE) +	\
	(BLE_MASTER_COUNT * MASTER_MEM_SIZE))

extern struct nrf_ipc_softc ipc_sc;
extern struct mdx_ringbuf_softc ringbuf_tx_sc;
extern struct mdx_ringbuf_softc ringbuf_rx_sc;

#define	BLE_STACK_SIZE	4096

static struct thread recv_td;
static struct thread send_td;
static uint8_t recv_td_stack[BLE_STACK_SIZE];
static uint8_t send_td_stack[BLE_STACK_SIZE];

static uint8_t ble_controller_mempool[MEMPOOL_SIZE];

static mdx_sem_t sem_recv;
static mdx_sem_t sem_send;
static mdx_sem_t sem_thread;

static void
print_build_rev(void)
{
	uint8_t ble_controller_rev[BLE_CONTROLLER_BUILD_REVISION_SIZE];
	int i;

	ble_controller_build_revision_get(ble_controller_rev);

	printf("Build revision: ");
	for (i = 0; i < BLE_CONTROLLER_BUILD_REVISION_SIZE; i++)
		printf("%x", ble_controller_rev[i]);

	printf("\n");
}

/* Send a packet to Bluetooth Controller. */

static void
ble_send(void *arg)
{
	struct mdx_ringbuf *rb;
	int err;

	while (1) {
		mdx_sem_wait(&sem_send);

		/* Send a packet to BLE controller */

		while ((err = mdx_ringbuf_head(&ringbuf_tx_sc, &rb)) == 0) {
#if 0
			printf("%s: got buf %x *buf %d bufsize %d\n",
			    __func__, (int)rb->buf,
			    *(volatile uint32_t *)rb->buf, rb->fill);
#endif
			switch (rb->user) {
			case BT_ACL_OUT:
				mdx_sem_wait(&sem_thread);
				hci_data_put(rb->buf);
				mdx_sem_post(&sem_thread);
				break;
			case BT_CMD:
				mdx_sem_wait(&sem_thread);
				hci_cmd_put(rb->buf);
				mdx_sem_post(&sem_thread);
				mdx_sem_post(&sem_recv);
				break;
			default:
				panic("unknown packet type");
			}
			mdx_ringbuf_submit(&ringbuf_tx_sc);
		}
	}
}

/* Receive data from Bluetooth Controller. */

static int
handle_hci_input(void)
{
	struct bt_hci_acl_hdr *hdr;
	struct mdx_ringbuf *rb;
	int err;

	err = mdx_ringbuf_head(&ringbuf_rx_sc, &rb);
	if (err)
		panic("could not get an entry in the ringbuf");

	mdx_sem_wait(&sem_thread);
	err = hci_data_get((uint8_t *)rb->buf);
	mdx_sem_post(&sem_thread);
	if (err == 0) {
		hdr = (void *)rb->buf;
		rb->fill = hdr->len + sizeof(struct bt_hci_acl_hdr);
		rb->user = BT_ACL_IN;
		mdx_ringbuf_submit(&ringbuf_rx_sc);
		nrf_ipc_trigger(&ipc_sc, 1);
		return (1);
	}

	return (0);
}

static int
handle_evt_input(void)
{
	struct mdx_ringbuf *rb;
	int err;

	err = mdx_ringbuf_head(&ringbuf_rx_sc, &rb);
	if (err)
		panic("could not get an entry in the ringbuf");

	mdx_sem_wait(&sem_thread);
	err = hci_evt_get((uint8_t *)rb->buf);
	mdx_sem_post(&sem_thread);
	if (err == 0) {
#if 0
		struct bt_hci_evt_hdr *evt_hdr;
		evt_hdr = (void *)rb->buf;
		rb->fill = evt_hdr->len + sizeof(struct bt_hci_evt_hdr);
#else
		rb->fill = 74;
#endif
		rb->user = BT_EVT;
		mdx_ringbuf_submit(&ringbuf_rx_sc);
		nrf_ipc_trigger(&ipc_sc, 1);
		return (1);
	}

	return (0);
}

static void
ble_recv(void *arg)
{
	int err1, err2;

	while (1) {
		mdx_sem_wait(&sem_recv);

		do {
			err1 = handle_hci_input();
			err2 = handle_evt_input();
		} while (err1 || err2);

		mdx_sched_yield();
	}
}

static void
ble_assertion_handler(const char *const file, const uint32_t line)
{

	printf("%s: %s:%d\n", __func__, file, line);

	while (1);
}

static void
host_signal(void)
{

	mdx_sem_post(&sem_recv);
}

void
ble_ipc_intr(void *arg)
{

	mdx_sem_post(&sem_send);
}

int
ble_test(void)
{
	nrf_lf_clock_cfg_t clock_cfg;
	struct thread *td;
	int err;

	mdx_sem_init(&sem_recv, 0);
	mdx_sem_init(&sem_send, 0);
	mdx_sem_init(&sem_thread, 1);

	td = &recv_td;
	bzero(td, sizeof(struct thread));
	td->td_stack = (void *)((uint32_t)recv_td_stack);
	td->td_stack_size = BLE_STACK_SIZE;
	mdx_thread_setup(td, "ble_recv",
			1, /* prio */
			0, /* quantum */
			ble_recv, NULL);
	if (td == NULL)
		panic("Could not create bt recv thread.");
	mdx_sched_add(td);

	td = &send_td;
	bzero(td, sizeof(struct thread));
	td->td_stack = (void *)((uint32_t)send_td_stack);
	td->td_stack_size = BLE_STACK_SIZE;
	mdx_thread_setup(td, "ble_send",
			1, /* prio */
			0, /* quantum */
			ble_send, NULL);
	if (td == NULL)
		panic("Could not create bt send thread.");
	mdx_sched_add(td);

	print_build_rev();

	bzero(&clock_cfg, sizeof(nrf_lf_clock_cfg_t));
	clock_cfg.accuracy = NRF_LF_CLOCK_ACCURACY_500_PPM;

#if 1
	clock_cfg.lf_clk_source = NRF_LF_CLOCK_SRC_XTAL;
#else
	clock_cfg.lf_clk_source = NRF_LF_CLOCK_SRC_RC;
	clock_cfg.rc_ctiv = BLE_CONTROLLER_RECOMMENDED_RC_CTIV;
	clock_cfg.rc_temp_ctiv = BLE_CONTROLLER_RECOMMENDED_RC_TEMP_CTIV;
#endif

	printf("%s: Initializing BLE controller\n", __func__);

	err = ble_controller_init(ble_assertion_handler,
	    &clock_cfg,
	    BLE_CONTROLLER_PROCESS_IRQn);

	if (err != 0) {
		printf("%s: Failed to initialize BLE controller, error %d\n",
		    __func__, err);
		return (err);
	};

	printf("%s: Enabling BLE controller\n", __func__);

	ble_controller_cfg_t cfg;

	cfg.master_count.count = BLE_MASTER_COUNT;
	err = ble_controller_cfg_set(BLE_CONTROLLER_DEFAULT_RESOURCE_CFG_TAG,
				     BLE_CONTROLLER_CFG_TYPE_MASTER_COUNT,
				     &cfg);

	cfg.slave_count.count = BLE_SLAVE_COUNT;
	err = ble_controller_cfg_set(BLE_CONTROLLER_DEFAULT_RESOURCE_CFG_TAG,
				     BLE_CONTROLLER_CFG_TYPE_SLAVE_COUNT,
				     &cfg);

	cfg.buffer_cfg.rx_packet_size = MAX_RX_PKT_SIZE;
	cfg.buffer_cfg.tx_packet_size = MAX_TX_PKT_SIZE;
	cfg.buffer_cfg.rx_packet_count = BLE_CONTROLLER_DEFAULT_RX_PACKET_COUNT;
	cfg.buffer_cfg.tx_packet_count = BLE_CONTROLLER_DEFAULT_TX_PACKET_COUNT;

	err = ble_controller_cfg_set(BLE_CONTROLLER_DEFAULT_RESOURCE_CFG_TAG,
				     BLE_CONTROLLER_CFG_TYPE_BUFFER_CFG,
				     &cfg);

	cfg.event_length.event_length_us =
		BLE_MAX_CONN_EVENT_LEN_DEFAULT;
	err = ble_controller_cfg_set(BLE_CONTROLLER_DEFAULT_RESOURCE_CFG_TAG,
				     BLE_CONTROLLER_CFG_TYPE_EVENT_LENGTH,
				     &cfg);

	err = ble_controller_enable(host_signal, ble_controller_mempool);
	if (err != 0) {
		printf("%s: Failed to enable BLE controller, error %d\n",
		    __func__, err);
		return (err);
	}

	printf("%s: BLE controller is ready\n", __func__);

	return (0);
}
