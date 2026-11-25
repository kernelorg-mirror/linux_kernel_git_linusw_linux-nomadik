// SPDX-License-Identifier: GPL-2.0-only
/*
 * DB8500 PRCMU clock register handling
 *
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Authors: Kumar Sanghvi <kumar.sanghvi@stericsson.com>
 *          Sundar Iyer <sundar.iyer@stericsson.com>
 *          Mattias Nilsson <mattias.i.nilsson@stericsson.com>
 */

#include <linux/bits.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/mfd/db8500-prcmu.h>
#include <linux/spinlock.h>

#include "clk-prcmu-hw.h"

/* Offsets in the shared PRCMU register window. */
#define PRCM_SGACLK_MGT		(0x014)
#define PRCM_UARTCLK_MGT	(0x018)
#define PRCM_MSP02CLK_MGT	(0x01C)
#define PRCM_I2CCLK_MGT		(0x020)
#define PRCM_SDMMCCLK_MGT	(0x024)
#define PRCM_SLIMCLK_MGT	(0x028)
#define PRCM_PER1CLK_MGT	(0x02C)
#define PRCM_PER2CLK_MGT	(0x030)
#define PRCM_PER3CLK_MGT	(0x034)
#define PRCM_PER5CLK_MGT	(0x038)
#define PRCM_PER6CLK_MGT	(0x03C)
#define PRCM_PER7CLK_MGT	(0x040)
#define PRCM_LCDCLK_MGT		(0x044)
#define PRCM_BMLCLK_MGT		(0x04C)
#define PRCM_HSITXCLK_MGT	(0x050)
#define PRCM_HSIRXCLK_MGT	(0x054)
#define PRCM_HDMICLK_MGT	(0x058)
#define PRCM_APEATCLK_MGT	(0x05C)
#define PRCM_APETRACECLK_MGT	(0x060)
#define PRCM_MCDECLK_MGT	(0x064)
#define PRCM_IPI2CCLK_MGT	(0x068)
#define PRCM_DSIALTCLK_MGT	(0x06C)
#define PRCM_DMACLK_MGT		(0x074)
#define PRCM_B2R2CLK_MGT	(0x078)
#define PRCM_TVCLK_MGT		(0x07C)
#define PRCM_SSPCLK_MGT		(0x280)
#define PRCM_RNGCLK_MGT		(0x284)
#define PRCM_UICCCLK_MGT	(0x27C)
#define PRCM_MSP1CLK_MGT	(0x288)
#define PRCM_ARM_CHGCLKREQ	0x114
#define PRCM_ARM_CHGCLKREQ_PRCM_ARM_CHGCLKREQ	BIT(0)
#define PRCM_ARM_CHGCLKREQ_PRCM_ARM_DIVSEL	BIT(16)
#define PRCM_ARMCLKFIX_MGT	0x0
#define PRCM_PLLSOC0_FREQ	   0x080
#define PRCM_PLLSOC1_FREQ	   0x084
#define PRCM_PLLARM_FREQ	   0x088
#define PRCM_PLLDDR_FREQ	   0x08C
#define PRCM_PLL_FREQ_D_MASK	GENMASK(7, 0)
#define PRCM_PLL_FREQ_N_MASK	GENMASK(13, 8)
#define PRCM_PLL_FREQ_R_MASK	GENMASK(18, 16)
#define PRCM_PLL_FREQ_SELDIV2	BIT(24)
#define PRCM_PLL_FREQ_DIV2EN	BIT(25)
#define PRCM_SEM                   0x400
#define PRCM_SEM_PRCM_SEM BIT(0)
#define PRCM_TCR                   0x1C8
#define PRCM_TCR_TENSEL_MASK       GENMASK(7, 0)
#define PRCM_TCR_STOP_TIMERS       BIT(16)
#define PRCM_TCR_DOZE_MODE         BIT(17)
#define PRCM_CLK_MGT_CLKPLLDIV_MASK		GENMASK(4, 0)
#define PRCM_CLK_MGT_CLKPLLSW_SOC0		BIT(5)
#define PRCM_CLK_MGT_CLKPLLSW_SOC1		BIT(6)
#define PRCM_CLK_MGT_CLKPLLSW_DDR		BIT(7)
#define PRCM_CLK_MGT_CLKPLLSW_MASK		GENMASK(7, 5)
#define PRCM_CLK_MGT_CLKEN			BIT(8)
#define PRCM_CLK_MGT_CLK38			BIT(9)
#define PRCM_CLK_MGT_CLK38DIV			BIT(11)
#define PRCM_SGACLK_MGT_SGACLKDIV_BY_2_5_EN	BIT(12)
#define PRCM_CGATING_BYPASS		0x134
#define PRCM_CGATING_BYPASS_ICN2	BIT(6)

