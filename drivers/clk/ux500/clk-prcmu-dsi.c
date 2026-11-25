// SPDX-License-Identifier: GPL-2.0-only
/*
 * DB8500 PRCMU DSI PLL and output clocks
 *
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Authors: Kumar Sanghvi <kumar.sanghvi@stericsson.com>
 *          Sundar Iyer <sundar.iyer@stericsson.com>
 *          Mattias Nilsson <mattias.i.nilsson@stericsson.com>
 */

#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/mfd/db8500-prcmu.h>

#include "clk-prcmu-hw.h"

#define PRCM_PLLDSI_FREQ		0x500
#define PRCM_MMIP_LS_CLAMP_SET     0x420
#define PRCM_MMIP_LS_CLAMP_CLR     0x424
#define PRCM_MMIP_LS_CLAMP_DSIPLL_CLAMP		BIT(11)
#define PRCM_MMIP_LS_CLAMP_DSIPLL_CLAMPI	BIT(22)
#define PRCM_PLLDSI_ENABLE         0x504
#define PRCM_PLLDSI_LOCKP          0x508
#define PRCM_DSI_PLLOUT_SEL        0x530
#define PRCM_DSITVCLK_DIV          0x52C
#define PRCM_APE_RESETN_SET        0x1E4
#define PRCM_APE_RESETN_CLR        0x1E8
#define PRCM_PLLDSI_ENABLE_PRCM_PLLDSI_ENABLE BIT(0)
#define PRCM_PLLDSI_LOCKP_PRCM_PLLDSI_LOCKP10	BIT(0)
#define PRCM_PLLDSI_LOCKP_PRCM_PLLDSI_LOCKP3	BIT(1)
#define PRCM_DSI_PLLOUT_SEL_DSI0_PLLOUT_DIVSEL_SHIFT	0
#define PRCM_DSI_PLLOUT_SEL_DSI0_PLLOUT_DIVSEL_MASK	GENMASK(2, 0)
#define PRCM_DSI_PLLOUT_SEL_DSI1_PLLOUT_DIVSEL_SHIFT	8
#define PRCM_DSI_PLLOUT_SEL_DSI1_PLLOUT_DIVSEL_MASK	GENMASK(10, 8)
#define PRCM_DSI_PLLOUT_SEL_OFF		0
#define PRCM_DSI_PLLOUT_SEL_PHI		1
#define PRCM_DSI_PLLOUT_SEL_PHI_2	2
#define PRCM_DSI_PLLOUT_SEL_PHI_4	3
#define PRCM_DSITVCLK_DIV_DSI0_ESC_CLK_DIV_SHIFT	0
#define PRCM_DSITVCLK_DIV_DSI0_ESC_CLK_DIV_MASK		GENMASK(7, 0)
#define PRCM_DSITVCLK_DIV_DSI1_ESC_CLK_DIV_SHIFT	8
#define PRCM_DSITVCLK_DIV_DSI1_ESC_CLK_DIV_MASK		GENMASK(15, 8)
#define PRCM_DSITVCLK_DIV_DSI2_ESC_CLK_DIV_SHIFT	16
#define PRCM_DSITVCLK_DIV_DSI2_ESC_CLK_DIV_MASK		GENMASK(23, 16)
#define PRCM_DSITVCLK_DIV_DSI0_ESC_CLK_EN		BIT(24)
#define PRCM_DSITVCLK_DIV_DSI1_ESC_CLK_EN		BIT(25)
#define PRCM_DSITVCLK_DIV_DSI2_ESC_CLK_EN		BIT(26)
#define PRCM_APE_RESETN_DSIPLL_RESETN BIT(14)

struct dsiclk {
	u32 divsel_mask;
	u32 divsel_shift;
	u32 divsel;
};

static struct dsiclk dsiclk[2] = {
	{
		.divsel_mask = PRCM_DSI_PLLOUT_SEL_DSI0_PLLOUT_DIVSEL_MASK,
		.divsel_shift = PRCM_DSI_PLLOUT_SEL_DSI0_PLLOUT_DIVSEL_SHIFT,
		.divsel = PRCM_DSI_PLLOUT_SEL_PHI,
	},
	{
		.divsel_mask = PRCM_DSI_PLLOUT_SEL_DSI1_PLLOUT_DIVSEL_MASK,
		.divsel_shift = PRCM_DSI_PLLOUT_SEL_DSI1_PLLOUT_DIVSEL_SHIFT,
		.divsel = PRCM_DSI_PLLOUT_SEL_PHI,
	}
};

struct dsiescclk {
	u32 en;
	u32 div_mask;
	u32 div_shift;
};

