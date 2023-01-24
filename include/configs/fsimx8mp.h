/*
 * Copyright (C) 2017 F&S Elektronik Systeme GmbH
 * Copyright (C) 2018-2019 F&S Elektronik Systeme GmbH
 * Copyright (C) 2021 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on i.MX8MP. This is
 * PicoCoreMX8MP.
 *
 * Activate with one of the following targets:
 *   make fsimx8mp_defconfig   Configure for i.MX8MP boards
 *   make                     Build uboot-spl.bin, u-boot.bin and u-boot-nodtb.bin.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
#define OCRAM_BASE		0x900000

 * TCM layout (SPL)
 * ----------------
 * unused
 *
 * OCRAM layout SPL                 U-Boot
 * ---------------------------------------------------------
 * 0x0090_0000: (Region reserved by ROM loader)(96KB)
 * 0x0091_8000: BOARD-CFG           BOARD-CFG (8KB)  CONFIG_FUS_BOARDCFG_ADDR
 * 0x0091_A000: BSS data            cfg_info  (2KB)  CONFIG_SPL_BSS_START_ADDR
 * 0x0091_A800: MALLOC_F pool       ---       (34KB) CONFIG_MALLOC_F_ADDR
 * 0x0092_3000: ---                 ---       (4KB)
 * 0x0092_7FF0: Global Data + Stack ---       (20KB) CONFIG_SPL_STACK
 * 0x0092_8000: SPL (<= ~208KB) (loaded by ROM-Loader, address defined by ATF)
 *     DRAM-FW: Training Firmware (up to 96KB, immediately behind end of SPL)
 * 0x0096_4000: DRAM Timing Data    ---       (16KB) CONFIG_SPL_DRAM_TIMING_ADDR
 * 0x0096_8000: ATF (8MP)           ATF       (96KB) CONFIG_SPL_ATF_ADDR
 * 0x0097_FFFF: END (8MP)
 *
 * The sum of SPL and DDR_FW must not exceed 240KB (0x3C000). However there is
 * still room to extend this region if SPL grows larger in the future, e.g. by
 * letting DRAM Timing Data overlap with ATF region.
 * After DRAM is available, SPL uses a MALLOC_R pool at 0x4220_0000.


 * NAND flash layout
 * -------------------------------------------------------------------------
 * Planned when U-Boot can write SPL/FCB/DBBT:
 * 0x0000_0000: BCB Copy 0 (FCB+DBBT) (128KB)               \
 * 0x0002_0000: BCB Copy 1 (FCB+DBBT) (128KB)                |
 * 0x0004_0000: SPL Copy 0 (256KB)                           |
 * 0x0008_0000: SPL Copy 1 (256KB)                           | "NBoot"
 * 0x000C_0000: Reserve in case of bad blocks (256KB)        |
 * 0x0010_0000: BOARD-CFG Copy 0 (8KB)                       |
 * 0x0010_2000: FIRMWARE Copy 0 (1528KB)                     |
 * 0x0028_0000: BOARD-CFG Copy 1 (8KB)                       |
 * 0x0028_0000: FIRMWARE Copy 1 (1528KB)                    /
 * 0x0040_0000: Refresh ...
 * Actually now, when SPL/FCB/DBBT/HDMI is written by NXP tool kobs:
 * 0x0000_0000: FCB Copy 0 (128KB)                          \
 * 0x0002_0000: FCB Copy 1 (128KB)                           |
 * 0x0004_0000: DBBT Copy 0 (128KB)                          |
 * 0x0006_0000: DBBT Copy 1 (128KB)                          |
 * 0x0008_0000: SPL Copy 0 (256KB)         Defined by FCB    | "NBoot"
 * 0x000C_0000: BOARD-CFG Copy 0 (8KB)     nboot-info: nboot-start[0]
 * 0x000C_2000: FIRMWARE Copy 0 (1272KB)                     |
 * 0x0020_0000: SPL Copy 1 (256KB)         Defined by Fuse   |
 * 0x0024_0000: BOARD-CFG Copy 1 (8KB)     nboot-info: nboot-start[1]
 * 0x0024_2000: FIRMWARE Copy 1 (1272KB)                    /
 * 0x0038_0000: Free (512KB)
 * 0x0040_0000: Refresh (512KB)
 * 0x0048_0000: UBootEnv (256KB)           FDT: u-boot,nand-env-offset
 * 0x004C_0000: UBootEnvRed (256KB)        FDT: u-boot,nand-env-offset-redundant
 * 0x0050_0000: UBoot_A (3MB)              nboot-info: uboot-start[0]
 * 0x0080_0000: UBoot_B/UBootRed (3MB)     nboot-info: uboot-start[1]
 * 0x00B0_0000: UserDef (2MB)
 * 0x00D0_0000: Kernel_A (32MB)
 * 0x02D0_0000: FDT_A (1MB)
 * 0x02E0_0000: Kernel_B (32MB, opt)
 * 0x04E0_0000: FDT_B (1MB, opt)
 * 0x04F0_0000: TargetFS as UBI Volumes
 *
 * Remarks:
 * - In this scenario, the fuses for the Secondary Image Offset have to be set
 *   to 1 (=2MB). This value can be set to 1MB*2^n, but the values for 0 and 2
 *   are switched so 0 becomes 4MB and 2 becomes 1MB.
 * - nboot-start[] is initialized with CONFIG_FUS_BOARDCFG_NAND0/1.
 * - If Kernel and FDT are part of the Rootfs, these partitions are dropped.
 * - If no Update with Set A and B is used, all _B partitions are dropped;
 *   UBoot_B is replaced by UserDef. This keeps all offsets up to and
 *   including FDT_A fix and also mtd numbers in Linux. In other words: the
 *   version with Update support just inserts Kernel_B and FDT_B in front of
 *   TargetFS and renames UserDef to UBoot_B.
 * - If the size of U-Boot will increase in the future, only UBoot_B must be
 *   moved. All other hardcoded offsets stay as they are.
 *
 * eMMC Layout
 * -----------
 * Scenario 1: NBoot is in Boot1/Boot2 HW-Partition
 *
 * Boot1/2 HW-Partition (Boot Offset for the Primary Image is 0x0000):
 * 0x0000_0000: SPL Copy 0/1 (512KB)       i.MX8X (always 0)
 * 0x0008_0000: BOARD-CFG Copy 0/1 (8KB)   nboot-info: nboot-start[0]
 * 0x0008_2000: FIRMWARE Copy 0/1 (120KB)
 * 0x0010_0000: --- (free, for compatibility reasons with fsimx8mm, 224KB)
 * 0x0013_8000: UBootEnv (16KB)            Defined in device tree
 * 0x0013_C000: UBootEnvRed (16KB)         Defined in device tree
 * 0x0014_0000: --- (free, may be used for U-Boot in the future)
 *
 * User HW-Partition:
 * 0x0000_0000: --- (free, space for GPT, 32KB)
 * 0x0000_8000: --- (free, 1248KB, may be used for UserDef/M4 image)
 * 0x0014_0000: U-Boot A (3MB)
 * 0x0044_0000: U-Boot B (3MB)
 * 0x0074_0000: --- (free, 768KB)
 * 0x0080_0000: Regular filesystem partitions (Kernel, TargetFS, etc)
 *
 * The goal here is to move U-Boot to the Boot partition in the next release
 * to get the whole 8MB reserved region in User space empty for UserDef/M4
 * image and to make writing whole filesystem images easier. Currently U-Boot
 * is always destroyed when this region is not skipped when writing.
 *
 * Remarks:
 * - In this scenario, setting the fuses for the Secondary Image Offset is not
 *   necessary.
 * - spl-start of nboot-info is ignored and silently assumed to be 0.
 * - nboot-start[] of nboot-info is set to CONFIG_FUS_BOARDCFG_MMC0/1 by the
 *   Makefile, but only nboot-start[0] and uboot-start[0] are taken for both
 *   copies in the two Boot HW-Partitions.
 * - If eMMC is configured to boot from Boot1, then this is the Primary Image
 *   and Boot2 is the Secondary Image. If eMMC is configured to boot from
 *   Boot2, then this is the Primary Image and Boot1 is the Secondary Image.
 * - The U-Boot environment is always stored with the Primary Image, i.e. the
 *   partition that is configured for boot.
 * - The reserved region size at the beginning of the User HW-Partition can
 *   stay at 8MB as with NXP, for example to hold the UserDef data or an M4
 *   image. Or it can be reduced to the size of the partition table which is
 *   one simple sector when using MBR.
 *
 * Scenario 2: NBoot is in User HW-Partition
 *
 * Boot1/2 HW-Partition:
 * 0x0000_0000: --- (completely empty)
 *
 * User HW-Partition (Boot Offset for the Primary Image is 0x8000):
 * 0x0000_0000: --- (space for GPT, 32KB)
 * 0x0000_8000: SPL Copy 0 (224KB)         i.MX8X; nboot-info: spl-start[0]
 * 0x0004_0000: BOARD-CFG Copy 0 (8KB)     nboot-info: nboot-start[0]
 * 0x0004_2000: FIRMWARE Copy 0 (760KB)
 * 0x0010_0000: SPL Copy 1 (224KB)         Secondary Image Offset, spl-start[1]
 * 0x0013_8000: UBootEnv (16KB)            Defined in device tree
 * 0x0013_C000: UBootEnvRed (16KB)         Defined in device tree
 * 0x0014_0000: U-Boot A (3MB)
 * 0x0044_0000: U-Boot B (3MB)
 * 0x0074_0000: BOARD-CFG Copy 1 (8KB)     nboot-info: nboot-start[1]
 * 0x0074_2000: FIRMWARE Copy 1 (760KB)
 * 0x0080_0000: Regular filesystem partitions (Kernel, TargetFS, etc)
 *
 * Remarks:
 * - In this scenario, the fuses for the Secondary Image Offset have to be set
 *   to 2 (=1MB). This value can be set to 1MB*2^n, so this is the smallest
 *   possible setting.
 * - The reserved region size stays at 8MB as with NXP.
 * - nboot-start[] of nboot-info is set to CONFIG_FUS_BOARDCFG_MMC0/1 by the
 *   Makefile, and both entries for spl-start, nboot-start and uboot-start are
 *   actually used.
 */

