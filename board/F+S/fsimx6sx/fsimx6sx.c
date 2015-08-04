/*
 * fsimx6sx.c
 *
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX6 CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/errno.h>
#ifdef CONFIG_CMD_NET
#include <miiphy.h>
#include <netdev.h>
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			/* PON_*, POFF_* */
#endif
#include <serial.h>			/* struct serial_device */

#ifdef CONFIG_GENERIC_MMC
#include <mmc.h>
#include <fsl_esdhc.h>			/* fsl_esdhc_initialize(), ... */
#endif

#ifdef CONFIG_CMD_LED
#include <status_led.h>			/* led_id_t */
#endif

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/setup.h>			/* struct tag_fshwconfig, ... */
#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/iomux.h>
#include <asm/imx-common/iomux-v3.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/crm_regs.h>		/* CCM_CCGR1, nandf clock settings */
#include <asm/arch/clock.h>		/* enable_fec_anatop_clock(), ... */

#include <linux/mtd/nand.h>		/* struct mtd_info, struct nand_chip */
#include <mtd/mxs_nand_fus.h>		/* struct mxs_nand_fus_platform_data */
#include <usb.h>			/* USB_INIT_HOST, USB_INIT_DEVICE */

/* ------------------------------------------------------------------------- */

#define NBOOT_ARGS_BASE (PHYS_SDRAM + 0x00001000) /* Arguments from NBoot */
#define BOOT_PARAMS_BASE (PHYS_SDRAM + 0x100)	  /* Arguments to Linux */

#define BT_EFUSA9X    0

/* Features set in tag_fshwconfig.chFeature1 */
#define FEAT1_2NDCAN  (1<<1)		/* 0: 1x CAN, 1: 2x CAN */
#define FEAT1_2NDLAN  (1<<4)		/* 0: 1x LAN, 1: 2x LAN */

/* Features set in tag_fshwconfig.chFeature2 */
#define FEAT2_M4      (1<<0)		/* CPU has Cortex-M4 core */

#define ACTION_RECOVER 0x00000040	/* Start recovery instead of update */

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)


#define UART_PAD_CTRL  (PAD_CTL_PUS_100K_UP |			\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |			\
	PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#if 0 // mx6
#define ENET_PAD_CTRL ( \
		PAD_CTL_PKE | PAD_CTL_PUE | /*Kiepfer*/ 	\
		PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |	\
		PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)
#else // mx6sx
#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
        PAD_CTL_SPEED_HIGH   |                                   \
        PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST)
#define ENET_CLK_PAD_CTRL  (PAD_CTL_SPEED_MED | \
        PAD_CTL_DSE_120ohm   | PAD_CTL_SRE_FAST)
#define ENET_RX_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |          \
        PAD_CTL_SPEED_HIGH   | PAD_CTL_SRE_FAST)
#endif

#define GPMI_PAD_CTRL0 (PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_PUS_100K_UP)
#define GPMI_PAD_CTRL1 (PAD_CTL_DSE_40ohm | PAD_CTL_SPEED_MED | PAD_CTL_SRE_FAST)
#define GPMI_PAD_CTRL2 (GPMI_PAD_CTRL0 | GPMI_PAD_CTRL1)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_22K_UP  | PAD_CTL_SPEED_LOW |		\
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)


struct board_info {
	char *name;			/* Device name */
	unsigned int mach_type;		/* Device machine ID */
	char *bootdelay;		/* Default value for bootdelay */
	char *updatecheck;		/* Default value for updatecheck */
	char *installcheck;		/* Default value for installcheck */
	char *recovercheck;		/* Default value for recovercheck */
	char *earlyusbinit;		/* Default value for earlyusbinit */
	char *console;			/* Default variable for console */
	char *login;			/* Default variable for login */
	char *mtdparts;			/* Default variable for mtdparts */
	char *network;			/* Default variable for network */
	char *init;			/* Default variable for init */
	char *rootfs;			/* Default variable for rootfs */
	char *kernel;			/* Default variable for kernel */
	char *fdt;			/* Default variable for device tree */
	char *fsload;			/* Default variable for load command */
};

