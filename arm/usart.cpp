/** \file usart.c
  *
  * \brief Implements stream I/O using the AVR's USART.
  *
  * This allows the host to communicate with the AVR over a serial link.
  * On some Arduinos, the USART is connected to a USB-to-serial bridge,
  * allowing the host to communicate with the AVR over a USB connection.
  * See initUsart() for serial communication parameters.
  *
  * This file is licensed as described by the file LICENCE.
  */


#include "usart.h"

#include "Arduino.h"

#include "../common.h"
#include "../endian.h"
#include "../hwinterface.h"
#include "hwinit.h"
#include "lcd_and_input.h"
#include "main.h"
#include "BLE.h"
#include "eink.h"
#include "../stream_comm.h"
#include "DueTimer/DueTimer.h"
#include "keypad_alpha.h"

/** Size of transmit buffer, in number of bytes.
  * \warning This must be a power of 2.
  * \warning This must be >= 16 and must be <= 256.
  */
#define TX_BUFFER_SIZE	256
/** Size of receive buffer, in number of bytes.
  * \warning This must be a power of 2.
  * \warning This must be >= 16 and must be <= 256.
  */
#define RX_BUFFER_SIZE	256

/** Bitwise AND mask for transmit buffer index. */
#define TX_BUFFER_MASK	(TX_BUFFER_SIZE - 1)
/** Bitwise AND mask for receive buffer index. */
#define RX_BUFFER_MASK	(RX_BUFFER_SIZE - 1)

uint8_t bluetooth_on;
uint8_t is_formatted;

bool moofOn;


/** Storage for the transmit buffer. */
//static volatile uint8_t tx_buffer[TX_BUFFER_SIZE];
/** Storage for the receive buffer. */
//static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
/** Index in the transmit buffer of the first character to send. */
//static volatile uint8_t tx_buffer_start;
/** Index in the receive buffer of the first character to get. */
//static volatile uint8_t rx_buffer_start;
/** Index + 1 in the transmit buffer of the last character to send. */
//static volatile uint8_t tx_buffer_end;
/** Index + 1 in the receive buffer of the last character to get. */
//static volatile uint8_t rx_buffer_end;
/** Is transmit buffer is full? */
//static volatile bool tx_buffer_full;
/** Is receive buffer is full? */
//static volatile bool rx_buffer_full;
/** Has a receive buffer overrun occurred? */
static volatile bool rx_buffer_overrun;

/** Number of bytes which can be received until the next acknowledgment must
  * be sent. */
static uint32_t rx_acknowledge;
/** Number of bytes which can be sent before waiting for the next
  * Acknowledgment to be received. */
static uint32_t tx_acknowledge;


/** Initializes USART0 with the parameters:
  * baud rate 57600, 8 data bits, no parity bit, 1 start bit, 0 stop bits.
  * This also clears the transmit/receive buffers.
  */
void initUsart(void)
{
	bluetooth_on = checkBLE();

	// Need to put a routine in here that will check the BLE module serial speed and set to the appropriate bps if necessary
	if(bluetooth_on==1)
	{

//		Serial1.begin(9600);
//		delay(10000);
//
////		const char* data = "TTM:BPS-115200";
//
//		Serial1.print("TTM:BPS-115200");
//
//
//		delay(30000);
//
//		Serial1.end();

		Serial1.begin(115200);

//		Serial1.begin(57600);
//		Serial1.begin(38400);
//		Serial1.begin(19200);
//		Serial1.begin(9600);
//		Serial1.begin(4800);




	}else{
		Serial.begin(57600);
	}
//	pmc_enable_periph_clk(ID_TRNG);
//	trng_enable(TRNG);

}

//void serialToggle(void)
//{
//	Serial.end();
//	Serial.begin(2400);
//}
//

void initFormatting(void)
{
	pmc_enable_periph_clk(ID_TRNG);
	trng_enable(TRNG);
	is_formatted = checkisFormatted();
}



/** Send one byte through USART0. If the transmit buffer is full, this will
  * block until it isn't.
  * \param data The byte to send.
  */
void usartSend(uint8_t data)
{
	message_slice_counter++;
	if (message_slice_counter == 80)
	{
		// 100 ms delay to let the BLE catch its breath.
		delay(100);
		message_slice_counter = 0;
	}

	if(bluetooth_on==1)
	{
		Serial1.write(data);
	}else{
		Serial.write(data);
	}


}

void usartSpew(void)
{
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);
//	usartSend(0x00);

