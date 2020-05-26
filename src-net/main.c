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

#include <arm/arm/nvic.h>
#include <arm/nordicsemi/nrf5340_net_core.h>

#include <dev/intc/intc.h>

#include <nrfxlib/ble_controller/include/ble_controller.h>
#include <nrfxlib/mpsl/include/mpsl.h>

#include "common.h"
#include "ble.h"

extern struct arm_nvic_softc nvic_sc;

struct mdx_ringbuf_softc ringbuf_tx_sc;
struct mdx_ringbuf_softc ringbuf_rx_sc;

static void
ble_rng_intr(void *arg, int irq)
{

	ble_controller_RNG_IRQHandler();
}

static void
ble_timer_intr(void *arg, int irq)
{

	MPSL_IRQ_TIMER0_Handler();
}

static void
ble_radio_intr(void *arg, int irq)
{

	MPSL_IRQ_RADIO_Handler();
}

static void
ble_rtc_intr(void *arg, int irq)
{

	MPSL_IRQ_RTC0_Handler();
}

static void
ble_power_intr(void *arg, int irq)
{

	MPSL_IRQ_CLOCK_Handler();
}

static void
nrf_egu0_intr(void *arg, int irq)
{

	mpsl_low_priority_process();
}

static void
config_ipc(void)
{
	mdx_device_t nvic, ipc;

	nvic = mdx_device_lookup_by_name("nvic", 0);
	if (nvic == NULL)
		panic("nvic device not found");

	ipc = mdx_device_lookup_by_name("nrf_ipc", 0);
	if (ipc == NULL)
		panic("ipc device not found");

	/* Send event 1 to channel 1 */
	nrf_ipc_configure_send(ipc, 1, (1 << 1));

	/* Receive event 0 on channel 0 */
	nrf_ipc_configure_recv(ipc, 0, (1 << 0), ble_ipc_intr, NULL);
	nrf_ipc_inten(ipc, 0, true);
}

int
main(void)
{
	mdx_device_t nvic;

	printf("Hello world!\n");

	config_ipc();

	nvic = mdx_device_lookup_by_name("nvic", 0);
	if (nvic == NULL)
		panic("nvic device not found");

	mdx_intc_setup(nvic, ID_EGU0,   nrf_egu0_intr,  NULL);
	mdx_intc_setup(nvic, ID_RNG,    ble_rng_intr,   NULL);
	mdx_intc_setup(nvic, ID_TIMER0, ble_timer_intr, NULL);
	mdx_intc_setup(nvic, ID_RADIO,  ble_radio_intr, NULL);
	mdx_intc_setup(nvic, ID_RTC0,   ble_rtc_intr,   NULL);
	mdx_intc_setup(nvic, ID_POWER,  ble_power_intr, NULL);

	mdx_ringbuf_init(&ringbuf_rx_sc,
	    (void *)RINGBUF_RX_BASE, RINGBUF_RX_BASE_SIZE,
	    (void *)RINGBUF_RX_BUF, RINGBUF_RX_BUF_SIZE);
	mdx_ringbuf_join(&ringbuf_tx_sc,
	    (void *)RINGBUF_TX_BASE);

	ble_test();

	while (1)
		mdx_usleep(2000000);

	return (0);
}
