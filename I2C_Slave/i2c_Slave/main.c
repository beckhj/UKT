
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

// Note: The LSB is the I2C r/w flag and must not be used for addressing!
#define 	SLAVE_ADDR_ATTINY       0x34
//#define 	SLAVE_ADDR_ATTINY       0b00110100

#define SDA	(1 << PORTA6)
#define SCL	(1 << PORTA4)

#define Light		(1 << PORTA0)
#define Encoder_2	(1 << PORTA1)
#define Encoder_1	(1 << PORTA2)
#define Encoder_T	(1 << PORTA3)
#define Key_1		(1 << PORTA5)
#define Key_2		(1 << PORTA7)

#define Encoder_T_pressed	0x01
#define Key_1_pressed		0x02
#define Key_2_pressed		0x04


#define PHASE_A     (PINA & Encoder_1)
#define PHASE_B     (PINA & Encoder_2)
int8_t new, diff;
volatile int8_t enc_delta;          // -128 ... 127
static int8_t last;

extern volatile bool is_transmitted;
uint8_t keys, old_keys,last_found_keys,send_keys,debounce_counter;
#define Debounce_Count 5
uint16_t AnalogRefresh;
#define AnalogRefreshCount 2000
uint16_t AnalogResult;
#define AnalogArrayLength 10
uint16_t AnalogArray[AnalogArrayLength];
uint8_t Analogptr;
int8_t enc_count;	
uint8_t restartflag=0xaa;

uint8_t c, d;
uint16_t swap;


#define Increment 0x01
#define Decrement 0xff
#define Nothing 0x00	
	


//####################################################################### Macros

#define uniq(LOW,HEIGHT)	((HEIGHT << 8)|LOW)			  // Create 16 bit number from two bytes
#define LOW_BYTE(x)        	(x & 0xff)					    // Get low byte from 16 bit number  
#define HIGH_BYTE(x)       	((x >> 8) & 0xff)			  // Get high byte from 16 bit number

#define sbi(ADDRESS,BIT) 	((ADDRESS) |= (1<<(BIT)))	// Set bit
#define cbi(ADDRESS,BIT) 	((ADDRESS) &= ~(1<<(BIT)))// Clear bit
#define	toggle(ADDRESS,BIT)	((ADDRESS) ^= (1<<BIT))	// Toggle bit

#define	bis(ADDRESS,BIT)	(ADDRESS & (1<<BIT))		  // Is bit set?
#define	bic(ADDRESS,BIT)	(!(ADDRESS & (1<<BIT)))		// Is bit clear?

//#################################################################### Variables

	uint8_t		byte1, byte2;
	uint16_t	buffer;
	uint8_t		high,low = 0;	// Variables used with uniq (high and low byte)


//################################################################# Main routine

void encode_set( unsigned char set )
{
	enc_delta = set;
}

int8_t encode_read1( void )         // read single step encoders
{
	int8_t val;
	
	cli();
	val = enc_delta;
	enc_delta = 0;
	sei();
	return val;                   // counts since last call
}
int8_t encode_read2( void )         // read two step encoders
{
	int8_t val;
	
	cli();
	val = enc_delta;
	enc_delta = val & 1;
	sei();
	return val >> 1;
}

    
int8_t encode_read4( void )         // read four step encoders
{
	int8_t val;
	
	cli();
	val = enc_delta;
	enc_delta = val & 3;
	sei();
	return val >> 2;
}

void encode_init( void )
{
	int8_t new;
	
	new = 0;
	if( PHASE_A )
	new = 3;
	if( PHASE_B )
	new ^= 1;                   // convert gray to binary
	last = new;                   // power on state
	enc_delta = 0;
}

/* ADC initialisieren */
void ADC_Init(void)
{
  // die Versorgungsspannung AVcc als Referenz wählen:
  //ADMUX |= ~(1<<REFS0);    //Versorgungsspannung als Referenz
  ADMUX = 0;    //Versorgungsspannung als Referenz
 
  // Bit ADFR ("free running") in ADCSRA steht beim Einschalten
  // schon auf 0, also single conversion
  ADCSRA = (1<<ADPS1) | (1<<ADPS0);     // Frequenzvorteiler
  ADCSRA |= (1<<ADEN);                  // ADC aktivieren
 
  /* nach Aktivieren des ADC wird ein "Dummy-Readout" empfohlen, man liest
     also einen Wert und verwirft diesen, um den ADC "warmlaufen zu lassen" */
 
  ADCSRA |= (1<<ADSC);                  // eine ADC-Wandlung 
  while (ADCSRA & (1<<ADSC) ) {         // auf Abschluss der Konvertierung warten
  }
  /* ADCW muss einmal gelesen werden, sonst wird Ergebnis der nächsten
     Wandlung nicht übernommen. */
  (void) ADCW;
}
 
