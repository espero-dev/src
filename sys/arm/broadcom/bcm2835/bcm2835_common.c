/*-
 * Copyright (C) 2008-2011 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kdb.h>
#include <sys/reboot.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/vmparam.h>

#ifndef INTRNG
static int
fdt_intc_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{

	if (fdt_is_compatible(node, "broadcom,bcm2835-armctrl-ic") ||
	    fdt_is_compatible(node, "brcm,bcm2836-armctrl-ic")) {
		*interrupt = fdt32_to_cpu(intr[0]);
		*trig = INTR_TRIGGER_CONFORM;
		*pol = INTR_POLARITY_CONFORM;
		return (0);
	}
#ifdef SOC_BCM2836
	if (fdt_is_compatible(node, "brcm,bcm2836-l1-intc")) {
		*interrupt = fdt32_to_cpu(intr[0]) + 72;
		*trig = INTR_TRIGGER_CONFORM;
		*pol = INTR_POLARITY_CONFORM;
		return (0);
	}
#endif
	return (ENXIO);
}


fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_intc_decode_ic,
	NULL
};
#endif /* INTRNG */
