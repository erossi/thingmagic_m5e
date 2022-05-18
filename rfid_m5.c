/* Copyright (C) 2016 Enrico Rossi

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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "rfid_m5.h"

/** @fn void CRC_calcCrc8(u16 *crcReg, u16 poly, u16 u8Data)
 * @ Standard CRC calculation on an 8-bit piece of data. To make it
 * CCITT-16, use poly=0x1021 and an initial crcReg=0xFFFF.
 *
 * Note: This function allows one to call it repeatedly to continue
 * calculating a CRC. Thus, the first time it's called, it
 * should have an initial crcReg of 0xFFFF, after which it
 * can be called with its own result.
 *
 * @param *crcRegPointer to current CRC register.
 * @param poly Polynomial to apply.
 * @param u8Datau8 data to perform CRC on.
 * @return None.
 */
void CRC_calcCrc8(uint16_t *crcReg, uint16_t u8Data)
{
	uint16_t i, xorFlag, bit;
	uint16_t dcdBitMask = 0x80;

	for(i=0; i<8; i++) {
		/*
		// Get the carry bit. This determines if the polynomial should be
		// xor'd with the CRC register.
		*/
		xorFlag = *crcReg & 0x8000;
		/* Shift the bits over by one. */
		*crcReg <<= 1;
		/* Shift in the next bit in the data byte */
		bit = ((u8Data & dcdBitMask) == dcdBitMask);
		*crcReg |= bit;

		/* XOR the polynomial */
		if(xorFlag)
			*crcReg = *crcReg ^ 0x1021;

		/* Shift over the dcd mask */
		dcdBitMask >>= 1;
	}
}

/*! \brief CRC calc
 *
 * The CRC is calculated on the Data Length, Command, Status Word, and
 * Data bytes. The header (SOH, 0xFF) is not included in the CRC.
 *
 * The field are taken from the global rfid struct.
 *
 * \param include_status boolean to include the status word in the CRC
 * calculation, used when the CRC is used on the received data.
 *
 * \sa struct rfid_t
 */
uint16_t m5_crc(const uint8_t include_status)
{
	uint16_t crc16;
	uint8_t i;

	crc16 = 0xffff;
	CRC_calcCrc8(&crc16, rfid->len);
	CRC_calcCrc8(&crc16, rfid->opcode);

	if (include_status) {
		/* MSB */
		CRC_calcCrc8(&crc16, (uint8_t)(rfid->status >> 8));
		/* LSB */
		CRC_calcCrc8(&crc16, (uint8_t)(rfid->status & 0xff));
	}

	for (i=0; i<rfid->len; i++)
		CRC_calcCrc8(&crc16, *(rfid->data + i));

	return(crc16);
}

/*! TX the struct to m5
 *
 * The packet structure is:
 * Hdr(1) + datalen(1) + Cmd(1) + data(N) + CRC(Hi + Lo)
 *
 */
void tx_pkt(void)
{
	uint8_t i;

	usart_putchar(RFID_USART, rfid->soh);
	usart_putchar(RFID_USART, rfid->len);
	usart_putchar(RFID_USART, rfid->opcode);

	for (i=0; i<(rfid->len); i++)
		usart_putchar(RFID_USART, *(rfid->data + i));

	/* Send CRC Hi */
	usart_putchar(RFID_USART, (uint8_t)(rfid->crc >> 8));
	/* Send CRC Lo */
	usart_putchar(RFID_USART, (uint8_t)(rfid->crc & 0xff));
}

/*! RX from m5
 *
 * Extract one single byte at a time from the buffer and store
 * it in the rfid structure. Every step is recorded in the rfid->error,
 * in case of failure it is possible to know at which step the problem occured.
 *
 * The rfid reply should be in 650msec max.
 *
 * The packet structure is:
 * Hdr(1) + datalen(1) + Cmd(1) + Status(2) + data(N) + CRC(Hi + Lo)
 *
 * \param timeout express in 100*msec
 * \return TRUE in case of a correct packet received.
 * \warning rfid.data must be already malloc-ed
 */
