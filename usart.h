/*
    USART - Serial port library.
    Copyright (C) 2005-2016 Enrico Rossi

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA  02110-1301  USA
 */

/*! \file usart.h
 * \brief RS232 - IO functions.
 *
 * options examples:
 *  End Of Line = CR
 * -D USARTn_EOL=0xd
 *
 *  Use the arduino device
 * -D USE_ARDUINO
 *
 *  Tx buffer size
 * -D USARTn_TXBUF_SIZE=16
 */

#ifndef _USART_H_
#define _USART_H_

#include "circular_buffer.h"

#ifdef USE_DEFAULT_H
#include "default.h"
#endif

/*! Arduino setup
 */
#ifdef USE_ARDUINO

#ifdef USE_USART1
#error You cannot use USE_USART1 with USE_ARDUINO
#endif

#define USART0_RX_vect USART_RX_vect
#define USART0_TX_vect USART_TX_vect

#endif

/*! Tx buffer size must have a default */
#ifndef USART0_TXBUF_SIZE
#define USART0_TXBUF_SIZE 16
#endif

/*! if the second serial port is in use */
#ifdef USE_USART1
#ifndef USART1_TXBUF_SIZE
#define USART1_TXBUF_SIZE 16
#endif /* USART1_TXBUF_SIZE */
#endif /* USE_USART1 */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/*! Structure with IO buffers and indexes */
struct usart_t {
	/*! receive struct. */
	struct cbuffer_t *rx;
	/*! transmit buffer. */
	char *tx;
	uint8_t tx_size;
	/*! flags. */
	volatile union {
		/* c11 only */
		struct {

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			/* lsb bit 0 */
			uint8_t b0:1;
			uint8_t b1:1;
			uint8_t eol:6;
#else
			/* msb */
			uint8_t eol:6;
			uint8_t b1:1;
			uint8_t b0:1;
#endif

		};

		uint8_t all;
	} flags;
};

/*! Global USART rxtx buffers pointer used inside the ISR routine. */
volatile struct usart_t *usart0;

#ifdef USE_USART1
volatile struct usart_t *usart1;
#endif

void usart_resume(const uint8_t port);
void usart_suspend(const uint8_t port);
volatile struct usart_t *usart_init(uint8_t port);
void usart_shut(uint8_t port);
char usart_getchar(const uint8_t port, const uint8_t locked);
void usart_putchar(const uint8_t port, const uint8_t c);
void usart_printstr(const uint8_t port, const char *s);
uint8_t usart_get(const uint8_t port, uint8_t *s, const uint8_t size);
uint8_t usart_getmsg(const uint8_t port, uint8_t *s, const uint8_t size);
void usart_clear_rx_buffer(const uint8_t port);

#endif
