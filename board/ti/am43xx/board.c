/*
 * board.c
 *
 * Board functions for TI AM43XX based boards
 *
 * Copyright (C) 2013, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <i2c.h>
#include <asm/errno.h>
#include <spl.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mux.h>
#include <asm/arch/ddr_defs.h>
#include <asm/emif.h>
#include <asm/gpio.h>
#include <asm/omap_gpio.h>
#include "board.h"
#include <miiphy.h>
#include <cpsw.h>

DECLARE_GLOBAL_DATA_PTR;

static struct ctrl_dev *cdev = (struct ctrl_dev *)CTRL_DEVICE_BASE;

/*
 * Read header information from EEPROM into global structure.
 */
static int read_eeprom(struct am43xx_board_id *header)
{
	/* Check if baseboard eeprom is available */
	if (i2c_probe(CONFIG_SYS_I2C_EEPROM_ADDR)) {
		printf("Could not probe the EEPROM at 0x%x\n",
		       CONFIG_SYS_I2C_EEPROM_ADDR);
		return -ENODEV;
	}

	/* read the eeprom using i2c */
	if (i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0, 2, (uchar *)header,
		     sizeof(struct am43xx_board_id))) {
		printf("Could not read the EEPROM\n");
		return -EIO;
	}

	if (header->magic != 0xEE3355AA) {
		/*
		 * read the eeprom using i2c again,
		 * but use only a 1 byte address
		 */
		if (i2c_read(CONFIG_SYS_I2C_EEPROM_ADDR, 0, 1, (uchar *)header,
			     sizeof(struct am43xx_board_id))) {
			printf("Could not read the EEPROM at 0x%x\n",
			       CONFIG_SYS_I2C_EEPROM_ADDR);
			return -EIO;
		}

		if (header->magic != 0xEE3355AA) {
			printf("Incorrect magic number (0x%x) in EEPROM\n",
			       header->magic);
			return -EINVAL;
		}
	}

	strncpy(am43xx_board_name, (char *)header->name, sizeof(header->name));
	am43xx_board_name[sizeof(header->name)] = 0;

	strncpy(am43xx_board_rev, (char *)header->version, sizeof(header->version));
	am43xx_board_rev[sizeof(header->version)] = 0;

	return 0;
}

#if defined(CONFIG_SPL_BUILD) || defined(CONFIG_QSPI_BOOT)

const struct dpll_params epos_evm_dpll_ddr = {
		266, 24, 1, -1, 1, -1, -1};
const struct dpll_params epos_evm_dpll_mpu = {
		600, 24, 1, -1, -1, -1, -1};
const struct dpll_params epos_evm_dpll_core = {
		1000, 24, -1, -1, 10, 8, 4};
const struct dpll_params epos_evm_dpll_per = {
		960, 24, 5, -1, -1, -1, -1};

const struct dpll_params gp_evm_dpll_ddr = {
		400, 23, 1, -1, 1, -1, -1};
const struct dpll_params gp_evm_dpll_mpu = {
		600, 23, 1, -1, -1, -1, -1};
const struct dpll_params gp_evm_dpll_core = {
		1000, 23, -1, -1, 10, 8, 4};
const struct dpll_params gp_evm_dpll_per = {
		960, 23, 5, -1, -1, -1, -1};

const struct ctrl_ioregs ioregs_lpddr2 = {
	.cm0ioctl		= LPDDR2_ADDRCTRL_IOCTRL_VALUE,
	.cm1ioctl		= LPDDR2_ADDRCTRL_WD0_IOCTRL_VALUE,
	.cm2ioctl		= LPDDR2_ADDRCTRL_WD1_IOCTRL_VALUE,
	.dt0ioctl		= LPDDR2_DATA0_IOCTRL_VALUE,
	.dt1ioctl		= LPDDR2_DATA0_IOCTRL_VALUE,
	.dt2ioctrl		= LPDDR2_DATA0_IOCTRL_VALUE,
	.dt3ioctrl		= LPDDR2_DATA0_IOCTRL_VALUE,
	.emif_sdram_config_ext	= 0x1,
};

const struct emif_regs emif_regs_lpddr2 = {
	.sdram_config			= 0x808012BA,
	.ref_ctrl			= 0x0000040D,
	.sdram_tim1			= 0xEA86B411,
	.sdram_tim2			= 0x103A094A,
	.sdram_tim3			= 0x0F6BA37F,
	.read_idle_ctrl			= 0x00050000,
	.zq_config			= 0x50074BE4,
	.temp_alert_config		= 0x0,
	.emif_rd_wr_lvl_rmp_win		= 0x0,
	.emif_rd_wr_lvl_rmp_ctl		= 0x0,
	.emif_rd_wr_lvl_ctl		= 0x0,
	.emif_ddr_phy_ctlr_1		= 0x0E084006,
	.emif_ddr_ext_phy_ctrl_1	= 0x04010040,
	.emif_ddr_ext_phy_ctrl_2	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_3	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_4	= 0x00500050,
	.emif_ddr_ext_phy_ctrl_5	= 0x00500050,
	.emif_rd_wr_exec_thresh		= 0x80000405,
	.emif_prio_class_serv_map	= 0x80000001,
	.emif_connect_id_serv_1_map	= 0x80000094,
	.emif_connect_id_serv_2_map	= 0x00000000,
	.emif_cos_config		= 0x000FFFFF
};

