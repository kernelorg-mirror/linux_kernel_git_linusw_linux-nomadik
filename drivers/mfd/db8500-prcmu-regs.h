/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) STMicroelectronics 2009
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Kumar Sanghvi <kumar.sanghvi@stericsson.com>
 * Author: Sundar Iyer <sundar.iyer@stericsson.com>
 *
 * PRCM Unit registers
 */

#ifndef __DB8500_PRCMU_REGS_H
#define __DB8500_PRCMU_REGS_H

#define BITS(_start, _end) ((BIT(_end) - BIT(_start)) + BIT(_end))

#define PRCM_ACLK_MGT		(0x004)
#define PRCM_SVAMMCSPCLK_MGT	(0x008)
#define PRCM_SIAMMDSPCLK_MGT	(0x00C)
#define PRCM_UNIPROCLK_MGT	(0x278)

#define PRCM_ARM_PLLDIVPS	(prcmu_base + 0x118)
#define PRCM_ARM_PLLDIVPS_ARM_BRM_RATE		0x3f
#define PRCM_ARM_PLLDIVPS_MAX_MASK		0xf

#define PRCM_PLLARM_LOCKP       (prcmu_base + 0x0a8)
#define PRCM_PLLARM_LOCKP_PRCM_PLLARM_LOCKP3	0x2

#define PRCM_PLLARM_ENABLE	(prcmu_base + 0x98)
#define PRCM_PLLARM_ENABLE_PRCM_PLLARM_ENABLE	0x1
#define PRCM_PLLARM_ENABLE_PRCM_PLLARM_COUNTON	0x100

#define PRCM_A9PL_FORCE_CLKEN	(prcmu_base + 0x19C)
#define PRCM_A9_RESETN_CLR	(prcmu_base + 0x1f4)
#define PRCM_A9_RESETN_SET	(prcmu_base + 0x1f0)
#define PRCM_ARM_LS_CLAMP	(prcmu_base + 0x30c)
#define PRCM_SRAM_A9		(prcmu_base + 0x308)

#define PRCM_A9PL_FORCE_CLKEN_PRCM_A9PL_FORCE_CLKEN BIT(0)
#define PRCM_A9PL_FORCE_CLKEN_PRCM_A9AXI_FORCE_CLKEN BIT(1)

/* CPU mailbox registers */
#define PRCM_MBOX_CPU_VAL	(prcmu_base + 0x0fc)
#define PRCM_MBOX_CPU_SET	(prcmu_base + 0x100)
#define PRCM_MBOX_CPU_CLR	(prcmu_base + 0x104)

#define PRCM_HOSTACCESS_REQ	(prcmu_base + 0x334)
#define PRCM_HOSTACCESS_REQ_HOSTACCESS_REQ 0x1
#define PRCM_HOSTACCESS_REQ_WAKE_REQ	BIT(16)
#define ARM_WAKEUP_MODEM	0x1

#define PRCM_ARM_IT1_CLR	(prcmu_base + 0x48C)
#define PRCM_ARM_IT1_VAL	(prcmu_base + 0x494)
#define PRCM_HOLD_EVT		(prcmu_base + 0x174)

#define PRCM_MOD_AWAKE_STATUS	(prcmu_base + 0x4A0)
#define PRCM_MOD_AWAKE_STATUS_PRCM_MOD_COREPD_AWAKE	BIT(0)
#define PRCM_MOD_AWAKE_STATUS_PRCM_MOD_AAPD_AWAKE	BIT(1)
#define PRCM_MOD_AWAKE_STATUS_PRCM_MOD_VMODEM_OFF_ISO	BIT(2)

#define PRCM_ITSTATUS0		(prcmu_base + 0x148)
#define PRCM_ITSTATUS1		(prcmu_base + 0x150)
#define PRCM_ITSTATUS2		(prcmu_base + 0x158)
#define PRCM_ITSTATUS3		(prcmu_base + 0x160)
#define PRCM_ITSTATUS4		(prcmu_base + 0x168)
#define PRCM_ITSTATUS5		(prcmu_base + 0x484)
#define PRCM_ITCLEAR5		(prcmu_base + 0x488)
#define PRCM_ARMIT_MASKXP70_IT	(prcmu_base + 0x1018)

/* System reset register */
#define PRCM_APE_SOFTRST	(prcmu_base + 0x228)

/* ePOD and memory power signal control registers */
#define PRCM_EPOD_C_SET            (prcmu_base + 0x410)
#define PRCM_SRAM_LS_SLEEP         (prcmu_base + 0x304)

/* Debug power control unit registers */
#define PRCM_POWER_STATE_SET       (prcmu_base + 0x254)

/* Miscellaneous unit registers */
#define PRCM_DSI_SW_RESET          (prcmu_base + 0x324)
#define PRCM_GPIOCR                (prcmu_base + 0x138)
#define PRCM_GPIOCR_DBG_STM_MOD_CMD1            0x800
#define PRCM_GPIOCR_DBG_UARTMOD_CMD0            0x1

/* GPIOCR register */
#define PRCM_GPIOCR_SPI2_SELECT BIT(23)

#define PRCM_DDR_SUBSYS_APE_MINBW	(prcmu_base + 0x438)

/* Miscellaneous unit registers */
#define PRCM_RESOUTN_SET		(prcmu_base + 0x214)
#define PRCM_RESOUTN_CLR		(prcmu_base + 0x218)

/* System reset register */
#define PRCM_APE_SOFTRST		(prcmu_base + 0x228)

#endif /* __DB8500_PRCMU_REGS_H */
