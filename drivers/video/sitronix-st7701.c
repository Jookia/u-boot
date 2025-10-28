// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019, Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 * U-Boot port copyright 2025 John Watts <contact@jookia.org>
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <dm/devres.h>
#include <dm/device_compat.h>
#include <dm.h>
#include <asm-generic/gpio.h>
#include <mipi_display.h>
#include <mipi_dsi.h>
#include <mipi_dbi.h>
#include <power/regulator.h>
#include <backlight.h>
#include <panel.h>

/* Command2 BKx selection command */
#define ST7701_CMD2BKX_SEL			0xFF
#define ST7701_CMD1				0
#define ST7701_CMD2				BIT(4)
#define ST7701_CMD2BK_MASK			GENMASK(3, 0)

/* Command2, BK0 commands */
#define ST7701_CMD2_BK0_PVGAMCTRL		0xB0 /* Positive Voltage Gamma Control */
#define ST7701_CMD2_BK0_NVGAMCTRL		0xB1 /* Negative Voltage Gamma Control */
#define ST7701_CMD2_BK0_LNESET			0xC0 /* Display Line setting */
#define ST7701_CMD2_BK0_PORCTRL			0xC1 /* Porch control */
#define ST7701_CMD2_BK0_INVSEL			0xC2 /* Inversion selection, Frame Rate Control */

/* Command2, BK1 commands */
#define ST7701_CMD2_BK1_VRHS			0xB0 /* Vop amplitude setting */
#define ST7701_CMD2_BK1_VCOM			0xB1 /* VCOM amplitude setting */
#define ST7701_CMD2_BK1_VGHSS			0xB2 /* VGH Voltage setting */
#define ST7701_CMD2_BK1_TESTCMD			0xB3 /* TEST Command Setting */
#define ST7701_CMD2_BK1_VGLS			0xB5 /* VGL Voltage setting */
#define ST7701_CMD2_BK1_PWCTLR1			0xB7 /* Power Control 1 */
#define ST7701_CMD2_BK1_PWCTLR2			0xB8 /* Power Control 2 */
#define ST7701_CMD2_BK1_SPD1			0xC1 /* Source pre_drive timing set1 */
#define ST7701_CMD2_BK1_SPD2			0xC2 /* Source EQ2 Setting */
#define ST7701_CMD2_BK1_MIPISET1		0xD0 /* MIPI Setting 1 */

/* Command2, BK0 bytes */
#define ST7701_CMD2_BK0_GAMCTRL_AJ_MASK		GENMASK(7, 6)
#define ST7701_CMD2_BK0_GAMCTRL_VC0_MASK	GENMASK(3, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC4_MASK	GENMASK(5, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC8_MASK	GENMASK(5, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC16_MASK	GENMASK(4, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC24_MASK	GENMASK(4, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC52_MASK	GENMASK(3, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC80_MASK	GENMASK(5, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC108_MASK	GENMASK(3, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC147_MASK	GENMASK(3, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC175_MASK	GENMASK(5, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC203_MASK	GENMASK(3, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC231_MASK	GENMASK(4, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC239_MASK	GENMASK(4, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC247_MASK	GENMASK(5, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC251_MASK	GENMASK(5, 0)
#define ST7701_CMD2_BK0_GAMCTRL_VC255_MASK	GENMASK(4, 0)
#define ST7701_CMD2_BK0_LNESET_LINE_MASK	GENMASK(6, 0)
#define ST7701_CMD2_BK0_LNESET_LDE_EN		BIT(7)
#define ST7701_CMD2_BK0_LNESET_LINEDELTA	GENMASK(1, 0)
#define ST7701_CMD2_BK0_PORCTRL_VBP_MASK	GENMASK(7, 0)
#define ST7701_CMD2_BK0_PORCTRL_VFP_MASK	GENMASK(7, 0)
#define ST7701_CMD2_BK0_INVSEL_ONES_MASK	GENMASK(5, 4)
#define ST7701_CMD2_BK0_INVSEL_NLINV_MASK	GENMASK(2, 0)
#define ST7701_CMD2_BK0_INVSEL_RTNI_MASK	GENMASK(4, 0)

/* Command2, BK1 bytes */
#define ST7701_CMD2_BK1_VRHA_MASK		GENMASK(7, 0)
#define ST7701_CMD2_BK1_VCOM_MASK		GENMASK(7, 0)
#define ST7701_CMD2_BK1_VGHSS_MASK		GENMASK(3, 0)
#define ST7701_CMD2_BK1_TESTCMD_VAL		BIT(7)
#define ST7701_CMD2_BK1_VGLS_ONES		BIT(6)
#define ST7701_CMD2_BK1_VGLS_MASK		GENMASK(3, 0)
#define ST7701_CMD2_BK1_PWRCTRL1_AP_MASK	GENMASK(7, 6)
#define ST7701_CMD2_BK1_PWRCTRL1_APIS_MASK	GENMASK(3, 2)
#define ST7701_CMD2_BK1_PWRCTRL1_APOS_MASK	GENMASK(1, 0)
#define ST7701_CMD2_BK1_PWRCTRL2_AVDD_MASK	GENMASK(5, 4)
#define ST7701_CMD2_BK1_PWRCTRL2_AVCL_MASK	GENMASK(1, 0)
#define ST7701_CMD2_BK1_SPD1_ONES_MASK		GENMASK(6, 4)
#define ST7701_CMD2_BK1_SPD1_T2D_MASK		GENMASK(3, 0)
#define ST7701_CMD2_BK1_SPD2_ONES_MASK		GENMASK(6, 4)
#define ST7701_CMD2_BK1_SPD2_T3D_MASK		GENMASK(3, 0)
#define ST7701_CMD2_BK1_MIPISET1_ONES		BIT(7)
#define ST7701_CMD2_BK1_MIPISET1_EOT_EN		BIT(3)

