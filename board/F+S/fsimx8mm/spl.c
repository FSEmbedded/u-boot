/*
 * (C) Copyright 2018-2021
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 * Patrick Jakob, F&S Elektronik Systeme GmbH, jakob@fs-net.de
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 * Philipp Gerbach, F&S Elektronik Systeme GmbH, gerbach@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MM CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#define DEBUG

#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <nand.h>
#include <asm/arch/imx8m_ddr.h>

#include <asm/sections.h>
#include <sdp.h>

#include <asm/mach-imx/boot_mode.h>	/* BOOT_TYPE_* */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */

DECLARE_GLOBAL_DATA_PTR;

#define BT_PICOCOREMX8MM 0x0
#define BT_PICOCOREMX8MX 0x1

static const char *board_names[] = {
	"PicoCoreMX8MM",
	"PicoCoreMX8MM",
	"(unknown)"
};

static unsigned int board_type;
static unsigned int board_rev;
static const char *board_name;
static char board_name_lc[32];		/* Lower case (for FDT match) */
static enum boot_device boot_dev_cfg;	/* Boot device from configuration */
static const char *boot_dev_cfg_name;
static bool boot_dev_init_done;

#ifdef CONFIG_POWER
#define I2C_PMIC_8MM	3
#define I2C_PMIC_8MX	0

#define I2C_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PE)
#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)

struct i2c_pads_info i2c_pad_info_8mm = {
	.scl = {
		.i2c_mode = IMX8MM_PAD_I2C4_SCL_I2C4_SCL | PC,
		.gpio_mode = IMX8MM_PAD_I2C4_SCL_GPIO5_IO20 | PC,
		.gp = IMX_GPIO_NR(5, 20),
	},
	.sda = {
		.i2c_mode = IMX8MM_PAD_I2C4_SDA_I2C4_SDA | PC,
		.gpio_mode = IMX8MM_PAD_I2C4_SDA_GPIO5_IO21 | PC,
		.gp = IMX_GPIO_NR(5, 21),
	},
};

struct i2c_pads_info i2c_pad_info_8mx = {
	.scl = {
		.i2c_mode = IMX8MM_PAD_I2C1_SCL_I2C1_SCL | PC,
		.gpio_mode = IMX8MM_PAD_I2C1_SCL_GPIO5_IO14 | PC,
		.gp = IMX_GPIO_NR(5, 14),
	},
	.sda = {
		.i2c_mode = IMX8MM_PAD_I2C1_SDA_I2C1_SDA | PC,
		.gpio_mode = IMX8MM_PAD_I2C1_SDA_GPIO5_IO15 | PC,
		.gp = IMX_GPIO_NR(5, 15),
	},
};

int power_init_board(void)
{
	struct pmic *p;
	int ret;

	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MM:
		setup_i2c(I2C_PMIC_8MM, CONFIG_SYS_I2C_SPEED, 0x7f,
			  &i2c_pad_info_8mm);
		ret = power_bd71837_init(I2C_PMIC_8MM);
		break;
	case BT_PICOCOREMX8MX:
		setup_i2c(I2C_PMIC_8MX, CONFIG_SYS_I2C_SPEED, 0x7f,
			  &i2c_pad_info_8mx);
		ret = power_bd71837_init(I2C_PMIC_8MX);
		break;
	}

	if (ret)
		printf("power init failed");

	p = pmic_get("BD71837");

	pmic_probe(p);


	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(p, BD71837_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(p, BD71837_REGLOCK, 0x1);

	/* increase VDD_SOC to typical value 0.85v before first DRAM access */
	pmic_reg_write(p, BD71837_BUCK1_VOLT_RUN, 0x0f);

	switch (board_type)
	{
	case BT_PICOCOREMX8MM:
		/* increase VDD_DRAM to 0.975v f-*or 3Ghz DDR */
		pmic_reg_write(p, BD71837_BUCK5_VOLT, 0x83);
		break;
	case BT_PICOCOREMX8MX:
		/* increase VDD_DRAM to 0.9v for 3Ghz DDR */
		pmic_reg_write(p, BD71837_BUCK5_VOLT, 0x2);

		/* increase NVCC_DRAM_1V35 to 1.35v for DDR3L */
		pmic_reg_write(p, BD71837_BUCK8_VOLT, 0x37);
		break;
	}

	/* lock the PMIC regs */
	pmic_reg_write(p, BD71837_REGLOCK, 0x11);

	return 0;
}
#endif

