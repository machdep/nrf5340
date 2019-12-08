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

#include "ble.h"

#define	BLE_CONTROLLER_PROCESS_IRQn	ID_EGU0

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
blectlr_assertion_handler(const char *const file, const uint32_t line)
{

	printf("%s\n", __func__);
}

static void
host_signal(void)
{

	printf("%s\n", __func__);
}

int
ble_test(void)
{
	nrf_lf_clock_cfg_t clock_cfg;
	int err;

	clock_cfg.lf_clk_source = NRF_LF_CLOCK_SRC_RC;
	clock_cfg.accuracy = NRF_LF_CLOCK_ACCURACY_500_PPM;
	clock_cfg.rc_ctiv = BLE_CONTROLLER_RECOMMENDED_RC_CTIV;
	clock_cfg.rc_temp_ctiv = BLE_CONTROLLER_RECOMMENDED_RC_TEMP_CTIV;

	printf("%s: Initializing ble controller\n", __func__);

	err = ble_controller_init(blectlr_assertion_handler,
	    &clock_cfg,
	    BLE_CONTROLLER_PROCESS_IRQn);

	if (err != 0) {
		printf("%s: controller init returned %d\n", __func__, err);
		return (err);
	};

	printf("%s: Enabling ble controller\n", __func__);

	err = ble_controller_enable(host_signal, ble_controller_mempool);
	if (err != 0) {
		printf("%s: controller enable returned %d\n", __func__, err);
		return (err);
	}

	return (0);
}
