
/*##############################################################################

	Name	: USI TWI Slave für den UKT

	autor	: Hans-Joachim Beck, der_ukt@web.de
	autor	: Martin Junghans	jtronics@gmx.de
	page	: www.jtronics.de
	License	: GNU General Public License 

	Created from Atmel source files for Application Note AVR312: 
    Using the USI Module as an I2C slave like a I2C-EEPROM.

  
//############################################################################*/

#ifndef 	F_CPU
#define 	F_CPU 8000000UL
#endif

#include 	<stdlib.h>
#include 	<avr/io.h>
#include 	<avr/interrupt.h>
#include 	<avr/pgmspace.h>
#include	<avr/wdt.h>
#include	<util/delay.h>

//################################################################## USI-TWI-I2C

#include 	"usiTwiSlave.h"    
#include	<avr/wdt.h>
#include	<util/delay.h> 		

// Note: The LSB is the I2C r/w flag and must not be used for addressing!
#define 	SLAVE_ADDR_ATTINY       0x36
//#define 	SLAVE_ADDR_ATTINY       0b00110100


#define TONE_OUT        PB3
#define TestPin         PB4




#define sbi(ADDRESS,BIT) 	((ADDRESS) |= (1<<(BIT)))	// Set bit
#define cbi(ADDRESS,BIT) 	((ADDRESS) &= ~(1<<(BIT)))// Clear bit
#define	toggle(ADDRESS,BIT)	((ADDRESS) ^= (1<<BIT))	// Toggle bit

#define	bis(ADDRESS,BIT)	(ADDRESS & (1<<BIT))		  // Is bit set?
#define	bic(ADDRESS,BIT)	(!(ADDRESS & (1<<BIT)))		// Is bit clear?

int count=0;
volatile uint8_t freq=255;
extern volatile bool is_transmitted;
extern volatile bool is_received;

uint8_t tone_on;
uint8_t tone_count;
volatile uint8_t mytimer;

void Timer_0_init(void)
{
  // Timer 0 konfigurieren
  TCCR0A = (1<<WGM01);
  TCCR0B = ((1<<CS01) | (1<<CS00));// | (1<<CS00)); // Teiler r64
  TCCR0B = (1<<CS01);// | (1<<CS00)); // Teiler r64
  TCNT0=0;	//Star Zählwert auf 0 setzen
  OCR0A=freq;	//Das ist der Wert, auf den der Zähler immer läuft, also der Teiler, hier 2ms
  TIMSK= (1 << OCF0A);
  
}

void Timer_1_init(void)
{
  // Timer 1 konfigurieren

  TCCR1 = ( (1 << CS12) | (1 << CS10) | (1 << CS11));	//  Clear Timer/Counter on Compare Match und PCK/8
  GTCCR=0;
  TCNT1=0;	//Star Zählwert auf 0 setzen
  TIMSK |= ( (1 << TOIE1)  );  
}

void delay_n_100ms(uint8_t n)
{
	uint8_t i;
	for (i= 0; i++ ; i < n)
		_delay_ms(100);
}

ISR ( TIM0_COMPA_vect )
{

	if (tone_on)
		toggle(PORTB,TONE_OUT);
	else
		sbi(PORTB,TONE_OUT);
		
}

ISR ( TIM1_OVF_vect )
{
	TCNT1=100;
	toggle(PORTB,TestPin);
	mytimer++;
	
}


volatile uint8_t i=0;
void set_tone(uint8_t tone)
{
	freq=tone;
	OCR0A=tone;
	tone_on=0xff;
}

void melody_1()
{

	for (uint8_t i=0; i< 3;i++)
	{
	set_tone(40);
	_delay_ms(10);

	//_delay_ms(10);
	tone_on=0;
	_delay_ms(5);
		
	set_tone(40);
	_delay_ms(10);

	tone_on=0;
	_delay_ms(5);


	set_tone(70);
	_delay_ms(15);
	
	tone_on=0;
	_delay_ms(5);
			
	
	}
	tone_on=0;	

}

void melody_3()
{

	for (uint8_t i=0; i< 1;i++)
	{
		set_tone(40);
		_delay_ms(10);

		//_delay_ms(10);
		tone_on=0;
		_delay_ms(5);
		
		set_tone(40);
		_delay_ms(10);

		tone_on=0;
		_delay_ms(5);


		set_tone(70);
		_delay_ms(15);
		
		tone_on=0;
		_delay_ms(5);
		
		set_tone(40);
		_delay_ms(10);		
		
	}
	tone_on=0;

}

void melody_4()
{

	for (uint8_t i=255; i > 100;i--)
	{
		set_tone(i);	
		_delay_ms(1);
		
	}
	tone_on=0;

}

void melody_2()
{

	//1
	set_tone(189);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(150);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(169);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(252);
	_delay_ms(150);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	tone_on=0;
	_delay_ms(20);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	wdt_reset();
	// 2
	set_tone(189);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(169);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(150);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(189);
	_delay_ms(150);
	if (is_received)
	{
		tone_on=0;
		return;
	}	
	tone_on=0;
	_delay_ms(80);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	wdt_reset();
	// 3
	set_tone(150);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(189);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(169);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(252);
	_delay_ms(150);

	if (is_received)
	{
		tone_on=0;
		return;
	}
	tone_on=0;
	_delay_ms(20);

	if (is_received)
		{
		tone_on=0;
		return;
	}
	// 4
	wdt_reset();
	set_tone(252);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(169);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(150);
	_delay_ms(100);
	if (is_received)
	{
		tone_on=0;
		return;
	}
	set_tone(189);
	_delay_ms(150);

	
	tone_on=0;

}

int main(void)
{	 
  
	cli();  // Disable interrupts
	DDRB |= (1 << TONE_OUT);
	sbi(PORTB,TONE_OUT);

	Timer_0_init();
	Timer_1_init();
	
	usiTwiSlaveInit(SLAVE_ADDR_ATTINY);	// TWI slave init

	is_received=false;
	
	wdt_enable(WDTO_4S);

	sei();  // Re-enable interrupts



while(1)
    {

		if (is_received)
		{
			is_received= false;			
			switch (rxbuffer[0])
			{

				case 1:
					melody_3();
					break;
				case 2:
					melody_1();
					break;
				case 3:
					melody_2();					
					break;
				case 4:
					melody_4();					
					break;
				case 10:

					break;
					
			}
			 OCR0A=freq;	//Das ist der Wert, auf den der Zähler immer läuft, also der Teiler, hier 2ms
		is_transmitted=false;
		is_received= false;
		wdt_reset();
		}
	
	
	} //end.while

} //end.main