#if 0 //#### Muss wieder nach fsimx8mm.c
#define USDHC1_CD_GPIO	IMX_GPIO_NR(1, 6)
/* SD_A_RST */
#define USDHC1_PWR_GPIO IMX_GPIO_NR(2, 10)

#define USDHC3_PWR_GPIO IMX_GPIO_NR(3, 16)

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
                         PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)


static iomux_v3_cfg_t const usdhc1_pads[] = {
	IMX8MM_PAD_SD1_CLK_USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_CMD_USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA0_USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA1_USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA2_USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA3_USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_RESET_B_GPIO2_IO10 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};
#if defined(CONFIG_SD_BOOT) && !defined(CONFIG_NAND_BOOT)
/*  */
static iomux_v3_cfg_t const usdhc3_pads[] = {
	IMX8MM_PAD_NAND_WE_B_USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_USDHC3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_USDHC3_DATA1| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_USDHC3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_USDHC3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_USDHC3_DATA4   | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE2_B_USDHC3_DATA5  | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE3_B_USDHC3_DATA6  | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_USDHC3_DATA7    | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE1_B_GPIO3_IO2     | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
	/* IMX8MM_PAD_NAND_CE1_B_USDHC3_STROBE | MUX_PAD_CTRL(USDHC_PAD_CTRL), */
	/* IMX8MM_PAD_NAND_READY_B_USDHC3_RESET_B */
	IMX8MM_PAD_NAND_READY_B_GPIO3_IO16 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};
#endif

static iomux_v3_cfg_t const cd_gpio_1[] = {
	IMX8MM_PAD_GPIO1_IO06_USDHC1_CD_B | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

/* Extended SDHC configuration. Pad count is without data signals, the data
 * signal count will be added automatically according to bus_width.
 */
struct fus_sdhc_cfg {
	const iomux_v3_cfg_t *const pads;
	const u8 count;
	const u8 index;
	u16 cd_gpio;
	struct fsl_esdhc_cfg esdhc;
};

enum usdhc_pads {
	usdhc1_ext, usdhc3_int
};

static struct fs_mmc_cfg sdhc_cfg[] = {
	/* pads,                    count, USDHC# */
	[usdhc1_ext] = { usdhc1_pads, 1,     1 },
#if defined(CONFIG_SD_BOOT) && !defined(CONFIG_NAND_BOOT)
	[usdhc3_int] = { usdhc3_pads, 4,     3 },
#endif
};

enum usdhc_cds {
	gpio1_io06
};

static const struct fs_mmc_cd sdhc_cd[] = {
	/* pad,          gpio */
	[gpio1_io06] = { cd_gpio_1,    IMX_GPIO_NR(1, 6) },
};

int board_mmc_init(bd_t *bd)
{
	int ret = 0;

	/* mmc0: USDHC1 (ext. micro SD slot on PicoCoreBBDSI), CD: GPIO1_IO06 */
	ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext],
			   &sdhc_cd[gpio1_io06]);
#if defined(CONFIG_SD_BOOT) && !defined(CONFIG_NAND_BOOT)	
	/* mmc1: USDHC3 (eMMC, if available), no CD */
	ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);

	gpio_request(USDHC3_PWR_GPIO, "usdhc3_reset");
	gpio_direction_output(USDHC3_PWR_GPIO, 0);
	udelay(500);
	gpio_direction_output(USDHC3_PWR_GPIO, 1);
#endif
	return ret;
}
#endif //###


#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MM_PAD_GPIO1_IO02_WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

static void wdog_init(void)
{
	struct wdog_regs *wdog = (struct wdog_regs*) WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);
}

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

static iomux_v3_cfg_t const uart_pads_mm[] = {
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MM_PAD_UART1_RXD_UART1_RX,
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MM_PAD_UART1_TXD_UART1_TX,
};

