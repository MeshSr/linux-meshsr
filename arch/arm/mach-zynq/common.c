/*
 * This file contains common code that is intended to be used across
 * boards so that it's not replicated.
 *
 *  Copyright (C) 2011 Xilinx
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk/zynq.h>
#include <linux/opp.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/memblock.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach-types.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/smp_scu.h>
#include <asm/hardware/cache-l2x0.h>

#include "common.h"

void __iomem *zynq_scu_base;

/**
 * zynq_memory_init() - Initialize special memory
 *
 * We need to stop things allocating the low memory as DMA can't work in
 * the 1st 512K of memory.  Using reserve vs remove is not totally clear yet.
 */
static void __init zynq_memory_init(void)
{
	/*
	 * Reserve the 0-0x4000 addresses (before page tables and kernel)
	 * which can't be used for DMA
	 */
	if (!__pa(PAGE_OFFSET))
		memblock_reserve(0, 0x4000);
}

#ifdef CONFIG_CPU_FREQ
#define CPUFREQ_MIN_FREQ_HZ	200000000
static unsigned int freq_divs[] __initdata = {
	2, 3
};

static long __init xilinx_calc_opp_freq(struct clk *clk, long rate)
{
	long rate_nearest = clk_round_rate_nearest(clk, rate);
	long rate_round = clk_round_rate(clk, rate_nearest / 1000 * 1000);

	if (rate_round != rate_nearest)
		rate_nearest += 1000;

	return rate_nearest;
}

/**
 * zynq_opp_init() - Register OPPs
 *
 * Registering frequency/voltage operating points for voltage and frequency
 * scaling. Currently we only support frequency scaling.
 */
static int __init zynq_opp_init(void)
{
	long freq;
	unsigned int i;
	struct device *dev = get_cpu_device(0);
	int ret = 0;
	struct clk *cpuclk = clk_get(NULL, "cpufreq_clk");

	if (!dev) {
		pr_warn("%s: no cpu device. DVFS not available.", __func__);
		return -ENODEV;
	}

	if (IS_ERR(cpuclk)) {
		pr_warn("%s: CPU clock not found. DVFS not available.",
				__func__);
		return PTR_ERR(cpuclk);
	}

	/* frequency/voltage operating points. For now use f only */
	freq = clk_get_rate(cpuclk);
	ret |= opp_add(dev, xilinx_calc_opp_freq(cpuclk, freq), 0);
	for (i = 0; i < ARRAY_SIZE(freq_divs); i++) {
		long tmp = xilinx_calc_opp_freq(cpuclk, freq / freq_divs[i]);
		if (tmp >= CPUFREQ_MIN_FREQ_HZ)
			ret |= opp_add(dev, tmp, 0);
	}
	freq = xilinx_calc_opp_freq(cpuclk, CPUFREQ_MIN_FREQ_HZ);
	if (freq >= CPUFREQ_MIN_FREQ_HZ && IS_ERR(opp_find_freq_exact(dev, freq,
				1)))
		ret |= opp_add(dev, freq, 0);

	if (ret)
		pr_warn("%s: Error adding OPPs.", __func__);

	return ret;
}
device_initcall(zynq_opp_init);
#endif

#ifdef CONFIG_CACHE_L2X0
static int __init zynq_l2c_init(void)
{
	/* 64KB way size, 8-way associativity, parity disabled,
	 * prefetching option */
#ifndef	CONFIG_XILINX_L2_PREFETCH
	return l2x0_of_init(0x02060000, 0xF0F0FFFF);
#else
	return l2x0_of_init(0x72060000, 0xF0F0FFFF);
#endif
}
early_initcall(zynq_l2c_init);
#endif


#ifdef CONFIG_XILINX_L1_PREFETCH
static void __init zynq_data_prefetch_enable(void *info)
{
	/*
	 * Enable prefetching in aux control register. L2 prefetch must
	 * only be enabled if the slave supports it (PL310 does)
	 */
	asm volatile ("mrc   p15, 0, r1, c1, c0, 1\n"
		      "orr   r1, r1, #6\n"
		      "mcr   p15, 0, r1, c1, c0, 1\n"
		      : : : "r1");
}
#endif

static void __init zynq_init_late(void)
{
	zynq_pm_late_init();

#ifdef CONFIG_XILINX_L1_PREFETCH
	on_each_cpu(zynq_data_prefetch_enable, NULL, 0);
#endif
}

/**
 * zynq_init_machine - System specific initialization, intended to be
 *		       called from board specific initialization.
 */
static void __init zynq_init_machine(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static void __init zynq_timer_init(void)
{
	zynq_slcr_init();
	clocksource_of_init();
}

static struct map_desc zynq_cortex_a9_scu_map __initdata = {
	.length	= SZ_256,
	.type	= MT_DEVICE,
};

static void __init zynq_scu_map_io(void)
{
	unsigned long base;

	base = scu_a9_get_base();
	zynq_cortex_a9_scu_map.pfn = __phys_to_pfn(base);
	/* Expected address is in vmalloc area that's why simple assign here */
	zynq_cortex_a9_scu_map.virtual = base;
	iotable_init(&zynq_cortex_a9_scu_map, 1);
	zynq_scu_base = (void __iomem *)base;
	BUG_ON(!zynq_scu_base);
}

/**
 * zynq_map_io - Create memory mappings needed for early I/O.
 */
static void __init zynq_map_io(void)
{
	debug_ll_io_init();
	zynq_scu_map_io();
}

static void __init zynq_irq_init(void)
{
	gic_arch_extn.flags = IRQCHIP_SKIP_SET_WAKE | IRQCHIP_MASK_ON_SUSPEND;
	irqchip_init();
}

static void zynq_system_reset(enum reboot_mode mode, const char *cmd)
{
	zynq_slcr_system_reset();
}

static const char * const zynq_dt_match[] = {
	"xlnx,zynq-7000",
	NULL
};

DT_MACHINE_START(XILINX_EP107, "Xilinx Zynq Platform")
	.smp		= smp_ops(zynq_smp_ops),
	.map_io		= zynq_map_io,
	.init_irq	= zynq_irq_init,
	.init_machine	= zynq_init_machine,
	.init_late	= zynq_init_late,
	.init_time	= zynq_timer_init,
	.dt_compat	= zynq_dt_match,
	.reserve	= zynq_memory_init,
	.restart	= zynq_system_reset,
MACHINE_END