#define CFIELD_PREP(_mask, _val)					\
	(((typeof(_mask))(_val) << (__builtin_ffsll(_mask) - 1)) & (_mask))

enum op_bias {
	OP_BIAS_OFF = 0,
	OP_BIAS_MIN,
	OP_BIAS_MIDDLE,
	OP_BIAS_MAX
};

struct st7701;

struct st7701_panel_desc {
	const struct display_timing *timing;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned int panel_sleep_delay;

	/* TFT matrix driver configuration, panel specific. */
	const u8	pv_gamma[16];	/* Positive voltage gamma control */
	const u8	nv_gamma[16];	/* Negative voltage gamma control */
	const u8	nlinv;		/* Inversion selection */
	const u32	vop_uv;		/* Vop in uV */
	const u32	vcom_uv;	/* Vcom in uV */
	const u16	vgh_mv;		/* Vgh in mV */
	const s16	vgl_mv;		/* Vgl in mV */
	const u16	avdd_mv;	/* Avdd in mV */
	const s16	avcl_mv;	/* Avcl in mV */
	const enum op_bias	gamma_op_bias;
	const enum op_bias	input_op_bias;
	const enum op_bias	output_op_bias;
	const u16	t2d_ns;		/* T2D in ns */
	const u16	t3d_ns;		/* T3D in ns */
	const bool	eot_en;

	/* GIP sequence, fully custom and undocumented. */
	void		(*gip_sequence)(struct st7701 *st7701);
};

struct st7701 {
	struct mipi_dsi_device *dsi;
	struct spi_slave *spi;
	struct mipi_dbi dbi;
	const struct st7701_panel_desc *desc;

	struct udevice *vdd;
	struct udevice *vddio;
	struct gpio_desc *reset;
	struct gpio_desc *dc;
	unsigned int sleep_delay;
	struct udevice *backlight;

	int (*write_command)(struct st7701 *st7701, u8 cmd, const u8 *seq,
			     size_t len);
};

static int st7701_dsi_write(struct st7701 *st7701, u8 cmd, const u8 *seq,
			    size_t len)
{
	return mipi_dsi_dcs_write(st7701->dsi, cmd, seq, len);
}

static int st7701_dbi_write(struct st7701 *st7701, u8 cmd, const u8 *seq,
			    size_t len)
{
	return mipi_dbi_command_buf(&st7701->dbi, cmd, seq, len);
}

#define ST7701_WRITE(st7701, cmd, seq...)				\
	{								\
		const u8 d[] = { seq };					\
		st7701->write_command(st7701, cmd, d, ARRAY_SIZE(d));	\
	}

static u8 st7701_vgls_map(struct st7701 *st7701)
{
	const struct st7701_panel_desc *desc = st7701->desc;
	struct {
		s32	vgl;
		u8	val;
	} map[16] = {
		{ -7060, 0x0 }, { -7470, 0x1 },
		{ -7910, 0x2 }, { -8140, 0x3 },
		{ -8650, 0x4 }, { -8920, 0x5 },
		{ -9210, 0x6 }, { -9510, 0x7 },
		{ -9830, 0x8 }, { -10170, 0x9 },
		{ -10530, 0xa }, { -10910, 0xb },
		{ -11310, 0xc }, { -11730, 0xd },
		{ -12200, 0xe }, { -12690, 0xf }
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(map); i++)
		if (desc->vgl_mv == map[i].vgl)
			return map[i].val;

	return 0;
}

static void st7701_switch_cmd_bkx(struct st7701 *st7701, bool cmd2, u8 bkx)
{
	u8 val;

	if (cmd2)
		val = ST7701_CMD2 | FIELD_PREP(ST7701_CMD2BK_MASK, bkx);
	else
		val = ST7701_CMD1;

	ST7701_WRITE(st7701, ST7701_CMD2BKX_SEL, 0x77, 0x01, 0x00, 0x00, val);
}

