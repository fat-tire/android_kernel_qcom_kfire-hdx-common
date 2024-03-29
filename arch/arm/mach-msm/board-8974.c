/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include "board-dt.h"
#include "clock.h"
#include "devices.h"
#include "spm.h"
#include "modem_notifier.h"
#include "lpm_resources.h"
#include "platsmp.h"
#include <linux/gpio.h>
#include <linux/mutex.h>
#include "amzn_ram_console.h"

#if defined(CONFIG_ARCH_MSM8974_THOR) || defined(CONFIG_ARCH_MSM8974_APOLLO)
enum WLANBT_STATUS {
    WLANOFF_BTOFF = 1,
    WLANOFF_BTON,
    WLANON_BTOFF,
    WLANON_BTON
};

static DEFINE_MUTEX(ath_wlanbt_mutex);
//unsigned gpio_wlan_sys_rest_en = 504;        // PMIC GPIO 33
/*static int ath_wlanbt_status = WLANOFF_BTOFF;*/
/*
static int ath6kl_power_control(int on, int wlan_gpio)
{

    int rc;

    if (on) {
        rc = gpio_request(wlan_gpio, "wlan sys_rst_n");
        if (rc) {
            pr_err("%s: unable to request gpio %d (%d)\n",
                    __func__, wlan_gpio, rc);
            return rc;
        }
        rc = gpio_direction_output(wlan_gpio, 0);
        msleep(200);
        rc = gpio_direction_output(wlan_gpio, 1);
        msleep(100);
    } else {
        gpio_set_value(wlan_gpio, 0);
        rc = gpio_direction_input(wlan_gpio);
        msleep(100);
        gpio_free(wlan_gpio);
    }

    return 0;
};
*/
/*
static int ath6kl_wlan_power(int on, int wlan_gpio)
{
    int ret = 0;

    mutex_lock(&ath_wlanbt_mutex);
    if (on) {
        if (ath_wlanbt_status == WLANOFF_BTOFF) {
            ret = ath6kl_power_control(1, wlan_gpio);
            ath_wlanbt_status = WLANON_BTOFF;
        } else if (ath_wlanbt_status == WLANOFF_BTON)
            ath_wlanbt_status = WLANON_BTON;
    } else {
        if (ath_wlanbt_status == WLANON_BTOFF) {
            ret = ath6kl_power_control(0, wlan_gpio);
            ath_wlanbt_status = WLANOFF_BTOFF;
        } else if (ath_wlanbt_status == WLANON_BTON)
            ath_wlanbt_status = WLANOFF_BTON;
    }
    mutex_unlock(&ath_wlanbt_mutex);
    pr_debug("%s on= %d, wlan_status= %d\n",
            __func__, on, ath_wlanbt_status);
    return ret;
};
*/
/*
static struct platform_device msm_wlan_power_device = {
    .name = "ath6kl_power",
    .dev            = {
        .platform_data = &ath6kl_wlan_power,
    },
};
*/

//unsigned gpio_bt_sys_rest_en = 505;  // PMIC GPIO 34
/*
static int ath3k_bt_power(int on, int wlan_gpio, int bt_gpio)
{
    int rc;

    mutex_lock(&ath_wlanbt_mutex);
    if (on) {
        if (ath_wlanbt_status == WLANOFF_BTOFF) {
            ath6kl_power_control(1,wlan_gpio);
            ath_wlanbt_status = WLANOFF_BTON;
        } else if (ath_wlanbt_status == WLANON_BTOFF)
            ath_wlanbt_status = WLANON_BTON;

        rc = gpio_request(bt_gpio, "bt sys_rst_n");
        if (rc) {
            pr_err("%s: unable to request gpio %d (%d)\n",
                    __func__, bt_gpio, rc);
            mutex_unlock(&ath_wlanbt_mutex);

            return rc;
        }
        rc = gpio_direction_output(bt_gpio, 0);
        msleep(20);
        rc = gpio_direction_output(bt_gpio, 1);
        msleep(100);
    } else {
        gpio_set_value(bt_gpio, 0);
        rc = gpio_direction_input(bt_gpio);
        msleep(100);
        gpio_free(bt_gpio);

        if (ath_wlanbt_status == WLANOFF_BTON) {
            ath6kl_power_control(0, wlan_gpio);
            ath_wlanbt_status = WLANOFF_BTOFF;
        } else if (ath_wlanbt_status == WLANON_BTON)
            ath_wlanbt_status = WLANON_BTOFF;
    }
    mutex_unlock(&ath_wlanbt_mutex);
    pr_debug("%s on= %d, wlan_status= %d\n",
            __func__, on, ath_wlanbt_status);
   return 0;
};
*/
/*
static struct platform_device msm_bt_power_device = {
    .name = "bt_power",
    .dev = {
        .platform_data = &ath3k_bt_power,
    },
};
*/
#endif