/* TODO: Reconcile with OMAP5/DRA7xx changes */
#define EMIF_EXT_PHY_CTRL_CONST_REG	0x14

const u32 ext_phy_ctrl_const_base_lpddr2[EMIF_EXT_PHY_CTRL_CONST_REG] = {
	0x00500050,
	0x00350035,
	0x00350035,
	0x00350035,
	0x00350035,
	0x00350035,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x40001000,
	0x08102040
};

const struct ctrl_ioregs ioregs_ddr3 = {
	.cm0ioctl		= DDR3_ADDRCTRL_IOCTRL_VALUE,
	.cm1ioctl		= DDR3_ADDRCTRL_WD0_IOCTRL_VALUE,
	.cm2ioctl		= DDR3_ADDRCTRL_WD1_IOCTRL_VALUE,
	.dt0ioctl		= DDR3_DATA0_IOCTRL_VALUE,
	.dt1ioctl		= DDR3_DATA0_IOCTRL_VALUE,
	.dt2ioctrl		= DDR3_DATA0_IOCTRL_VALUE,
	.dt3ioctrl		= DDR3_DATA0_IOCTRL_VALUE,
	.emif_sdram_config_ext	= 0x0143,
};

const struct emif_regs ddr3_emif_regs_400Mhz = {
	.sdram_config			= 0x638413B2,
	.ref_ctrl			= 0x00000C30,
	.sdram_tim1			= 0xEAAAD4DB,
	.sdram_tim2			= 0x266B7FDA,
	.sdram_tim3			= 0x107F8678,
	.read_idle_ctrl			= 0x00050000,
	.zq_config			= 0x50074BE4,
	.temp_alert_config		= 0x0,
	.emif_ddr_phy_ctlr_1		= 0x0E004008,
	.emif_ddr_ext_phy_ctrl_1	= 0x08020080,
	.emif_ddr_ext_phy_ctrl_2	= 0x00400040,
	.emif_ddr_ext_phy_ctrl_3	= 0x00400040,
	.emif_ddr_ext_phy_ctrl_4	= 0x00400040,
	.emif_ddr_ext_phy_ctrl_5	= 0x00400040,
	.emif_rd_wr_exec_thresh		= 0x80000405,
	.emif_prio_class_serv_map	= 0x80000001,
	.emif_connect_id_serv_1_map	= 0x80000094,
	.emif_connect_id_serv_2_map	= 0x00000000,
	.emif_cos_config		= 0x000FFFFF
};

const u32 ext_phy_ctrl_const_base_ddr3[EMIF_EXT_PHY_CTRL_CONST_REG] = {
	0x00400040,
	0x00350035,
	0x00350035,
	0x00350035,
	0x00350035,
	0x00350035,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00340034,
	0x00340034,
	0x00340034,
	0x00340034,
	0x00340034,
	0x0,
	0x0,
	0x40000000,
	0x08102040
};

/* EMIF DDR3 Configurations are different for beta AM43X GP EVMs */
const struct emif_regs ddr3_emif_regs_400Mhz_beta = {
	.sdram_config			= 0x638413B2,
	.ref_ctrl			= 0x00000C30,
	.sdram_tim1			= 0xEAAAD4DB,
	.sdram_tim2			= 0x266B7FDA,
	.sdram_tim3			= 0x107F8678,
	.read_idle_ctrl			= 0x00050000,
	.zq_config			= 0x50074BE4,
	.temp_alert_config		= 0x0,
	.emif_ddr_phy_ctlr_1		= 0x0E004008,
	.emif_ddr_ext_phy_ctrl_1	= 0x08020080,
	.emif_ddr_ext_phy_ctrl_2	= 0x00000065,
	.emif_ddr_ext_phy_ctrl_3	= 0x00000091,
	.emif_ddr_ext_phy_ctrl_4	= 0x000000B5,
	.emif_ddr_ext_phy_ctrl_5	= 0x000000E5,
	.emif_rd_wr_exec_thresh		= 0x80000405,
	.emif_prio_class_serv_map	= 0x80000001,
	.emif_connect_id_serv_1_map	= 0x80000094,
	.emif_connect_id_serv_2_map	= 0x00000000,
	.emif_cos_config		= 0x000FFFFF
};