static void st7701_init_sequence(struct st7701 *st7701)
{
	const struct st7701_panel_desc *desc = st7701->desc;
	const struct display_timing *timing = desc->timing;
	const u8 linecount8 = timing->vactive.typ / 8;
	const u8 linecountrem2 = (timing->vactive.typ % 8) / 2;
	const u32 htotal = timing->hactive.typ + timing->hfront_porch.typ + \
			   timing->hback_porch.typ + timing->hsync_len.typ;

	ST7701_WRITE(st7701, MIPI_DCS_SOFT_RESET, 0x00);

	/* We need to wait 5ms before sending new commands */
	mdelay(5);

	ST7701_WRITE(st7701, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);

	mdelay(st7701->sleep_delay);

	/* Command2, BK0 */
	st7701_switch_cmd_bkx(st7701, true, 0);

	st7701->write_command(st7701, ST7701_CMD2_BK0_PVGAMCTRL, desc->pv_gamma,
			      ARRAY_SIZE(desc->pv_gamma));
	st7701->write_command(st7701, ST7701_CMD2_BK0_NVGAMCTRL, desc->nv_gamma,
			      ARRAY_SIZE(desc->nv_gamma));
	/*
	 * Vertical line count configuration:
	 * Line[6:0]: select number of vertical lines of the TFT matrix in
	 *            multiples of 8 lines
	 * LDE_EN: enable sub-8-line granularity line count
	 * Line_delta[1:0]: add 0/2/4/6 extra lines to line count selected
	 *                  using Line[6:0]
	 *
	 * Total number of vertical lines:
	 * LN = ((Line[6:0] + 1) * 8) + (LDE_EN ? Line_delta[1:0] * 2 : 0)
	 */
	ST7701_WRITE(st7701, ST7701_CMD2_BK0_LNESET,
		   FIELD_PREP(ST7701_CMD2_BK0_LNESET_LINE_MASK, linecount8 - 1) |
		   (linecountrem2 ? ST7701_CMD2_BK0_LNESET_LDE_EN : 0),
		   FIELD_PREP(ST7701_CMD2_BK0_LNESET_LINEDELTA, linecountrem2));
	ST7701_WRITE(st7701, ST7701_CMD2_BK0_PORCTRL,
		   FIELD_PREP(ST7701_CMD2_BK0_PORCTRL_VBP_MASK,
			      timing->vback_porch.typ),
		   FIELD_PREP(ST7701_CMD2_BK0_PORCTRL_VFP_MASK,
			      timing->vfront_porch.typ));
	/*
	 * Horizontal pixel count configuration:
	 * PCLK = 512 + (RTNI[4:0] * 16)
	 * The PCLK is number of pixel clock per line, which matches
	 * mode htotal. The minimum is 512 PCLK.
	 */
	ST7701_WRITE(st7701, ST7701_CMD2_BK0_INVSEL,
		   ST7701_CMD2_BK0_INVSEL_ONES_MASK |
		   FIELD_PREP(ST7701_CMD2_BK0_INVSEL_NLINV_MASK, desc->nlinv),
		   FIELD_PREP(ST7701_CMD2_BK0_INVSEL_RTNI_MASK,
			      (clamp(htotal, 512U, 1008U) - 512) / 16));

	/* Command2, BK1 */
	st7701_switch_cmd_bkx(st7701, true, 1);

	/* Vop = 3.5375V + (VRHA[7:0] * 0.0125V) */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_VRHS,
		   FIELD_PREP(ST7701_CMD2_BK1_VRHA_MASK,
			      DIV_ROUND_CLOSEST(desc->vop_uv - 3537500, 12500)));

	/* Vcom = 0.1V + (VCOM[7:0] * 0.0125V) */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_VCOM,
		   FIELD_PREP(ST7701_CMD2_BK1_VCOM_MASK,
			      DIV_ROUND_CLOSEST(desc->vcom_uv - 100000, 12500)));

	/* Vgh = 11.5V + (VGHSS[7:0] * 0.5V) */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_VGHSS,
		   FIELD_PREP(ST7701_CMD2_BK1_VGHSS_MASK,
			      DIV_ROUND_CLOSEST(clamp(desc->vgh_mv,
						      (u16)11500,
						      (u16)17000) - 11500,
						500)));

	ST7701_WRITE(st7701, ST7701_CMD2_BK1_TESTCMD, ST7701_CMD2_BK1_TESTCMD_VAL);

	/* Vgl is non-linear */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_VGLS,
		   ST7701_CMD2_BK1_VGLS_ONES |
		   FIELD_PREP(ST7701_CMD2_BK1_VGLS_MASK, st7701_vgls_map(st7701)));

	ST7701_WRITE(st7701, ST7701_CMD2_BK1_PWCTLR1,
		   FIELD_PREP(ST7701_CMD2_BK1_PWRCTRL1_AP_MASK,
			      desc->gamma_op_bias) |
		   FIELD_PREP(ST7701_CMD2_BK1_PWRCTRL1_APIS_MASK,
			      desc->input_op_bias) |
		   FIELD_PREP(ST7701_CMD2_BK1_PWRCTRL1_APOS_MASK,
			      desc->output_op_bias));

	/* Avdd = 6.2V + (AVDD[1:0] * 0.2V) , Avcl = -4.4V - (AVCL[1:0] * 0.2V) */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_PWCTLR2,
		   FIELD_PREP(ST7701_CMD2_BK1_PWRCTRL2_AVDD_MASK,
			      DIV_ROUND_CLOSEST(desc->avdd_mv - 6200, 200)) |
		   FIELD_PREP(ST7701_CMD2_BK1_PWRCTRL2_AVCL_MASK,
			      DIV_ROUND_CLOSEST(-4400 - desc->avcl_mv, 200)));

	/* T2D = 0.2us * T2D[3:0] */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_SPD1,
		   ST7701_CMD2_BK1_SPD1_ONES_MASK |
		   FIELD_PREP(ST7701_CMD2_BK1_SPD1_T2D_MASK,
			      DIV_ROUND_CLOSEST(desc->t2d_ns, 200)));

	/* T3D = 4us + (0.8us * T3D[3:0]) */
	ST7701_WRITE(st7701, ST7701_CMD2_BK1_SPD2,
		   ST7701_CMD2_BK1_SPD2_ONES_MASK |
		   FIELD_PREP(ST7701_CMD2_BK1_SPD2_T3D_MASK,
			      DIV_ROUND_CLOSEST(desc->t3d_ns - 4000, 800)));

	ST7701_WRITE(st7701, ST7701_CMD2_BK1_MIPISET1,
		   ST7701_CMD2_BK1_MIPISET1_ONES |
		   (desc->eot_en ? ST7701_CMD2_BK1_MIPISET1_EOT_EN : 0));
}