static iomux_v3_cfg_t const uart_pads_mx[] = {
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MM_PAD_SAI2_RXC_UART1_RX,
	MUX_PAD_CTRL(UART_PAD_CTRL) | IMX8MM_PAD_SAI2_RXFS_UART1_TX,
};

/* Setup and start serial debug port */
static void config_uart(int board_type)
{
	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MM:
		/* Setup UART pads */
		imx_iomux_v3_setup_multiple_pads(uart_pads_mm,
						 ARRAY_SIZE(uart_pads_mm));
		break;

	case BT_PICOCOREMX8MX:
		/* Setup UART pads */
		imx_iomux_v3_setup_multiple_pads(uart_pads_mx,
						 ARRAY_SIZE(uart_pads_mx));
		break;
	}

	preloader_console_init();
}

/* Pad settings if using NAND flash */
#define NAND_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_HYS)
#define NAND_PAD_READY0_CTRL (PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PUE)

static iomux_v3_cfg_t const nand_pads[] = {
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_ALE_RAWNAND_ALE,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_CE0_B_RAWNAND_CE0_B,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_CLE_RAWNAND_CLE,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA00_RAWNAND_DATA00,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA01_RAWNAND_DATA01,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA02_RAWNAND_DATA02,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA03_RAWNAND_DATA03,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA04_RAWNAND_DATA04,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA05_RAWNAND_DATA05,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA06_RAWNAND_DATA06,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_DATA07_RAWNAND_DATA07,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_RE_B_RAWNAND_RE_B,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_WE_B_RAWNAND_WE_B,
	MUX_PAD_CTRL(NAND_PAD_CTRL) | IMX8MM_PAD_NAND_WP_B_RAWNAND_WP_B,
	MUX_PAD_CTRL(NAND_PAD_READY0_CTRL)
				    | IMX8MM_PAD_NAND_READY_B_RAWNAND_READY_B,
};

static void fs_spl_init_nand_pads(void)
{
	puts("### Configuring NAND pins\n");
	imx_iomux_v3_setup_multiple_pads(nand_pads, ARRAY_SIZE(nand_pads));
	boot_dev_init_done = true;
}

/* Pad settings if using eMMC */
#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE \
			 |PAD_CTL_PE | PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)

static iomux_v3_cfg_t const emmc_pads[] = {
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_WE_B_USDHC3_CLK,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_WP_B_USDHC3_CMD,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_DATA04_USDHC3_DATA0,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_DATA05_USDHC3_DATA1,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_DATA06_USDHC3_DATA2,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_DATA07_USDHC3_DATA3,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_RE_B_USDHC3_DATA4,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_CE2_B_USDHC3_DATA5,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_CE3_B_USDHC3_DATA6,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_CLE_USDHC3_DATA7,
//###	MUX_PAD_CTRL(USDHC_PAD_CTRL) | IMX8MM_PAD_NAND_CE1_B_USDHC3_STROBE,
	MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL) | IMX8MM_PAD_NAND_CE1_B_GPIO3_IO2,
	/* IMX8MM_PAD_NAND_READY_B_USDHC3_RESET_B */
	MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL) | IMX8MM_PAD_NAND_READY_B_GPIO3_IO16,
};

static void fs_spl_init_emmc_pads(void)
{
	puts("### Configuring eMMC pins\n");
	imx_iomux_v3_setup_multiple_pads(emmc_pads, ARRAY_SIZE(emmc_pads));
	boot_dev_init_done = true;
}

#define OPEN_DRAIN_PAD_CTRL (PAD_CTL_DSE6 | PAD_CTL_ODE)

static iomux_v3_cfg_t const lvds_rst_8mm_120_pads =
	IMX8MM_PAD_GPIO1_IO13_GPIO1_IO13  | MUX_PAD_CTRL(OPEN_DRAIN_PAD_CTRL);

static iomux_v3_cfg_t const lvds_rst_8mm_130_pads =
	IMX8MM_PAD_SAI3_TXFS_GPIO4_IO31  | MUX_PAD_CTRL(OPEN_DRAIN_PAD_CTRL);

static iomux_v3_cfg_t const lvds_rst_8mx_110_pads =
	IMX8MM_PAD_GPIO1_IO08_GPIO1_IO8  | MUX_PAD_CTRL(OPEN_DRAIN_PAD_CTRL);

