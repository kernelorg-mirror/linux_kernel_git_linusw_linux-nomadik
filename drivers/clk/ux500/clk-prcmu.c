// SPDX-License-Identifier: GPL-2.0-only
/*
 * PRCMU clock implementation for ux500 platform.
 *
 * Copyright (C) 2012 ST-Ericsson SA
 * Author: Ulf Hansson <ulf.hansson@linaro.org>
 */

#include <linux/bits.h>
#include <linux/clk-provider.h>
#include <linux/mfd/db8500-prcmu.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/err.h>
#include "clk.h"
#include "clk-prcmu-hw.h"

#define to_clk_prcmu(_hw) container_of(_hw, struct clk_prcmu, hw)
#define to_clk_prcmu_clkout(_hw) container_of(_hw, struct clk_prcmu_clkout, hw)

struct clk_prcmu {
	struct clk_hw hw;
	u8 cg_sel;
	int opp_requested;
};

struct clk_prcmu_clkout {
	struct clk_hw hw;
	u8 clkout_id;
	u8 source;
	u8 divider;
};

/* PRCMU clock operations. */

static int clk_prcmu_prepare(struct clk_hw *hw)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);

	return ux500_prcmu_clk_request(clk->cg_sel, true);
}

static void clk_prcmu_unprepare(struct clk_hw *hw)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);
	if (ux500_prcmu_clk_request(clk->cg_sel, false))
		pr_err("clk_prcmu: %s failed to disable %s.\n", __func__,
		       clk_hw_get_name(hw));
}

static unsigned long clk_prcmu_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);
	return ux500_prcmu_clk_rate(clk->cg_sel);
}

static const struct {
	unsigned long db8500_rate;
	unsigned long db8520_rate;
	u8 opp;
} armss_opps[] = {
	{  199680000,  199680000, ARM_EXTCLK },
	{  399360000,  399360000, ARM_50_OPP },
	{  798720000,  798720000, ARM_100_OPP },
	{  998400000, 1152000000, ARM_MAX_OPP },
};

static unsigned long clk_prcmu_armss_rate(unsigned int index)
{
	const struct prcmu_fw_version *fw = prcmu_get_fw_version();

	if (fw && fw->project == PRCMU_FW_PROJECT_U8520)
		return armss_opps[index].db8520_rate;

	return armss_opps[index].db8500_rate;
}

static unsigned long clk_prcmu_round_armss_rate(unsigned long rate)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(armss_opps) - 1; i++)
		if (rate <= clk_prcmu_armss_rate(i))
			break;

	return clk_prcmu_armss_rate(i);
}

static int clk_prcmu_set_armss_rate(unsigned long rate)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(armss_opps); i++)
		if (rate == clk_prcmu_armss_rate(i))
			return db8500_prcmu_set_arm_opp(armss_opps[i].opp);

	return -EINVAL;
}

static int clk_prcmu_set_clock_rate(u8 clock, unsigned long rate)
{
	if (clock == PRCMU_ARMSS)
		return clk_prcmu_set_armss_rate(rate);

	return ux500_prcmu_clk_set_rate(clock, rate);
}

static int clk_prcmu_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);

	if (clk->cg_sel == PRCMU_ARMSS)
		req->rate = clk_prcmu_round_armss_rate(req->rate);
	else
		req->rate = ux500_prcmu_clk_round_rate(clk->cg_sel, req->rate);

	return 0;
}

static int clk_prcmu_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);
	return clk_prcmu_set_clock_rate(clk->cg_sel, rate);
}

static int clk_prcmu_opp_prepare(struct clk_hw *hw)
{
	int err;
	struct clk_prcmu *clk = to_clk_prcmu(hw);

	if (!clk->opp_requested) {
		err = prcmu_qos_add_requirement(PRCMU_QOS_APE_OPP,
						(char *)clk_hw_get_name(hw),
						100);
		if (err) {
			pr_err("clk_prcmu: %s fail req APE OPP for %s.\n",
				__func__, clk_hw_get_name(hw));
			return err;
		}
		clk->opp_requested = 1;
	}

	err = ux500_prcmu_clk_request(clk->cg_sel, true);
	if (err) {
		prcmu_qos_remove_requirement(PRCMU_QOS_APE_OPP,
					(char *)clk_hw_get_name(hw));
		clk->opp_requested = 0;
		return err;
	}

	return 0;
}

