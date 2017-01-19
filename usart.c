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

#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include "usart.h"

/*! \brief Interrupt rx.
 *
 * IRQ functions triggered every incoming char from the serial
 * ports.
 * If USARTn_EOL defined, every incoming USARTn_EOL chars increments
 * the message counter in the usartn struct.
 *
 */
ISR(USART0_RX_vect)
{
	uint8_t rxc;

	/*! First copy the rx char from the device rx buffer. */
	rxc = UDR0;

#ifdef USART0_EOL
	if (rxc == USART0_EOL)
		usart0->flags.eol++;
#endif /* USART0_EOL */

	cbuffer_push(usart0->rx, rxc);
}

#ifdef USE_USART1
/*! Repeat it for the second serial port if used. */
ISR(USART1_RX_vect)
{
	uint8_t rxc;

	/*! First copy the rx char from the device rx buffer. */
	rxc = UDR1;

#ifdef USART1_EOL
	if (rxc == USART1_EOL)
		usart1->flags.eol++;
#endif /* USART1_EOL */

	cbuffer_push(usart1->rx, rxc);
}
#endif /* USE_USART1 */

/*! Start the usart port.
 *
 * See datasheet for more info, generally it is better to put fixed
 * parameter from the datasheet instead of calculating it.
 *
 * if it is possible to improve baud rate error by using 2x clk

#if defined(U2X0)
	UCSR0A = _BV(U2X0);
	UBRR0H = (uint8_t)((F_CPU / 8UL / USART0_BAUD - 1) >> 8);
	UBRR0L = (uint8_t)(F_CPU / 8UL / USART0_BAUD - 1);
#else
	UBRR0H = (uint8_t)((F_CPU / 16UL / USART0_BAUD - 1) >> 8);
	UBRR0L = (uint8_t)(F_CPU / 16UL / USART0_BAUD - 1);
#endif

 * example of initializations
 * Tx only without Rx
 * UCSR0B = _BV(TXEN0);
 * Tx and Rx only without interrupt
 * UCSR0B = _BV(TXEN0) | _BV(RXEN0);
 * Rx only with interrupt
 * UCSR0B = _BV(RXCIE0) | _BV(RXEN0);
 * Rx with interrupt and Tx normal
 * UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);
 * Rx and Tx with interrupt
 * UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXCIE0) | _BV(TXEN0);
 *
 * \parameters port the serial port number.
 */
void usart_resume(const uint8_t port)
{
	if (port) {

#ifdef USE_USART1
		cbuffer_clear(usart1->rx);
		usart1->flags.all = 0;
		usart1->tx[0] = 0;

		/*! 9600 bps */
		UCSR1A = _BV(U2X1);
		UBRR1H = 0;
		UBRR1L = 12;

		/*! tx/rxI enable, 8n1 */
		UCSR1C = _BV(UCSZ10) | _BV(UCSZ11);
		UCSR1B = _BV(RXCIE1) | _BV(RXEN1) | _BV(TXEN1);
#endif

	} else {
		cbuffer_clear(usart0->rx);
		usart0->flags.all = 0;
		usart0->tx[0] = 0;

		/*! 9600 bps */
		UCSR0A = _BV(U2X0);
		UBRR0H = 0;
		UBRR0L = 12;
		/*! tx/rxI enable, 8n1 */
		UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);
		UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);
	}
}

/*! Disable the usart port. */
void usart_suspend(const uint8_t port)
{
	if (port) {

#ifdef USE_USART1
		UCSR1B = 0;
		usart_getchar(1, FALSE);
		/* I do not care about resetting all the parameters.
		UCSR1C = 0;
		UBRR1L = 0;
		UCSR1A &= ~_BV(U2X1);
		*/
#endif

	} else {
		UCSR0B = 0;
		usart_getchar(0, FALSE);
		/* I do not care about resetting all the parameters.
		UCSR0C = 0;
		UBRR0L = 0;
		UCSR0A &= ~_BV(U2X0);
		*/
	}
}

/*! Init the usart port.
 *
 * Allocates the global usart pointer.
 *
 * \note does NOT start the port(s).
 * \note Compiler should initialize globals pointers to NULL.
 * \warning this should be called only once in the code
 * to avoid memory leak.
 */
volatile struct usart_t *usart_init(const uint8_t port)
{
	if (port) {

#ifdef USE_USART1
		/* buffer allocated already? */
		if (!usart1) {
			usart1 = malloc(sizeof(struct usart_t));
			usart1->rx = cbuffer_init();
			usart1->tx = malloc(USART1_TXBUF_SIZE);
			usart1->tx_size = USART1_TXBUF_SIZE;
		}

		return(usart1);
#else
		return(NULL);
#endif /* USE_USART1 */

	} else {
		if (!usart0) {
			usart0 = malloc(sizeof(struct usart_t));
			usart0->rx = cbuffer_init();
			usart0->tx = malloc(USART0_TXBUF_SIZE);
			usart0->tx_size = USART0_TXBUF_SIZE;
		}

		return(usart0);
	}
}