#if defined(CONFIG_MMC) && defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc,usb"
#elif defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "mmc"
#elif defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define UPDATE_DEF "usb"
#else
#define UPDATE_DEF NULL
#endif
#if defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
#define EARLY_USB "1"
#else
#define EARLY_USB NULL
#endif

const struct board_info fs_board_info[8] = {
	{	/* 0 (BT_EFUSA9X) */
		.name = "efusA9X",
		.mach_type = 0xFFFFFFFF,
		.bootdelay = "3",
		.updatecheck = UPDATE_DEF,
		.installcheck = UPDATE_DEF,
		.recovercheck = UPDATE_DEF,
		.earlyusbinit = EARLY_USB,
		.console = ".console_serial",
		.login = ".login_serial",
		.mtdparts = ".mtdparts_std",
		.network = ".network_off",
		.init = ".init_init",
		.rootfs = ".rootfs_ubifs",
		.kernel = ".kernel_nand",
		.fdt = ".fdt_nand",
		.fsload = ".fsload_fat",
	},
	{	/* 1 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 2 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 3 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 4 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 5 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 6 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
	{	/* 7 (unknown) */
		.name = "unknown",
		.mach_type = 0,
	},
};

/* String used for system prompt */
static char fs_sys_prompt[20];

/* Copy of the NBoot args, split into hwconfig and m4config */
static struct tag_fshwconfig fs_nboot_args;
static struct tag_fsm4config fs_m4_args;

/* Get the number of the debug port reported by NBoot */
static unsigned int get_debug_port(unsigned int dwDbgSerPortPA)
{
	unsigned int port = 6;
	struct serial_device *sdev;

	do {
		sdev = get_serial_device(--port);
		if (sdev && sdev->dev.priv == (void *)dwDbgSerPortPA)
			return port;
	} while (port);

	return CONFIG_SYS_UART_PORT;
}

struct serial_device *default_serial_console(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	/* As long as GD_FLG_RELOC is not set, we can not access fs_nboot_args
	   and therefore have to use the NBoot args at NBOOT_ARGS_BASE.
	   However GD_FLG_RELOC may be set before the NBoot arguments are
	   copied from NBOOT_ARGS_BASE to fs_nboot_args (see board_init()
	   below). But then at least the .bss section and therefore
	   fs_nboot_args is cleared. So if fs_nboot_args.dwDbgSerPortPA is 0,
	   the structure is not yet copied and we still have to look at
	   NBOOT_ARGS_BASE. Otherwise we can (and must) use fs_nboot_args. */
	if ((gd->flags & GD_FLG_RELOC) && fs_nboot_args.dwDbgSerPortPA)
		pargs = &fs_nboot_args;
	else
		pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;

	return get_serial_device(get_debug_port(pargs->dwDbgSerPortPA));
}

/* Check board type */
int checkboard(void)
{
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;

	printf("Board: %s Rev %u.%02u (%dx DRAM)\n",
	       fs_board_info[pargs->chBoardType - 8].name,
	       pargs->chBoardRev / 100, pargs->chBoardRev % 100,
	       pargs->dwNumDram);

#if 0 //###
	printf("dwNumDram = 0x%08x\n", pargs->dwNumDram);
	printf("dwMemSize = 0x%08x\n", pargs->dwMemSize);
	printf("dwFlashSize = 0x%08x\n", pargs->dwFlashSize);
	printf("dwDbgSerPortPA = 0x%08x\n", pargs->dwDbgSerPortPA);
	printf("chBoardType = 0x%02x\n", pargs->chBoardType);
	printf("chBoardRev = 0x%02x\n", pargs->chBoardRev);
	printf("chFeatures1 = 0x%02x\n", pargs->chFeatures1);
	printf("chFeatures2 = 0x%02x\n", pargs->chFeatures2);
#endif

	return 0;
}

/* Set the available RAM size. We have a memory bank starting at 0x10000000
   that can hold up to 3840MB of RAM. However up to now we only have 256MB or
   512MB on F&S i.MX6 boards. */
int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs;

	pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	gd->ram_size = pargs->dwMemSize << 20;
	gd->ram_base = PHYS_SDRAM;

	return 0;
}

