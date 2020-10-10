#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "serial.h"

/* pin assignments
 * B[0,1]: D0,D1
 * D[2,7]: D2-D7
 * B2: SPI ~SS/shiftreg ~OE
 * B3: SPI MOSI
 * B4: SPI MISO (unused)
 * B5: SPI SCK
 * C0: programming mode PGM
 * C1: RAM ~CS
 * C2: RAM ~WE
 * C3: RAM ~OE
 * C5: ~SYSRST
 */
#define PB_SS		0x04
#define PB_MOSI		0x08
#define PB_MISO		0x10
#define PB_SCK		0x20
#define PC_PGM		0x01
#define PC_CS		0x02
#define PC_WE		0x04
#define PC_OE		0x08
#define PC_SYSRST	0x20

static void proc_cmd(char *input);
static void start_prog(void);
static void end_prog(void);
static void write_ram(uint16_t addr, uint8_t val);
static uint8_t read_ram(uint16_t addr);
static void set_data_bus(unsigned char val);
static void set_addr_bus(uint16_t addr);
static void sys_reset(void);
static inline void iodelay(void);

static int echo;
static int progmode;
static uint16_t addr;

static char input[128];
static unsigned char inp_cidx;


int main(void)
{
	/* SPI (SS/MOSI/SCK) are outputs, as are the low 2 data bus bits */
	DDRB = PB_SS | PB_MOSI | PB_SCK | 3;
	PORTB = PB_SS;	/* drive SS high when not in programming mode */
	DDRC = 0xff;	/* control signals are all outputs */
	PORTC = PC_CS | PC_WE | PC_OE | PC_SYSRST;
	DDRD = 0xff;	/* always drive the (high bits of the) data bus */
	PORTD = 0;

	/* disable all pullups */
	MCUCR |= 1 << PUD;

	/* enable SPI (used to talk to the address bus shift registers)
	 * SPI master mode, clock / 16, CPOL=0/CPHA=0 because shiftreg clocks on the
	 * rising edge, and MSB first
	 */
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);

	/* init the serial port we use to talk to the host */
	init_serial(38400);
	sei();

	for(;;) {
		if(have_input()) {
			int c = getchar();
			if(echo) {
				putchar(c);
			}

			if(c == '\r' || c == '\n') {
				input[inp_cidx] = 0;
				proc_cmd(input);
				inp_cidx = 0;
			} else if(inp_cidx < sizeof input - 1) {
				input[inp_cidx++] = c;
			}
		}
	}
	return 0;
}

static void proc_cmd(char *input)
{
	char *endp;
	int data;

	switch(input[0]) {
	case 'e':
		echo = input[1] == '1' ? 1 : 0;
		printf("OK echo %s\n", echo ? "on" : "off");
		break;

	case 'p':
		if(progmode) {
			puts("ERR already in programming mode");
		} else {
			start_prog();
			puts("OK programming mode");
		}
		break;

	case 'b':
		puts("OK booting");
		if(!progmode) {
			sys_reset();
		} else {
			end_prog();
		}
		break;

	case 'a':
		addr = strtol(input + 1, &endp, 0);
		printf("OK address: %x\n", (unsigned int)addr);
		break;

	case 'w':
		if(!progmode) {
			puts("ERR not in programming mode");
			break;
		}
		data = strtol(input + 1, &endp, 0);
		write_ram(addr++, data);
		puts("OK");
		break;

	case 'r':
		if(!progmode) {
			puts("ERR not in programming mode");
			break;
		}
		data = read_ram(addr++);
		printf("OK %d\n", (int)data);
		break;

	case '?':
		puts("OK command help");
		puts(" e 0|1: turn echo on/off");
		puts(" p: enter programming mode");
		puts(" b: exit programming mode and boot up");
		puts(" a <addr>: set address counter (0-ffff)");
		puts(" w <data>: store data byte and increment address (0-255)");
		puts(" r: read data byte and increment address");
		puts(" ?: print command help");
		break;

	default:
		printf("ERR unknown command: '%c'\n", input[0]);
	}
}

static void start_prog(void)
{
	/* hold reset low, and take over the RAM */
	PORTC &= ~PC_SYSRST;
	PORTC |= PC_PGM;

	progmode = 1;
}

static void end_prog(void)
{
	/* PGM low and return reset high */
	PORTC &= ~PC_PGM;
	PORTC |= PC_SYSRST;

	progmode = 0;
}

static void write_ram(uint16_t addr, uint8_t val)
{
	set_addr_bus(addr);
	set_data_bus(val);
	PORTC |= PC_OE;
	PORTC &= ~(PC_WE | PC_CS);
	iodelay();
	PORTC |= PC_CS;
	PORTC |= PC_WE;
}

static uint8_t read_ram(uint16_t addr)
{
	uint8_t val;
	set_addr_bus(addr);
	PORTC |= PC_WE;
	PORTC &= ~(PC_CS | PC_OE);
	iodelay();
	val = (PORTD & 0xfe) | (PORTB & 3);
	PORTC |= PC_CS | PC_OE;
	return val;
}

static void set_data_bus(unsigned char val)
{
	PORTB = (PORTB & 0xfe) | (val & 3);
	PORTC = val;
}

static void set_addr_bus(uint16_t addr)
{
	PORTB &= ~PB_SS;
	SPDR = (uint8_t)addr;
	while(!(SPSR & (1 << SPIF)));
	SPDR = (uint8_t)(addr >> 8);
	while(!(SPSR & (1 << SPIF)));

	PORTB |= PB_SS;
	iodelay();
	PORTB &= ~PB_SS;	/* SS must return low because the shiftreg OE is tied to it */
}

static void sys_reset(void)
{
	PORTC &= ~PC_SYSRST;
	iodelay();
	PORTC |= PC_SYSRST;
}

static inline void iodelay(void)
{
	asm volatile("nop");
	asm volatile("nop");
	asm volatile("nop");
}