static void ts8550b_gip_sequence(struct st7701 *st7701)
{
	/**
	 * ST7701_SPEC_V1.2 is unable to provide enough information above this
	 * specific command sequence, so grab the same from vendor BSP driver.
	 */
	ST7701_WRITE(st7701, 0xE0, 0x00, 0x00, 0x02);
	ST7701_WRITE(st7701, 0xE1, 0x0B, 0x00, 0x0D, 0x00, 0x0C, 0x00, 0x0E,
		   0x00, 0x00, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE2, 0x33, 0x33, 0x44, 0x44, 0x64, 0x00, 0x66,
		   0x00, 0x65, 0x00, 0x67, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE3, 0x00, 0x00, 0x33, 0x33);
	ST7701_WRITE(st7701, 0xE4, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE5, 0x0C, 0x78, 0x3C, 0xA0, 0x0E, 0x78, 0x3C,
		   0xA0, 0x10, 0x78, 0x3C, 0xA0, 0x12, 0x78, 0x3C, 0xA0);
	ST7701_WRITE(st7701, 0xE6, 0x00, 0x00, 0x33, 0x33);
	ST7701_WRITE(st7701, 0xE7, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE8, 0x0D, 0x78, 0x3C, 0xA0, 0x0F, 0x78, 0x3C,
		   0xA0, 0x11, 0x78, 0x3C, 0xA0, 0x13, 0x78, 0x3C, 0xA0);
	ST7701_WRITE(st7701, 0xEB, 0x02, 0x02, 0x39, 0x39, 0xEE, 0x44, 0x00);
	ST7701_WRITE(st7701, 0xEC, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xED, 0xFF, 0xF1, 0x04, 0x56, 0x72, 0x3F, 0xFF,
		   0xFF, 0xFF, 0xFF, 0xF3, 0x27, 0x65, 0x40, 0x1F, 0xFF);
}

static void dmt028vghmcmi_1a_gip_sequence(struct st7701 *st7701)
{
	ST7701_WRITE(st7701, 0xEE, 0x42);
	ST7701_WRITE(st7701, 0xE0, 0x00, 0x00, 0x02);

	ST7701_WRITE(st7701, 0xE1,
		   0x04, 0xA0, 0x06, 0xA0,
			   0x05, 0xA0, 0x07, 0xA0,
			   0x00, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE2,
		   0x00, 0x00, 0x00, 0x00,
			   0x00, 0x00, 0x00, 0x00,
			   0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE3,
		   0x00, 0x00, 0x22, 0x22);
	ST7701_WRITE(st7701, 0xE4, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE5,
		   0x0C, 0x90, 0xA0, 0xA0,
			   0x0E, 0x92, 0xA0, 0xA0,
			   0x08, 0x8C, 0xA0, 0xA0,
			   0x0A, 0x8E, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xE6,
		   0x00, 0x00, 0x22, 0x22);
	ST7701_WRITE(st7701, 0xE7, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE8,
		   0x0D, 0x91, 0xA0, 0xA0,
			   0x0F, 0x93, 0xA0, 0xA0,
			   0x09, 0x8D, 0xA0, 0xA0,
			   0x0B, 0x8F, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xEB,
		   0x00, 0x00, 0xE4, 0xE4,
			   0x44, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xED,
		   0xFF, 0xF5, 0x47, 0x6F,
			   0x0B, 0xA1, 0xAB, 0xFF,
			   0xFF, 0xBA, 0x1A, 0xB0,
			   0xF6, 0x74, 0x5F, 0xFF);
	ST7701_WRITE(st7701, 0xEF,
		   0x08, 0x08, 0x08, 0x40,
			   0x3F, 0x64);

	st7701_switch_cmd_bkx(st7701, false, 0);

	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xE6, 0x7C);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x0E);

	st7701_switch_cmd_bkx(st7701, false, 0);
	ST7701_WRITE(st7701, 0x11);
	mdelay(120);

	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x0C);
	mdelay(10);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x00);

	st7701_switch_cmd_bkx(st7701, false, 0);
	ST7701_WRITE(st7701, 0x11);
	mdelay(120);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x00);

	st7701_switch_cmd_bkx(st7701, false, 0);

	ST7701_WRITE(st7701, 0x3A, 0x70);
}

static void kd50t048a_gip_sequence(struct st7701 *st7701)
{
	/**
	 * ST7701_SPEC_V1.2 is unable to provide enough information above this
	 * specific command sequence, so grab the same from vendor BSP driver.
	 */
	ST7701_WRITE(st7701, 0xE0, 0x00, 0x00, 0x02);
	ST7701_WRITE(st7701, 0xE1, 0x08, 0x00, 0x0A, 0x00, 0x07, 0x00, 0x09,
		   0x00, 0x00, 0x33, 0x33);
	ST7701_WRITE(st7701, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE3, 0x00, 0x00, 0x33, 0x33);
	ST7701_WRITE(st7701, 0xE4, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE5, 0x0E, 0x60, 0xA0, 0xA0, 0x10, 0x60, 0xA0,
		   0xA0, 0x0A, 0x60, 0xA0, 0xA0, 0x0C, 0x60, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xE6, 0x00, 0x00, 0x33, 0x33);
	ST7701_WRITE(st7701, 0xE7, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE8, 0x0D, 0x60, 0xA0, 0xA0, 0x0F, 0x60, 0xA0,
		   0xA0, 0x09, 0x60, 0xA0, 0xA0, 0x0B, 0x60, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xEB, 0x02, 0x01, 0xE4, 0xE4, 0x44, 0x00, 0x40);
	ST7701_WRITE(st7701, 0xEC, 0x02, 0x01);
	ST7701_WRITE(st7701, 0xED, 0xAB, 0x89, 0x76, 0x54, 0x01, 0xFF, 0xFF,
		   0xFF, 0xFF, 0xFF, 0xFF, 0x10, 0x45, 0x67, 0x98, 0xBA);
}

