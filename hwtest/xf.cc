/*
 * Copyright (C) 2018 Marcin Kościelnicki <koriakin@0x04.net>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pgraph.h"
#include "nva.h"
#include "nvhw/fp.h"
#include "nvhw/xf.h"

namespace hwtest {
namespace pgraph {

class XfVecTest : public StateTest {
protected:
	void adjust_orig() override {
		int vop = rnd() & 0xf;
		uint32_t opa = 0;
		uint32_t opb = 0;
		uint32_t opc = 0;
		uint32_t src[3] = {
			(uint32_t)((rnd() & 0x7fc0) | 0x0001),
			(uint32_t)((rnd() & 0x7fc0) | 0x0005),
			(uint32_t)((rnd() & 0x7fc0) | 0x0003),
		};
		insrt(opa, 0, 1, 1);
		insrt(opa, 3, 8, 0x63);
		insrt(opa, 12, 4, rnd() & 0xf);
		insrt(opa, 28, 4, src[2]);
		insrt(opb, 0, 11, src[2] >> 4);
		insrt(opb, 11, 15, src[1]);
		insrt(opb, 26, 6, src[0]);
		insrt(opc, 0, 9, src[0] >> 6);
		insrt(opc, 13, 8, 0x62);
		insrt(opc, 21, 4, vop);
		orig.xfpr[0][0] = 0x0f000000;
		orig.xfpr[0][1] = 0x0c000000;
		orig.xfpr[0][2] = 0x002c001b;
		orig.xfpr[1][0] = 0x0f100000;
		orig.xfpr[1][1] = 0x0c000000;
		orig.xfpr[1][2] = 0x002c201b;
		orig.xfpr[2][0] = opa;
		orig.xfpr[2][1] = opb;
		orig.xfpr[2][2] = opc;
		insrt(orig.xf_mode_a, 8, 8, 0);

		/* Have some fun. */
		for (int i = 0x60; i < 0x63; i++)
			for (int j = 0; j < 4; j++) {
				switch (rnd() & 0xf) {
					case 0x0:
						orig.xfctx[i][j] &= 0x807fffff;
						break;
					case 0x1:
						orig.xfctx[i][j] &= ~0x7fffff;
						break;
					case 0x2:
						orig.xfctx[i][j] &= 0x80000000;
						orig.xfctx[i][j] |= 0x7f800000;
						break;
					case 0x3:
						orig.xfctx[i][j] |= 0x7f800000;
						break;
				}
			}

		/* Ensure clear path. */
		insrt(orig.idx_state_a, 20, 4, 0);
		orig.fd_state_unk18 = 0;
		orig.fd_state_unk20 = 0;
		orig.fd_state_unk30 = 0;
		if (extr(orig.idx_state_a, 20, 4) == 0xf)
			insrt(orig.idx_state_a, 20, 4, 0);
		insrt(orig.idx_state_b, 16, 5, 0);
		insrt(orig.idx_state_b, 24, 5, 0);
		orig.debug_d &= 0xefffffff;
	}
	void mutate() override {
		nva_wr32(cnum, 0x400f50, 0x16000);
		nva_wr32(cnum, 0x400f54, 0);
		pgraph_xf_cmd(&exp, 6, 0, 1, 0, 0);
		uint32_t src[3][4];
		uint32_t vres[4] = {0};
		uint32_t iwa = orig.xfpr[2][0];
		uint32_t iwb = orig.xfpr[2][1];
		uint32_t iwc = orig.xfpr[2][2];
		int wm = extr(iwa, 12, 4);
		int vop = extr(iwc, 21, 4);
		uint32_t iws[3] = {
			(uint32_t)(extr(iwb, 26, 6) | extr(iwc, 0, 9) << 6),
			(uint32_t)extr(iwb, 11, 15),
			(uint32_t)(extr(iwa, 28, 4) | extr(iwb, 0, 11) << 4),
		};
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++) {
				src[i][j] = orig.xfctx[0x60+i][extr(iws[i], 12 - j * 2, 2)];
				if (extr(iws[i], 14, 1))
					src[i][j] ^= 0x80000000;
#if 0
#endif
			}
		switch (vop) {
			case 0x00:
			default:
				/* NOP */
				wm = 0;
				break;
			case 0x01:
				/* MOV */
				for (int i = 0; i < 4; i++)
					vres[i] = src[0][i];
				break;
			case 0x02:
				/* MUL */
				for (int i = 0; i < 4; i++)
					vres[i] = fp32_mul(src[0][i], src[1][i], FP_RZ, true);
				break;
			case 0x03:
				/* ADD */
				for (int i = 0; i < 4; i++)
					vres[i] = xf_add(src[0][i], src[2][i]);
				break;
			case 0x04:
				/* MAD */
				for (int i = 0; i < 4; i++)
					vres[i] = xf_add(fp32_mul(src[0][i], src[1][i], FP_RZ, true), src[2][i]);
				break;
			case 0x05:
				/* DP3 */
				for (int i = 0; i < 3; i++)
					vres[i] = fp32_mul(src[0][i], src[1][i], FP_RZ, true);
				vres[0] = vres[1] = vres[2] = vres[3] = xf_sum3(vres);
				break;
			case 0x06:
				/* DPH */
				for (int i = 0; i < 3; i++)
					vres[i] = fp32_mul(src[0][i], src[1][i], FP_RZ, true);
				vres[3] = fp32_mul(FP32_ONE, src[1][3], FP_RZ, true);
				vres[0] = vres[1] = vres[2] = vres[3] = xf_sum4(vres);
				break;
			case 0x07:
				/* DP4 */
				for (int i = 0; i < 4; i++)
					vres[i] = fp32_mul(src[0][i], src[1][i], FP_RZ, true);
				vres[0] = vres[1] = vres[2] = vres[3] = xf_sum4(vres);
				break;
			case 0x08:
				/* DST */
				vres[0] = FP32_ONE;
				vres[1] = fp32_mul(src[0][1], src[1][1], FP_RZ, true);
				vres[2] = src[0][2];
				vres[3] = src[1][3];
				break;
			case 0x09:
				/* MIN */
				for (int i = 0; i < 4; i++)
					vres[i] = xf_min(src[0][i], src[1][i]);
				break;
			case 0x0a:
				/* MAX */
				for (int i = 0; i < 4; i++)
					vres[i] = xf_max(src[0][i], src[1][i]);
				break;
			case 0x0b:
				/* SLT */
				for (int i = 0; i < 4; i++)
					vres[i] = xf_islt(src[0][i], src[1][i]) ? FP32_ONE : 0;
				break;
			case 0x0c:
				/* SGE */
				for (int i = 0; i < 4; i++)
					vres[i] = xf_islt(src[0][i], src[1][i]) ? 0 : FP32_ONE;
				break;
		}
		for (int i = 0; i < 4; i++)
			if (wm & 1 << (3 - i))
				exp.xfctx[0x63][i] = vres[i];

	}
