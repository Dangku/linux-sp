/*
 * gc0310.c - gc0310 Image Sensor Driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "gc0310.h"

// Bayer RAW8 (RGGB pattern)
static const struct regval gc0310_raw8_640x480_regs[] = {
	//system reg
	{0xfe, 0xf0}, {0xfe, 0xf0}, {0xfe, 0x00}, {0xfc, 0x0e},
	{0xfc, 0x0e}, {0xf2, 0x80}, {0xf3, 0x00}, {0xf7, 0x33},
	{0xf8, 0x03}, {0xf9, 0x8e}, {0xfa, 0x11},

	//CISCTL reg
	{0x00, 0x2f}, {0x01, 0x0f}, {0x02, 0x04}, {0x03, 0x03},
	{0x04, 0xe8}, {0x05, 0x01}, {0x06, 0x0a}, {0x07, 0x00},
	{0x08, 0x89}, {0x09, 0x00}, {0x0a, 0x00}, {0x0b, 0x00},
	{0x0c, 0x06}, {0x0d, 0x01}, {0x0e, 0xe8}, {0x0f, 0x02},
	{0x10, 0x88}, {0x17, 0x14}, {0x18, 0x1a}, {0x19, 0x14},
	{0x1b, 0x48}, {0x1e, 0x6b}, {0x1f, 0x28}, {0x20, 0x89},
	{0x21, 0x49}, {0x22, 0xb0}, {0x23, 0x04}, {0x24, 0x16},
	{0x34, 0x20},

	// BLK
	{0x26, 0x23}, {0x28, 0xff}, {0x29, 0x00}, {0x33, 0x18},
	{0x37, 0x20}, {0x47, 0x80}, {0x4e, 0x66}, {0xa8, 0x02},
	{0xa9, 0x80},

	//ISP reg
	{0x40, 0xff}, {0x41, 0x21}, {0x42, 0xcf}, {0x44, 0x18},
	{0x45, 0xaf}, {0x46, 0x03}, {0x4a, 0x11}, {0x4b, 0x01},
	{0x4c, 0x20}, {0x4d, 0x05}, {0x4f, 0x01}, {0x50, 0x01},
	{0x55, 0x01}, {0x56, 0xe0}, {0x57, 0x02}, {0x58, 0x80},

	// GAIN
	{0x70, 0x50}, {0x5a, 0x98}, {0x5b, 0xdc}, {0x5c, 0xfe},
	{0x77, 0x74}, {0x78, 0x40}, {0x79, 0x5f},

	// DNDD
	{0x82, 0x05}, {0x83, 0x0b},

	// EEINTP
	{0x8f, 0xff}, {0x90, 0x8c}, {0x91, 0x90}, {0x92, 0x06},
	{0x93, 0x04}, {0x95, 0x46}, {0x96, 0x46},

	// ASDE
	{0xfe, 0x00}, {0x9a, 0x20}, {0x9b, 0x80}, {0x9c, 0x40},
	{0x9d, 0x80}, {0xa1, 0x30}, {0xa2, 0x32}, {0xa4, 0x30},
	{0xa5, 0x30}, {0xaa, 0x50}, {0xac, 0x22},

	// GAMMA
	{0xbf, 0x12}, {0xc0, 0x1d}, {0xc1, 0x35}, {0xc2, 0x4e},
	{0xc3, 0x63}, {0xc4, 0x76}, {0xc5, 0x87}, {0xc6, 0xa2},
	{0xc7, 0xb8}, {0xc8, 0xca}, {0xc9, 0xd8}, {0xca, 0xe3},
	{0xcb, 0xeb}, {0xcc, 0xf0}, {0xcd, 0xf8}, {0xce, 0xfc},
	{0xcf, 0xff},

	// YCP
	{0xd0, 0x40}, {0xd1, 0x34}, {0xd2, 0x34}, {0xd3, 0x40},
	{0xd6, 0xf2}, {0xd7, 0x1b}, {0xd8, 0x18}, {0xdd, 0x73},

	// AEC
	{0xfe, 0x01}, {0x05, 0x30}, {0x06, 0x75}, {0x07, 0x40},
	{0x08, 0xb0}, {0x0a, 0xc5}, {0x0c, 0x00}, {0x12, 0x52},
	{0x13, 0x30}, {0x1f, 0x30}, {0x20, 0x40}, {0x25, 0x00},
	{0x26, 0x7d}, {0x27, 0x02}, {0x28, 0x71}, {0x29, 0x03},
	{0x2a, 0xe8}, {0x2b, 0x05}, {0x2c, 0xdc}, {0x2d, 0x07},
	{0x2e, 0x53}, {0x3c, 0x20}, {0x3e, 0x40}, {0x3f, 0x5c},
	{0x40, 0x7b}, {0x41, 0xbd}, {0x42, 0xf6}, {0x43, 0x63},
	{0x03, 0x60}, {0x44, 0x03},

	// AWB
	{0x1c, 0x91}, {0x21, 0x15}, {0x50, 0x80}, {0x56, 0x04},
	{0x59, 0x08}, {0x5b, 0x02}, {0x61, 0x8d}, {0x62, 0xa7},
	{0x63, 0xd0}, {0x65, 0x06}, {0x66, 0x06}, {0x67, 0x84},
	{0x69, 0x08}, {0x6a, 0x25}, {0x6b, 0x01}, {0x6c, 0x00},
	{0x6d, 0x02}, {0x6e, 0xf0}, {0x6f, 0x80}, {0x78, 0xaf},
	{0x79, 0x75}, {0x7a, 0x40}, {0x7b, 0x50}, {0x7c, 0x0c},
	{0x90, 0x00}, {0x91, 0x00}, {0xa6, 0xb7}, {0xa7, 0x94},
	{0x92, 0xfb}, {0x93, 0xd8}, {0xa9, 0xb7}, {0xaa, 0x93},
	{0x95, 0x1b}, {0x96, 0xfc}, {0xab, 0xae}, {0xac, 0x87},
	{0x97, 0x4f}, {0x98, 0x1b}, {0xae, 0xc4}, {0xaf, 0xaf},
	{0x9a, 0x47}, {0x9b, 0x1b}, {0xb0, 0xd0}, {0xb1, 0xab},
	{0x9c, 0x67}, {0x9d, 0x51}, {0xb3, 0x00}, {0xb4, 0x00},
	{0x9f, 0x00}, {0xa0, 0x00}, {0xb5, 0x00}, {0xb6, 0x00},
	{0xa1, 0x00}, {0xa2, 0x00}, {0x86, 0x00}, {0x87, 0x00},
	{0x88, 0x00}, {0x89, 0x00}, {0x8b, 0x00}, {0x8c, 0x00},
	{0x8d, 0x00}, {0x8e, 0x00}, {0x94, 0x50}, {0x99, 0xa6},
	{0x9e, 0xaa}, {0xa3, 0x00}, {0x8a, 0x00}, {0xa8, 0x50},
	{0xad, 0x55}, {0xb2, 0x55}, {0xb7, 0x00}, {0x8f, 0x00},
	{0xb8, 0xe6}, {0xb9, 0x6f},

	// CC
	{0xfe, 0x01}, {0xd0, 0x40}, {0xd1, 0xf9}, {0xd2, 0x02},
	{0xd3, 0xff}, {0xd4, 0x40}, {0xd5, 0x00}, {0xd6, 0x3f},
	{0xd7, 0xf8}, {0xd8, 0x02}, {0xd9, 0x08}, {0xda, 0x40},
	{0xdb, 0x00},

	// LSC
	{0xfe, 0x01}, {0x76, 0x80}, {0xc1, 0x3c}, {0xc2, 0x50},
	{0xc3, 0x00}, {0xc4, 0x48}, {0xc5, 0x20}, {0xc6, 0x1d},
	{0xc7, 0x10}, {0xc8, 0x00}, {0xc9, 0x00}, {0xdc, 0x20},
	{0xdd, 0x10}, {0xdf, 0x00}, {0xde, 0x00},

	// Histogram
	{0x01, 0x10}, {0x0b, 0x31}, {0x0e, 0x50}, {0x0f, 0x0f},
	{0x10, 0x6e}, {0x12, 0xa0}, {0x15, 0x60}, {0x16, 0x60},
	{0x17, 0xe0},

	// Measure Window
	{0xcc, 0x0c}, {0xcd, 0x10}, {0xce, 0xa0}, {0xcf, 0xe6},

	// dark sun
	{0x45, 0xf7}, {0x46, 0xff}, {0x47, 0x15}, {0x48, 0x03},
	{0x4f, 0x60},

	// MIPI
	{0xfe, 0x03}, {0x01, 0x03}, {0x02, 0x22}, {0x03, 0x14},
	{0x04, 0x01}, {0x05, 0x00}, {0x06, 0x80}, {0x10, 0x94},
	{0x11, 0x2a}, {0x12, 0x80}, {0x13, 0x02}, {0x15, 0x12},
	{0x17, 0x01}, {0xfe, 0x00},
};

// YUY2/YUYV, YUV422
static const struct regval gc0310_yuy2_640x480_regs[] = {
	// system reg
	{0xfe,0xf0}, {0xfe,0xf0}, {0xfe,0x00}, {0xfc,0x0e},
	{0xfc,0x0e}, {0xf2,0x80}, {0xf3,0x00}, {0xf7,0x1b},
	{0xf8,0x04}, {0xf9,0x8e}, {0xfa,0x11},

	// MIPI reg
	{0xfe,0x03}, {0x40,0x08}, {0x42,0x00}, {0x43,0x00},
	{0x01,0x03}, {0x10,0x84},
	{0x01,0x03}, {0x02,0x11}, {0x03,0x94}, {0x04,0x01},
	{0x05,0x00}, {0x06,0x80}, {0x11,0x1e}, {0x12,0x00},
	{0x13,0x05}, {0x15,0x10}, {0x21,0x10}, {0x22,0x01},
	{0x23,0x10}, {0x24,0x02}, {0x25,0x10}, {0x26,0x03},
	{0x29,0x02}, {0x2a,0x0a}, {0x2b,0x04}, {0xfe,0x00},

	// CISCTL reg
	{0x00,0x2f}, {0x01,0x0f}, {0x02,0x04}, {0x03,0x03},
	{0x04,0x50}, {0x09,0x00}, {0x0a,0x00}, {0x0b,0x00},
	{0x0c,0x04}, {0x0d,0x01}, {0x0e,0xe8}, {0x0f,0x02},
	{0x10,0x88}, {0x16,0x00}, {0x17,0x14}, {0x18,0x1a},
	{0x19,0x14}, {0x1b,0x48}, {0x1c,0x1c}, {0x1e,0x6b},
	{0x1f,0x28}, {0x20,0x8b}, {0x21,0x49}, {0x22,0xb0},
	{0x23,0x04}, {0x24,0x16}, {0x34,0x20},

	// BLK reg
	{0x26,0x23}, {0x28,0xff}, {0x29,0x00}, {0x32,0x00},
	{0x33,0x10}, {0x37,0x20}, {0x38,0x10}, {0x47,0x80},
	{0x4e,0x66}, {0xa8,0x02}, {0xa9,0x80},

	// ISP reg
	{0x40,0xff}, {0x41,0x21}, {0x42,0xcf}, {0x44,0x02},
	{0x45,0xa8}, {0x46,0x02}, {0x4a,0x11}, {0x4b,0x01},
	{0x4c,0x20}, {0x4d,0x05}, {0x4f,0x01}, {0x50,0x01},
	{0x55,0x01}, {0x56,0xe0}, {0x57,0x02}, {0x58,0x80},

	// GAIN
	{0x70,0x70}, {0x5a,0x84}, {0x5b,0xc9}, {0x5c,0xed},
	{0x77,0x74}, {0x78,0x40}, {0x79,0x5f},

	// DNDD
	{0x82,0x14}, {0x83,0x0b}, {0x89,0xf0},

	// EEINTP
	{0x8f,0xaa}, {0x90,0x8c}, {0x91,0x90}, {0x92,0x03},
	{0x93,0x03}, {0x94,0x05}, {0x95,0x65}, {0x96,0xf0},

	// ASDE
	{0xfe,0x00}, {0x9a,0x20}, {0x9b,0x80}, {0x9c,0x40},
	{0x9d,0x80}, {0xa1,0x30}, {0xa2,0x32}, {0xa4,0x30},
	{0xa5,0x30}, {0xaa,0x10}, {0xac,0x22},

	// GAMMA
	{0xfe,0x00}, {0xbf,0x08}, {0xc0,0x16}, {0xc1,0x28},
	{0xc2,0x41}, {0xc3,0x5a}, {0xc4,0x6c}, {0xc5,0x7a},
	{0xc6,0x96}, {0xc7,0xac}, {0xc8,0xbc}, {0xc9,0xc9},
	{0xca,0xd3}, {0xcb,0xdd}, {0xcc,0xe5}, {0xcd,0xf1},
	{0xce,0xfa}, {0xcf,0xff},

	// YCP
	{0xd0,0x40}, {0xd1,0x34}, {0xd2,0x34}, {0xd3,0x40},
	{0xd6,0xf2}, {0xd7,0x1b}, {0xd8,0x18}, {0xdd,0x03},

	// AEC
	{0xfe,0x01}, {0x05,0x30}, {0x06,0x75}, {0x07,0x40},
	{0x08,0xb0}, {0x0a,0xc5}, {0x0b,0x11}, {0x0c,0x00},
	{0x12,0x52}, {0x13,0x38}, {0x18,0x95}, {0x19,0x96},
	{0x1f,0x20}, {0x20,0xc0}, {0x3e,0x40}, {0x3f,0x57},
	{0x40,0x7d}, {0x03,0x60}, {0x44,0x02},

	// AWB
	{0xfe,0x01}, {0x1c,0x91}, {0x21,0x15}, {0x50,0x80},
	{0x56,0x04}, {0x59,0x08}, {0x5b,0x02}, {0x61,0x8d},
	{0x62,0xa7}, {0x63,0xd0}, {0x65,0x06}, {0x66,0x06},
	{0x67,0x84}, {0x69,0x08}, {0x6a,0x25}, {0x6b,0x01},
	{0x6c,0x00}, {0x6d,0x02}, {0x6e,0xf0}, {0x6f,0x80},
	{0x76,0x80}, {0x78,0xaf}, {0x79,0x75}, {0x7a,0x40},
	{0x7b,0x50}, {0x7c,0x0c}, {0x90,0xc9}, {0x91,0xbe},
	{0x92,0xe2}, {0x93,0xc9}, {0x95,0x1b}, {0x96,0xe2},
	{0x97,0x49}, {0x98,0x1b}, {0x9a,0x49}, {0x9b,0x1b},
	{0x9c,0xc3}, {0x9d,0x49}, {0x9f,0xc7}, {0xa0,0xc8},
	{0xa1,0x00}, {0xa2,0x00}, {0x86,0x00}, {0x87,0x00},
	{0x88,0x00}, {0x89,0x00}, {0xa4,0xb9}, {0xa5,0xa0},
	{0xa6,0xba}, {0xa7,0x92}, {0xa9,0xba}, {0xaa,0x80},
	{0xab,0x9d}, {0xac,0x7f}, {0xae,0xbb}, {0xaf,0x9d},
	{0xb0,0xc8}, {0xb1,0x97}, {0xb3,0xb7}, {0xb4,0x7f},
	{0xb5,0x00}, {0xb6,0x00}, {0x8b,0x00}, {0x8c,0x00},
	{0x8d,0x00}, {0x8e,0x00}, {0x94,0x55}, {0x99,0xa6},
	{0x9e,0xaa}, {0xa3,0x0a}, {0x8a,0x00}, {0xa8,0x55},
	{0xad,0x55}, {0xb2,0x55}, {0xb7,0x05}, {0x8f,0x00},
	{0xb8,0xcb}, {0xb9,0x9b},

	// CC
	{0xfe,0x01}, {0xd0,0x38}, {0xd1,0x00}, {0xd2,0x02},
	{0xd3,0x04}, {0xd4,0x38}, {0xd5,0x12}, {0xd6,0x30},
	{0xd7,0x00}, {0xd8,0x0a}, {0xd9,0x16}, {0xda,0x39},
	{0xdb,0xf8},

	// LSC
	{0xfe,0x01}, {0xc1,0x3c}, {0xc2,0x50}, {0xc3,0x00},
	{0xc4,0x40}, {0xc5,0x30}, {0xc6,0x30}, {0xc7,0x10},
	{0xc8,0x00}, {0xc9,0x00}, {0xdc,0x20}, {0xdd,0x10},
	{0xdf,0x00}, {0xde,0x00},

	// Histogram
	{0x01,0x10}, {0x0b,0x31}, {0x0e,0x50}, {0x0f,0x0f},
	{0x10,0x6e}, {0x12,0xa0}, {0x15,0x60}, {0x16,0x60},
	{0x17,0xe0},

	// Measure Window
	{0xcc,0x0c}, {0xcd,0x10}, {0xce,0xa0}, {0xcf,0xe6},

	// dark sun
	{0x45,0xf7}, {0x46,0xff}, {0x47,0x15}, {0x48,0x03},
	{0x4f,0x60},

	// banding
	{0xfe,0x00}, {0x05,0x02}, {0x06,0xd1}, {0x07,0x00},
	{0x08,0x22}, {0xfe,0x01}, {0x25,0x00}, {0x26,0x6a},
	{0x27,0x02}, {0x28,0x12}, {0x29,0x03}, {0x2a,0x50},
	{0x2b,0x05}, {0x2c,0xcc}, {0x2d,0x07}, {0x2e,0x74},
	{0x3c,0x20}, {0xfe,0x00},

	// MIPI reg
	{0xfe,0x03}, {0x10,0x94}, {0xfe,0x00},
};

static const struct regval gc0310_start_settings[] = {
	{0xfe, 0x30}, {0xfe, 0x03}, {0x16, 0x09}, {0x10, 0x94},
	{0x01, 0x03}, {0xfe, 0x00},
	//{0xfe, 0x03}, {0x10, 0x94}, {0xfe, 0x00},
};

static const struct regval gc0310_stop_settings[] = {
	{0xfe, 0x03}, {0x16, 0x05}, {0x10, 0x84}, {0x01, 0x00},
	{0xfe, 0x00},
	//{0xfe, 0x03}, {0x10, 0x84}, {0xfe, 0x00},
};


static const struct gc0310_mode supported_modes[] = {
	{
		.width = 640,
		.height = 480,
		.reg_list = gc0310_raw8_640x480_regs,
		.reg_num = ARRAY_SIZE(gc0310_raw8_640x480_regs),
	},
	{
		.width = 640,
		.height = 480,
		.reg_list = gc0310_yuy2_640x480_regs,
		.reg_num = ARRAY_SIZE(gc0310_yuy2_640x480_regs),
	}
};

/* Read registers up to 4 at a time */
static int gc0310_read_reg(struct i2c_client *client, u8 reg, unsigned int len, u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	int ret;

	if ((len > 4) || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);
	return 0;
}

