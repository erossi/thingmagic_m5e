/* Copyright (C) 2015, 2016 Enrico Rossi

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*! \file rfid_m5.h
 * \brief Functions to read (rs232) rfid.
 */

#ifndef RFIDM5_H
#define RFIDM5_H

#include "usart.h"

/*! Serial port */
#define RFID_USART 1
/*! deprecated */
#define RFID_USART_PORT 1
/*! Enable port */
#define RFID_PORT PORTA
/*! Enable ddr */
#define RFID_DDR DDRA
/*! Enable pin */
#define RFID_EN PA3
/*! How many attempt should be made to read a code */
#define RFID_READ_RETRY 10

/* RFID_SIZE represent the code as it should be sent to
 * the server in byte, not its representation in char.
 * RFID_BUFFER_SIZE is the maximum string received from the reader.
 */
#define RFID_SIZE 16
#define RFID_BUFFER_SIZE 0xff

/*! Tag password check */
/*
#ifndef RFID_M5_PASSWORD
#define RFID_M5_PASSWORD 0x00, 0x00, 0x00, 0x00
#endif
*/

/*! Set the TX Read Power.
 *
 * RFID_M5_LOWTXPWR if defined reduce the output power in antenna.
 *
 * \see M5eFamilyDevGuide book pag.35
 * \see Command 0x62
 *
 * dBm = 10log(mW) (log base 10).
 * mW = 10^(dBm/10)
 *
 * With the EU config.
 * The minimum is 0x03e8 which is 10dBm
 * The Max is 08fc which is 23dBm
 * 10dBm = 10mW (0x03e8)
 * 17dBm = 50mW
 * 17.78dBm = 60mW (0x06f2)
 * 23dBm = 199.5mW (0x08fc)
 */
#ifdef RFID_M5_LOWTXPWR
#define RFID_M5_TX_RDBM_H 0x06
#define RFID_M5_TX_RDBM_L 0xf2
#endif

/*! Tag singulation field
 *
 *  Fix for your needs.
 *
 * \note make it changable during runtime.
 */
#define RFID_M5_SINGULATION 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/*! a single rfid record
 *
 * \bug the struct changes based on the RFID_M5 defs,
 * which can be different during the compilation where the
 * -D RFID_M5 is present or not.
 */
struct rfid_t {
	/*! rfid code size */
	uint8_t size;
	/*! usart struct. */
	volatile struct usart_t *usart;
	uint8_t *data;
	uint8_t soh;
	uint8_t len;
	uint8_t opcode;
	uint16_t status;
	uint16_t crc;
	uint8_t error;
};

/*! Globals */
struct rfid_t *rfid;

uint8_t rfid_read(uint8_t *data);
void rfid_suspend(void);
uint8_t rfid_resume(void);
struct rfid_t* rfid_init(void);
void rfid_shut(void);

#endif