#ifndef __FSIMX8MP_H
#define __FSIMX8MP_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

/* disable FAT write because its doesn't work
 *  with F&S FAT driver
 */
#undef CONFIG_FAT_WRITE

/* need for F&S bootaux */
#define M4_BOOTROM_BASE_ADDR           MCU_BOOTROM_BASE_ADDR
#define IMX_SIP_SRC_M4_START           IMX_SIP_SRC_MCU_START
#define IMX_SIP_SRC_M4_STARTED         IMX_SIP_SRC_MCU_STARTED

#define CONFIG_SYS_UART_PORT	1	/* Default UART port */
#define CONFIG_CONS_INDEX       (CONFIG_SYS_UART_PORT)

#define CONFIG_SPL_MAX_SIZE		(152 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(1024 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

/*
 * The memory layout on stack:  DATA section save + gd + early malloc
 * the idea is re-use the early malloc (CONFIG_SYS_MALLOC_F_LEN) with
 * CONFIG_SYS_SPL_MALLOC_START
 */
#define CONFIG_FUS_BOARDCFG_ADDR	0x00918000
#define CONFIG_SPL_BSS_START_ADDR      0x0091A000
#define CONFIG_SPL_BSS_MAX_SIZE		0x2000	/* 8 KB */

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x927FF0
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT

/* Offsets in eMMC where BOARD-CFG and FIRMWARE are stored */
#define CONFIG_FUS_BOARDCFG_MMC0 0x00080000
#define CONFIG_FUS_BOARDCFG_MMC1 0x00740000

#define CONFIG_SYS_SPL_MALLOC_START    0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE     SZ_1M //SZ_512K	/* 512 KB */
#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF

/* These addresses are hardcoded in ATF */
#define CONFIG_SPL_USE_ATF_ENTRYPOINT
#define CONFIG_SPL_ATF_ADDR 0x00968000
#define CONFIG_SPL_TEE_ADDR 0x56000000

/* TCM Address where DRAM Timings are loaded to */
#define CONFIG_SPL_DRAM_TIMING_ADDR 0x00964000

#define CONFIG_MALLOC_F_ADDR		0x91A800 /* malloc f used before GD_FLG_FULL_MALLOC_INIT set */

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_PCA9450

#define CONFIG_SYS_I2C

#endif

/* Add F&S update */
#define CONFIG_CMD_UPDATE
#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG

#define CONFIG_REMAKE_ELF
/* ENET Config */
/* ENET1 */
#define CONFIG_SYS_DISCOVER_PHY

#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "eth0" /* Set qos to primary since we use its MDIO */
#define FDT_SEQ_MACADDR_FROM_ENV

