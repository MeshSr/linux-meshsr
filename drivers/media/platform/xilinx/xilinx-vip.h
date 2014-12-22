/*
 * Xilinx Video IP Core
 *
 * Copyright (C) 2013 Ideas on Board SPRL
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __XILINX_VIP_H__
#define __XILINX_VIP_H__

#include <linux/io.h>
#include <media/v4l2-subdev.h>

/* Xilinx Video IP Control Registers */
#define XVIP_CTRL_CONTROL			0x0000
#define XVIP_CTRL_CONTROL_SW_ENABLE		(1 << 0)
#define XVIP_CTRL_CONTROL_REG_UPDATE		(1 << 1)
#define XVIP_CTRL_CONTROL_BYPASS		(1 << 4)
#define XVIP_CTRL_CONTROL_TEST_PATTERN		(1 << 5)
#define XVIP_CTRL_CONTROL_FRAME_SYNC_RESET	(1 << 30)
#define XVIP_CTRL_CONTROL_SW_RESET		(1 << 31)
#define XVIP_CTRL_STATUS			0x0004
#define XVIP_CTRL_STATUS_PROC_STARTED		(1 << 0)
#define XVIP_CTRL_STATUS_EOF			(1 << 1)
#define XVIP_CTRL_ERROR				0x0008
#define XVIP_CTRL_ERROR_SLAVE_EOL_EARLY		(1 << 0)
#define XVIP_CTRL_ERROR_SLAVE_EOL_LATE		(1 << 1)
#define XVIP_CTRL_ERROR_SLAVE_SOF_EARLY		(1 << 2)
#define XVIP_CTRL_ERROR_SLAVE_SOF_LATE		(1 << 3)
#define XVIP_CTRL_IRQ_ENABLE			0x000c
#define XVIP_CTRL_IRQ_ENABLE_PROC_STARTED	(1 << 0)
#define XVIP_CTRL_IRQ_EOF			(1 << 1)
#define XVIP_CTRL_VERSION			0x0010
#define XVIP_CTRL_VERSION_MAJOR_MASK		(0xff << 24)
#define XVIP_CTRL_VERSION_MAJOR_SHIFT		24
#define XVIP_CTRL_VERSION_MINOR_MASK		(0xff << 16)
#define XVIP_CTRL_VERSION_MINOR_SHIFT		16
#define XVIP_CTRL_VERSION_REVISION_MASK		(0xf << 12)
#define XVIP_CTRL_VERSION_REVISION_SHIFT	12
#define XVIP_CTRL_VERSION_PATCH_MASK		(0xf << 8)
#define XVIP_CTRL_VERSION_PATCH_SHIFT		8
#define XVIP_CTRL_VERSION_INTERNAL_MASK		(0xff << 0)
#define XVIP_CTRL_VERSION_INTERNAL_SHIFT	0

/* Xilinx Video IP Timing Registers */
#define XVIP_TIMING_ACTIVE_SIZE			0x0020
#define XVIP_TIMING_ACTIVE_VSIZE_MASK		(0x7ff << 16)
#define XVIP_TIMING_ACTIVE_VSIZE_SHIFT		16
#define XVIP_TIMING_ACTIVE_HSIZE_MASK		(0x7ff << 0)
#define XVIP_TIMING_ACTIVE_HSIZE_SHIFT		0
#define XVIP_TIMING_OUTPUT_ENCODING		0x0028
#define XVIP_TIMING_OUTPUT_NBITS_8		(0 << 4)
#define XVIP_TIMING_OUTPUT_NBITS_10		(1 << 4)
#define XVIP_TIMING_OUTPUT_NBITS_12		(2 << 4)
#define XVIP_TIMING_OUTPUT_NBITS_16		(3 << 4)
#define XVIP_TIMING_OUTPUT_NBITS_MASK		(3 << 4)
#define XVIP_TIMING_OUTPUT_NBITS_SHIFT		4
#define XVIP_TIMING_VIDEO_FORMAT_YUV422		(0 << 0)
#define XVIP_TIMING_VIDEO_FORMAT_YUV444		(1 << 0)
#define XVIP_TIMING_VIDEO_FORMAT_RGB		(2 << 0)
#define XVIP_TIMING_VIDEO_FORMAT_YUV420		(3 << 0)
#define XVIP_TIMING_VIDEO_FORMAT_MASK		(3 << 0)
#define XVIP_TIMING_VIDEO_FORMAT_SHIFT		0

/**
 * struct xvip_device - Xilinx Video IP device structure
 * @subdev: V4L2 subdevice
 * @dev: (OF) device
 * @iomem: device I/O register space remapped to kernel virtual memory
 */
struct xvip_device {
	struct v4l2_subdev subdev;
	struct device *dev;
	void __iomem *iomem;
};

/**
 * struct xvip_video_format - Xilinx Video IP video format description
 * @name: AXI4 format name
 * @width: AXI4 format width in bits per component
 * @bpp: bytes per pixel (when stored in memory)
 * @code: media bus format code
 * @fourcc: V4L2 pixel format FCC identifier
 */
struct xvip_video_format {
	const char *name;
	unsigned int width;
	unsigned int bpp;
	unsigned int code;
	u32 fourcc;
};

const struct xvip_video_format *xvip_get_format_by_code(unsigned int code);
const struct xvip_video_format *xvip_get_format_by_fourcc(u32 fourcc);
const struct xvip_video_format *xvip_of_get_format(struct device_node *node);

static inline u32 xvip_read(struct xvip_device *xvip, u32 addr)
{
	return ioread32(xvip->iomem + addr);
}

static inline void xvip_write(struct xvip_device *xvip, u32 addr, u32 value)
{
	iowrite32(value, xvip->iomem + addr);
}

#endif /* __XILINX_VIP_H__ */