/* Now RAM is valid, U-Boot is relocated. From now on we can use variables */
int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct tag_fshwconfig *pargs = (struct tag_fshwconfig *)NBOOT_ARGS_BASE;
	unsigned int board_type = pargs->chBoardType - 8;

	/* Save a copy of the NBoot args */
	memcpy(&fs_nboot_args, pargs, sizeof(struct tag_fshwconfig));
	fs_nboot_args.chBoardType = board_type;
	fs_nboot_args.dwSize = sizeof(struct tag_fshwconfig);
	memcpy(&fs_m4_args, pargs+1, sizeof(struct tag_fsm4config));
	fs_m4_args.dwSize = sizeof(struct tag_fsm4config);

	gd->bd->bi_arch_number = fs_board_info[board_type].mach_type;
	gd->bd->bi_boot_params = BOOT_PARAMS_BASE;

	/* Prepare the command prompt */
	sprintf(fs_sys_prompt, "%s # ", fs_board_info[board_type].name);

#if 0 //###
	printf("### PLL_ARM=0x%08x\n", readl(0x20c8000));
	printf("### PLL_USB1=0x%08x\n", readl(0x20c8010));
	printf("### PLL_USB2=0x%08x\n", readl(0x20c8020));
	printf("### PLL_SYS=0x%08x\n", readl(0x20c8030));
	printf("### PLL_AUDIO=0x%08x\n", readl(0x20c8070));
	printf("### PLL_VIDEO=0x%08x\n", readl(0x20c80A0));
	printf("### PLL_ENET=0x%08x\n", readl(0x20c80e0));

	enable_usb2_anatop_clock();
	printf("### PLL_USB2=0x%08x\n", readl(0x20c8020));
#endif

	return 0;
}