uint8_t rx_pkt(uint16_t timeout)
{
	uint8_t i;

#define END 0
#define SOH 1
#define LEN 2
#define CMD 3
#define STATUS 4
#define DATA 5
#define CRC 6

	i = 0;
	rfid->error = SOH;

	while (timeout) {
		_delay_ms(10);

		switch (rfid->error) {
			case SOH:
				if (usart_get(RFID_USART_PORT, &rfid->soh, 1)
						&& (rfid->soh == 0xff))
					rfid->error = LEN;
				break;
			case LEN:
				if (usart_get(RFID_USART_PORT, &rfid->len, 1))
					rfid->error = CMD;
				break;
			case CMD:
				if (usart_get(RFID_USART_PORT, &rfid->opcode, 1)) {
					/* next step requires 2 read-cycle */
					i = 0;
					rfid->error = STATUS;
				}

			case STATUS:
				if (i < 2) {
					if (usart_get(RFID_USART_PORT, (uint8_t *)&rfid->status + 1 - i, 1))
						i++;
				} else {
					/* next step requires many read-cycle */
					i = 0;
					rfid->error = DATA;
				}

				break;
			case DATA:
				if (i < rfid->len) {
					if (usart_get(RFID_USART_PORT, rfid->data + i, 1))
						i++;
				} else {
					/* next step requires 2 read-cycle */
					i = 0;
					rfid->error = CRC;
				}

				break;
			case CRC:
				if (i < 2) {
					/* Little Endian, MSB come first */
					if (usart_get(RFID_USART_PORT, (uint8_t *)&rfid->crc + 1 - i, 1))
						i++;
				} else {
					if (m5_crc(TRUE) == rfid->crc)
						rfid->error = END;

					timeout = 1;
				}

				break;
			case END:
			default:
				timeout = 1;
				break;
		}

		timeout--;
	}

	return(rfid->error);
}

/*! Send a command to the device and get the ACK/ANSWER
 *
 * Prepare the correct rfid field (SOH and CRC) the send it to
 * the device, wait for the answer/ack and checkit.
 *
 * \return TRUE command send and ack properly received.
 */
uint8_t send_cmd(void)
{
	uint8_t cmd;

	cmd = rfid->opcode;
	rfid->soh = 0xff;
	rfid->crc = m5_crc(FALSE);
	tx_pkt();
	/* reply in 650msec max */
	rx_pkt(500);

	if ((!rfid->error) && (rfid->opcode == cmd) && (!rfid->status))
		return(TRUE);
	else
		return(FALSE);
}

/*! Get the RFID code.
 *
 * String size of the code is RFID size * 2 plus the CRC plus \0.
 *
 * \param data pre-allocated byte space.
 * \return TRUE rfid code is present, FALSE no valid code.
 */
uint8_t rfid_read(uint8_t* data)
{
	uint8_t ok;

#ifdef RFID_M5_PASSWORD
	static const uint8_t PROGMEM cmd_read[] = {
		0x03, 0xe8,
		0x02, 0x01, 0x00, 0x00,0x00,0x02,
		0x08,
		RFID_M5_PASSWORD,
		0x00, 0x00, 0x00, 0x00,
		RFID_M5_SINGULATION};
#endif

	rfid->data = malloc(0xff);
	usart_clear_rx_buffer(RFID_USART);

#ifdef RFID_M5_PASSWORD
	/* # Read the EPC of a locked TAG
	 * cmd:                28 (read memory)
	 * Time out:           03e8
	 * Singulation Option: 02 (select singulation on TID)
	 * Read Membank:       01 (EPC)
	 * Read Address:       00000002 (starting from 2 word inside the EPC memory)
	 * Word count:         08 (16 Byte EPC lenght)
	 * Access code:        xxxxxxxx (to be changed!)
	 * Singulation addr:   00000000 (TID starting address)
	 * Signulation lenght: 08 bit (1 byte to match to select the tag)
	 * Singulation data:   e2 (select the tag with TID starting with e2)
	 *
	 * > 28 03e8 02 01 00000002 08 xxxxxxxx 00000000 01 e2
	 */
	rfid->len = 0x1e;
	rfid->opcode = 0x28;
	memcpy_P(rfid->data, cmd_read, rfid->len);
	ok = send_cmd();

	/* \bug copy rfid->len or rfid->size ? FIXME */
	if (ok)
		memcpy(data, rfid->data + 1, rfid->size);
#else
	/* Read the EPC of the 1st tag available
	 * ff022103e8d509
	 * 0x03e8 1 sec. timeout
	 * 0x2710 10 sec. timeout
	 */
	rfid->len = 0x02;
	rfid->opcode = 0x21;
	rfid->data[0] = 0x03;
	rfid->data[1] = 0xe8;
	ok = send_cmd();

	/*! \bug copy rfid->len or rfid->size ? FIXME */
	if (ok)
		memcpy(data, rfid->data, rfid->size);
#endif

	free(rfid->data);
	return(ok);
}