#define CONFIG_FEC_XCV_TYPE             RGMII
//#define CONFIG_FEC_MXC_PHYADDR          5
#define FEC_QUIRK_ENET_MAC

//#define DWC_NET_PHYADDR					4
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY     (1 * SZ_1M)     /* 1M */
#endif

#define PHY_ANEG_TIMEOUT 20000

#define CONFIG_PHY_ATHEROS
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_ROOTPATH		"/rootfs"

#endif



#define CONFIG_BOOTFILE		"Image"
#define CONFIG_PREBOOT
#ifdef CONFIG_FS_UPDATE_SUPPORT
	#define CONFIG_BOOTCOMMAND	"run selector; run set_bootargs; run kernel; run fdt"
#else
	#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; run fdt"
#endif

#define MTDIDS_DEFAULT		""
#define MTDPART_DEFAULT		""
#define MTDPARTS_STD		"setenv mtdparts "
#define MTDPARTS_UBIONLY	"setenv mtdparts "

/* Add some variables that are not predefined in U-Boot. For example set
   fdt_high to 0xffffffff to avoid that the device tree is relocated to the
   end of memory before booting, which is not necessary in our setup (and
   would result in problems if RAM is larger than ~1,7GB).

   All entries with content "undef" will be updated in board_late_init() with
   a board-specific value (detected at runtime).

   We use ${...} here to access variable names because this will work with the
   simple command line parser, who accepts $(...) and ${...}, and also the
   hush parser, who accepts ${...} and plain $... without any separator.

   If a variable is meant to be called with "run" and wants to set an
   environment variable that contains a ';', we can either enclose the whole
   string to set in (escaped) double quotes, or we have to escape the ';' with
   a backslash. However when U-Boot imports the environment from NAND into its
   hash table in RAM, it interprets escape sequences, which will remove a
   single backslash. So we actually need an escaped backslash, i.e. two
   backslashes. Which finally results in having to type four backslashes here,
   as each backslash must also be escaped with a backslash in C. */
#define BOOT_WITH_FDT "\\\\; booti ${loadaddr} - ${fdtaddr}\0"

#ifdef CONFIG_BOOTDELAY
#define FSBOOTDELAY
#else
#define FSBOOTDELAY "bootdelay=undef\0"
#endif

#if defined(CONFIG_ENV_IS_IN_MMC)
	#define FILSEIZE2BLOCKCOUNT "block_size=200\0" 	\
		"filesize2blockcount=" \
			"setexpr test_rest \\${filesize} % \\${block_size}; " \
			"if test \\${test_rest} = 0; then " \
				"setexpr blockcount \\${filesize} / \\${block_size}; " \
			"else " \
				"setexpr blockcount \\${filesize} / \\${block_size}; " \
				"setexpr blocckount \\${blockcount} + 1; " \
			"fi;\0"
#else
	#define FILSEIZE2BLOCKCOUNT
#endif