/* Write registers up to 4 at a time */
static int gc0310_write_reg(struct i2c_client *client, u8 reg, u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	buf[0] = reg;

	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 1;
	val_i = 4 - len;

	while (val_i < 4) {
		buf[buf_i++] = val_p[val_i++];
	}

	if (i2c_master_send(client, buf, len + 1) != len + 1)
		return -EIO;

	return 0;
}

static int gc0310_write_array(struct i2c_client *client, const struct regval *regs, int num)
{
	u32 i;
	int ret = 0;
	//int val;

	FUNC_DEBUG();

	for (i = 0; ((ret == 0) && (i < num)); i++) {
		ret = gc0310_write_reg(client, regs[i].addr,
					GC0310_REG_VALUE_08BIT,
					regs[i].val);
		//gc0310_read_reg(client, regs[i].addr, GC0310_REG_VALUE_08BIT, &val);
		//	printk("i=%4d:, reg=%4x, val=%4x, rb_val=%4x\n",
		//	i, regs[i].addr, regs[i].val, val);
	}

	return ret;
}

static int __gc0310_start_stream(struct gc0310 *gc0310)
{
	int ret;

	FUNC_DEBUG();

	ret = gc0310_write_array(gc0310->client, gc0310->cur_mode->reg_list, gc0310->cur_mode->reg_num);
	if (ret) {
		return ret;
	}

	ret = gc0310_write_array(gc0310->client, gc0310_start_settings, ARRAY_SIZE(gc0310_start_settings));
	if (ret) {
		return ret;
	}

	return 0;
}