static void clk_prcmu_opp_unprepare(struct clk_hw *hw)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);

	if (ux500_prcmu_clk_request(clk->cg_sel, false)) {
		pr_err("clk_prcmu: %s failed to disable %s.\n", __func__,
			clk_hw_get_name(hw));
		return;
	}

	if (clk->opp_requested) {
		prcmu_qos_remove_requirement(PRCMU_QOS_APE_OPP,
					(char *)clk_hw_get_name(hw));
		clk->opp_requested = 0;
	}
}

static int clk_prcmu_opp_volt_prepare(struct clk_hw *hw)
{
	int err;
	struct clk_prcmu *clk = to_clk_prcmu(hw);

	if (!clk->opp_requested) {
		err = db8500_prcmu_request_ape_opp_100_voltage(true);
		if (err) {
			pr_err("clk_prcmu: %s fail req APE OPP VOLT for %s.\n",
				__func__, clk_hw_get_name(hw));
			return err;
		}
		clk->opp_requested = 1;
	}

	err = ux500_prcmu_clk_request(clk->cg_sel, true);
	if (err) {
		db8500_prcmu_request_ape_opp_100_voltage(false);
		clk->opp_requested = 0;
		return err;
	}

	return 0;
}

static void clk_prcmu_opp_volt_unprepare(struct clk_hw *hw)
{
	struct clk_prcmu *clk = to_clk_prcmu(hw);

	if (ux500_prcmu_clk_request(clk->cg_sel, false)) {
		pr_err("clk_prcmu: %s failed to disable %s.\n", __func__,
			clk_hw_get_name(hw));
		return;
	}

	if (clk->opp_requested) {
		db8500_prcmu_request_ape_opp_100_voltage(false);
		clk->opp_requested = 0;
	}
}

static const struct clk_ops clk_prcmu_scalable_ops = {
	.prepare = clk_prcmu_prepare,
	.unprepare = clk_prcmu_unprepare,
	.recalc_rate = clk_prcmu_recalc_rate,
	.determine_rate = clk_prcmu_determine_rate,
	.set_rate = clk_prcmu_set_rate,
};

static const struct clk_ops clk_prcmu_gate_ops = {
	.prepare = clk_prcmu_prepare,
	.unprepare = clk_prcmu_unprepare,
	.recalc_rate = clk_prcmu_recalc_rate,
};

static const struct clk_ops clk_prcmu_scalable_rate_ops = {
	.recalc_rate = clk_prcmu_recalc_rate,
	.determine_rate = clk_prcmu_determine_rate,
	.set_rate = clk_prcmu_set_rate,
};

static const struct clk_ops clk_prcmu_rate_ops = {
	.recalc_rate = clk_prcmu_recalc_rate,
};

static const struct clk_ops clk_prcmu_opp_gate_ops = {
	.prepare = clk_prcmu_opp_prepare,
	.unprepare = clk_prcmu_opp_unprepare,
	.recalc_rate = clk_prcmu_recalc_rate,
};

static const struct clk_ops clk_prcmu_opp_volt_scalable_ops = {
	.prepare = clk_prcmu_opp_volt_prepare,
	.unprepare = clk_prcmu_opp_volt_unprepare,
	.recalc_rate = clk_prcmu_recalc_rate,
	.determine_rate = clk_prcmu_determine_rate,
	.set_rate = clk_prcmu_set_rate,
};