struct clk_mgt {
	u32 offset;
	u32 pllsw;
	int branch;
	bool clk38div;
};

static DEFINE_SPINLOCK(clk_mgt_lock);

#define CLK_MGT_ENTRY(_name, _branch, _clk38div)[PRCMU_##_name] = \
	{ PRCM_##_name##_MGT, 0, _branch, _clk38div }
static struct clk_mgt clk_mgt[PRCMU_NUM_REG_CLOCKS] = {
	CLK_MGT_ENTRY(SGACLK, PLL_DIV, false),
	CLK_MGT_ENTRY(UARTCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(MSP02CLK, PLL_FIX, true),
	CLK_MGT_ENTRY(MSP1CLK, PLL_FIX, true),
	CLK_MGT_ENTRY(I2CCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(SDMMCCLK, PLL_DIV, true),
	CLK_MGT_ENTRY(SLIMCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(PER1CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(PER2CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(PER3CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(PER5CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(PER6CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(PER7CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(LCDCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(BMLCLK, PLL_DIV, true),
	CLK_MGT_ENTRY(HSITXCLK, PLL_DIV, true),
	CLK_MGT_ENTRY(HSIRXCLK, PLL_DIV, true),
	CLK_MGT_ENTRY(HDMICLK, PLL_FIX, false),
	CLK_MGT_ENTRY(APEATCLK, PLL_DIV, true),
	CLK_MGT_ENTRY(APETRACECLK, PLL_DIV, true),
	CLK_MGT_ENTRY(MCDECLK, PLL_DIV, true),
	CLK_MGT_ENTRY(IPI2CCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(DSIALTCLK, PLL_FIX, false),
	CLK_MGT_ENTRY(DMACLK, PLL_DIV, true),
	CLK_MGT_ENTRY(B2R2CLK, PLL_DIV, true),
	CLK_MGT_ENTRY(TVCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(SSPCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(RNGCLK, PLL_FIX, true),
	CLK_MGT_ENTRY(UICCCLK, PLL_FIX, false),
};

static bool prcmu_is_ulppll_disabled(void)
{
	struct prcmu_fw_version *ver;

	ver = prcmu_get_fw_version();
	return ver && ver->project == PRCMU_FW_PROJECT_U8420_SYSCLK;
}

static int request_timclk(bool enable)
{
	u32 val;

	/*
	 * On the U8420_CLKSEL firmware, the ULP (Ultra Low Power)
	 * PLL is disabled so we cannot use doze mode, this will
	 * stop the clock on this firmware.
	 */
	if (prcmu_is_ulppll_disabled())
		val = 0;
	else
		val = (PRCM_TCR_DOZE_MODE | PRCM_TCR_TENSEL_MASK);

	if (!enable)
		val |= PRCM_TCR_STOP_TIMERS |
			PRCM_TCR_DOZE_MODE |
			PRCM_TCR_TENSEL_MASK;

	db8500_prcmu_write(PRCM_TCR, val);

	return 0;
}

static int request_clock(u8 clock, bool enable)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&clk_mgt_lock, flags);

	/* Grab the HW semaphore. */
	while ((db8500_prcmu_read(PRCM_SEM) & PRCM_SEM_PRCM_SEM) != 0)
		cpu_relax();

	val = db8500_prcmu_read(clk_mgt[clock].offset);
	if (enable) {
		val |= (PRCM_CLK_MGT_CLKEN | clk_mgt[clock].pllsw);
	} else {
		clk_mgt[clock].pllsw = (val & PRCM_CLK_MGT_CLKPLLSW_MASK);
		val &= ~(PRCM_CLK_MGT_CLKEN | PRCM_CLK_MGT_CLKPLLSW_MASK);
	}
	db8500_prcmu_write(clk_mgt[clock].offset, val);

	/* Release the HW semaphore. */
	db8500_prcmu_write(PRCM_SEM, 0);

	spin_unlock_irqrestore(&clk_mgt_lock, flags);

	return 0;
}

static int request_sga_clock(u8 clock, bool enable)
{
	u32 val;
	int ret;

	if (enable) {
		val = db8500_prcmu_read(PRCM_CGATING_BYPASS);
		db8500_prcmu_write(PRCM_CGATING_BYPASS, val | PRCM_CGATING_BYPASS_ICN2);
	}

	ret = request_clock(clock, enable);

	if (!ret && !enable) {
		val = db8500_prcmu_read(PRCM_CGATING_BYPASS);
		db8500_prcmu_write(PRCM_CGATING_BYPASS, val & ~PRCM_CGATING_BYPASS_ICN2);
	}

	return ret;
}

int ux500_prcmu_clk_request(u8 clock, bool enable)
{
	if (clock == PRCMU_SGACLK)
		return request_sga_clock(clock, enable);
	else if (clock < PRCMU_NUM_REG_CLOCKS)
		return request_clock(clock, enable);
	else if (clock == PRCMU_TIMCLK)
		return request_timclk(enable);
	else if ((clock == PRCMU_DSI0CLK) || (clock == PRCMU_DSI1CLK))
		return ux500_prcmu_request_dsiclk((clock - PRCMU_DSI0CLK), enable);
	else if (clock >= PRCMU_DSI0ESCCLK && clock <= PRCMU_DSI2ESCCLK)
		return ux500_prcmu_request_dsiescclk((clock - PRCMU_DSI0ESCCLK), enable);
	else if (clock == PRCMU_PLLDSI)
		return ux500_prcmu_request_plldsi(enable);
	else if (clock == PRCMU_SYSCLK)
		return db8500_prcmu_request_sysclk(enable);
	else if ((clock == PRCMU_PLLSOC0) || (clock == PRCMU_PLLSOC1))
		return db8500_prcmu_request_pll(clock, enable);
	else
		return -EINVAL;
}

unsigned long ux500_prcmu_pll_rate(unsigned int reg, unsigned long src_rate,
				  int branch)
{
	u64 rate;
	u32 val;
	u32 d;
	u32 div = 1;

	val = db8500_prcmu_read(reg);

	rate = src_rate;
	rate *= ((val & PRCM_PLL_FREQ_D_MASK) >> PRCM_PLL_FREQ_D_SHIFT);

	d = ((val & PRCM_PLL_FREQ_N_MASK) >> PRCM_PLL_FREQ_N_SHIFT);
	if (d > 1)
		div *= d;

	d = ((val & PRCM_PLL_FREQ_R_MASK) >> PRCM_PLL_FREQ_R_SHIFT);
	if (d > 1)
		div *= d;

	if (val & PRCM_PLL_FREQ_SELDIV2)
		div *= 2;

	if ((branch == PLL_FIX) || ((branch == PLL_DIV) &&
		(val & PRCM_PLL_FREQ_DIV2EN) &&
		((reg == PRCM_PLLSOC0_FREQ) ||
		 (reg == PRCM_PLLARM_FREQ) ||
		 (reg == PRCM_PLLDDR_FREQ))))
		div *= 2;

	(void)do_div(rate, div);

	return (unsigned long)rate;
}

#define ROOT_CLOCK_RATE 38400000

static unsigned long clock_rate(u8 clock)
{
	u32 val;
	u32 pllsw;
	unsigned long rate = ROOT_CLOCK_RATE;

	val = db8500_prcmu_read(clk_mgt[clock].offset);

	if (val & PRCM_CLK_MGT_CLK38) {
		if (clk_mgt[clock].clk38div && (val & PRCM_CLK_MGT_CLK38DIV))
			rate /= 2;
		return rate;
	}

	val |= clk_mgt[clock].pllsw;
	pllsw = (val & PRCM_CLK_MGT_CLKPLLSW_MASK);

	if (pllsw == PRCM_CLK_MGT_CLKPLLSW_SOC0)
		rate = ux500_prcmu_pll_rate(PRCM_PLLSOC0_FREQ, rate, clk_mgt[clock].branch);
	else if (pllsw == PRCM_CLK_MGT_CLKPLLSW_SOC1)
		rate = ux500_prcmu_pll_rate(PRCM_PLLSOC1_FREQ, rate, clk_mgt[clock].branch);
	else if (pllsw == PRCM_CLK_MGT_CLKPLLSW_DDR)
		rate = ux500_prcmu_pll_rate(PRCM_PLLDDR_FREQ, rate, clk_mgt[clock].branch);
	else
		return 0;

	if ((clock == PRCMU_SGACLK) &&
		(val & PRCM_SGACLK_MGT_SGACLKDIV_BY_2_5_EN)) {
		u64 r = (rate * 10);

		(void)do_div(r, 25);
		return (unsigned long)r;
	}
	val &= PRCM_CLK_MGT_CLKPLLDIV_MASK;
	if (val)
		return rate / val;
	else
		return 0;
}

static unsigned long armss_rate(void)
{
	u32 r;
	unsigned long rate;

	r = db8500_prcmu_read(PRCM_ARM_CHGCLKREQ);

	if (r & PRCM_ARM_CHGCLKREQ_PRCM_ARM_CHGCLKREQ) {
		/* External ARMCLKFIX clock */

		rate = ux500_prcmu_pll_rate(PRCM_PLLDDR_FREQ, ROOT_CLOCK_RATE, PLL_FIX);

		/* Check PRCM_ARM_CHGCLKREQ divider */
		if (!(r & PRCM_ARM_CHGCLKREQ_PRCM_ARM_DIVSEL))
			rate /= 2;

		/* Check PRCM_ARMCLKFIX_MGT divider */
		r = db8500_prcmu_read(PRCM_ARMCLKFIX_MGT);
		r &= PRCM_CLK_MGT_CLKPLLDIV_MASK;
		rate /= r;

	} else {/* ARM PLL */
		rate = ux500_prcmu_pll_rate(PRCM_PLLARM_FREQ, ROOT_CLOCK_RATE, PLL_DIV);
	}

	return rate;
}

unsigned long ux500_prcmu_clk_rate(u8 clock)
{
	if (clock < PRCMU_NUM_REG_CLOCKS)
		return clock_rate(clock);
	else if (clock == PRCMU_TIMCLK)
		return prcmu_is_ulppll_disabled() ?
			32768 : ROOT_CLOCK_RATE / 16;
	else if (clock == PRCMU_SYSCLK)
		return ROOT_CLOCK_RATE;
	else if (clock == PRCMU_PLLSOC0)
		return ux500_prcmu_pll_rate(PRCM_PLLSOC0_FREQ, ROOT_CLOCK_RATE, PLL_RAW);
	else if (clock == PRCMU_PLLSOC1)
		return ux500_prcmu_pll_rate(PRCM_PLLSOC1_FREQ, ROOT_CLOCK_RATE, PLL_RAW);
	else if (clock == PRCMU_ARMSS)
		return armss_rate();
	else if (clock == PRCMU_PLLDDR)
		return ux500_prcmu_pll_rate(PRCM_PLLDDR_FREQ, ROOT_CLOCK_RATE, PLL_RAW);
	else if (clock == PRCMU_PLLDSI)
		return ux500_prcmu_plldsi_rate();
	else if ((clock == PRCMU_DSI0CLK) || (clock == PRCMU_DSI1CLK))
		return ux500_prcmu_dsiclk_rate(clock - PRCMU_DSI0CLK);
	else if (clock >= PRCMU_DSI0ESCCLK && clock <= PRCMU_DSI2ESCCLK)
		return ux500_prcmu_dsiescclk_rate(clock - PRCMU_DSI0ESCCLK);
	else
		return 0;
}

static unsigned long clock_source_rate(u32 clk_mgt_val, int branch)
{
	if (clk_mgt_val & PRCM_CLK_MGT_CLK38)
		return ROOT_CLOCK_RATE;
	clk_mgt_val &= PRCM_CLK_MGT_CLKPLLSW_MASK;
	if (clk_mgt_val == PRCM_CLK_MGT_CLKPLLSW_SOC0)
		return ux500_prcmu_pll_rate(PRCM_PLLSOC0_FREQ, ROOT_CLOCK_RATE, branch);
	else if (clk_mgt_val == PRCM_CLK_MGT_CLKPLLSW_SOC1)
		return ux500_prcmu_pll_rate(PRCM_PLLSOC1_FREQ, ROOT_CLOCK_RATE, branch);
	else if (clk_mgt_val == PRCM_CLK_MGT_CLKPLLSW_DDR)
		return ux500_prcmu_pll_rate(PRCM_PLLDDR_FREQ, ROOT_CLOCK_RATE, branch);
	else
		return 0;
}

u32 ux500_prcmu_clk_divider(unsigned long src_rate, unsigned long rate)
{
	u32 div;

	div = (src_rate / rate);
	if (div == 0)
		return 1;
	if (rate < (src_rate / div))
		div++;
	return div;
}

static long round_clock_rate(u8 clock, unsigned long rate)
{
	u32 val;
	u32 div;
	unsigned long src_rate;
	long rounded_rate;

	val = db8500_prcmu_read(clk_mgt[clock].offset);
	src_rate = clock_source_rate((val | clk_mgt[clock].pllsw),
		clk_mgt[clock].branch);
	div = ux500_prcmu_clk_divider(src_rate, rate);
	if (val & PRCM_CLK_MGT_CLK38) {
		if (clk_mgt[clock].clk38div) {
			if (div > 2)
				div = 2;
		} else {
			div = 1;
		}
	} else if ((clock == PRCMU_SGACLK) && (div == 3)) {
		u64 r = (src_rate * 10);

		(void)do_div(r, 25);
		if (r <= rate)
			return (unsigned long)r;
	}
	rounded_rate = (src_rate / min_t(u32, div, 31));

	return rounded_rate;
}

long ux500_prcmu_clk_round_rate(u8 clock, unsigned long rate)
{
	if (clock < PRCMU_NUM_REG_CLOCKS)
		return round_clock_rate(clock, rate);
	else if (clock == PRCMU_PLLDSI)
		return ux500_prcmu_round_plldsi_rate(rate);
	else if ((clock == PRCMU_DSI0CLK) || (clock == PRCMU_DSI1CLK))
		return ux500_prcmu_round_dsiclk_rate(rate);
	else if (clock >= PRCMU_DSI0ESCCLK && clock <= PRCMU_DSI2ESCCLK)
		return ux500_prcmu_round_dsiescclk_rate(rate);
	else
		return (long)ux500_prcmu_clk_rate(clock);
}

static void set_clock_rate(u8 clock, unsigned long rate)
{
	u32 val;
	u32 div;
	unsigned long src_rate;
	unsigned long flags;

	spin_lock_irqsave(&clk_mgt_lock, flags);

	/* Grab the HW semaphore. */
	while ((db8500_prcmu_read(PRCM_SEM) & PRCM_SEM_PRCM_SEM) != 0)
		cpu_relax();

	val = db8500_prcmu_read(clk_mgt[clock].offset);
	src_rate = clock_source_rate((val | clk_mgt[clock].pllsw),
		clk_mgt[clock].branch);
	div = ux500_prcmu_clk_divider(src_rate, rate);
	if (val & PRCM_CLK_MGT_CLK38) {
		if (clk_mgt[clock].clk38div) {
			if (div > 1)
				val |= PRCM_CLK_MGT_CLK38DIV;
			else
				val &= ~PRCM_CLK_MGT_CLK38DIV;
		}
	} else if (clock == PRCMU_SGACLK) {
		val &= ~(PRCM_CLK_MGT_CLKPLLDIV_MASK |
			PRCM_SGACLK_MGT_SGACLKDIV_BY_2_5_EN);
		if (div == 3) {
			u64 r = (src_rate * 10);

			(void)do_div(r, 25);
			if (r <= rate) {
				val |= PRCM_SGACLK_MGT_SGACLKDIV_BY_2_5_EN;
				div = 0;
			}
		}
		val |= min_t(u32, div, 31);
	} else {
		val &= ~PRCM_CLK_MGT_CLKPLLDIV_MASK;
		val |= min_t(u32, div, 31);
	}
	db8500_prcmu_write(clk_mgt[clock].offset, val);

	/* Release the HW semaphore. */
	db8500_prcmu_write(PRCM_SEM, 0);

	spin_unlock_irqrestore(&clk_mgt_lock, flags);
}

int ux500_prcmu_clk_set_rate(u8 clock, unsigned long rate)
{
	if (clock < PRCMU_NUM_REG_CLOCKS)
		set_clock_rate(clock, rate);
	else if (clock == PRCMU_PLLDSI)
		return ux500_prcmu_set_plldsi_rate(rate);
	else if ((clock == PRCMU_DSI0CLK) || (clock == PRCMU_DSI1CLK))
		ux500_prcmu_set_dsiclk_rate((clock - PRCMU_DSI0CLK), rate);
	else if (clock >= PRCMU_DSI0ESCCLK && clock <= PRCMU_DSI2ESCCLK)
		ux500_prcmu_set_dsiescclk_rate((clock - PRCMU_DSI0ESCCLK), rate);
	return 0;
}
