/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MIPI DBI Bus support
 *
 * Copyright 2024 John Watts <contact@jookia.org>
 */
#ifndef MIPI_DBI_H
#define MIPI_DBI_H

#include <asm/gpio.h>
#include <mipi_display.h>
#include <spi.h>

/**
 * struct mipi_dbi - MIPI DBI bus info
 *
 * This contains information about a MIPI DBI bus.
 * Use mipi_dbi_spi_init to create and initialize this structure.
 *
 * @spi:	SPI slave this bus operates on.
 */
struct mipi_dbi {
	struct spi_slave *spi;
};

/**
 * mipi_dbi_spi_init - Creates a new MIPI DBI bus
 *
 * Creates and sets up a 'struct mipi_dbi' using the provided SPI slave
 * and optional D/C GPIO.
 *
 * @slave:	SPI slave the bus is on
 * @dbi:	Destination mipi_dbi structure to initialize
 * @dc:		D/C GPIO (NULL if unused)
 *
 * Returns: 0 on success, -1 on failure.
 */
int mipi_dbi_spi_init(struct spi_slave *slave, struct mipi_dbi *dbi,
		      struct gpio_desc *dc);

/**
 * mipi_dbi_command_buf - Sends a command and data over the bus
 *
 * Sends a command and any optional data over a bus.
 *
 * @dbi:	MIPI DBI bus to use
 * @cmd:	MIPI DBI command
 * @data:	Command data (NULL if len is 0)
 * @len:	Length of data in bytes
 *
 * Returns: 0 on success, -1 on failure.
 */
int mipi_dbi_command_buf(struct mipi_dbi *dbi, u8 cmd, const u8 *data, size_t len);

/**
 * mipi_dbi_command - Sends a command and data sequence over the bus
 *
 * Sends a command and any optional data over a bus.
 * The data is a variadic sequence.
 *
 * @dbi:	MIPI DBI bus to use
 * @cmd:	MIPI DBI command
 * @seq:	Command data bytes
 *
 * Returns: 0 on success, -1 on failure.
 */
#define mipi_dbi_command(dbi, cmd, seq...) \
({ \
	const u8 data[] = { seq }; \
	mipi_dbi_command_buf(dbi, cmd, data, ARRAY_SIZE(data)); \
})

#endif /* MIPI_DBI_H */