//	writeEinkDisplay("spew", false, COL_1_X, LINE_1_Y, "",false,5,30, "",false,5,50, "",false,5,70, "",false,0,0);

	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);
	usartSend(0xDE);
	usartSend(0xAD);
	usartSend(0xBE);
	usartSend(0xEF);

}



/** Receive one byte through USART0. If there isn't a byte in the receive
  * buffer, this will block until there is.
  * \return The byte that was received.
  */
uint8_t usartReceive(void)
{
	uint8_t r;
	byte rTmp[1];
//	int bluetooth_on;
//	bluetooth_on = checkBLE();

	if(bluetooth_on==1)
	{
		Serial1.readBytes(rTmp, 1);
	}else{
		Serial.readBytes(rTmp, 1);
	}

	// The check in the loop doesn't need to be atomic, because the worst
	// that can happen is that the loop spins one extra time.
//	while ((rx_buffer_start == rx_buffer_end) && !rx_buffer_full)
//	{
//		// do nothing
//	}
//		cli();
//		r = rx_buffer[rx_buffer_start];
//		rx_buffer_start++;
//		rx_buffer_start = (uint8_t)(rx_buffer_start & RX_BUFFER_MASK);
//		rx_buffer_full = false;
//		sei();
////		usartSpew();
//	if (rTmp[0] == 0x23){
//		usartSpew();
//	};
	r = rTmp[0];
	return r;
}



/** This is called if a stream read or write error occurs. It never returns.
  * \warning Only call this if the error is unrecoverable. It halts the CPU.
  */
static void streamReadOrWriteError(void)
{
	streamError();
//	cli();
//	sleep_mode();
	for (;;)
	{
		// do nothing
	}
}

/** Grab one byte from the communication stream. There is no way for this
  * function to indicate a read error. This is intentional; it
  * makes program flow simpler (no need to put checks everywhere). As a
  * consequence, this function should only return if the received byte is
  * free of read errors.
  *
  * Previously, if a read or write error occurred, processPacket() would
  * return, an error message would be displayed and execution would halt.
  * There is no reason why this couldn't be done inside streamGetOneByte()
  * or streamPutOneByte(). So nothing was lost by omitting the ability to
  * indicate read or write errors.
  *
  * Perhaps the argument can be made that if this function indicated read
  * errors, the caller could attempt some sort of recovery. Perhaps
  * processPacket() could send something to request the retransmission of
  * a packet. But retransmission requests are something which can be dealt
  * with by the implementation of the stream. Thus a caller of
  * streamGetOneByte() will assume that the implementation handles things
  * like automatic repeat request, flow control and error detection and that
  * if a true "stream read error" occurs, the communication link is shot to
  * bits and nothing the caller can do will fix that.
  * \return The received byte.
  */
uint8_t streamGetOneByte(void)
{
	uint8_t one_byte;

	one_byte = usartReceive();
	rx_acknowledge--;
	if (rx_acknowledge == 0)
	{
		// Send acknowledgement to other side.
		uint8_t buffer[4];
		uint8_t i;

		rx_acknowledge = RX_BUFFER_SIZE;
		writeU32LittleEndian(buffer, rx_acknowledge);
		usartSend(0xff);
		for (i = 0; i < 4; i++)
		{
			usartSend(buffer[i]);
		}
	}
	if (rx_buffer_overrun)
	{
		streamReadOrWriteError();
	}
	return one_byte;
}

/** Send one byte to the communication stream. There is no way for this
  * function to indicate a write error. This is intentional; it
  * makes program flow simpler (no need to put checks everywhere). As a
  * consequence, this function should only return if the byte was sent
  * free of write errors.
  *
  * See streamGetOneByte() for some justification about why write errors
  * aren't indicated by a return value.
  * \param one_byte The byte to send.
  */
void streamPutOneByte(uint8_t one_byte)
{
	usartSend(one_byte);
	tx_acknowledge--;
	if (tx_acknowledge == 0)
	{
		// Need to wait for acknowledgment from other side.
		uint8_t buffer[4];
		uint8_t i;

		do
		{
			// do nothing
		} while (usartReceive() != 0xff);
		for (i = 0; i < 4; i++)
		{
			buffer[i] = usartReceive();
		}
		tx_acknowledge = readU32LittleEndian(buffer);
	}
}

void moof(void)
{
	moofOn = true;
}

void powerDown()
{
	pmc_enable_backupmode();
}

void startTimer1(void){
	Timer1.attachInterrupt(moof).setPeriod(1000000000).start();
}
void startTimer2(void){
	Timer2.attachInterrupt(powerDown).setPeriod(500000000).start();
}

void stopTimer1(void){
	Timer1.stop();
}

void stopTimer2(void){
	Timer2.stop();
}