static struct memtype_reserve msm8974_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm8974_paddr_to_memtype(phys_addr_t paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msm8974_reserve_info __initdata = {
	.memtype_reserve_table = msm8974_reserve_table,
	.paddr_to_memtype = msm8974_paddr_to_memtype,
};

void __init msm_8974_reserve(void)
{
	reserve_info = &msm8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8974_reserve_table);
	msm_reserve();	
	amzn_ram_console_init(AMZN_RAM_CONSOLE_START_DEFAULT, AMZN_RAM_CONSOLE_SIZE_DEFAULT);
}

static void __init msm8974_early_memory(void)
{
	reserve_info = &msm8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8974_reserve_table);
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8974_add_drivers(void)
{
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_lpmrs_module_init();
	rpm_regulator_smd_driver_init();
	msm_spm_device_init();
	krait_power_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8974_rumi_clock_init_data);
	else
		msm_clock_init(&msm8974_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}

static struct of_dev_auxdata msm8974_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,ehci-host", 0xF9A55000, \
			"msm_ehci_host", NULL),
	OF_DEV_AUXDATA("qcom,dwc-usb3-msm", 0xF9200000, \
			"msm_dwc3", NULL),
	OF_DEV_AUXDATA("qcom,usb-bam-msm", 0xF9304000, \
			"usb_bam", NULL),
#if defined(CONFIG_SPI_QSD) || defined(CONFIG_SPI_QUP)
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9924000, \
			"spi_qsd.1", NULL),
#endif
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98E4000, \
			"msm_sdcc.4", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98E4900, \
			"msm_sdcc.4", NULL),
	OF_DEV_AUXDATA("qcom,msm-rng", 0xF9BFF000, \
			"msm_rng", NULL),
	OF_DEV_AUXDATA("qcom,qseecom", 0xFE806000, \
			"qseecom", NULL),
	OF_DEV_AUXDATA("qcom,mdss_mdp", 0xFD900000, "mdp.0", NULL),
	OF_DEV_AUXDATA("qcom,msm-tsens", 0xFC4A8000, \
			"msm-tsens", NULL),
	OF_DEV_AUXDATA("qcom,qcedev", 0xFD440000, \
			"qcedev.0", NULL),
	OF_DEV_AUXDATA("qcom,qcrypto", 0xFD440000, \
			"qcrypto.0", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, \
			"msm_hsic_host", NULL),
	{}
};

static void __init msm8974_map_io(void)
{
	msm_map_8974_io();
}

#define DDR_VENDOR_ID_SAMSUNG	0x1
#define DDR_VENDOR_ID_ELPIDA	0x3
#define DDR_VENDOR_ID_HYNIX	0x6
#define DDR_VENDOR_ID_MICRON	0xFF

void __init msm8974_print_ddr_vendor_id(void)
{
	uint64_t *vendor_id_smem;
	uint8_t vendor_id = 0;
	char *vendor;

	vendor_id_smem = smem_alloc(SMEM_ID_VENDOR1, sizeof(uint64_t));

	if (vendor_id_smem) {
		vendor_id = (uint8_t) *vendor_id_smem;
	}

	switch (vendor_id) {
	case DDR_VENDOR_ID_SAMSUNG:
		vendor = "Samsung";
		break;

	case DDR_VENDOR_ID_ELPIDA:
		vendor = "Elpida";
		break;

	case DDR_VENDOR_ID_HYNIX:
		vendor = "Hynix";
		break;

	case DDR_VENDOR_ID_MICRON:
		vendor = "Micron";
		break;

	default:
		vendor = "Unknown";
	}

	printk(KERN_ERR "ddr: I def:ddrinfo:vendor=%s:\n", vendor);
}

void __init msm8974_init(void)
{
	struct of_dev_auxdata *adata = msm8974_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm_8974_init_gpiomux();
	regulator_has_full_constraints();
	board_dt_populate(adata);
	msm8974_add_drivers();

	msm8974_print_ddr_vendor_id();
}

void __init msm8974_init_very_early(void)
{
	msm8974_early_memory();
}

static const char *msm8974_dt_match[] __initconst = {
	"qcom,msm8974",
	"qcom,apq8074",
	NULL
};

DT_MACHINE_START(MSM8974_DT, "Qualcomm MSM 8974 (Flattened Device Tree)")
	.map_io = msm8974_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8974_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8974_dt_match,
	.reserve = msm_8974_reserve,
	.init_very_early = msm8974_init_very_early,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