const u32 ext_phy_ctrl_const_base_ddr3_beta[EMIF_EXT_PHY_CTRL_CONST_REG] = {
	0x00000000,
	0x00000045,
	0x00000046,
	0x00000048,
	0x00000047,
	0x00000000,
	0x0000004C,
	0x00000070,
	0x00000085,
	0x000000A3,
	0x00000000,
	0x0000000C,
	0x00000030,
	0x00000045,
	0x00000063,
	0x00000000,
	0x0,
	0x0,
	0x40000000,
	0x08102040
};

/* EMIF DDR3 Configurations are different for production AM43X GP EVMs */
const struct emif_regs ddr3_emif_regs_400Mhz_production = {
	.sdram_config			= 0x638413B2,
	.ref_ctrl			= 0x00000C30,
	.sdram_tim1			= 0xEAAAD4DB,
	.sdram_tim2			= 0x266B7FDA,
	.sdram_tim3			= 0x107F8678,
	.read_idle_ctrl			= 0x00050000,
	.zq_config			= 0x50074BE4,
	.temp_alert_config		= 0x0,
	.emif_ddr_phy_ctlr_1		= 0x0E004008,
	.emif_ddr_ext_phy_ctrl_1	= 0x08020080,
	.emif_ddr_ext_phy_ctrl_2	= 0x00000066,
	.emif_ddr_ext_phy_ctrl_3	= 0x00000091,
	.emif_ddr_ext_phy_ctrl_4	= 0x000000B9,
	.emif_ddr_ext_phy_ctrl_5	= 0x000000E6,
	.emif_rd_wr_exec_thresh		= 0x80000405,
	.emif_prio_class_serv_map	= 0x80000001,
	.emif_connect_id_serv_1_map	= 0x80000094,
	.emif_connect_id_serv_2_map	= 0x00000000,
	.emif_cos_config		= 0x000FFFFF
};

const u32 ext_phy_ctrl_const_base_ddr3_production[EMIF_EXT_PHY_CTRL_CONST_REG] = {
	0x00000000,
	0x00000044,
	0x00000044,
	0x00000046,
	0x00000046,
	0x00000000,
	0x00000059,
	0x00000077,
	0x00000093,
	0x000000A8,
	0x00000000,
	0x00000019,
	0x00000037,
	0x00000053,
	0x00000068,
	0x00000000,
	0x0,
	0x0,
	0x40000000,
	0x08102040
};



const struct dpll_params *get_dpll_ddr_params(void)
{
	if (board_is_eposevm())
		return &epos_evm_dpll_ddr;
	else
		return &gp_evm_dpll_ddr;
}

const struct dpll_params *get_dpll_mpu_params(void)
{
	if (board_is_eposevm())
		return &epos_evm_dpll_mpu;
	else
		return &gp_evm_dpll_mpu;
}

const struct dpll_params *get_dpll_core_params(void)
{
	struct am43xx_board_id header;

	enable_i2c0_pin_mux();
	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	if (board_is_eposevm())
		return &epos_evm_dpll_core;
	else
		return &gp_evm_dpll_core;
}

const struct dpll_params *get_dpll_per_params(void)
{
	if (board_is_eposevm())
		return &epos_evm_dpll_per;
	else
		return &gp_evm_dpll_per;
}

void set_uart_mux_conf(void)
{
	enable_uart0_pin_mux();
}

void set_mux_conf_regs(void)
{
	enable_board_pin_mux();
}

static void enable_vtt_regulator(void)
{
	u32 temp;

	/*GPIO_VTTEN - GPIO5_7 PINMUX Setup*/
	writel(0x7, CTRL_BASE + 0x0A5C);

	writel(0x102, PRCM_BASE + 0x8C98);

	/* Poll if module is functional */
	while ((readl(PRCM_BASE + 0x8C98) & 0x30000) != 0x0)
		;

	/* enable module */
	writel(0x0, GPIO5_BASE + 0x0130);

	/*enable output for GPIO5_7*/
	writel((1 << 7), GPIO5_BASE + 0x0194);
	temp = readl(GPIO5_BASE + 0x0134);
	temp = temp & ~(1 << 7);
	writel(temp, GPIO5_BASE + 0x0134);
}

void sdram_init(void)
{
	if (board_is_eposevm()) {
		do_sdram_init(&ioregs_lpddr2, &emif_regs_lpddr2,
			      ext_phy_ctrl_const_base_lpddr2,
			      EMIF_SDRAM_TYPE_LPDDR2);
	} else {
		enable_vtt_regulator();

		if(board_is_evm_14_or_later())
			do_sdram_init(&ioregs_ddr3, &ddr3_emif_regs_400Mhz_production,
			      ext_phy_ctrl_const_base_ddr3_production,
			      EMIF_SDRAM_TYPE_DDR3);
		else if(board_is_evm_12_or_later())
			do_sdram_init(&ioregs_ddr3, &ddr3_emif_regs_400Mhz_beta,
			      ext_phy_ctrl_const_base_ddr3_beta,
			      EMIF_SDRAM_TYPE_DDR3);
		else
			do_sdram_init(&ioregs_ddr3, &ddr3_emif_regs_400Mhz,
			      ext_phy_ctrl_const_base_ddr3,
			      EMIF_SDRAM_TYPE_DDR3);
	}
}
#endif