static void rg_arc_gip_sequence(struct st7701 *st7701)
{
	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xEF, 0x08);
	st7701_switch_cmd_bkx(st7701, true, 0);
	ST7701_WRITE(st7701, 0xC7, 0x04);
	ST7701_WRITE(st7701, 0xCC, 0x38);
	st7701_switch_cmd_bkx(st7701, true, 1);
	ST7701_WRITE(st7701, 0xB9, 0x10);
	ST7701_WRITE(st7701, 0xBC, 0x03);
	ST7701_WRITE(st7701, 0xC0, 0x89);
	ST7701_WRITE(st7701, 0xE0, 0x00, 0x00, 0x02);
	ST7701_WRITE(st7701, 0xE1, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
		   0x00, 0x00, 0x20, 0x20);
	ST7701_WRITE(st7701, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE3, 0x00, 0x00, 0x33, 0x00);
	ST7701_WRITE(st7701, 0xE4, 0x22, 0x00);
	ST7701_WRITE(st7701, 0xE5, 0x04, 0x5C, 0xA0, 0xA0, 0x06, 0x5C, 0xA0,
		   0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE6, 0x00, 0x00, 0x33, 0x00);
	ST7701_WRITE(st7701, 0xE7, 0x22, 0x00);
	ST7701_WRITE(st7701, 0xE8, 0x05, 0x5C, 0xA0, 0xA0, 0x07, 0x5C, 0xA0,
		   0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xEB, 0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xEC, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xED, 0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF,
		   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF);
	ST7701_WRITE(st7701, 0xEF, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54);
	st7701_switch_cmd_bkx(st7701, false, 0);
	ST7701_WRITE(st7701, MIPI_DCS_SET_ADDRESS_MODE, 0x17);
	ST7701_WRITE(st7701, MIPI_DCS_SET_PIXEL_FORMAT, 0x77);
	ST7701_WRITE(st7701, MIPI_DCS_EXIT_SLEEP_MODE, 0x00);
	mdelay(120);
}

static void rg28xx_gip_sequence(struct st7701 *st7701)
{
	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xEF, 0x08);

	st7701_switch_cmd_bkx(st7701, true, 0);
	ST7701_WRITE(st7701, 0xC3, 0x02, 0x10, 0x02);
	ST7701_WRITE(st7701, 0xC7, 0x04);
	ST7701_WRITE(st7701, 0xCC, 0x10);

	st7701_switch_cmd_bkx(st7701, true, 1);
	ST7701_WRITE(st7701, 0xEE, 0x42);
	ST7701_WRITE(st7701, 0xE0, 0x00, 0x00, 0x02);

	ST7701_WRITE(st7701, 0xE1, 0x04, 0xA0, 0x06, 0xA0, 0x05, 0xA0, 0x07, 0xA0,
		   0x00, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE3, 0x00, 0x00, 0x22, 0x22);
	ST7701_WRITE(st7701, 0xE4, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE5, 0x0C, 0x90, 0xA0, 0xA0, 0x0E, 0x92, 0xA0, 0xA0,
		   0x08, 0x8C, 0xA0, 0xA0, 0x0A, 0x8E, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xE6, 0x00, 0x00, 0x22, 0x22);
	ST7701_WRITE(st7701, 0xE7, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE8, 0x0D, 0x91, 0xA0, 0xA0, 0x0F, 0x93, 0xA0, 0xA0,
		   0x09, 0x8D, 0xA0, 0xA0, 0x0B, 0x8F, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xEB, 0x00, 0x00, 0xE4, 0xE4, 0x44, 0x00, 0x40);
	ST7701_WRITE(st7701, 0xED, 0xFF, 0xF5, 0x47, 0x6F, 0x0B, 0xA1, 0xBA, 0xFF,
		   0xFF, 0xAB, 0x1A, 0xB0, 0xF6, 0x74, 0x5F, 0xFF);
	ST7701_WRITE(st7701, 0xEF, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54);

	st7701_switch_cmd_bkx(st7701, false, 0);

	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xE6, 0x16);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x0E);

	st7701_switch_cmd_bkx(st7701, false, 0);
	ST7701_WRITE(st7701, MIPI_DCS_SET_ADDRESS_MODE, 0x10);
	ST7701_WRITE(st7701, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x0C);
	mdelay(10);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x00);
	st7701_switch_cmd_bkx(st7701, false, 0);
}

static void fs028vg047_gip_sequence(struct st7701 *st7701)
{
	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xEF, 0x08);

	st7701_switch_cmd_bkx(st7701, true, 0);
	ST7701_WRITE(st7701, 0xCC, 0x10);

	st7701_switch_cmd_bkx(st7701, true, 1);
	ST7701_WRITE(st7701, 0xEE, 0x42);
	ST7701_WRITE(st7701, 0xE0, 0x00, 0x00, 0x02);

	ST7701_WRITE(st7701, 0xE1, 0x04, 0xA0, 0x06, 0xA0, 0x05, 0xA0, 0x07, 0xA0,
		   0x00, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		   0x00, 0x00, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xE3, 0x00, 0x00, 0x22, 0x22);
	ST7701_WRITE(st7701, 0xE4, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE5, 0x0C, 0x90, 0xA0, 0xA0, 0x0E, 0x92, 0xA0, 0xA0,
		   0x08, 0x8C, 0xA0, 0xA0, 0x0A, 0x8E, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xE6, 0x00, 0x00, 0x22, 0x22);
	ST7701_WRITE(st7701, 0xE7, 0x44, 0x44);
	ST7701_WRITE(st7701, 0xE8, 0x0D, 0x91, 0xA0, 0xA0, 0x0F, 0x93, 0xA0, 0xA0,
		   0x09, 0x8D, 0xA0, 0xA0, 0x0B, 0x8F, 0xA0, 0xA0);
	ST7701_WRITE(st7701, 0xEB, 0x00, 0x00, 0xE4, 0xE4, 0x44, 0x00, 0x00);
	ST7701_WRITE(st7701, 0xED, 0xFF, 0xF5, 0x47, 0x6F, 0x0B, 0xA1, 0xAB, 0xFF,
		   0xFF, 0xBA, 0x1A, 0xB0, 0xF6, 0x74, 0x5F, 0xFF);
	ST7701_WRITE(st7701, 0xEF, 0x08, 0x08, 0x08, 0x40, 0x3F, 0x64);

	st7701_switch_cmd_bkx(st7701, false, 0);

	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xE6, 0x7C);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x0E);

	st7701_switch_cmd_bkx(st7701, false, 0);
	ST7701_WRITE(st7701, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);

	st7701_switch_cmd_bkx(st7701, true, 3);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x0C);
	mdelay(10);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x00);

	st7701_switch_cmd_bkx(st7701, false, 0);
	ST7701_WRITE(st7701, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(120);
	ST7701_WRITE(st7701, 0xE8, 0x00, 0x00);

	st7701_switch_cmd_bkx(st7701, false, 0);

	ST7701_WRITE(st7701, MIPI_DCS_SET_PIXEL_FORMAT, 0x70);
}