/* nand flash pads */
static iomux_v3_cfg_t const nfc_pads[] = {
	IOMUX_PADS(PAD_NAND_CLE__RAWNAND_CLE | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_ALE__RAWNAND_ALE | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_NAND_WP_B__RAWNAND_WP_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_READY_B__RAWNAND_READY_B | MUX_PAD_CTRL(GPMI_PAD_CTRL0)),
	IOMUX_PADS(PAD_NAND_CE0_B__RAWNAND_CE0_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_RE_B__RAWNAND_RE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_WE_B__RAWNAND_WE_B | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA00__RAWNAND_DATA00 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA01__RAWNAND_DATA01 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA02__RAWNAND_DATA02 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA03__RAWNAND_DATA03 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA04__RAWNAND_DATA04 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA05__RAWNAND_DATA05 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA06__RAWNAND_DATA06 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
	IOMUX_PADS(PAD_NAND_DATA07__RAWNAND_DATA07 | MUX_PAD_CTRL(GPMI_PAD_CTRL2)),
};

/* Register NAND devices. We actually split the NAND into two virtual devices
   to allow different ECC strategies for NBoot and the rest. */
void board_nand_init(void)
{
	int reg;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	struct mxs_nand_fus_platform_data pdata;

	/* config gpmi nand iomux */
	SETUP_IOMUX_PADS(nfc_pads);

#if 0 // ### Keep clock source as set by NBoot
	/* Set GPMI clock source (enfc) to USB0 (480MHz) */
	reg = readl(&mxc_ccm->cs2cdr);
	reg &= ~(3 << 16);
	reg |= (2 << 16);
	writel(reg, &mxc_ccm->cs2cdr);
#endif

	/* Enable GPMI and BCH clocks */
	reg = readl(&mxc_ccm->CCGR4);
	reg |= 0xFF003000;
	writel(reg, &mxc_ccm->CCGR4);

	/* Enable APBH DMA clock*/
	reg = readl(&mxc_ccm->CCGR0);
	reg |= 0x00000030;
	writel(reg, &mxc_ccm->CCGR0);

	/* The first device skips the NBoot region (2 blocks) to protect it
	   from inadvertent erasure. The skipped region can not be written
	   and is always read as 0xFF. */
	pdata.options = NAND_BBT_SCAN2NDPAGE;
	pdata.timing0 = 0;
	pdata.ecc_strength = fs_nboot_args.chECCtype;
	pdata.skipblocks = 2;
	pdata.flags = MXS_NAND_CHUNK_1K;
#ifdef CONFIG_NAND_REFRESH
	pdata.backup_sblock = CONFIG_SYS_NAND_BACKUP_START_BLOCK;
	pdata.backup_eblock = CONFIG_SYS_NAND_BACKUP_END_BLOCK;
#endif
	mxs_nand_register(0, &pdata);

#if CONFIG_SYS_MAX_NAND_DEVICE > 1
	/* The second device just consists of the NBoot region (2 blocks) and
	   is software write-protected by default. It uses a different ECC
	   strategy. ### TODO ### In fact we actually need special code to
	   store the NBoot image. */
	pdata.options |= NAND_SW_WRITE_PROTECT;
	pdata.ecc_strength = 40;
	pdata.flags = MXS_NAND_SKIP_INVERSE;
#ifdef CONFIG_NAND_REFRESH
	pdata.backupstart = 0;
	pdata.backupend = 0;
#endif
	mxs_nand_register(0, &pdata);
#endif
}

void board_nand_state(struct mtd_info *mtd, unsigned int state)
{
	/* Save state to pass it to Linux later */
	fs_nboot_args.chECCstate |= (unsigned char)state;
}

size_t get_env_size(void)
{
	return ENV_SIZE_DEF_LARGE;
}

size_t get_env_range(void)
{
	return ENV_RANGE_DEF_LARGE;
}

size_t get_env_offset(void)
{
	return ENV_OFFSET_DEF_LARGE;
}

#ifdef CONFIG_CMD_UPDATE
enum update_action board_check_for_recover(void)
{
	/* If the board should do an automatic recovery is given in the
	   dwAction value. Currently this is only defined for CUBEA5, AGATEWAY
	   and HGATEWAY. If a special button is pressed for a defined time
	   when power is supplied, the system should be reset to the default
	   state, i.e. perform a complete recovery. The button is detected in
	   NBoot, but recovery takes place in U-Boot. */
	if (fs_nboot_args.dwAction & ACTION_RECOVER)
		return UPDATE_ACTION_RECOVER;
	return UPDATE_ACTION_UPDATE;
}
#endif

#ifdef CONFIG_GENERIC_MMC
static iomux_v3_cfg_t const usdhc1_pads[] = {
	IOMUX_PADS(PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO1_IO03__GPIO1_IO_3 | MUX_PAD_CTRL(NO_PAD_CTRL)),	/* WP */
	IOMUX_PADS(PAD_GPIO1_IO02__GPIO1_IO_2 | MUX_PAD_CTRL(NO_PAD_CTRL)),	/* CD */
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	IOMUX_PADS(PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO1_IO07__GPIO1_IO_7 | MUX_PAD_CTRL(NO_PAD_CTRL)),	/* WP */
	IOMUX_PADS(PAD_GPIO1_IO06__GPIO1_IO_6 | MUX_PAD_CTRL(NO_PAD_CTRL)),	/* CD */
};

struct fsl_esdhc_cfg esdhc_cfg[] = {
	{
		.esdhc_base = USDHC1_BASE_ADDR,
		.sdhc_clk = MXC_ESDHC_CLK,
		.max_bus_width = 4,
	},
	{
		.esdhc_base = USDHC2_BASE_ADDR,
		.sdhc_clk = MXC_ESDHC2_CLK,
		.max_bus_width = 4,
	},
	{
		.esdhc_base = USDHC3_BASE_ADDR,
		.sdhc_clk = MXC_ESDHC3_CLK,
		.max_bus_width = 4,
	},
	{
		.esdhc_base = USDHC4_BASE_ADDR,
		.sdhc_clk = MXC_ESDHC4_CLK,
		.max_bus_width = 4,
	},
};

int board_mmc_getcd(struct mmc *mmc)
{
	int cd_pin;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
		if (mmc->block_dev.dev == 0)
			cd_pin = IMX_GPIO_NR(1, 6); /* USDHC2 */
		else
			cd_pin = IMX_GPIO_NR(1, 2); /* USDHC1 */
		return !gpio_get_value(cd_pin);

	default:
		return 1;		/* Assume card is present */
	}
}