int board_init(void)
{
	struct l3f_cfg_bwlimiter *bwlimiter = (struct l3f_cfg_bwlimiter *)L3F_CFG_BWLIMITER;
	u32 mreqprio_0, mreqprio_1, modena_init0_bw_fractional,
	    modena_init0_bw_integer, modena_init0_watermark_0;

	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;
	gpmc_init();

	/* Clear all important bits for DSS errata that may need to be tweaked*/
	mreqprio_0 = readl(&cdev->mreqprio_0) & MREQPRIO_0_SAB_INIT1_MASK &
	                   MREQPRIO_0_SAB_INIT0_MASK;

	mreqprio_1 = readl(&cdev->mreqprio_1) & MREQPRIO_1_DSS_MASK;

	modena_init0_bw_fractional = readl(&bwlimiter->modena_init0_bw_fractional) &
	                                   BW_LIMITER_BW_FRAC_MASK;

	modena_init0_bw_integer = readl(&bwlimiter->modena_init0_bw_integer) &
	                                BW_LIMITER_BW_INT_MASK;

	modena_init0_watermark_0 = readl(&bwlimiter->modena_init0_watermark_0) &
	                                 BW_LIMITER_BW_WATERMARK_MASK;

	/* Setting MReq Priority of the DSS*/
	mreqprio_0 |= 0x77;

	/*
	 * Set L3 Fast Configuration Register
	 * Limiting bandwith for ARM core to 700 MBPS
	 */
	modena_init0_bw_fractional |= 0x10;
	modena_init0_bw_integer |= 0x3;

	writel(mreqprio_0, &cdev->mreqprio_0);
	writel(mreqprio_1, &cdev->mreqprio_1);

	writel(modena_init0_bw_fractional, &bwlimiter->modena_init0_bw_fractional);
	writel(modena_init0_bw_integer, &bwlimiter->modena_init0_bw_integer);
	writel(modena_init0_watermark_0, &bwlimiter->modena_init0_watermark_0);

	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	char safe_string[HDR_NAME_LEN + 1];
	struct am43xx_board_id header;

	if (read_eeprom(&header) < 0)
		puts("Could not get board ID.\n");

	/* Now set variables based on the header. */
	strncpy(safe_string, (char *)header.name, sizeof(header.name));
	safe_string[sizeof(header.name)] = 0;
	setenv("board_name", safe_string);

	strncpy(safe_string, (char *)header.version, sizeof(header.version));
	safe_string[sizeof(header.version)] = 0;
	setenv("board_rev", safe_string);
#endif
	if (board_is_eposevm()) {
		gpio_request(CONFIG_QSPI_SEL_GPIO, "qspi_gpio");
		gpio_direction_output(CONFIG_QSPI_SEL_GPIO, 0);
	}
	return 0;
}
#endif

#ifdef CONFIG_DRIVER_TI_CPSW

static void cpsw_control(int enabled)
{
	/* Additional controls can be added here */
	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_addr	= 16,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_addr	= 1,
	},
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= CPSW_MDIO_BASE,
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 1,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.bd_ram_ofs		= 0x2000,
	.mac_control		= (1 << 5),
	.control		= cpsw_control,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};

int board_eth_init(bd_t *bis)
{
	int rv;
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = readl(&cdev->macid0l);
	mac_hi = readl(&cdev->macid0h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("ethaddr")) {
		puts("<ethaddr> not set. Validating first E-fuse MAC\n");
		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("ethaddr", mac_addr);
	}

	mac_lo = readl(&cdev->macid1l);
	mac_hi = readl(&cdev->macid1h);
	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!getenv("eth1addr")) {
		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("eth1addr", mac_addr);
	}

	if (board_is_eposevm()) {
		writel(RMII_MODE_ENABLE | RMII_CHIPCKL_ENABLE, &cdev->miisel);
		cpsw_slaves[0].phy_if = PHY_INTERFACE_MODE_RMII;
		cpsw_slaves[0].phy_addr = 16;
	} else {
		writel(RGMII_MODE_ENABLE, &cdev->miisel);
		cpsw_slaves[0].phy_if = PHY_INTERFACE_MODE_RGMII;
		cpsw_slaves[0].phy_addr = 0;
	}

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);

	return rv;
}
#endif
