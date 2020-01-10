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
#include <sys/time.h>

#include <time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/log.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/conn.h>
#include <bluetooth/driver.h>

#include <arm/arm/nvic.h>
#include <arm/nordicsemi/nrf5340_net_core.h>

#include "common.h"
#include "ble.h"

extern struct nrf_ipc_softc ipc_sc;
extern struct mdx_ringbuf_softc ringbuf_tx_sc;
extern struct mdx_ringbuf_softc ringbuf_rx_sc;

#define	USEC_TO_TICKS(n)	(n)
#define	ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))

static struct bt_conn *g_conn;
static struct mdx_callout c;
static struct bt_gatt_discover_params params;
static struct bt_uuid lt_uuid = {
	.type = BT_UUID_16,
	.u16 = BT_UUID_CTS_CURRENT_TIME,
};

static const struct bt_eir ad[] = {
	{
		.len = 2,
		.type = BT_EIR_FLAGS,
		.data = { BT_LE_AD_LIMITED | BT_LE_AD_NO_BREDR },
	},
	{
		.len = 7,
		.type = BT_EIR_UUID16_ALL,
		.data = { 0x0d, 0x18, 0x0f, 0x18, 0x05, 0x18 },
	},
	{
		.len = 17,
		.type = BT_EIR_UUID128_ALL,
		.data = { 0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
			  0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12 },
	},
	{ }
};

static const struct bt_eir sd[] = {
	{
		.len = 6,
		.type = BT_EIR_NAME_COMPLETE,
		.data = "mdepx",
	},
	{
		.len = 5,
		.type = BT_EIR_NAME_SHORTENED,
		.data = "mdep",
	},
	{ }
};

static void
ble_recv(void)
{
	struct mdx_ringbuf *rb;
	struct bt_buf *buf;
	int err;

	while (1) {
		err = mdx_ringbuf_head(&ringbuf_rx_sc, &rb);
		if (err)
			break;

		buf = bt_buf_get(rb->user, 0);
		if (!buf)
			panic("no bufs");

		memcpy(bt_buf_tail(buf), rb->buf, rb->fill);
		buf->len += rb->fill;

#if 0
		struct bt_hci_evt_hdr *evt;
		if (rb->user == 1) {
			evt = (struct bt_hci_evt_hdr *)buf->data;
			printf("%s: evt %x len %d buflen %d\n",
			    __func__, evt->evt, evt->len, buf->len);
		} else
			printf("%s: acl buflen %d\n",
			    __func__, buf->len);
#endif

		bt_recv(buf);

		mdx_ringbuf_submit(&ringbuf_rx_sc);
	};
}

void
ble_ipc_intr(void *arg)
{

	ble_recv();
}

static int
bt_send(struct bt_buf *buf)
{
	struct mdx_ringbuf *rb;
	int err;

	err = mdx_ringbuf_head(&ringbuf_tx_sc, &rb);
	if (err)
		panic("could not send bufs: no ring entries available");

	if (buf->len > rb->bufsize)
		panic("could not send bufs: ring entry buf is too small");

	memcpy(rb->buf, buf->data, buf->len);
	rb->fill = buf->len;

	switch(buf->type) {
	case BT_ACL_OUT:
		rb->user = BT_ACL_OUT;
		break;
	case BT_CMD:
		rb->user = BT_CMD;
		break;
	default:
		panic("unknown type %d", buf->type);
	}

	mdx_ringbuf_submit(&ringbuf_tx_sc);

	/* Notify network core. */
	nrf_ipc_trigger(&ipc_sc, 0);

	return (0);
}

static int
bt_open(void)
{

	return (0);
}

static struct bt_driver drv = {
	.head_reserve	= 0,
	.open		= bt_open,
	.send		= bt_send,
};

