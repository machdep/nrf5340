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

extern struct arm_nvic_softc nvic_sc;

struct nrf_ipc_softc ipc_sc;
struct mdx_ringbuf_softc ringbuf_tx_sc;
struct mdx_ringbuf_softc ringbuf_rx_sc;

#include <arm/arm/nvic.h>
#include <arm/nordicsemi/nrf5340_app_core.h>

#include "ble.h"

static void
ipc_config(void)
{

	nrf_ipc_init(&ipc_sc, NRF_IPC);
	arm_nvic_setup_intr(&nvic_sc, ID_IPC, nrf_ipc_intr, &ipc_sc);

	/* Receive event 1 on channel 1 */
	nrf_ipc_configure_recv(&ipc_sc, 1, (1 << 1), ble_ipc_intr, NULL);
	nrf_ipc_inten(&ipc_sc, 1, true);

	/* Send event 0 to channel 0 */
	nrf_ipc_configure_send(&ipc_sc, 0, (1 << 0));
}

int
main(void)
{

	printf("Hello world!\n");

	ipc_config();

	/* Give some time for the NET core to startup. */
	mdx_usleep(500000);

	ble_test();

	while (1)
		mdx_usleep(2000000);

	return (0);
}