static int __gc0310_stop_stream(struct gc0310 *gc0310)
{
	int ret;

	FUNC_DEBUG();

	ret = gc0310_write_array(gc0310->client, gc0310_stop_settings, ARRAY_SIZE(gc0310_stop_settings));
	if (ret) {
		return ret;
	}

	return 0;
}

static int gc0310_s_stream(struct v4l2_subdev *sd, int on)
{
	struct gc0310 *gc0310 = to_gc0310(sd);
	int ret = 0;

	FUNC_DEBUG();

	mutex_lock(&gc0310->mutex);
	//on = !!on;
	if (on == gc0310->streaming)
		goto unlock_and_return;

	if (on) {
		ret = __gc0310_start_stream(gc0310);
		if (ret) {
			DBG_ERR("Start streaming failed while write sensor registers!\n");
			goto unlock_and_return;
		}
	} else {
		__gc0310_stop_stream(gc0310);
	}

	gc0310->streaming = on;

unlock_and_return:
	mutex_unlock(&gc0310->mutex);

	return ret;
}

static struct v4l2_subdev_video_ops gc0310_subdev_video_ops = {
	.s_stream       = gc0310_s_stream,
};
static struct v4l2_subdev_ops gc0310_subdev_ops = {
	.video          = &gc0310_subdev_video_ops,
};

