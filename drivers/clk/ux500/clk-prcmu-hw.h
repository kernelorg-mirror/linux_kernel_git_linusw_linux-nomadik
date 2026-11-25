/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef UX500_CLK_PRCMU_HW_H
#define UX500_CLK_PRCMU_HW_H

#include <linux/types.h>

int ux500_prcmu_clk_request(u8 clock, bool enable);
unsigned long ux500_prcmu_clk_rate(u8 clock);
long ux500_prcmu_clk_round_rate(u8 clock, unsigned long rate);
int ux500_prcmu_clk_set_rate(u8 clock, unsigned long rate);

#endif
