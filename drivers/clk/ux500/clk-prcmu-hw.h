/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef UX500_CLK_PRCMU_HW_H
#define UX500_CLK_PRCMU_HW_H

#include <linux/types.h>

#define PRCM_PLL_FREQ_D_SHIFT	0
#define PRCM_PLL_FREQ_N_SHIFT	8
#define PRCM_PLL_FREQ_R_SHIFT	16

enum {
	PLL_RAW,
	PLL_FIX,
	PLL_DIV
};

int ux500_prcmu_clk_request(u8 clock, bool enable);
unsigned long ux500_prcmu_clk_rate(u8 clock);
long ux500_prcmu_clk_round_rate(u8 clock, unsigned long rate);
int ux500_prcmu_clk_set_rate(u8 clock, unsigned long rate);

unsigned long ux500_prcmu_pll_rate(unsigned int reg, unsigned long src_rate,
				  int branch);
u32 ux500_prcmu_clk_divider(unsigned long src_rate, unsigned long rate);

int ux500_prcmu_request_plldsi(bool enable);
int ux500_prcmu_request_dsiclk(u8 n, bool enable);
int ux500_prcmu_request_dsiescclk(u8 n, bool enable);
unsigned long ux500_prcmu_dsiclk_rate(u8 n);
unsigned long ux500_prcmu_dsiescclk_rate(u8 n);
unsigned long ux500_prcmu_plldsi_rate(void);
long ux500_prcmu_round_plldsi_rate(unsigned long rate);
long ux500_prcmu_round_dsiclk_rate(unsigned long rate);
long ux500_prcmu_round_dsiescclk_rate(unsigned long rate);
int ux500_prcmu_set_plldsi_rate(unsigned long rate);
void ux500_prcmu_set_dsiclk_rate(u8 n, unsigned long rate);
void ux500_prcmu_set_dsiescclk_rate(u8 n, unsigned long rate);

#endif
