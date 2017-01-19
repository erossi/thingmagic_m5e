/*
    Circular Buffer, a string oriented circular buffer implementation.
    Copyright (C) 2015, 2016 Enrico Rossi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "circular_buffer.h"

/*! Clear the buffer.
 */
void cbuffer_clear(struct cbuffer_t *cbuffer)
{
	cbuffer->idx = 0;
	cbuffer->start = 0;
	cbuffer->len = 0;
	cbuffer->overflow = FALSE;
}

/*! Initialize the buffer.
 *
 * \param plugin enable the check_eom function plugin.
 * \return the allocated struct.
 */
struct cbuffer_t *cbuffer_init(void)
{
	struct cbuffer_t *cbuffer;

	cbuffer = malloc(sizeof(struct cbuffer_t));
	cbuffer->size = CBUF_SIZE;
	cbuffer->buffer = malloc(CBUF_SIZE);

	if (cbuffer->buffer)
		cbuffer->TOP = CBUF_SIZE - 1;

	cbuffer_clear(cbuffer);
	return (cbuffer);
}

/*! Remove the buffer.
 */
void cbuffer_shut(struct cbuffer_t *cbuffer)
{
	free(cbuffer->buffer);
	free(cbuffer);
}

/*! Copy a byte from the buffer to the message area.
 *
 * data[j] = buffer[start]
 *
 * \warning cbuffer->start gets modified.
 * \note there is not protection, the next char in the buffer will be
 * extracted and indexes modified event if it should not.
 */
uint8_t bcpy(struct cbuffer_t *cbuffer,
	     uint8_t *data, const uint8_t size, uint8_t j)
{
	if (j < size) {
		*(data + j) = *(cbuffer->buffer + cbuffer->start);
		j++;
	}
#ifdef CBUF_OVR_CHAR
	*(cbuffer->buffer + cbuffer->start) = CBUF_OVR_CHAR;
#endif

	if (cbuffer->start == cbuffer->TOP)
		cbuffer->start = 0;
	else
		cbuffer->start++;

	cbuffer->len--;
	return (j);
}

/*! Get the stored buffer.
 *
 * Fetch from the start_index (cbuffer->start) to the current index (cbuffer->idx).
 *
 * \return the number of bytes received.
 */
uint8_t cbuffer_pop(struct cbuffer_t * cbuffer, uint8_t * data,
		    const uint8_t size)
{
	uint8_t index, j;

	j = 0;

	if (cbuffer->len) {
		/* freeze the index, while cbuffer->idx can be changed by
		 * volatile call to add a new bytes to the buffer.
		 */
		index = cbuffer->idx;

		/* Copy the 1st char.
		 * In an overflow condition, the start = index, therefor it
		 * will exit without getting anything back.
		 */
		if (cbuffer->overflow)
			j = bcpy(cbuffer, data, size, j);

		while ((cbuffer->start != index) && (j < size))
			j = bcpy(cbuffer, data, size, j);

		/* unlock the buffer */
		cbuffer->overflow = FALSE;
	}

	return (j);
}

/*! get a message present in the buffer.
 *
 * If no EOM is found then all the content of the buffer
 * is copied and no EOM or \0 is added to the end.
 *
 * If the size of *data is less then the message in the buffer, then
 * *data get filled and the rest of the message is lost.
 *
 * \param cbuffer the circular buffer.
 * \param data the area where to copy the message if found.
 * \param size sizeof(data)
 * \param eom the EndOfMessage char.
 * \return the number of char copied.
 *
 * \warning if the *data is filled, no EOM or \0 is added at the end.
 */
uint8_t cbuffer_popm(struct cbuffer_t *cbuffer,
		     uint8_t *data, const uint8_t size, const uint8_t eom)
{
	uint8_t index, j, loop;

	j = 0;

	/* if there is something in the buffer */
	if (cbuffer->len) {
		/* freeze the index, while cbuffer->idx can be changed by
		 * volatile call to add a new bytes to the buffer.
		 */
		index = cbuffer->idx;
		loop = TRUE;

		/* Check if overflow and copy the 1st char manually.
		 * In an overflow condition, start == index, therefore
		 * no chars will be copied normally.
		 */
		if (cbuffer->overflow) {
			/* if the 1st char is an EOM, copy it and exit */
			if (*(cbuffer->buffer + cbuffer->start) == eom)
				loop = FALSE;

			/* copy a byte, cbuffer->start changed */
			j = bcpy(cbuffer, data, size, j);
		}

		/* Extract all the date in the buffer until:
		 * - EOM is found or
		 * - buffer is empty.
		 *
		 * WARN: do NOT merge this with the previous in a
		 * do..while loop, because bcpy() modifies cbuffer
		 * indexes.
		 */
		while (loop && (cbuffer->start != index)) {
			/* if an EOM is found.
			 * \note need to be done before bcpy() because
			 * cbuffer->start gets modified later.
			 */
			if (*(cbuffer->buffer + cbuffer->start) == eom)
				loop = FALSE;

			/* copy a byte, cbuffer->start changed */
			j = bcpy(cbuffer, data, size, j);
		}

		/* unlock the buffer */
		cbuffer->overflow = FALSE;
	}

	return (j);
}

/*! add data to the buffer.
 *
 * \note if overflow and EOM then the last char must be the EOM.
 */
uint8_t cbuffer_push(struct cbuffer_t * cbuffer, char rxc)
{
	/* If the buffer is full (overflow flag)
	 * do nothing.
	 */
	if (cbuffer->overflow) {
		return (FALSE);
	} else {
		/* catch overflow */
		if (cbuffer->start) {
			/* idx next to start? */
			if (cbuffer->idx == (cbuffer->start - 1))
				cbuffer->overflow = TRUE;
		} else {
			/* idx next to the end of the buffer? */
			if (cbuffer->idx == cbuffer->TOP)
				cbuffer->overflow = TRUE;
		}

		*(cbuffer->buffer + cbuffer->idx) = rxc;

		if (cbuffer->idx == cbuffer->TOP)
			cbuffer->idx = 0;
		else
			cbuffer->idx++;

		cbuffer->len++;
		return (TRUE);
	}
}