#ifdef CONFIG_FS_UPDATE_SUPPORT

	#define FS_UPDATE_SUPPORT "BOOT_ORDER=A B\0" \
	"BOOT_ORDER_ALT=undef\0" \
	"BOOT_A_LEFT=3\0" \
	"BOOT_B_LEFT=3\0" \
	"rauc_cmd=rauc.slot=A\0" \
	"selector=undef\0" \
	".selector_mmc=setenv selector " \
	"'if test \"x${BOOT_ORDER_ALT}\" != \"x${BOOT_ORDER}\"; then	"														\
	"setenv rauc_cmd undef; "																								\
		"for BOOT_SLOT in \"${BOOT_ORDER}\"; do "																			\
		  "if test \"x${BOOT_SLOT}\" = \"xA\" && test ${BOOT_A_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is A\"; "																\
			  "setexpr BOOT_A_LEFT ${BOOT_A_LEFT} - 1; "																	\
			  "setenv boot_partition 1;"																					\
			  "setenv rootfs_partition 5;"																					\
			  "setenv rauc_cmd rauc.slot=A;"																				\
		  "elif test \"x${BOOT_SLOT}\" = \"xB\" && test ${BOOT_B_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is B\"; "																\
			  "setexpr BOOT_B_LEFT ${BOOT_B_LEFT} - 1; "																	\
			  "setenv boot_partition 2;"																					\
			  "setenv rootfs_partition 6;"																					\
			  "setenv rauc_cmd rauc.slot=B;"																				\
		  "fi;"																												\
		"done;"																												\
	"saveenv;"																												\
	"fi;'\0"																												\
	"'if test \"x${BOOT_ORDER_ALT}\" != \"x${BOOT_ORDER}\"; then	"														\
	"setenv rauc_cmd undef; "																								\
		"for BOOT_SLOT in \"${BOOT_ORDER}\"; do "																			\
		  "if test \"x${BOOT_SLOT}\" = \"xA\" && test ${BOOT_A_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is A\"; "																\
			  "setexpr BOOT_A_LEFT ${BOOT_A_LEFT} - 1; "																	\
			  "setenv boot_partition KernelA;"																				\
			  "setenv fdt_partition FDTA;"																					\
			  "setenv rootfs_partition rootfsA;"																			\
			  "setenv rootfs_ubi_number 0;"																					\
			  "setenv rauc_cmd rauc.slot=A;"																				\
		  "elif test \"x${BOOT_SLOT}\" = \"xB\" && test ${BOOT_B_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is B\"; "																\
			  "setexpr BOOT_B_LEFT ${BOOT_B_LEFT} - 1; "																	\
			  "setenv boot_partition KernelB;"																				\
			  "setenv rootfs_partition rootfsB;"																			\
			  "setenv fdt_partition FDTB;"																					\
			  "setenv rootfs_ubi_number 1;"																					\
			  "setenv rauc_cmd rauc.slot=B;"																				\
		  "fi;"																												\
		"done;"																												\
	"saveenv;"																												\
	"fi;'\0"																												\
	"boot_partition=undef\0" \
	".boot_partition_mmc= setenv boot_partition 1\0"\
	"rootfs_partition=undef\0"\
	".rootfs_partition_mmc=setenv rootfs_partition 4\0"\

	#define ROOTFS_MEM 	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p\\\\${boot_partition} rootwait\0"
	#define KERNEL_MEM 	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev}:\\\\${boot_partition}\0" \

	#define FDT_MEM 	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev}:\\\\${boot_partition} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

	#define BOOTARGS 	"set_rootfs=undef\0" \
				".set_rootfs_mmc=setenv set_rootfs 'setenv rootfs root=/dev/mmcblk${mmcdev}p${rootfs_partition} rootfstype=squashfs rootwait'\0" \
				"set_bootargs=run set_rootfs; setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra} ${rauc_cmd}\0"
#else


	#define FS_UPDATE_SUPPORT
	#define ROOTFS_MEM 	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0"

	#define KERNEL_MEM	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev} . ${bootfile}\0"

	#define FDT_MEM 	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

	#define BOOTARGS "set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"