/*! Deallocate the usart port struct.
 */
void usart_shut(uint8_t port)
{
	if (port) {

#ifdef USE_USART1
		if (usart1) {
			cbuffer_shut(usart1->rx);
			free(usart1->tx);
			free((void *)usart1);
		}
#endif /* USE_USART1 */

	} else {
		if (usart0) {
			cbuffer_shut(usart0->rx);
			free(usart0->tx);
			free((void *)usart0);
		}
	}
}

/*! Get a char directly from the usart port. */
char usart_getchar(const uint8_t port, const uint8_t locked)
{
	if (port) {

#ifdef USE_USART1
		if (locked) {
			loop_until_bit_is_set(UCSR1A, RXC1);
			return(UDR1);
		} else {
			if (bit_is_set(UCSR1A, RXC1))
				return(UDR1);
			else
				return(FALSE);
		}
#else
		return(FALSE);
#endif /* USE_USART1 */

	} else {
		if (locked) {
			loop_until_bit_is_set(UCSR0A, RXC0);
			return(UDR0);
		} else {
			if (bit_is_set(UCSR0A, RXC0))
				return(UDR0);
			else
				return(FALSE);
		}
	}
}

void usart_clear_rx_buffer(const uint8_t port)
{
	if (port) {

#ifdef USE_USART1
		usart1->flags.eol = 0;
		cbuffer_clear(usart1->rx);
#endif /* USE_USART1 */

	} else {
		usart0->flags.eol = 0;
		cbuffer_clear(usart0->rx);
	}
}

/*! get the RX buffer of a given maxsize.
 *
 * \param port the serial port.
 * \param s the area to copy the message to.
 * \param size the sizeof(s).
 * \note s must have the allocated size.
 */
uint8_t usart_get(const uint8_t port, uint8_t *s, const uint8_t size)
{
	uint8_t ok;

	if (port) {

#ifdef USE_USART1
		ok = cbuffer_pop(usart1->rx, s, size);
#else
		ok = 0;
#endif /* USE_USART1 */

	} else {
		ok = cbuffer_pop(usart0->rx, s, size);
	}

	return(ok);
}

/*! get the message from the RX buffer of a given maxsize.
 *
 * Caller function should check that a message is present before.
 *
 * \param port the serial port.
 * \param s the string to copy the message to.
 * \param size the sizeof(s).
 * \note s must have the allocated size, safety termination is in place.
 */
uint8_t usart_getmsg(const uint8_t port, uint8_t *s, const uint8_t size)
{
	uint8_t ok;

	if (port) {

#if defined (USE_USART1) && defined (USART1_EOL)
		ok = cbuffer_popm(usart1->rx, s, size, USART1_EOL);

		/* if a message has been taken only!
		 * Possible race condition if there where no
		 * messages on popm, but there is a flags.eol here.
		 */
		if (ok && usart1->flags.eol)
			usart1->flags.eol--;
#else
		ok = 0;
#endif /* USE_USART1 */

	} else {

#if defined (USART0_EOL)
		ok = cbuffer_popm(usart0->rx, s, size, USART0_EOL);

		if (ok && usart0->flags.eol)
			usart0->flags.eol--;
#else
		ok = 0;
#endif /* USART0_EOL */
	}

	return(ok);
}

/*! Send character c down the USART Tx, wait until tx holding register
 * is empty.
 *
 * \parameter port the serial port.
 * \parameter c the char to send.
 */
void usart_putchar(const uint8_t port, const uint8_t c)
{
	if (port) {

#ifdef USE_USART1
		loop_until_bit_is_set(UCSR1A, UDRE1);
		UDR1 = c;
#endif /* USE_USART1 */

	} else {
		loop_until_bit_is_set(UCSR0A, UDRE0);
		UDR0 = c;
	}
}

/*! Send a C (NUL-terminated) string down the USART Tx.
 *
 * If the string passed is NULL, then print the TX buffer of the
 * selected port.
 *
 * \parameter port the serial port.
 * \parameter s the string to send. If there is no string then
 * send the content of the tx_buffer.
 */
void usart_printstr(const uint8_t port, const char *s)
{
	if (port) {

#ifdef USE_USART1
		if (!s)
			s = usart1->tx;

		while (*s)
			usart_putchar(port, *s++);
#endif /* USE_USART1 */

	} else {
		if (!s)
			s = usart0->tx;

		while (*s)
			usart_putchar(port, *s++);
	}
}