static void fs_board_early_init(void)
{
	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MM:
		if (board_rev < 130)
			imx_iomux_v3_setup_pad(lvds_rst_8mm_120_pads);
		else
			imx_iomux_v3_setup_pad(lvds_rst_8mm_130_pads);
		break;
	case BT_PICOCOREMX8MX:
			imx_iomux_v3_setup_pad(lvds_rst_8mx_110_pads);
		break;
	}
}

/* Do the basic board setup when we have our final BOARD-CFG */
static void basic_init(void)
{
	void *fdt = fs_image_get_cfg_addr(false);
	int offs = fs_image_get_cfg_offs(fdt);
	int i;
	char c;

	board_name = fdt_getprop(fdt, offs, "board-name", NULL);
	for (i = 0; i < ARRAY_SIZE(board_names); i++) {
		if (!strcmp(board_name, board_names[i]))
			break;
	}
	board_type = i;

	/* Switch to lower case for device tree name */
	i = 0;
	do {
		c = board_name[i];
		if ((c >= 'A') && (c <= 'Z'))
			c += 'a' - 'A';
		board_name_lc[i++] = c;
	} while (c);

	board_rev = fdt_getprop_u32_default_node(fdt, offs, 0,
						 "board-rev", 100);
	printf("### board_name=%s, lc=%s\n", board_name, board_name_lc);

	config_uart(board_type);

	if (!boot_dev_init_done) {
		switch (boot_dev_cfg) {
		case NAND_BOOT:
			fs_spl_init_nand_pads();
			break;

		case MMC3_BOOT:
			fs_spl_init_emmc_pads();
			break;

		default:
			printf("Unknown boot device %s (%d)\n",
			       boot_dev_cfg_name, boot_dev_cfg);
			break;
		}
	}

	fs_board_early_init();
	power_init_board();
}

/* If board is already configured, load config from NAND or eMMC */
static int fs_spl_early_load_boardcfg(void)
{
	int err;

	printf("### fused_boot_dev=%u\n", fs_board_get_boot_dev_from_fuses());
	switch (fs_board_get_boot_dev_from_fuses()) {
	case NAND_BOOT:
		fs_spl_init_nand_pads();
		nand_init();
	puts("###B\n");
		err = fs_image_cfg_nand();
	puts("###C\n");
		break;

	case MMC3_BOOT:
		fs_spl_init_emmc_pads();
		//### TODO: init mmc
		err = fs_image_cfg_mmc();
		break;

	case USB_BOOT:
	default:
		/* Board not configured, await config as part of USB config */
		err = -ENOENT;
		break;
	}

	return err;
}

void board_init_f(ulong dummy)
{
	int ret;
	unsigned int jobs_todo;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	wdog_init();
	timer_init();

#if 1
	/*
	 * Enable this to have early debug output before BOARD-CFG is loaded
	 * You have to provide the board type, we do not know it yet
	 */
	config_uart(BT_PICOCOREMX8MM);
	puts("###Hello\n");
#endif

	/* Init malloc_f pool and boot stages */
	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}
	enable_tzc380();

	/* Load BOARD-CFG from boot device configured in fuses */
	jobs_todo = FSIMG_JOB_CFG | FSIMG_JOB_DRAM | FSIMG_JOB_ATF;
#ifdef CONFIG_IMX_OPTEE
	jobs_todo |= FSIMG_JOB_TEE;
#endif
	if (!fs_spl_early_load_boardcfg()) {
		/* Load remaining part from current boot device */
	puts("###D\n");
		jobs_todo &= ~FSIMG_JOB_CFG;
		switch (get_boot_device()) {
		case NAND_BOOT:
	puts("###E\n");
			jobs_todo = fs_image_fw_nand(jobs_todo, basic_init);
			break;
		case MMC3_BOOT:
			jobs_todo = fs_image_fw_mmc(jobs_todo, basic_init);
			break;
		case USB_BOOT:
		default:
	puts("###F\n");
			/* Loading is done below */
			break;
		}
	}

	puts("###G\n");
	/* On load errors or if configured for USB, start system with SDP */
	if (jobs_todo) {
		puts("###H\n");
		fs_image_all_sdp(jobs_todo, basic_init);
	}

	/* At this point we have a valid system configuration */