static int st7701_prepare(struct st7701 *st7701)
{
	int ret;

	dm_gpio_set_value(st7701->reset, false);

	if (CONFIG_IS_ENABLED(DM_REGULATOR)) {
		ret = regulator_set_enable(st7701->vdd, true);
		if (ret < 0)
			return ret;
		ret = regulator_set_enable(st7701->vddio, true);
		if (ret < 0)
			return ret;
	}
	mdelay(20);

	dm_gpio_set_value(st7701->reset, true);
	mdelay(150);

	st7701_init_sequence(st7701);

	if (st7701->desc->gip_sequence)
		st7701->desc->gip_sequence(st7701);

	/* Disable Command2 */
	st7701_switch_cmd_bkx(st7701, false, 0);

	return 0;
}

static const struct display_timing ts8550b_timing = {
	.pixelclock.typ		= 27500000,

	.hactive.typ		= 480,
	.hfront_porch.typ	= 38,
	.hback_porch.typ	= 12,
	.hsync_len.typ		= 12,

	.vactive.typ		= 854,
	.vfront_porch.typ	= 18,
	.vback_porch.typ	= 4,
	.vsync_len.typ		= 8,
};

static const struct st7701_panel_desc ts8550b_desc = {
	.timing = &ts8550b_timing,
	.lanes = 2,
	.format = MIPI_DSI_FMT_RGB888,
	.panel_sleep_delay = 80, /* panel need extra 80ms for sleep out cmd */

	.pv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xe),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x15),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xf),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x23),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x13),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x12),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x2b),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x34),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xe),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0x2) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x15),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xf),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x13),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x7),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x9),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x22),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x10),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0xe),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x2c),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x34),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nlinv = 7,
	.vop_uv = 4400000,
	.vcom_uv = 337500,
	.vgh_mv = 15000,
	.vgl_mv = -9510,
	.avdd_mv = 6600,
	.avcl_mv = -4400,
	.gamma_op_bias = OP_BIAS_MAX,
	.input_op_bias = OP_BIAS_MIN,
	.output_op_bias = OP_BIAS_MIN,
	.t2d_ns = 1600,
	.t3d_ns = 10400,
	.eot_en = true,
	.gip_sequence = ts8550b_gip_sequence,
};

static const struct display_timing dmt028vghmcmi_1a_timing = {
	.pixelclock.typ		= 22325000,

	.hactive.typ		= 480,
	.hfront_porch.typ	= 40,
	.hback_porch.typ	= 20,
	.hsync_len.typ		= 4,

	.vactive.typ		= 640,
	.vfront_porch.typ	= 2,
	.vback_porch.typ	= 16,
	.vsync_len.typ		= 40,

	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW,
};

static const struct st7701_panel_desc dmt028vghmcmi_1a_desc = {
	.timing = &dmt028vghmcmi_1a_timing,
	.lanes = 2,
	.format = MIPI_DSI_FMT_RGB888,
	.panel_sleep_delay = 5, /* panel need extra 5ms for sleep out cmd */

	.pv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0x10),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x17),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xd),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x6),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x7),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x1f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x11),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0xe),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x29),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x30),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xd),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x14),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xe),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x6),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x20),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x13),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x13),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x26),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x30),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nlinv = 1,
	.vop_uv = 4800000,
	.vcom_uv = 1650000,
	.vgh_mv = 15000,
	.vgl_mv = -10170,
	.avdd_mv = 6600,
	.avcl_mv = -4400,
	.gamma_op_bias = OP_BIAS_MIDDLE,
	.input_op_bias = OP_BIAS_MIN,
	.output_op_bias = OP_BIAS_MIN,
	.t2d_ns = 1600,
	.t3d_ns = 10400,
	.eot_en = true,
	.gip_sequence = dmt028vghmcmi_1a_gip_sequence,
};

static const struct display_timing kd50t048a_timing = {
	.pixelclock.typ		= 27500000,

	.hactive.typ		= 480,
	.hfront_porch.typ	= 2,
	.hback_porch.typ	= 2,
	.hsync_len.typ		= 10,

	.vactive.typ		= 854,
	.vfront_porch.typ	= 2,
	.vback_porch.typ	= 17,
	.vsync_len.typ		= 2,
};

static const struct st7701_panel_desc kd50t048a_desc = {
	.timing = &kd50t048a_timing,
	.lanes = 2,
	.format = MIPI_DSI_FMT_RGB888,
	.panel_sleep_delay = 0,

	.pv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xd),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x14),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xd),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x10),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x2),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x1e),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x13),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 2) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x23),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x29),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x18)
	},
	.nv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xc),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x14),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xc),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x10),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x3),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x7),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x20),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x13),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 2) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x24),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x29),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x18)
	},
	.nlinv = 1,
	.vop_uv = 4887500,
	.vcom_uv = 937500,
	.vgh_mv = 15000,
	.vgl_mv = -9510,
	.avdd_mv = 6600,
	.avcl_mv = -4400,
	.gamma_op_bias = OP_BIAS_MIDDLE,
	.input_op_bias = OP_BIAS_MIN,
	.output_op_bias = OP_BIAS_MIN,
	.t2d_ns = 1600,
	.t3d_ns = 10400,
	.eot_en = true,
	.gip_sequence = kd50t048a_gip_sequence,
};