static struct dsiescclk dsiescclk[3] = {
	{
		.en = PRCM_DSITVCLK_DIV_DSI0_ESC_CLK_EN,
		.div_mask = PRCM_DSITVCLK_DIV_DSI0_ESC_CLK_DIV_MASK,
		.div_shift = PRCM_DSITVCLK_DIV_DSI0_ESC_CLK_DIV_SHIFT,
	},
	{
		.en = PRCM_DSITVCLK_DIV_DSI1_ESC_CLK_EN,
		.div_mask = PRCM_DSITVCLK_DIV_DSI1_ESC_CLK_DIV_MASK,
		.div_shift = PRCM_DSITVCLK_DIV_DSI1_ESC_CLK_DIV_SHIFT,
	},
	{
		.en = PRCM_DSITVCLK_DIV_DSI2_ESC_CLK_EN,
		.div_mask = PRCM_DSITVCLK_DIV_DSI2_ESC_CLK_DIV_MASK,
		.div_shift = PRCM_DSITVCLK_DIV_DSI2_ESC_CLK_DIV_SHIFT,
	}
};

static inline bool plldsi_locked(void)
{
	return (db8500_prcmu_read(PRCM_PLLDSI_LOCKP) &
		(PRCM_PLLDSI_LOCKP_PRCM_PLLDSI_LOCKP10 |
		 PRCM_PLLDSI_LOCKP_PRCM_PLLDSI_LOCKP3)) ==
		(PRCM_PLLDSI_LOCKP_PRCM_PLLDSI_LOCKP10 |
		 PRCM_PLLDSI_LOCKP_PRCM_PLLDSI_LOCKP3);
}

int ux500_prcmu_request_plldsi(bool enable)
{
	u32 clamp = PRCM_MMIP_LS_CLAMP_DSIPLL_CLAMP |
		    PRCM_MMIP_LS_CLAMP_DSIPLL_CLAMPI;
	int r = 0;
	u32 val;

	db8500_prcmu_write(enable ? PRCM_MMIP_LS_CLAMP_CLR :
			   PRCM_MMIP_LS_CLAMP_SET, clamp);

	val = db8500_prcmu_read(PRCM_PLLDSI_ENABLE);
	if (enable)
		val |= PRCM_PLLDSI_ENABLE_PRCM_PLLDSI_ENABLE;
	else
		val &= ~PRCM_PLLDSI_ENABLE_PRCM_PLLDSI_ENABLE;
	db8500_prcmu_write(PRCM_PLLDSI_ENABLE, val);

	if (enable) {
		unsigned int i;
		bool locked = plldsi_locked();

		for (i = 10; !locked && (i > 0); --i) {
			udelay(100);
			locked = plldsi_locked();
		}
		if (locked) {
			db8500_prcmu_write(PRCM_APE_RESETN_SET,
					   PRCM_APE_RESETN_DSIPLL_RESETN);
		} else {
			db8500_prcmu_write(PRCM_MMIP_LS_CLAMP_SET, clamp);
			val &= ~PRCM_PLLDSI_ENABLE_PRCM_PLLDSI_ENABLE;
			db8500_prcmu_write(PRCM_PLLDSI_ENABLE, val);
			r = -EAGAIN;
		}
	} else {
		db8500_prcmu_write(PRCM_APE_RESETN_CLR,
				   PRCM_APE_RESETN_DSIPLL_RESETN);
	}
	return r;
}

int ux500_prcmu_request_dsiclk(u8 n, bool enable)
{
	u32 val;

	val = db8500_prcmu_read(PRCM_DSI_PLLOUT_SEL);
	val &= ~dsiclk[n].divsel_mask;
	val |= ((enable ? dsiclk[n].divsel : PRCM_DSI_PLLOUT_SEL_OFF) <<
		dsiclk[n].divsel_shift);
	db8500_prcmu_write(PRCM_DSI_PLLOUT_SEL, val);
	return 0;
}

int ux500_prcmu_request_dsiescclk(u8 n, bool enable)
{
	u32 val;

	val = db8500_prcmu_read(PRCM_DSITVCLK_DIV);
	enable ? (val |= dsiescclk[n].en) : (val &= ~dsiescclk[n].en);
	db8500_prcmu_write(PRCM_DSITVCLK_DIV, val);
	return 0;
}

unsigned long ux500_prcmu_plldsi_rate(void)
{
	return ux500_prcmu_pll_rate(PRCM_PLLDSI_FREQ,
				   ux500_prcmu_clk_rate(PRCMU_HDMICLK),
				   PLL_RAW);
}

unsigned long ux500_prcmu_dsiclk_rate(u8 n)
{
	u32 divsel;
	u32 div = 1;

	divsel = db8500_prcmu_read(PRCM_DSI_PLLOUT_SEL);
	divsel = ((divsel & dsiclk[n].divsel_mask) >> dsiclk[n].divsel_shift);

	if (divsel == PRCM_DSI_PLLOUT_SEL_OFF)
		divsel = dsiclk[n].divsel;
	else
		dsiclk[n].divsel = divsel;

	switch (divsel) {
	case PRCM_DSI_PLLOUT_SEL_PHI_4:
		div *= 2;
		fallthrough;
	case PRCM_DSI_PLLOUT_SEL_PHI_2:
		div *= 2;
		fallthrough;
	case PRCM_DSI_PLLOUT_SEL_PHI:
		return ux500_prcmu_plldsi_rate() / div;
	default:
		return 0;
	}
}