public:
	using StateTest::StateTest;
};

class XfScaTest : public StateTest {
protected:
	void adjust_orig() override {
		int sop = rnd() & 7;
		uint32_t opa = 0;
		uint32_t opb = 0;
		uint32_t opc = 0;
		uint32_t src[3] = {
			(uint32_t)((rnd() & 0x7fc0) | 0x0001),
			(uint32_t)((rnd() & 0x7fc0) | 0x0005),
			(uint32_t)((rnd() & 0x7fc0) | 0x0003),
		};
		insrt(opa, 0, 1, 1);
		insrt(opa, 3, 8, 0x63);
		insrt(opa, 12, 4, rnd() & 0xf);
		insrt(opa, 13, 1, 1);
		insrt(opa, 28, 4, src[2]);
		insrt(opb, 0, 11, src[2] >> 4);
		insrt(opb, 11, 15, src[1]);
		insrt(opb, 26, 6, src[0]);
		insrt(opc, 0, 9, src[0] >> 6);
		insrt(opc, 13, 8, 0x62);
		insrt(opc, 25, 3, sop);
		orig.xfpr[0][0] = 0x0f000000;
		orig.xfpr[0][1] = 0x0c000000;
		orig.xfpr[0][2] = 0x002c001b;
		orig.xfpr[1][0] = 0x0f100000;
		orig.xfpr[1][1] = 0x0c000000;
		orig.xfpr[1][2] = 0x002c201b;
		orig.xfpr[2][0] = opa;
		orig.xfpr[2][1] = opb;
		orig.xfpr[2][2] = opc;
		insrt(orig.xf_mode_a, 8, 8, 0);

		/* Have some fun. */
		for (int i = 0x60; i < 0x63; i++)
			for (int j = 0; j < 4; j++) {
				switch (rnd() & 0xf) {
					case 0x0:
						orig.xfctx[i][j] &= 0x807fffff;
						break;
					case 0x1:
						orig.xfctx[i][j] &= ~0x7fffff;
						break;
					case 0x2:
						orig.xfctx[i][j] &= 0x80000000;
						orig.xfctx[i][j] |= 0x7f800000;
						break;
					case 0x3:
						orig.xfctx[i][j] |= 0x7f800000;
						break;
					case 0x4:
					case 0x5:
					case 0x6:
					case 0x7:
						insrt(orig.xfctx[i][j], 23, 9, (rnd() & 7) + 0x7e);
						break;
				}
			}

		/* Ensure clear path. */
		insrt(orig.idx_state_a, 20, 4, 0);
		orig.fd_state_unk18 = 0;
		orig.fd_state_unk20 = 0;
		orig.fd_state_unk30 = 0;
		if (extr(orig.idx_state_a, 20, 4) == 0xf)
			insrt(orig.idx_state_a, 20, 4, 0);
		insrt(orig.idx_state_b, 16, 5, 0);
		insrt(orig.idx_state_b, 24, 5, 0);
		orig.debug_d &= 0xefffffff;
	}
	void mutate() override {
		nva_wr32(cnum, 0x400f50, 0x16000);
		nva_wr32(cnum, 0x400f54, 0);
		pgraph_xf_cmd(&exp, 6, 0, 1, 0, 0);
		uint32_t src[3][4];
		uint32_t sres[4] = {0};
		uint32_t iwa = orig.xfpr[2][0];
		uint32_t iwb = orig.xfpr[2][1];
		uint32_t iwc = orig.xfpr[2][2];
		int wm = extr(iwa, 12, 4);
		int sop = extr(iwc, 25, 3);
		uint32_t iws[3] = {
			(uint32_t)(extr(iwb, 26, 6) | extr(iwc, 0, 9) << 6),
			(uint32_t)extr(iwb, 11, 15),
			(uint32_t)(extr(iwa, 28, 4) | extr(iwb, 0, 11) << 4),
		};
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++) {
				src[i][j] = orig.xfctx[0x60+i][extr(iws[i], 12 - j * 2, 2)];
				if (extr(iws[i], 14, 1))
					src[i][j] ^= 0x80000000;
#if 0
#endif
			}
		switch (sop) {
			case 0x00:
			default:
				/* NOP */
				wm = 0;
				break;
			case 0x01:
				/* MOV */
				for (int i = 0; i < 4; i++)
					sres[i] = src[2][i];
				break;
			case 0x02:
				/* RCP */
				sres[0] = xf_rcp(src[2][0], false, true);
				for (int i = 0; i < 4; i++)
					sres[i] = sres[0];
				break;
			case 0x03:
				/* RCC */
				sres[0] = xf_rcp(src[2][0], true, true);
				for (int i = 0; i < 4; i++)
					sres[i] = sres[0];
				break;
			case 0x04:
				/* RSQ */
				sres[0] = xf_rsq(src[2][0], true);
				for (int i = 0; i < 4; i++)
					sres[i] = sres[0];
				break;
			case 0x05:
				/* EXP */
				sres[0] = xf_exp_flr(src[2][0]);
				sres[1] = xf_exp_frc(src[2][0]);
				sres[2] = xf_exp(src[2][0]);
				sres[3] = FP32_ONE;
				break;
			case 0x06:
				/* LOG */
				sres[0] = xf_log_e(src[2][0]);
				sres[1] = xf_log_f(src[2][0]);
				sres[2] = xf_log(src[2][0]);
				sres[3] = FP32_ONE;
				break;
			case 0x07:
				xf_lit(sres, src[2]);
				break;
		}
		for (int i = 0; i < 4; i++)
			if (wm & 1 << (3 - i))
				exp.xfctx[0x63][i] = sres[i];

	}
public:
	using StateTest::StateTest;
};

bool XfTests::supported() {
	return chipset.card_type == 0x20;
}

Test::Subtests XfTests::subtests() {
	return {
		{"vec", new XfVecTest(opt, rnd())},
		{"sca", new XfScaTest(opt, rnd())},
	};
}

}
}