/*! Suspend call.
 *
 * \ingroup sleep_group
 */
void rfid_suspend(void)
{
	usart_suspend(RFID_USART_PORT);
}

/*! Resume call.
 *
 * \ingroup sleep_group
 */
uint8_t rfid_resume(void)
{
	rfid->data = malloc(0xff);
	usart_resume(RFID_USART_PORT);
	_delay_ms(100);

	/* Boot Firmware (04h):
	 * ff00041d0b
	 *
	 * The maximum time required to boot the application firmware is 650ms.
	 */
	rfid->len = 0;
	rfid->opcode = 0x04;
	send_cmd();

	/* The firmware may be already started */
	if (rfid->error && (rfid->status == 0x0101))
		rfid->error = FALSE;

	if (!rfid->error) {
		/* Set Current Region (97h)
		 * EU: ff0197024bbf
		 * EU3: ff0197084bb5
		 */
		rfid->len = 0x01;
		rfid->opcode = 0x97;
		rfid->data[0] = 0x02;
		send_cmd();
	}

	if (!rfid->error) {
		/* Set Current Tag Protocol (93h) [to Gen2]
		 * ff02930005517d
		 */
		rfid->len = 0x02;
		rfid->opcode = 0x93;
		rfid->data[0] = 0;
		rfid->data[1] = 0x05;
		send_cmd();
	}

	if (!rfid->error) {
		/* Set power mode (to min, it is off, but the tx still the same)
		 * also it consume a lot less.
		 * -> ff01980344be
		 * <- ff009800008671
		 */
		rfid->len = 0x01;
		rfid->opcode = 0x98;
		rfid->data[0] = 0x03;
		send_cmd();
	}

#ifdef RFID_M5_LOWTXPWR
	if (!rfid->error) {
		/* set the tx (read) power to the minimum (03e8 from above)
		 * -> ff029203e842b1
		 * <- ff00920000273b
		 */
		rfid->len = 0x02;
		rfid->opcode = 0x92;
		rfid->data[0] = RFID_M5_TX_RDBM_H;
		rfid->data[1] = RFID_M5_TX_RDBM_L;
		send_cmd();
	}
#endif

	if (!rfid->error) {
		/* Set the Reader config (max epc lenght) to 496 bits
		 * > 9a 01 02 01
		 * -> ff039a010201ad5c
		 * <- ff009a0000a633
		 */
		rfid->len = 0x03;
		rfid->opcode = 0x9a;
		rfid->data[0] = 0x01;
		rfid->data[1] = 0x02;
		rfid->data[2] = 0x01;
		send_cmd();
	}

	free(rfid->data);
	return(rfid->error);
}

/*! Initialize the USART port and the rfid struct.
 */
struct rfid_t* rfid_init(void)
{
	rfid = malloc(sizeof(struct rfid_t));
	rfid->usart = usart_init(RFID_USART_PORT);
	rfid->size = RFID_SIZE;
	/* data should be allocated on a usage needs */
	/* rfid->data = malloc(0xff); */
	return(rfid);
}

/*! Shutdown the USART port and remove the rfid struct.
 */
void rfid_shut(void)
{
	rfid_suspend();
	/* free(rfid->data); */
	usart_shut(RFID_USART_PORT);
	free(rfid);
}