unsigned long ux500_prcmu_dsiescclk_rate(u8 n)
{
	u32 div;

	div = db8500_prcmu_read(PRCM_DSITVCLK_DIV);
	div = ((div & dsiescclk[n].div_mask) >> (dsiescclk[n].div_shift));
	return ux500_prcmu_clk_rate(PRCMU_TVCLK) / max_t(u32, div, 1);
}

#define MIN_PLL_VCO_RATE 600000000ULL
#define MAX_PLL_VCO_RATE 1680640000ULL

long ux500_prcmu_round_plldsi_rate(unsigned long rate)
{
	long rounded_rate = 0;
	unsigned long src_rate;
	unsigned long rem;
	u32 r;

	src_rate = ux500_prcmu_clk_rate(PRCMU_HDMICLK);
	rem = rate;

	for (r = 7; (rem > 0) && (r > 0); r--) {
		u64 d;

		d = (r * rate);
		(void)do_div(d, src_rate);
		if (d < 6)
			d = 6;
		else if (d > 255)
			d = 255;
		d *= src_rate;
		if (((2 * d) < (r * MIN_PLL_VCO_RATE)) ||
			((r * MAX_PLL_VCO_RATE) < (2 * d)))
			continue;
		(void)do_div(d, r);
		if (rate < d) {
			if (rounded_rate == 0)
				rounded_rate = (long)d;
			break;
		}
		if ((rate - d) < rem) {
			rem = (rate - d);
			rounded_rate = (long)d;
		}
	}
	return rounded_rate;
}

long ux500_prcmu_round_dsiclk_rate(unsigned long rate)
{
	u32 div;
	unsigned long src_rate;
	long rounded_rate;

	src_rate = ux500_prcmu_plldsi_rate();
	div = ux500_prcmu_clk_divider(src_rate, rate);
	rounded_rate = (src_rate / ((div > 2) ? 4 : div));

	return rounded_rate;
}

long ux500_prcmu_round_dsiescclk_rate(unsigned long rate)
{
	u32 div;
	unsigned long src_rate;
	long rounded_rate;

	src_rate = ux500_prcmu_clk_rate(PRCMU_TVCLK);
	div = ux500_prcmu_clk_divider(src_rate, rate);
	rounded_rate = (src_rate / min_t(u32, div, 255));

	return rounded_rate;
}

int ux500_prcmu_set_plldsi_rate(unsigned long rate)
{
	unsigned long src_rate;
	unsigned long rem;
	u32 pll_freq = 0;
	u32 r;

	src_rate = ux500_prcmu_clk_rate(PRCMU_HDMICLK);
	rem = rate;

	for (r = 7; (rem > 0) && (r > 0); r--) {
		u64 d;
		u64 hwrate;

		d = (r * rate);
		(void)do_div(d, src_rate);
		if (d < 6)
			d = 6;
		else if (d > 255)
			d = 255;
		hwrate = (d * src_rate);
		if (((2 * hwrate) < (r * MIN_PLL_VCO_RATE)) ||
			((r * MAX_PLL_VCO_RATE) < (2 * hwrate)))
			continue;
		(void)do_div(hwrate, r);
		if (rate < hwrate) {
			if (pll_freq == 0)
				pll_freq = (((u32)d << PRCM_PLL_FREQ_D_SHIFT) |
					(r << PRCM_PLL_FREQ_R_SHIFT));
			break;
		}
		if ((rate - hwrate) < rem) {
			rem = (rate - hwrate);
			pll_freq = (((u32)d << PRCM_PLL_FREQ_D_SHIFT) |
				(r << PRCM_PLL_FREQ_R_SHIFT));
		}
	}
	if (pll_freq == 0)
		return -EINVAL;

	pll_freq |= (1 << PRCM_PLL_FREQ_N_SHIFT);
	db8500_prcmu_write(PRCM_PLLDSI_FREQ, pll_freq);

	return 0;
}

void ux500_prcmu_set_dsiclk_rate(u8 n, unsigned long rate)
{
	u32 val;
	u32 div;

	div = ux500_prcmu_clk_divider(ux500_prcmu_plldsi_rate(), rate);

	dsiclk[n].divsel = (div == 1) ? PRCM_DSI_PLLOUT_SEL_PHI :
			   (div == 2) ? PRCM_DSI_PLLOUT_SEL_PHI_2 :
			   /* else */	PRCM_DSI_PLLOUT_SEL_PHI_4;

	val = db8500_prcmu_read(PRCM_DSI_PLLOUT_SEL);
	val &= ~dsiclk[n].divsel_mask;
	val |= (dsiclk[n].divsel << dsiclk[n].divsel_shift);
	db8500_prcmu_write(PRCM_DSI_PLLOUT_SEL, val);
}

void ux500_prcmu_set_dsiescclk_rate(u8 n, unsigned long rate)
{
	u32 val;
	u32 div;

	div = ux500_prcmu_clk_divider(ux500_prcmu_clk_rate(PRCMU_TVCLK), rate);
	val = db8500_prcmu_read(PRCM_DSITVCLK_DIV);
	val &= ~dsiescclk[n].div_mask;
	val |= (min_t(u32, div, 255) << dsiescclk[n].div_shift);
	db8500_prcmu_write(PRCM_DSITVCLK_DIV, val);
}
