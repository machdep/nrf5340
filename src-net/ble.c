/*-
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
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

#include <arm/arm/nvic.h>
#include <arm/nordicsemi/nrf5340_net_core.h>

#include <nrfxlib/ble_controller/include/ble_controller.h>
#include <nrfxlib/ble_controller/include/ble_controller_hci.h>

#include "ble.h"

#define	USEC_TO_TICKS(n)	(n)

#define	BLE_CONTROLLER_PROCESS_IRQn	ID_EGU0
#define	BLE_MAX_CONN_EVENT_LEN_DEFAULT	2500

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

static uint8_t ble_controller_mempool[MEMPOOL_SIZE];

static void
ble_assertion_handler(const char *const file, const uint32_t line)
{

	printf("%s: %s:%d\n", __func__, file, line);

	while (1);
}

static void
host_signal(void)
{

	printf("%s\n", __func__);
}

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

int
ble_test(void)
{
	nrf_lf_clock_cfg_t clock_cfg;
	int err;

	print_build_rev();

	clock_cfg.lf_clk_source = NRF_LF_CLOCK_SRC_RC;
	clock_cfg.accuracy = NRF_LF_CLOCK_ACCURACY_500_PPM;
	clock_cfg.rc_ctiv = BLE_CONTROLLER_RECOMMENDED_RC_CTIV;
	clock_cfg.rc_temp_ctiv = BLE_CONTROLLER_RECOMMENDED_RC_TEMP_CTIV;

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