static const struct display_timing rg_arc_timing = {
	.pixelclock.typ		= 25600000,

	.hactive.typ		= 480,
	.hfront_porch.typ	= 60,
	.hback_porch.typ	= 60,
	.hsync_len.typ		= 42,

	.vactive.typ		= 640,
	.vfront_porch.typ	= 10,
	.vback_porch.typ	= 16,
	.vsync_len.typ		= 4,
};

static const struct st7701_panel_desc rg_arc_desc = {
	.timing = &rg_arc_timing,
	.lanes = 2,
	.format = MIPI_DSI_FMT_RGB888,
	.panel_sleep_delay = 80,

	.pv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0x01) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0x16),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x1d),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0x0e),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x12),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x06),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x0c),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x0a),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x09),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x25),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x00),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x03),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x00),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x3f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x3f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1c)
	},
	.nv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0x01) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0x16),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x1e),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0x0e),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x06),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x0c),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x08),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x09),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x26),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x00),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x15),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x00),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x3f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x3f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1c)
	},
	.nlinv = 0,
	.vop_uv = 4500000,
	.vcom_uv = 762500,
	.vgh_mv = 15000,
	.vgl_mv = -9510,
	.avdd_mv = 6600,
	.avcl_mv = -4400,
	.gamma_op_bias = OP_BIAS_MIDDLE,
	.input_op_bias = OP_BIAS_MIN,
	.output_op_bias = OP_BIAS_MIN,
	.t2d_ns = 1600,
	.t3d_ns = 10400,
	.eot_en = true,
	.gip_sequence = rg_arc_gip_sequence,
};

static const struct display_timing rg28xx_timing = {
	.pixelclock.typ		= 22325000,

	.hactive.typ		= 480,
	.hfront_porch.typ	= 40,
	.hback_porch.typ	= 20,
	.hsync_len.typ		= 4,

	.vactive.typ		= 640,
	.vfront_porch.typ	= 2,
	.vback_porch.typ	= 16,
	.vsync_len.typ		= 40,

	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW,
};

static const struct st7701_panel_desc rg28xx_desc = {
	.timing = &rg28xx_timing,

	.panel_sleep_delay = 80,

	.pv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0x10),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x17),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xd),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x6),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x7),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x1f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x11),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0xe),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x29),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x30),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xd),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x14),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xe),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x6),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x20),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x13),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x13),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x26),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x30),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nlinv = 7,
	.vop_uv = 4800000,
	.vcom_uv = 1512500,
	.vgh_mv = 15000,
	.vgl_mv = -11730,
	.avdd_mv = 6600,
	.avcl_mv = -4400,
	.gamma_op_bias = OP_BIAS_MIDDLE,
	.input_op_bias = OP_BIAS_MIN,
	.output_op_bias = OP_BIAS_MIN,
	.t2d_ns = 1600,
	.t3d_ns = 10400,
	.eot_en = true,
	.gip_sequence = rg28xx_gip_sequence,
};

static const struct display_timing fs028vg047_timing = {
	.pixelclock.typ		= 22325000,

	.hactive.typ		= 480,
	.hfront_porch.typ	= 40,
	.hback_porch.typ	= 20,
	.hsync_len.typ		= 4,

	.vactive.typ		= 640,
	.vfront_porch.typ	= 2,
	.vback_porch.typ	= 16,
	.vsync_len.typ		= 40,

	.flags = DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW,
};

static const struct st7701_panel_desc fs028vg047_desc = {
	.timing = &fs028vg047_timing,

	.panel_sleep_delay = 0,

	.pv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0x10),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x17),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xd),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x6),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x7),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x1f),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x11),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0xe),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x29),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x30),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nv_gamma = {
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC0_MASK, 0),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC4_MASK, 0xd),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC8_MASK, 0x14),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC16_MASK, 0xe),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC24_MASK, 0x11),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC52_MASK, 0x6),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC80_MASK, 0x4),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC108_MASK, 0x8),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC147_MASK, 0x8),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC175_MASK, 0x20),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC203_MASK, 0x5),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC231_MASK, 0x13),

		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC239_MASK, 0x13),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC247_MASK, 0x26),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC251_MASK, 0x30),
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_AJ_MASK, 0) |
		CFIELD_PREP(ST7701_CMD2_BK0_GAMCTRL_VC255_MASK, 0x1f)
	},
	.nlinv = 1,
	.vop_uv = 4800000,
	.vcom_uv = 1650000,
	.vgh_mv = 15000,
	.vgl_mv = -10170,
	.avdd_mv = 6600,
	.avcl_mv = -4400,
	.gamma_op_bias = OP_BIAS_MIDDLE,
	.input_op_bias = OP_BIAS_MIN,
	.output_op_bias = OP_BIAS_MIN,
	.t2d_ns = 1600,
	.t3d_ns = 10400,
	.eot_en = true,
	.gip_sequence = fs028vg047_gip_sequence,
};

static const struct st7701_panel_desc *panel_descs[] = {
	&rg_arc_desc,
	&dmt028vghmcmi_1a_desc,
	&kd50t048a_desc,
	&ts8550b_desc,
	&rg28xx_desc,
	&fs028vg047_desc,
};