#endif

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS					\
	FS_UPDATE_SUPPORT 						\
	"bd_kernel=undef\0"						\
	"bd_fdt=undef\0"							\
	"bd_rootfs=undef\0"						\
	"initrd_addr=0x43800000\0"					\
	"initrd_high=0xffffffffffffffff\0"				\
	"console=undef\0"						\
	".console_none=setenv console\0"				\
	".console_serial=setenv console console=${sercon},${baudrate}\0" \
	".console_display=setenv console console=tty1\0"		\
	"login=undef\0"							\
	".login_none=setenv login login_tty=null\0"			\
	".login_serial=setenv login login_tty=${sercon},${baudrate}\0"	\
	".login_display=setenv login login_tty=tty1\0"			\
	"mtdids=undef\0"						\
	"mtdparts=undef\0"						\
	".mtdparts_std=" MTDPARTS_STD "\0"				\
	"mmcdev=undef\0"						\
	".network_off=setenv network\0"					\
	".network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	".network_dhcp=setenv network ip=dhcp\0"			\
	"rootfs=undef\0"						\
	ROOTFS_MEM							\
	".rootfs_nfs=setenv rootfs root=/dev/nfs nfsroot=${serverip}:${rootpath}\0" \
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0"		\
	"kernel=undef\0"						\
	KERNEL_MEM 							\
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0"		\
	".kernel_nfs=setenv kernel nfs . ${serverip}:${rootpath}/${bootfile}\0" \
	".kernel_usb=setenv kernel usb start\\\\; load usb 0 . ${bootfile}\0" \
	"fdt=undef\0"							\
	"fdtaddr=0x43100000\0"						\
	FDT_MEM 							\
	".fdt_none=setenv fdt booti\0"					\
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_nfs=setenv fdt nfs ${fdtaddr} ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT \
	".fdt_usb=setenv fdt usb start\\\\; load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	FILSEIZE2BLOCKCOUNT					\
	"mode=undef\0"							\
	".mode_rw=setenv mode rw\0"					\
	".mode_ro=setenv mode ro\0"					\
	"netdev=eth0\0"							\
	"init=undef\0"							\
	".init_init=setenv init\0"					\
	".init_linuxrc=setenv init init=linuxrc\0"			\
	"sercon=undef\0"						\
	"installcheck=undef\0"						\
	"updatecheck=undef\0"						\
	"recovercheck=undef\0"						\
	"platform=undef\0"						\
	"arch=fsimx8mp\0"						\
	"bootfdt=undef\0"						\
	"m4_uart4=disable\0"						\
	FSBOOTDELAY							\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	BOOTARGS

/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

#define CONFIG_SYS_INIT_RAM_ADDR	0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x00080000
#define CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_INIT_RAM_SIZE
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define SECURE_PARTITIONS	"UBoot", "Kernel", "FDT", "Images"

/************************************************************************
 * Environment
 ************************************************************************/

/* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#define CONFIG_ENV_OVERWRITE			/* Allow overwriting ethaddr */

/* Fallback values if values in the device tree are missing/damaged */
//#define CONFIG_ENV_OFFSET_REDUND 0x104000

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		SZ_32M

/* Totally 1GB LPDDR4 */
/* Link Definitions */
#define CONFIG_LOADADDR		0x40480000

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM		0x40000000
#define PHYS_SDRAM_SIZE		0x40000000	/* 1 GB */

#define CONFIG_SYS_MEMTEST_START	0x40000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + \
					(PHYS_SDRAM_SIZE >> 1))

/* have to define for F&S serial_mxc driver */
#define UART1_BASE UART1_BASE_ADDR
#define UART2_BASE UART2_BASE_ADDR
#define UART3_BASE UART3_BASE_ADDR
#define UART4_BASE UART4_BASE_ADDR
#define UART5_BASE 0xFFFFFFFF

#define CONFIG_MXC_UART_BASE		UART2_BASE_ADDR

/* Monitor Command Prompt */

/************************************************************************
 * Command Line Editor (Shell)
 ************************************************************************/
#ifdef CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

/* Input and print buffer sizes */
#define CONFIG_SYS_CBSIZE	512	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	640	/* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	/* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */

#define CONFIG_IMX_BOOTAUX

#define CONFIG_FSL_USDHC
/* SPL use the CONFIG_SYS_MMC_ENV_DEV in
 * serial download mode. Otherwise use
 * board_mmc_get_env_dev function.
 * (s. mmc_get_env_dev in mmc_env.c)
 */
#define CONFIG_SYS_MMC_ENV_DEV		2 /* USDHC3 */
#define CONFIG_SYS_FSL_ESDHC_ADDR	0

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#ifdef CONFIG_FSL_FSPI
#define FSL_FSPI_FLASH_SIZE		SZ_32M
#define FSL_FSPI_FLASH_NUM		1
#define FSPI0_BASE_ADDR			0x30bb0000
#define FSPI0_AMBA_BASE			0x0
#define CONFIG_FSPI_QUAD_SUPPORT

#define CONFIG_SYS_FSL_FSPI_AHB
#endif
#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_USBD_HS

#define CONFIG_CMD_USB_MASS_STORAGE
#define CONFIG_USB_GADGET_MASS_STORAGE
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#endif

#define CONFIG_USB_MAX_CONTROLLER_COUNT		2
#define CONFIG_USB_GADGET_VBUS_DRAW		2

#ifdef CONFIG_DM_VIDEO
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

#endif