int board_mmc_init(bd_t *bis)
{
	int index;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;
	unsigned int gpio_cd = 0xFFFFFFFF;
	int ret;

	ccgr6 = readl(&mxc_ccm->CCGR6);

	/* Configure first SD card slot */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
		/* USDHC2: ext. SD slot (connector, normal-size SD slot on
		   efus SKIT), Write Protect (WP) on GPIO_7 (ignored), Card
		   Detect (CD) on GPIO_6 (GPIO1_IO6) */
		SETUP_IOMUX_PADS(usdhc2_pads);
		gpio_cd = IMX_GPIO_NR(1, 6);
		ccgr6 |= (3 << 4);
		index = 1;
		break;

	default:
		return 0;		/* Unknown device */
	}

	if (gpio_cd != 0xFFFFFFFF)
		gpio_direction_input(gpio_cd);
	writel(ccgr6, &mxc_ccm->CCGR6);

	esdhc_cfg[index].sdhc_clk = mxc_get_clock(esdhc_cfg[index].sdhc_clk);
	ret = fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
	if (ret)
		return ret;

#if 0 //### TODO: activate micro-SD card, but only if WLAN is not mounted
	/* Configure second SD card slot (if available) */
	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
		/* USDHC1: ext. SD slot (connector, micro SD slot on efus
		   SKIT), Write Protect (WP) on GPIO_3 (ignored), Card Detect
		   (CD) on GPIO_2 (GPIO1_IO2). Alternatively used as SDIO for
		   on-board WLAN. */
		gpio_cd = IMX_GPIO_NR(1, 2);
		SETUP_IOMUX_PADS(usdhc1_pads);
		ccgr6 |= (3 << 2);
		index = 0;
		break;

	default:
		return 0; 		/* No more SD card slots */
	}

	if (gpio_cd != 0xFFFFFFFF)
		gpio_direction_input(gpio_cd);
	writel(ccgr6, &mxc_ccm->CCGR6);

	esdhc_cfg[index].sdhc_clk = mxc_get_clock(esdhc_cfg[index].sdhc_clk);
	ret = fsl_esdhc_initialize(bis, &esdhc_cfg[index]);
	if (ret)
		return ret;
#endif //###

	/* ### TODO: efusA9X has a third SDIO slot on USDHC4 with 8 data lines,
	   for on-board EMMC. */

	return 0;
}
#endif

#ifdef CONFIG_USB_EHCI_MX6
/* USB Host power (efusA9X) on GPIO_12 (GPIO1_IO12) */
static iomux_v3_cfg_t const usb_pwr_pads[] = {
//###	IOMUX_PADS(PAD_GPIO1_IO12__GPIO1_IO_12 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	IOMUX_PADS(PAD_GPIO1_IO12__USB_OTG2_PWR | MUX_PAD_CTRL(NO_PAD_CTRL)),
//###	IOMUX_PADS(PAD_GPIO1_IO13__ANATOP_OTG2_ID | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)

int board_ehci_hcd_init(int port)
{
	u32 *usbnc_usb_ctrl;

	if (port > 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
#if 0
		SETUP_IOMUX_PADS(usb_pwr_pads);

		/* Enable USB Host power */
		gpio_direction_output(IMX_GPIO_NR(1, 12), 1);
#endif
		break;

	default:
		break;
	}

#if 1 //###
	usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
				 port * 4);

	/* Set Power polarity */
	setbits_le32(usbnc_usb_ctrl, UCTRL_PWR_POL);
#endif //###

        return 0;
}

#if 0 //###
int board_ehci_power(int port, int on)
{
	if (port != 1)
		return 0;

	switch (fs_nboot_args.chBoardType) {
	case BT_EFUSA9X:
		SETUP_IOMUX_PADS(usb_pwr_pads);

		/* Enable USB Host power */
		gpio_direction_output(IMX_GPIO_NR(1, 12), on);

		break;

	default:
		break;
	}

	return 0;
}
#endif