static int st7701_panel_enable_backlight(struct udevice *dev)
{
	struct st7701 *st7701 = dev_get_priv(dev);
	int err;

	if (st7701->dsi) {
		err = mipi_dsi_attach(st7701->dsi);
		if (err < 0) {
			dev_err(dev, "Failed to attach DSI: %d\n", err);
			return err;
		}
	} else {
		err = mipi_dbi_spi_init(st7701->spi, &st7701->dbi, st7701->dc);
		if (err) {
			dev_err(dev, "MPI DBI init failed: %d\n", err);
			return err;
		}
	}

	err = st7701_prepare(st7701);
	if (err < 0) {
		dev_err(dev, "Failed to prepare ST7701: %d\n", err);
		return err;
	}

	ST7701_WRITE(st7701, MIPI_DCS_SET_DISPLAY_ON, 0x00);

	if (st7701->backlight) {
		/* Wait for the picture to be ready before enabling backlight */
		mdelay(120);
		err = backlight_enable(st7701->backlight);
	}

	return err;
}

static int st7701_panel_set_backlight(struct udevice *dev, int percent)
{
	struct st7701 *st7701 = dev_get_priv(dev);
	int ret;

	if (!st7701->backlight)
		return 0;

	ret = backlight_enable(st7701->backlight);
	if (ret)
		return ret;

	return backlight_set_brightness(st7701->backlight, percent);
}

static int st7701_panel_get_display_timing(struct udevice *dev,
					   struct display_timing *timing)
{
	struct st7701 *st7701 = dev_get_priv(dev);
	const struct display_timing *our_timing = st7701->desc->timing;

	memcpy(timing, our_timing, sizeof(*our_timing));

	return 0;
}

static int st7701_panel_of_to_plat(struct udevice *dev)
{
	struct st7701 *st7701 = dev_get_priv(dev);
	int panel_desc_index = dev_get_driver_data(dev);
	int err;

	st7701->desc = panel_descs[panel_desc_index];

	if (CONFIG_IS_ENABLED(DM_REGULATOR)) {
		err = device_get_supply_regulator(dev, "VDD-supply",
						  &st7701->vdd);
		if (err) {
			dev_err(dev, "Failed to get VDD supply: %d\n", err);
			return err;
		}

		err = device_get_supply_regulator(dev, "VDDIO-supply",
						  &st7701->vddio);
		if (err) {
			dev_err(dev, "Failed to get VDDIO supply: %d\n", err);
			return err;
		}
	}

	st7701->reset = devm_gpiod_get(dev, "reset", GPIOD_IS_OUT);
	if (IS_ERR(st7701->reset)) {
		dev_err(dev, "Failed to get reset GPIO: %d\n", err);
		return PTR_ERR(st7701->reset);
	}

	st7701->dc = devm_gpiod_get_optional(dev, "dc", GPIOD_IS_OUT);
	if (IS_ERR(st7701->dc)) {
		dev_err(dev, "Failed to get D/CX GPIO: %d\n", err);
		return PTR_ERR(st7701->reset);
	}

	/**
	 * Once sleep out has been issued, ST7701 IC required to wait 120ms
	 * before initiating new commands.
	 *
	 * On top of that some panels might need an extra delay to wait, so
	 * add panel specific delay for those cases. As now this panel specific
	 * delay information is referenced from those panel BSP driver, example
	 * ts8550b and there is no valid documentation for that.
	 */
	st7701->sleep_delay = 120 + st7701->desc->panel_sleep_delay;

	err = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &st7701->backlight);
	if (err) {
		dev_err(dev, "Failed to get backlight: %d\n", err);
		return err;
	}

	return 0;
}

static int st7701_dsi_panel_probe(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *dsi = plat->device;
	struct st7701 *st7701 = dev_get_priv(dev);

	st7701->dsi = dsi;
	st7701->write_command = st7701_dsi_write;

	plat->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	plat->format = st7701->desc->format;
	plat->lanes = st7701->desc->lanes;

	return 0;
}

static int st7701_dbi_panel_probe(struct udevice *dev)
{
	struct spi_slave *spi = dev_get_parent_priv(dev);
	struct st7701 *st7701 = dev_get_priv(dev);

	st7701->spi = spi;
	st7701->write_command = st7701_dbi_write;

	return 0;
}

static const struct panel_ops st7701_panel_ops = {
	.enable_backlight = st7701_panel_enable_backlight,
	.set_backlight = st7701_panel_set_backlight,
	.get_display_timing = st7701_panel_get_display_timing,
};

static const struct udevice_id st7701_dsi_panel_ids[] = {
	{ .compatible = "anbernic,rg-arc-panel", .data = 0 },
	{ .compatible = "densitron,dmt028vghmcmi-1a", .data = 1 },
	{ .compatible = "elida,kd50t048a", .data = 2 },
	{ .compatible = "techstar,ts8550b", .data = 3 },
	{ }
};

static const struct udevice_id st7701_dbi_panel_ids[] = {
	{ .compatible = "anbernic,rg28xx-panel", .data = 4 },
	{ .compatible = "fascontek,fs028vg047", .data = 5 },
	{ }
};

U_BOOT_DRIVER(st7701_dsi_panel) = {
	.name			  = "st7701_dsi_panel",
	.id			  = UCLASS_PANEL,
	.of_match		  = st7701_dsi_panel_ids,
	.ops			  = &st7701_panel_ops,
	.of_to_plat		  = st7701_panel_of_to_plat,
	.probe			  = st7701_dsi_panel_probe,
	.plat_auto		  = sizeof(struct mipi_dsi_panel_plat),
	.priv_auto		  = sizeof(struct st7701),
};

U_BOOT_DRIVER(st7701_dbi_panel) = {
	.name			  = "st7701_dbi_panel",
	.id			  = UCLASS_PANEL,
	.of_match		  = st7701_dbi_panel_ids,
	.ops			  = &st7701_panel_ops,
	.of_to_plat		  = st7701_panel_of_to_plat,
	.probe			  = st7701_dbi_panel_probe,
	.priv_auto		  = sizeof(struct st7701),
};
