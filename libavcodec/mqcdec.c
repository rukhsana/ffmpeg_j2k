/*
 * MQ-coder decoder
 * Copyright (c) 2007 Kamil Nowosad
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * MQ-coder decoder
 * @file
 * @author Kamil Nowosad
 */

#include "mqc.h"

static void bytein(MqcState *mqc)
{
    if (*mqc->bp == 0xff){
        if (*(mqc->bp+1) > 0x8f)
            mqc->c++;
        else{
            mqc->bp++;
            mqc->c += 2 + 0xfe00 - (*mqc->bp << 9);
        }
    } else{
        mqc->bp++;
        mqc->c += 1 + 0xff00 - (*mqc->bp << 8);
    }
}

static int exchange(MqcState *mqc, uint8_t *cxstate, int lps)
{
    int d;
    if ((mqc->a < ff_mqc_qe[*cxstate]) ^ (!lps)){
        if (lps)
            mqc->a = ff_mqc_qe[*cxstate];
        d = *cxstate & 1;
        *cxstate = ff_mqc_nmps[*cxstate];
    } else{
        if (lps)
            mqc->a = ff_mqc_qe[*cxstate];
        d = 1 - (*cxstate & 1);
        *cxstate = ff_mqc_nlps[*cxstate];
    }
    // renormd:
    do{
        if (!(mqc->c & 0xff)){
            mqc->c -= 0x100;
            bytein(mqc);
        }
        mqc->a += mqc->a;
        mqc->c += mqc->c;
    } while (!(mqc->a & 0x8000));
    return d;
}

void ff_mqc_initdata(MqcState *mqc, uint8_t *bp)
{
  //ff_mqc_init_contexts(mqc);
    mqc->bp = bp;
    mqc->c = (*mqc->bp ^ 0xff) << 16;
    bytein(mqc);
    mqc->c = mqc->c << 7;
    mqc->a = 0x8000;
}

static void mqc_bytein1(MqcState *mqc) {
	if (mqc->bp != mqc->bpend) {
		uint8_t c;
		if (mqc->bp + 1 != mqc->bpend) {
			c = *(mqc->bp + 1);
		} else {
			c = 0xff;
		}
		if (*mqc->bp == 0xff) {
			if (c > 0x8f) {
				mqc->c += 0xff00;
				mqc->ct = 8;
			} else {
				mqc->bp++;
				mqc->c += c << 9;
				mqc->ct = 7;
			}
		} else {
			mqc->bp++;
			mqc->c += c << 8;
			mqc->ct = 8;
		}
	} else {
		mqc->c += 0xff00;
		mqc->ct = 8;
	}
}
static void mqc_renormd(MqcState *mqc) {
	do {
		if (mqc->ct == 0) {
			mqc_bytein1(mqc);
		}
		mqc->a <<= 1;
		mqc->c <<= 1;
		mqc->ct--;
	} while (mqc->a < 0x8000);
}

void ff_mqc_initdata1(MqcState *mqc, uint8_t *bp, int len) {
	mqc->bpstart = bp;
	mqc->bpend = bp + len;
	mqc->bp = bp;
	if (len==0) mqc->c = 0xff << 16;
	else mqc->c = *mqc->bp << 16;
	mqc_bytein1(mqc);
	mqc->c <<= 7;
	mqc->ct -= 7;
	mqc->a = 0x8000;
}


static int mqc_mpsexchange(MqcState *mqc, uint8_t *cxstate) {
	int d;
	if (mqc->a < ff_mqc_qe[*cxstate]) {
		d = 1 - *cxstate & 1;
		*cxstate = ff_mqc_nlps[*cxstate];
	} else {
		d = *cxstate & 1;
		*cxstate = ff_mqc_nmps[*cxstate];
	}
	
	return d;
}

static int mqc_lpsexchange(MqcState *mqc, uint8_t *cxstate) {
	int d;
	if (mqc->a < ff_mqc_qe[*cxstate]) {
		mqc->a = ff_mqc_qe[*cxstate];
		d = *cxstate & 1;
		*cxstate = ff_mqc_nmps[*cxstate];
	} else {
		mqc->a = ff_mqc_qe[*cxstate];
		d = 1 - *cxstate & 1;
		*cxstate = ff_mqc_nlps[*cxstate];
	}
	
	return d;
}

int ff_mqc_decode1(MqcState *mqc, uint8_t *cxstate) {
	int d;
	mqc->a -= ff_mqc_qe[*cxstate];
	if ((mqc->c >> 16) < ff_mqc_qe[*cxstate]) {
		d = mqc_lpsexchange(mqc, cxstate);
		mqc_renormd(mqc);
	} else {
		mqc->c -= ff_mqc_qe[*cxstate] << 16;
		if ((mqc->a & 0x8000) == 0) {
			d = mqc_mpsexchange(mqc, cxstate);
			mqc_renormd(mqc);
		} else {
			d = *cxstate & 1;
		}
	}

	return d;
}

int ff_mqc_decode(MqcState *mqc, uint8_t *cxstate)
{
    mqc->a -= ff_mqc_qe[*cxstate];
    if ((mqc->c >> 16) < mqc->a){
        if (mqc->a & 0x8000)
            return *cxstate & 1;
        else
            return exchange(mqc, cxstate, 0);
    } else {
        mqc->c -= mqc->a << 16;
        return exchange(mqc, cxstate, 1);
    }
}