/* ADC Einzelmessung */
uint16_t ADC_Read( uint8_t channel )
{
  // Kanal waehlen, ohne andere Bits zu beeinflußen
  ADMUX = (ADMUX & ~(0x1F)) | (channel & 0x1F);
  ADCSRA |= (1<<ADSC);            // eine Wandlung "single conversion"
  while (ADCSRA & (1<<ADSC) ) {   // auf Abschluss der Konvertierung warten
  }
  return ADCW;                    // ADC auslesen und zurückgeben
}

void Timer_0_init(void)
{
	// Timer 0 konfigurieren
	TCCR0A = (1<<WGM01);
	TCCR0B = ((0<<CS02) | (1<<CS01) | (0<<CS00)); // Vorteiler 64
	// (8 000 000/64 / 1000 Hz= 125, 1 weniger muss in den Timer
	TCNT0=0;	//Star Zählwert auf 0 setzen
	OCR0A=124;	// 1ms bei Vorteiler 64
	TIFR0= (1 << OCF0A);
	TIMSK0 |= ((1<<OCIE0A) ); // Compare Interrupt erlauben
}

ISR ( TIM0_COMPA_vect )
{
	new = 0;
	if( PHASE_A )
	new = 3;
	if( PHASE_B )
	new ^= 1;                   // convert gray to binary
	diff = last - new;                // difference last - new
	if( diff & 1 )
	{               // bit 0 = value (1)
		last = new;                 // store new as next last
		enc_delta += (diff & 2) - 1;        // bit 1 = direction (+/-)
	}
	 	
	int8_t result=encode_read4();
	if (result != Nothing)
	{
		if (result == Increment)
		enc_count++;
		else
		enc_count--;
	}
}

void DataTransmitted(void)
{
	enc_count=0;
	send_keys=0;
	restartflag=0x55;
	is_transmitted=false;
	txbuffer[0]=0;
	wdt_reset();
}

int main(void)
{	 
  
	cli();  // Disable interrupts
	
	usiTwiSlaveInit(SLAVE_ADDR_ATTINY);	// TWI slave init
	ADC_Init();
	Timer_0_init();
	encode_init();
	keys=0;
	old_keys=0;
	last_found_keys=0;
	send_keys=0;
	debounce_counter=0;
	enc_count=0;	
	restartflag=0xaa;
	for (uint8_t i=0; i < AnalogArrayLength; i++)
		AnalogArray[i]=0;
	AnalogRefresh=0;
	Analogptr=0;
	PORTA |= (Encoder_1 | Encoder_2 | Encoder_T | Key_1 | Key_2);		//Pullup Ein
	DDRB |=  (1 << PORTB0) ;// B0 als Ausgang
	wdt_enable(WDTO_4S);
	sei();  // Re-enable interrupts



while(1)
    {
	//############################################ Read data from reception buffer
	sbi(PORTB,PORTB0);
	//if (is_transmitted == true)
	//{
		//enc_count=0;
		//send_keys=0;
		//is_transmitted=false;
	//}
	volatile static uint8_t help;
	//Tasten
	keys=0;
	help=PINA;
	if (!(PINA & Encoder_T))
		keys |= Encoder_T_pressed;
	if (!(PINA & Key_1))
		keys |= Key_1_pressed;
	if (!(help & Key_2))
		keys |= Key_2_pressed;
	

	if ((keys ^ last_found_keys) > 0)
	{
		if (keys == old_keys)
		{
			debounce_counter++;
			if (debounce_counter > Debounce_Count)
			{
				//stabil, jetzt nur positive Wechsel auswerten
				send_keys|=(keys ^ send_keys)  & keys;
				last_found_keys = keys;				
			}
		}
		else
		{
			old_keys=keys;
			debounce_counter=0;
		}		
	}

	txbuffer[0]=send_keys;
		    
	//Encoder
	//Wird im Interrupt verarbeitet
	
	txbuffer[1]=enc_count;
	
	//Helligkeitswert
	AnalogArray[Analogptr++]=ADC_Read(0);
		if (Analogptr > (AnalogArrayLength -1))
		Analogptr=0;		
	AnalogResult=0;
	for (uint8_t i=0;i < AnalogArrayLength; i++)
		AnalogResult += AnalogArray[i];
	AnalogResult  /= AnalogArrayLength;
	txbuffer[2]=(uint8_t) (AnalogResult >> 8) & 0xff;
	txbuffer[3]=(uint8_t) AnalogResult & 0xff;
	txbuffer[4]=restartflag;
	txbuffer[5]=txbuffer[0] + txbuffer[1] + txbuffer[2] + txbuffer[3] + txbuffer[4];
	cbi(PORTB,PORTB0);
	
	
	_delay_ms(1);
	
	} //end.while
} //end.main