#if 0 //####
	{
		int i;
		u32 tcm, ram;

		puts("### Checking RAM\n");
		for (i=0; i<0x30000; i+=4) {
			tcm = *(u32 *)(0x7e0000UL + i);
			*(u32 *)(0x40000000UL + i) = tcm;
		}
		for (i=0; i<0x30000; i+=4) {
			ram = *(u32 *)(0x40000000UL + i);
			tcm = *(u32 *)(0x7e0000UL + i);
			if (ram != tcm)
				printf("### mismatch: ram=0x%08x, tcm=0x%08x\n", ram, tcm);
		}
	}
#endif //####

	board_init_r(NULL, 0);
}

/* BL_ON */
#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pad =
	IMX8MM_PAD_SPDIF_TX_GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL);

void spl_board_init(void)
{
	imx_iomux_v3_setup_pad(bl_on_pad);
	/* backlight off*/
	gpio_request(BL_ON_PAD, "BL_ON");
	gpio_direction_output(BL_ON_PAD, 0);

#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif
	puts("Normal Boot\n");
}

/* Return the offset where U-Boot starts in NAND */
uint32_t spl_nand_get_uboot_raw_page(void)
{
	printf("### Loading U-Boot from NAND offset 0x%x\n", uboot_offs);
	return uboot_offs;
}

/* Return the sector number where U-Boot starts in eMMC (User HW partition) */
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc)
{
	printf("### Loading U-Boot from MMC offset 0x%x\n", uboot_offs);
	return uboot_offs / 512;
}

/* U-Boot is always loaded from the User HW partition */
int spl_boot_part(struct mmc *mmc)
{
	puts("### Loading U-Boot from part 0\n");
	return 0;
}

/*
 * Provide our own boot order, which in fact has just one entry: the current
 * boot device. Unfortunately the regular SPL infrastructure of U-Boot uses an
 * own set of boot devices (defined in arch/arm/include/asm/spl.h) that
 * differs from NXP's definitions in arch/arm/include/asm/mach-imx/boot_mode.h.
 * Here only BOOT_DEVICE_MMC1, BOOT_DEVICE_MMC2 and BOOT_DEVICE_MMC2_2 are
 * known as MMC devices. These values do not actually refer to a hardware
 * device of this number, they simply mean the MMC index. So BOOT_DEVICE_MMC1
 * is the first MMC device with index 0 and both BOOT_DEVICE_MMC2 as well as
 * BOOT_DEVICE_MMC2_2 point to the second MMC device with index 1. See
 * spl_mmc_get_device_index() in common/spl/spl_mmc.c. So at some point, the
 * NXP boot device has to be converted to U-Boot's device. This happens in
 * spl_boot_device() in arch/arm/mach-imx/spl.c.
 *
 * Unfortunately this mapping there does not fit our needs. We only have one
 * MMC device active in SPL, and this is the boot device. It will always be on
 * index 0, no matter what USDHC port we actually boot from. So we always have
 * to map to BOOT_DEVICE_MMC1. By overwriting the weak board_boot_order() in
 * common/spl/spl.c, we can change this to our needs.
 */
void board_boot_order(u32 *spl_boot_list)
{
	puts("### board_boot_order()\n");
	switch (get_boot_device()) {
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		break;
	case NAND_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_NAND;
		break;
	case USB_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_BOARD;
		break;
	default:
		spl_boot_list[0] = BOOT_DEVICE_NONE;
		break;
	}
}

#ifdef CONFIG_SPL_LOAD_FIT
/*
 * This function is called for each appended device tree. If we signal a match
 * (return value 0), the referenced device tree (and only this) is loaded
 * behind U-Boot. So from the view of U-Boot, it always has the right device
 * tree when starting. See doc/README.multi-dtb-fit for details.
 */
int board_fit_config_name_match(const char *name)
{
	debug("%s: %s\n", __func__, name);
	printf("### I am: %s, test for %s\n", board_name_lc, name);

	return strcmp(name, board_name_lc);
}
#endif