int board_usb_phy_mode(int port)
{
	if (port == 0)
		return USB_INIT_DEVICE;
	return USB_INIT_HOST;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
void setup_var(const char *varname, const char *content, int runvar)
{
	char *envvar = getenv(varname);

	/* If variable is not set or does not contain string "undef", do not
	   change it */
	if (!envvar || strcmp(envvar, "undef"))
		return;

	/* Either set variable directly with value ... */
	if (!runvar) {
		setenv(varname, content);
		return;
	}

	/* ... or set variable by running the variable with name in content */
	content = getenv(content);
	if (content)
		run_command(content, 0);
}

/* Use this slot to init some final things before the network is started. We
   set up some environment variables for things that are board dependent and
   can't be defined as a fix value in fsvybrid.h. As an unset value is valid
   for some of these variables, we check for the special value "undef". Any
   of these variables that holds this value will be replaced with the
   board-specific value. */
int board_late_init(void)
{
	unsigned int boardtype = fs_nboot_args.chBoardType;
	const struct board_info *bi = &fs_board_info[boardtype];

	/* Set sercon variable if not already set */
	if (strcmp(getenv("sercon"), "undef") == 0) {
		char sercon[DEV_NAME_SIZE];

		sprintf(sercon, "%s%c", CONFIG_SYS_SERCON_NAME,
			'0' + get_debug_port(fs_nboot_args.dwDbgSerPortPA));
		setenv("sercon", sercon);
	}

	/* Set platform variable if not already set */
	if (strcmp(getenv("platform"), "undef") == 0) {
		char lcasename[20];
		char *p = bi->name;
		char *l = lcasename;
		char c;

		do {
			c = *p++;
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			*l++ = c;
		} while (c);

		setenv("platform", lcasename);
	}

	/* Set some variables with a direct value */
	setup_var("bootdelay", bi->bootdelay, 0);
	setup_var("updatecheck", bi->updatecheck, 0);
	setup_var("installcheck", bi->installcheck, 0);
	setup_var("recovercheck", bi->recovercheck, 0);
	setup_var("earlyusbinit", bi->earlyusbinit, 0);
	setup_var("mtdids", MTDIDS_DEFAULT, 0);
	setup_var("partition", MTDPART_DEFAULT, 0);
	setup_var("mode", CONFIG_MODE, 0);

	/* Set some variables by runnning another variable */
	setup_var("console", bi->console, 1);
	setup_var("login", bi->login, 1);
	setup_var("mtdparts", bi->mtdparts, 1);
	setup_var("fsload", bi->fsload, 1);
	setup_var("network", bi->network, 1);
	setup_var("init", bi->init, 1);
	setup_var("rootfs", bi->rootfs, 1);
	setup_var("kernel", bi->kernel, 1);
	setup_var("fdt", bi->fdt, 1);
	setup_var("bootargs", "set_bootargs", 1);

	return 0;
}
#endif

#ifdef CONFIG_CMD_NET
/* enet pads definition */
static iomux_v3_cfg_t const enet_pads_rgmii[] = {
	IOMUX_PADS(PAD_ENET2_CRS__ENET1_MDIO | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_COL__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET2_RX_CLK__ENET2_REF_CLK_25M | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL)),
	IOMUX_PADS(PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TXC__ENET1_RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD0__ENET1_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD1__ENET1_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD2__ENET1_TX_DATA_2 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TD3__ENET1_TX_DATA_3 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_TX_CTL__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RXC__ENET1_RX_CLK | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD0__ENET1_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD1__ENET1_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD2__ENET1_RX_DATA_2 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RD3__ENET1_RX_DATA_3 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII1_RX_CTL__ENET1_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TXC__ENET2_RGMII_TXC | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD0__ENET2_TX_DATA_0 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD1__ENET2_TX_DATA_1 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD2__ENET2_TX_DATA_2 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TD3__ENET2_TX_DATA_3 | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_TX_CTL__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RXC__ENET2_RX_CLK | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD0__ENET2_RX_DATA_0 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD1__ENET2_RX_DATA_1 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD2__ENET2_RX_DATA_2 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RD3__ENET2_RX_DATA_3 | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),
	IOMUX_PADS(PAD_RGMII2_RX_CTL__ENET2_RX_EN | MUX_PAD_CTRL(ENET_RX_PAD_CTRL)),	/* Phy Interrupt */
	IOMUX_PADS(PAD_ENET2_TX_CLK__GPIO2_IO_9 | MUX_PAD_CTRL(NO_PAD_CTRL)),
	/* Phy Reset */
	IOMUX_PADS(PAD_ENET1_MDC__GPIO2_IO_2 | MUX_PAD_CTRL(NO_PAD_CTRL)),
};

