// SPDX-License-Identifier: GPL-2.0
/*
 * MIPI DBI Bus support
 *
 * Copyright 2024 John Watts <contact@jookia.org>
 */

#include <mipi_dbi.h>

int mipi_dbi_spi_init(struct spi_slave *slave, struct mipi_dbi *dbi,
		      struct gpio_desc *dc)
{
	/* D/C GPIO isn't supported yet */
	if (dc)
		return -1;

	dbi->spi = slave;

	return 0;
}

int mipi_dbi_xfer(struct mipi_dbi *dbi, u8 data, int pos, int len)
{
	struct spi_slave *spi = dbi->spi;
	bool is_data = (pos != 0);
	int flags = 0;
	u8 buf[2];

	/* Mimic Linux's behaviour of pulling CS active each word */
	flags |= SPI_XFER_ONCE;

	buf[0] = (is_data ? 0x80 : 0x00) | (data >> 1);
	buf[1] = ((data & 0x1) << 7);

	return spi_xfer(spi, 9, &buf, NULL, flags);
}

int mipi_dbi_command_buf(struct mipi_dbi *dbi, u8 cmd, const u8 *data, size_t len)
{
	struct spi_slave *spi = dbi->spi;
	int wordlen;
	int retval = -1;

	if (spi_claim_bus(spi))
		return -1;

	wordlen = spi_set_wordlen(spi, 9);
	if (wordlen == -1)
		goto done;

	if (mipi_dbi_xfer(dbi, cmd, 0, len) != 0)
		goto done;

	for (int i = 1; i <= len; ++i) {
		u8 dat = data[i - 1];

		if (mipi_dbi_xfer(dbi, dat, i, len) != 0)
			goto done;
	}

	retval = 0;

done:
	if (wordlen != -1)
		spi_set_wordlen(spi, wordlen);

	spi_release_bus(spi);

	return retval;
}