static int gc0310_check_sensor_id(struct gc0310 *gc0310, struct i2c_client *client)
{
	u32 val = 0;
	int ret;

	ret = gc0310_read_reg(client, GC0310_REG_CHIP_ID,
			      GC0310_REG_VALUE_16BIT, &val);
	if ((ret != 0) || (val != CHIP_ID)) {
		DBG_ERR("Unexpected sensor (id = 0x%04x, ret = %d)!\n", val, ret);
		return -1;
	}
	DBG_INFO("Check sensor id success (id = 0x%04x).\n", val);

	return 0;
}

static int gc0310_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct gc0310 *gc0310;
	struct v4l2_subdev *sd;
	int ret;

	FUNC_DEBUG();

	gc0310 = devm_kzalloc(dev, sizeof(*gc0310), GFP_KERNEL);
	if (!gc0310) {
		DBG_ERR("Failed to allocate memory for \'gc0310\'!\n");
		return -ENOMEM;
	}

#ifdef CONFIG_GC0310_YUY2
	gc0310->sensor_data.mode = 1;
	gc0310->sensor_data.fourcc = V4L2_PIX_FMT_YUYV;
#else
	gc0310->sensor_data.mode = 0;
	gc0310->sensor_data.fourcc = V4L2_PIX_FMT_SRGGB8;