int board_eth_init(bd_t *bis)
{
	u32 reg, gpr1;
	int ret;
	int phy_addr;
	enum xceiver_type xcv_type;
	enum enet_freq freq;
	struct iomuxc *iomux_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
        struct anatop_regs *anatop = (struct anatop_regs *)ANATOP_BASE_ADDR;

	/* Set the IOMUX for ENET */
	switch (fs_nboot_args.chBoardType) {
	default:
		/* Use 1 GBit/s LAN on RGMII pins */
		SETUP_IOMUX_PADS(enet_pads_rgmii);
		/* ENET CLK is generated in PHY and is an input */
		gpr1 = readl(&iomux_regs->gpr[1]);
		gpr1 &= ~IOMUX_GPR1_FEC1_MASK;
		writel(gpr1, &iomux_regs->gpr[1]);

		freq = ENET_125MHz;
		//###freq = ENET_25MHz;
		break;
	}

        reg = readl(&anatop->pll_enet);
        reg |= BM_ANADIG_PLL_ENET_REF_25M_ENABLE;
        writel(reg, &anatop->pll_enet);

	/* Activate ENET PLL */
	ret = enable_fec_anatop_clock(freq);
	if (ret < 0)
		return ret;

	/* Reset the PHY */
	switch (fs_nboot_args.chBoardType) {
	default:
		/* Atheros AR8035: reset phy (GPIO_2_2) for at least 0.5 ms */
		gpio_direction_output(IMX_GPIO_NR(2, 2), 0);
		udelay(500);
		gpio_set_value(IMX_GPIO_NR(2, 2), 1);
		phy_addr = 4;
		xcv_type = RGMII;
		break;
	}

	//### TODO: Add second ETH port

	return fecmxc_initialize_multi_type(bis, -1, phy_addr, IMX_FEC_BASE,
					    xcv_type);
}
#endif /* CONFIG_CMD_NET */

/* Return the board name; we have different boards that use this file, so we
   can not define the board name with CONFIG_SYS_BOARDNAME */
char *get_board_name(void)
{
	return fs_board_info[fs_nboot_args.chBoardType].name;
}


/* Return the system prompt; we can not define it with CONFIG_SYS_PROMPT
   because we want to include the board name, which is variable (see above) */
char *get_sys_prompt(void)
{
	return fs_sys_prompt;
}

/* ### FIXME: arch/arm/cpu/armv7/mx6/soc.c passes the CPU type in this value
   by default. imx-lib in buildroot builds a kernel module and defines its own
   macros for cpu_is_mx6q() and cpu_is_mx6dl() that rely on this value here.
   Actually this is wrong and imx-lib should use the macros from kernel source
   arch/arm/plat-mxc/include/mach/mxc.h instead. But as long as imx-lib does
   it this wrong way, we must not override the original function from
   arch/arm/cpu/armv7/mx6/soc.c here. */
#if 0 //###
/* Return the board revision; this is called when Linux is started and the
   value is passed to Linux */
unsigned int get_board_rev(void)
{
	return fs_nboot_args.chBoardRev;
}
#endif

/* Return a pointer to the hardware configuration; this is called when Linux
   is started and the structure is passed to Linux */
struct tag_fshwconfig *get_board_fshwconfig(void)
{
	return &fs_nboot_args;
}

/* Return a pointer to the M4 image and configuration; this is called when
   Linux is started and the structure is passed to Linux */
struct tag_fsm4config *get_board_fsm4config(void)
{
	return &fs_m4_args;
}