static struct clk_hw *clk_reg_prcmu(const char *name,
				    const char *parent_name,
				    u8 cg_sel,
				    unsigned long rate,
				    unsigned long flags,
				    const struct clk_ops *clk_prcmu_ops)
{
	struct clk_init_data clk_prcmu_init = {};
	struct clk_prcmu *clk;
	int ret;

	if (!name) {
		pr_err("clk_prcmu: %s invalid arguments passed\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	clk = kzalloc_obj(*clk);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	clk->cg_sel = cg_sel;
	clk->opp_requested = 0;
	/* "rate" can be used for changing the initial frequency */
	if (rate)
		clk_prcmu_set_clock_rate(cg_sel, rate);

	clk_prcmu_init.name = name;
	clk_prcmu_init.ops = clk_prcmu_ops;
	clk_prcmu_init.flags = flags;
	clk_prcmu_init.parent_names = (parent_name ? &parent_name : NULL);
	clk_prcmu_init.num_parents = (parent_name ? 1 : 0);
	clk->hw.init = &clk_prcmu_init;

	ret = clk_hw_register(NULL, &clk->hw);
	if (ret)
		goto free_clk;

	return &clk->hw;

free_clk:
	kfree(clk);
	pr_err("clk_prcmu: %s failed to register clk\n", __func__);
	return ERR_PTR(-ENOMEM);
}

struct clk_hw *clk_reg_prcmu_scalable(const char *name,
				      const char *parent_name,
				      u8 cg_sel,
				      unsigned long rate,
				      unsigned long flags)
{
	return clk_reg_prcmu(name, parent_name, cg_sel, rate, flags,
			&clk_prcmu_scalable_ops);
}

struct clk_hw *clk_reg_prcmu_gate(const char *name,
				  const char *parent_name,
				  u8 cg_sel,
				  unsigned long flags)
{
	return clk_reg_prcmu(name, parent_name, cg_sel, 0, flags,
			&clk_prcmu_gate_ops);
}

struct clk_hw *clk_reg_prcmu_scalable_rate(const char *name,
					   const char *parent_name,
					   u8 cg_sel,
					   unsigned long rate,
					   unsigned long flags)
{
	return clk_reg_prcmu(name, parent_name, cg_sel, rate, flags,
			&clk_prcmu_scalable_rate_ops);
}

struct clk_hw *clk_reg_prcmu_rate(const char *name,
				  const char *parent_name,
				  u8 cg_sel,
				  unsigned long flags)
{
	return clk_reg_prcmu(name, parent_name, cg_sel, 0, flags,
			&clk_prcmu_rate_ops);
}

struct clk_hw *clk_reg_prcmu_opp_gate(const char *name,
				      const char *parent_name,
				      u8 cg_sel,
				      unsigned long flags)
{
	return clk_reg_prcmu(name, parent_name, cg_sel, 0, flags,
			&clk_prcmu_opp_gate_ops);
}

struct clk_hw *clk_reg_prcmu_opp_volt_scalable(const char *name,
					       const char *parent_name,
					       u8 cg_sel,
					       unsigned long rate,
					       unsigned long flags)
{
	return clk_reg_prcmu(name, parent_name, cg_sel, rate, flags,
			&clk_prcmu_opp_volt_scalable_ops);
}

/* External programmable clock outputs */

#define PRCM_CLKOCR		   0x1CC
#define PRCM_CLKOCR_CLKODIV0_SHIFT	0
#define PRCM_CLKOCR_CLKODIV0_MASK	GENMASK(5, 0)
#define PRCM_CLKOCR_CLKOSEL0_SHIFT	6
#define PRCM_CLKOCR_CLKOSEL0_MASK	GENMASK(8, 6)
#define PRCM_CLKOCR_CLKODIV1_SHIFT	16
#define PRCM_CLKOCR_CLKODIV1_MASK	GENMASK(21, 16)
#define PRCM_CLKOCR_CLKOSEL1_SHIFT	22
#define PRCM_CLKOCR_CLKOSEL1_MASK	GENMASK(24, 22)
#define PRCM_CLKOCR_CLK1TYPE		BIT(28)

static DEFINE_SPINLOCK(clkout_lock);

/**
 * clk_prcmu_config_clkout - Configure one of the programmable clock outputs.
 * @clkout:	The CLKOUT number (0 or 1).
 * @source:	The clock to be used (one of the PRCMU_CLKSRC_*).
 * @div:	The divider to be applied.
 *
 * Configures one of the programmable clock outputs (CLKOUTs).
 * @div should be in the range [1,63] to request a configuration, or 0 to
 * inform that the configuration is no longer requested.
 */
static int clk_prcmu_config_clkout(u8 clkout, u8 source, u8 div)
{
	static int requests[2];
	int r = 0;
	unsigned long flags;
	u32 val;
	u32 bits;
	u32 mask;
	u32 div_mask;

	BUG_ON(clkout > 1);
	BUG_ON(div > 63);
	BUG_ON((clkout == 0) && (source > PRCMU_CLKSRC_CLK009));

	if (!div && !requests[clkout])
		return -EINVAL;

	if (clkout == 0) {
		div_mask = PRCM_CLKOCR_CLKODIV0_MASK;
		mask = (PRCM_CLKOCR_CLKODIV0_MASK | PRCM_CLKOCR_CLKOSEL0_MASK);
		bits = ((source << PRCM_CLKOCR_CLKOSEL0_SHIFT) |
			(div << PRCM_CLKOCR_CLKODIV0_SHIFT));
	} else {
		div_mask = PRCM_CLKOCR_CLKODIV1_MASK;
		mask = (PRCM_CLKOCR_CLKODIV1_MASK | PRCM_CLKOCR_CLKOSEL1_MASK |
			PRCM_CLKOCR_CLK1TYPE);
		bits = ((source << PRCM_CLKOCR_CLKOSEL1_SHIFT) |
			(div << PRCM_CLKOCR_CLKODIV1_SHIFT));
	}
	bits &= mask;

	spin_lock_irqsave(&clkout_lock, flags);

	val = db8500_prcmu_read(PRCM_CLKOCR);
	if (val & div_mask) {
		if (div) {
			if ((val & mask) != bits) {
				r = -EBUSY;
				goto unlock_and_return;
			}
		} else {
			if ((val & mask & ~div_mask) != bits) {
				r = -EINVAL;
				goto unlock_and_return;
			}
		}
	}
	db8500_prcmu_write(PRCM_CLKOCR, bits | (val & ~mask));
	requests[clkout] += (div ? 1 : -1);

unlock_and_return:
	spin_unlock_irqrestore(&clkout_lock, flags);

	return r;
}

static int clk_prcmu_clkout_prepare(struct clk_hw *hw)
{
	struct clk_prcmu_clkout *clk = to_clk_prcmu_clkout(hw);

	return clk_prcmu_config_clkout(clk->clkout_id, clk->source, clk->divider);
}

static void clk_prcmu_clkout_unprepare(struct clk_hw *hw)
{
	struct clk_prcmu_clkout *clk = to_clk_prcmu_clkout(hw);
	int ret;

	/* The clkout clock is disabled by dividing by 0 */
	ret = clk_prcmu_config_clkout(clk->clkout_id, clk->source, 0);
	if (ret)
		pr_err("clk_prcmu: %s failed to disable %s\n", __func__,
		       clk_hw_get_name(hw));
}

static unsigned long clk_prcmu_clkout_recalc_rate(struct clk_hw *hw,
						  unsigned long parent_rate)
{
	struct clk_prcmu_clkout *clk = to_clk_prcmu_clkout(hw);

	return (parent_rate / clk->divider);
}

static u8 clk_prcmu_clkout_get_parent(struct clk_hw *hw)
{
	struct clk_prcmu_clkout *clk = to_clk_prcmu_clkout(hw);

	return clk->source;
}

static int clk_prcmu_clkout_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_prcmu_clkout *clk = to_clk_prcmu_clkout(hw);

	clk->source = index;
	/* Make sure the change reaches the hardware immediately */
	if (clk_hw_is_prepared(hw))
		return clk_prcmu_clkout_prepare(hw);
	return 0;
}

static const struct clk_ops clk_prcmu_clkout_ops = {
	.prepare = clk_prcmu_clkout_prepare,
	.unprepare = clk_prcmu_clkout_unprepare,
	.recalc_rate = clk_prcmu_clkout_recalc_rate,
	.determine_rate = clk_hw_determine_rate_no_reparent,
	.get_parent = clk_prcmu_clkout_get_parent,
	.set_parent = clk_prcmu_clkout_set_parent,
};

struct clk_hw *clk_reg_prcmu_clkout(const char *name,
				    const char * const *parent_names,
				    int num_parents,
				    u8 source, u8 divider)

{
	struct clk_init_data clk_prcmu_clkout_init = {};
	struct clk_prcmu_clkout *clk;
	u8 clkout_id;
	int ret;

	if (!name) {
		pr_err("clk_prcmu_clkout: %s invalid arguments passed\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!strcmp(name, "clkout1"))
		clkout_id = 0;
	else if (!strcmp(name, "clkout2"))
		clkout_id = 1;
	else {
		pr_err("clk_prcmu_clkout: %s bad clock name\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	clk = kzalloc_obj(*clk);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	clk->clkout_id = clkout_id;
	clk->source = source;
	clk->divider = divider;

	clk_prcmu_clkout_init.name = name;
	clk_prcmu_clkout_init.ops = &clk_prcmu_clkout_ops;
	clk_prcmu_clkout_init.flags = CLK_GET_RATE_NOCACHE;
	clk_prcmu_clkout_init.parent_names = parent_names;
	clk_prcmu_clkout_init.num_parents = num_parents;
	clk->hw.init = &clk_prcmu_clkout_init;

	ret = clk_hw_register(NULL, &clk->hw);
	if (ret)
		goto free_clkout;

	return &clk->hw;
free_clkout:
	kfree(clk);
	pr_err("clk_prcmu_clkout: %s failed to register clk\n", __func__);
	return ERR_PTR(-ENOMEM);
}