#endif
	gc0310->client = client;
	gc0310->cur_mode = &supported_modes[gc0310->sensor_data.mode];

	mutex_init(&gc0310->mutex);

	sd = &gc0310->subdev;
	v4l2_i2c_subdev_init(sd, client, &gc0310_subdev_ops);
	DBG_INFO("Initialized V4L2 I2C subdevice.\n");

	ret = gc0310_check_sensor_id(gc0310, client);
	if (ret) {
		return ret;
	}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &gc0310_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#endif

	ret = v4l2_async_register_subdev(sd);
	if (ret) {
		DBG_ERR("V4L2 async register subdevice failed.\n");
		goto err_clean_entity;
	}
	DBG_INFO("Registered V4L2 sub-device successfully.\n");

	v4l2_set_subdev_hostdata(sd, &gc0310->sensor_data);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif

	return ret;
}

static int gc0310_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0310 *gc0310 = to_gc0310(sd);

	FUNC_DEBUG();

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif
	v4l2_ctrl_handler_free(&gc0310->ctrl_handler);
	mutex_destroy(&gc0310->mutex);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gc0310_of_match[] = {
	{ .compatible = "ovti,gc0310" },
	{},
};
MODULE_DEVICE_TABLE(of, gc0310_of_match);
#endif

static const struct i2c_device_id gc0310_match_id[] = {
	{ "gc0310", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, gc0310_match_id);

static struct i2c_driver gc0310_i2c_driver = {
	.probe          = gc0310_probe,
	.remove         = gc0310_remove,
	.id_table       = gc0310_match_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "gc0310",
		.of_match_table = of_match_ptr(gc0310_of_match),
	},
};


static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&gc0310_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&gc0310_i2c_driver);
}

module_init(sensor_mod_init);
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("Sunplus gc0310 sensor driver");
MODULE_LICENSE("GPL v2");
