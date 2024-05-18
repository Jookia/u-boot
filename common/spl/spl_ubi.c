// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2016
 * Ladislav Michl <ladis@linux-mips.org>
 */

#include <config.h>
#include <image.h>
#include <nand.h>
#include <onenand_uboot.h>
#include <ubispl.h>
#include <spl.h>

static ulong ram_spl_load_read(struct spl_load_info *load, ulong sector,
			       ulong count, void *buf)
{
	char *ubi_contents = load->priv;

	memcpy(buf, ubi_contents + sector, count);

	return count;
}

int spl_ubi_load_image(struct spl_image_info *spl_image,
		       struct spl_boot_device *bootdev)
{
	struct legacy_img_hdr *header;
	struct ubispl_info info;
	struct ubispl_load volumes[2];
	int ret = 1;

	switch (bootdev->boot_device) {
#ifdef CONFIG_SPL_SPINAND_SUPPORT
	case BOOT_DEVICE_SPINAND:
		spinand_init();
		info.read = spinand_spl_read_block;
		info.peb_size = CONFIG_SPL_SPINAND_BLOCK_SIZE;
		break;
#endif
#ifdef CONFIG_SPL_NAND_SUPPORT
	case BOOT_DEVICE_NAND:
		nand_init();
		info.read = nand_spl_read_block;
		info.peb_size = CONFIG_SYS_NAND_BLOCK_SIZE;
		break;
#endif
#ifdef CONFIG_SPL_ONENAND_SUPPORT
	case BOOT_DEVICE_ONENAND:
		info.read = onenand_spl_read_block;
		info.peb_size = CFG_SYS_ONENAND_BLOCK_SIZE;
		break;
#endif
	default:
		goto out;
	}
	info.ubi = (struct ubi_scan_info *)CONFIG_SPL_UBI_INFO_ADDR;
	info.fastmap = IS_ENABLED(CONFIG_MTD_UBI_FASTMAP);

	info.peb_offset = CONFIG_SPL_UBI_PEB_OFFSET;
	info.vid_offset = CONFIG_SPL_UBI_VID_OFFSET;
	info.leb_start = CONFIG_SPL_UBI_LEB_START;
	info.peb_count = CONFIG_SPL_UBI_MAX_PEBS - info.peb_offset;

#if CONFIG_IS_ENABLED(OS_BOOT)
	if (!spl_start_uboot()) {
		volumes[0].vol_id = CONFIG_SPL_UBI_LOAD_KERNEL_ID;
		volumes[0].load_addr = (void *)CONFIG_SYS_LOAD_ADDR;
		volumes[1].vol_id = CONFIG_SPL_UBI_LOAD_ARGS_ID;
		volumes[1].load_addr = (void *)CONFIG_SPL_PAYLOAD_ARGS_ADDR;

		ret = ubispl_load_volumes(&info, volumes, 2);
		if (!ret) {
			header = (struct legacy_img_hdr *)volumes[0].load_addr;
			spl_parse_image_header(spl_image, bootdev, header);
			puts("Linux loaded.\n");
			goto out;
		}
		puts("Loading Linux failed, falling back to U-Boot.\n");
	}
#endif
	/* Ensure there's enough room for the full UBI volume! */
	header = (void *)CONFIG_SYS_LOAD_ADDR;
#ifdef CONFIG_SPL_UBI_LOAD_BY_VOLNAME
	volumes[0].vol_id = -1;
	strlcpy(volumes[0].name,
		CONFIG_SPL_UBI_LOAD_MONITOR_VOLNAME,
		UBI_VOL_NAME_MAX + 1);
#else
	volumes[0].vol_id = CONFIG_SPL_UBI_LOAD_MONITOR_ID;
#endif
	volumes[0].load_addr = (void *)header;

	ret = ubispl_load_volumes(&info, volumes, 1);
	if (ret)
		goto out;

	spl_parse_image_header(spl_image, bootdev, header);

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		printf("Found FIT\n");
		load.priv = (char *)header;
		load.read = ram_spl_load_read;
		spl_set_bl_len(&load, 1);

		ret = spl_load_simple_fit(spl_image, &load, 0, header);
	}

out:
#ifdef CONFIG_SPL_SPINAND_SUPPORT
	if (bootdev->boot_device == BOOT_DEVICE_SPINAND)
		spinand_deselect();
#endif
#ifdef CONFIG_SPL_NAND_SUPPORT
	if (bootdev->boot_device == BOOT_DEVICE_NAND)
		nand_deselect();
#endif
	return ret;
}

/* Use priority 0 so that UBI will override all NAND methods */
SPL_LOAD_IMAGE_METHOD("NAND", 0, BOOT_DEVICE_NAND, spl_ubi_load_image);
SPL_LOAD_IMAGE_METHOD("OneNAND", 0, BOOT_DEVICE_ONENAND, spl_ubi_load_image);
SPL_LOAD_IMAGE_METHOD("SPINAND", 0, BOOT_DEVICE_SPINAND, spl_ubi_load_image);