static void
read_cts(struct bt_conn *conn, int err,
    const void *data, uint16_t length)
{
	struct tm tm;
	const uint8_t *buf;
	struct timespec ts;
	uint16_t year;
	time_t t;

	buf = (const uint8_t *)data;

	memcpy(&year, buf, 2); /* year */
	tm.tm_year = year - 1900;
	tm.tm_mon = buf[2] - 1;
	tm.tm_mday = buf[3];
	tm.tm_hour = buf[4];
	tm.tm_min = buf[5];
	tm.tm_sec = buf[6];

	t = mktime(&tm);

	printf("current time %d\n", t);

	ts.tv_sec = t;
	ts.tv_nsec = 0;

	mdx_clock_settime(CLOCK_REALTIME, &ts);

	bzero(&tm, sizeof(struct tm));
	while (1) {
		mdx_clock_gettime(CLOCK_REALTIME, &ts);
		gmtime_r(&ts.tv_sec, &tm);

		printf("%s: %d/%d/%d %d:%d:%d\n", __func__,
		    tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
		    tm.tm_hour, tm.tm_min, tm.tm_sec);

		mdx_tsleep(1000000);
	}
}

static uint8_t
discover_func(const struct bt_gatt_attr *attr, void *user_data)
{
	uint16_t handle;
	uint16_t offset;
	int err;

	/* CTS descriptor found */

	printf("%s: handle 0x%x user_data %p\n",
	    __func__, attr->handle, user_data);

	handle = attr->handle;
	offset = 0;

	err = bt_gatt_read(g_conn, handle, offset, read_cts);
	if (err)
		printf("%s: err %d\n", __func__, err);

	return (BT_GATT_ITER_STOP);
}

static void
discover_cts_descr(void *arg)
{
	struct bt_conn *conn;
	int err;

	conn = arg;

	bzero(&params, sizeof(struct bt_gatt_discover_params));
	params.uuid = &lt_uuid;
	params.func = discover_func;
	params.start_handle = 1;
	params.end_handle = 0xffff;
	err = bt_gatt_discover_descriptor(conn, &params);
	if (err)
		printf("%s: err %d\n", __func__, err);
}

static void
connected(struct bt_conn *conn)
{

	printf("%s\n", __func__);

	g_conn = conn;

	mdx_callout_init(&c);
	mdx_callout_set(&c, 4000000, discover_cts_descr, conn);
}

static void
disconnected(struct bt_conn *conn)
{

	printf("%s\n", __func__);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static struct bt_uuid gap_uuid = {
	.type = BT_UUID_16,
	.u16 = BT_UUID_GAP,
};

static struct bt_uuid device_name_uuid = {
	.type = BT_UUID_16,
	.u16 = BT_UUID_GAP_DEVICE_NAME,
};
 
static struct bt_gatt_chrc name_chrc = {
	.properties = BT_GATT_CHRC_READ,
	.value_handle = 0x0003,
	.uuid = &device_name_uuid,
};

static const struct bt_gatt_attr attrs[] = {
	BT_GATT_PRIMARY_SERVICE(0x0001, &gap_uuid),
	BT_GATT_CHARACTERISTIC(0x0002, &name_chrc),
};

int
ble_test(void)
{
	bt_addr_t addr;
	int err;

	/* Setup ring buffer. */
	mdx_ringbuf_init(&ringbuf_tx_sc,
	    (void *)RINGBUF_TX_BASE, RINGBUF_TX_BASE_SIZE,
	    (void *)RINGBUF_TX_BUF, RINGBUF_TX_BUF_SIZE);
	mdx_ringbuf_join(&ringbuf_rx_sc,
	    (void *)RINGBUF_RX_BASE);

	bt_driver_register(&drv);

	err = bt_init();
	if (err)
		panic("Failed to initialize bluetooth, err %d\n", err);

	printf("Bluetooth initialized\n");

	addr.val[0] = 0x03;
	addr.val[1] = 0x01;
	addr.val[2] = 0x03;
	addr.val[3] = 0x04;
	addr.val[4] = 0x05;
	addr.val[5] = 0x03;

	hci_le_set_random_address(&addr);

	bt_gatt_register(attrs, ARRAY_SIZE(attrs));

	err = bt_start_advertising(BT_LE_ADV_IND, ad, sd);
	if (err != 0) {
		printf("Could not start advertising. Error %d\n", err);
		return (MDX_ERROR);
	}

	bt_conn_cb_register(&conn_callbacks);

	return (MDX_OK);
}
