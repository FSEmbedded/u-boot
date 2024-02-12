/*
 * fsimx8ulp.c
 *
 * (C) Copyright 2024
 * Claudio Senatore, F&S Elektronik Systeme GmbH, senatore@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8ULP CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/arch/imx8ulp-pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/pcc.h>
#include <asm/arch/sys_proto.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <power-domain.h>
#include <dt-bindings/power/imx8ulp-power.h>
#include <fdt_support.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_USB_ETHER
#ifndef CONFIG_DM_ETH

int board_eth_init(struct bd_info *bis)
{
	int rc = 0;

	rc = usb_eth_initialize(bis);
	return rc;
}
#endif
#endif


#if IS_ENABLED(CONFIG_FEC_MXC)
#define ENET_CLK_PAD_CTRL	(PAD_CTL_PUS_UP | PAD_CTL_DSE | PAD_CTL_IBE_ENABLE)
static iomux_cfg_t const enet_clk_pads[] = {
	IMX8ULP_PAD_PTE19__ENET0_REFCLK | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
};

static int setup_fec(void)
{
	/*
	 * Since ref clock and timestamp clock are from external,
	 * set the iomux prior the clock enablement
	 */
	imx8ulp_iomux_setup_multiple_pads(enet_clk_pads, ARRAY_SIZE(enet_clk_pads));

	/* Select enet time stamp clock: 001 - External Timestamp Clock */
	cgc1_enet_stamp_sel(1);

	/* enable FEC PCC */
	pcc_clock_enable(4, ENET_PCC4_SLOT, true);
	pcc_reset_peripheral(4, ENET_PCC4_SLOT, false);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);
	return 0;
}
#endif

int board_init(void)
{

#if defined(CONFIG_FEC_MXC)
	setup_fec();
#endif

	return 0;
}

int board_early_init_f(void)
{
	return 0;
}

int board_late_init(void)
{
	ulong addr;

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

	/* clear fdtaddr to avoid obsolete data */
	addr = env_get_hex("fdt_addr_r", 0);
	if (addr)
		memset((void *)addr, 0, 0x400);

	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
#ifdef TARGET_FSIMX8ULP
static iomux_cfg_t const recovery_pad[] = {
	IMX8ULP_PAD_PTF7__PTF7 | MUX_PAD_CTRL(PAD_CTL_IBE_ENABLE),
};
#endif
int is_recovery_key_pressing(void)
{
#ifdef TARGET_FSIMX8ULP
	int ret;
	struct gpio_desc desc;

	imx8ulp_iomux_setup_multiple_pads(recovery_pad, ARRAY_SIZE(recovery_pad));

	ret = dm_gpio_lookup_name("GPIO3_7", &desc);
	if (ret) {
		printf("%s lookup GPIO3_7 failed ret = %d\n", __func__, ret);
		return 0;
	}

	ret = dm_gpio_request(&desc, "recovery");
	if (ret) {
		printf("%s request recovery pad failed ret = %d\n", __func__, ret);
		return 0;
	}

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_IN);

	ret = dm_gpio_get_value(&desc);
	if (ret < 0) {
                printf("%s error in retrieving GPIO value ret = %d\n", __func__, ret);
                return 0;
        }

	dm_gpio_free(desc.dev, &desc);

	return !ret;
#else
	return 0;
#endif
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/


void board_quiesce_devices(void)
{
	/* Disable the power domains may used in u-boot before entering kernel */
#if CONFIG_IS_ENABLED(POWER_DOMAIN)
	struct udevice *scmi_devpd;
	int ret, i;
	struct power_domain pd;
	ulong ids[] = {
		IMX8ULP_PD_FLEXSPI2, IMX8ULP_PD_USB0, IMX8ULP_PD_USDHC0,
		IMX8ULP_PD_USDHC1, IMX8ULP_PD_USDHC2_USB1, IMX8ULP_PD_DCNANO,
		IMX8ULP_PD_MIPI_DSI};

	ret = uclass_get_device(UCLASS_POWER_DOMAIN, 0, &scmi_devpd);
	if (ret) {
		printf("Cannot get scmi devpd: err=%d\n", ret);
		return;
	}

	pd.dev = scmi_devpd;

	for (i = 0; i < ARRAY_SIZE(ids); i++) {
		pd.id = ids[i];
		ret = power_domain_off(&pd);
		if (ret)
			printf("power_domain_off %lu failed: err=%d\n", ids[i], ret);
	}
#endif
}

#if  defined(CONFIG_OF_BOARD_SETUP)
static int fdt_fixpu_memory_node(void *blob)
{
	u64 base[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	
	base[0] = gd->bd->bi_dram[0].start;
	size[0] = gd->bd->bi_dram[0].size;

	return fdt_fixup_memory_banks(blob, base, size, 1);
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	return fdt_fixpu_memory_node(blob);
}
#endif