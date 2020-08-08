/*
 * WasabiTeeTimer160702_ohne_Div.c
 *
 * Created: 02.07.2016 23:47:28
 *
 * LED 7-Segment: SC56 -11SRWA(gem. Kathode) 1...E; 2...D; 3,8...GND; 4...C; 5...DP; 6...B; 7...A; 9...F; 10...G
 * ATMEGA32: 11,32...GND; 10...Vcc; 30...AVcc; 32...Aref; 12,13...XTAL
 * 2N7000: N-CH-MOSFET (BeenezumirSchriftnachunten) D-G-S
 *
 * Author : 25mmHg
 */

#define F_CPU 8000000UL
#define MAX_S 599U
#define SEKUNDEN_EINER  (1<<PA7) // 2N7000 Gate
#define SEKUNDEN_ZEHNER (1<<PA6) // 2N7000 Gate
#define MINUTEN_EINER   (1<<PA5) // 2N7000 Gate
#define ENTPRELLZEIT 10
#define SEGMENT_ANODEN 0xFF // 2do Dezimalpunkt beachten??
#define SEGMENT_KATHODEN (SEKUNDEN_EINER | SEKUNDEN_ZEHNER | MINUTEN_EINER)
#define TASTER (1<<PB2) // INT2
#define SERVO  (1<<PD4) // OC1B=PD4
#define SOUND  (1<<PD5) // unused OC1A=PD5
#define TEST   (1<<PA4) // for debug and TEST
#define TIMER0_TOP_VALUE 124   //   ((uint8_t) (F_CPU /64)* (1/1000)-1)   //124 (Timerclock * Zeit)-1
#define TIMER1_TOP_VALUE 19999 //   ((uint16_t)(F_CPU / 1)*(20/1000)-1)   //19999 (dito)
#define TIMER1_PULSE_SERVO_DOWN 1000 // ((uint16_t)(F_CPU /1)*(1/1000))   //1000
#define TIMER1_PULSE_SERVO_UP   2000 // ((uint16_t)(F_CPU /1)*(2/1000))   //2000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

volatile      uint8_t flagINT2 = 0;
volatile      uint8_t run = 0;     // 2do soll dann wieder 0 sein
volatile      uint8_t katode = 0;  // Timer0 compare match Segment_Zähler
volatile      uint16_t run_ms = 0; // Timer0 compare match ms_Zähler
volatile      uint16_t run_s = 0;  // Timer0 compare match s_Zähler

uint16_t presetwert = 60;
const uint8_t anoden[]={63,6,91,79,102,109,125,7,127,111,128}; //Codierung der 7-Segment-Anzeigen PC0(SEG_A) bis PC6(SEG_G)  auch enthalten PC7(DP)

void show(uint16_t);
void runservo(void);
void teetime(void);

ISR(INT2_vect)
{
	GICR &= ~(1<<INT2);     // Externer Interrupt (Taster) blockiert, wird in Hauptschleife nach Entprellzeit wieder freigegeben
	flagINT2 = 1;
	run++;
	if (run > 2) run = 0;   // 0...Preset, 1...Countdown, 2...Tee ist fertig
}

ISR (TIMER0_COMP_vect)
{
	katode++;
	if (katode > 3) katode = 0;
	if ((run == 1) && (run_s < MAX_S))
	{
		run_ms++;
		if (run_ms > 999)
		{
			run_ms = 0;
			run_s++;
		}
	}
}

void show(uint16_t s2show)
{
    static uint16_t oldtemp;
	static uint8_t min, s10er, s1er;
	uint16_t temp = s2show;
	if (temp != oldtemp)
	{
		PORTA |= TEST;
		min = 0;
		s10er = 0;
		s1er = 0;
		while(temp >= 60)
		{
			min++;
			temp -= 60;
		}
		while(temp >= 10)
		{
			s10er++;
			temp -= 10;
		}
		while(temp > 0)
		{
			s1er++;
			temp -= 1;
		}
		oldtemp = s2show;
		PORTA &= ~TEST;
	}
	
	
	if (katode == 0)
	{
		PORTA |= MINUTEN_EINER;
		PORTA &= ~(SEKUNDEN_ZEHNER | SEKUNDEN_EINER);
		if (min < 1)     PORTC = 0;
		// else if (run_ms > 500)  PORTC = anoden[min]; // Gimmik
		else             PORTC = anoden[min] | anoden[10];
	}
	else if (katode == 1)
	{
		PORTA |= SEKUNDEN_ZEHNER;
		PORTA &= ~(MINUTEN_EINER | SEKUNDEN_EINER);
		if (s2show < 10) PORTC = 0;
		else             PORTC = anoden[s10er];
	}
	else if (katode == 2)
	{
		PORTA = SEKUNDEN_EINER;
		PORTA &= ~(SEKUNDEN_ZEHNER | MINUTEN_EINER);
		PORTC = anoden[s1er];
	}
	else
	{
		PORTA &= ~(MINUTEN_EINER | SEKUNDEN_ZEHNER | SEKUNDEN_EINER);
		PORTC = 0;
	}
}

void runservo()
{
	if (run == 2) OCR1B = TIMER1_PULSE_SERVO_DOWN;
	else          OCR1B = TIMER1_PULSE_SERVO_UP;
}

void teetime()
{
	if (run == 2) PORTD |= SOUND;
	else          PORTD &= ~SOUND;
}

int main(void)
{
	cli();
	
	//init PORTS
	DDRD  |= SERVO | SOUND;
	DDRC  |= SEGMENT_ANODEN;
	DDRA  |= SEGMENT_KATHODEN | TEST;
	PORTB |= TASTER;        // Pull-up Taster
	
	//init ADC
	ADMUX  |= (1<<MUX0);    // PA1(Pin 39), S.215 für PA0(Pin40) Zeile auskommentieren
	ADMUX  |= (1<<REFS0);   // S.214 REF ist 2,56V an Aref mit C abblocken
	ADMUX  |= (1<<REFS1);   // dito
	ADCSRA |= (1<<ADEN);    // power ADC, S.202
	//ADCSRA |= (1<<ADPS0);         // S.217 Prescaler
	ADCSRA |= (1<<ADPS1);   // dito
	ADCSRA |= (1<<ADPS2);   // dito
	ADCSRA |= (1<<ADATE);   // S.203 Autotrigger
	ADCSRA |= (1<<ADSC);    // ADC gestartet
	_delay_ms(10);
	(void)ADCW;              // erster ausgelesener Wert nur zur Veranschaulichung
	
	//init Clock
	TCCR0 |= (1<<WGM01);     // CTC Modus
	TCCR0 |= (1<<CS00);      // Prescaler 1 (wenn CS02:00 == 0 Timer gestoppt), 64, 1024
	TCCR0 |= (1<<CS01);      // Prescaler 8, 64
	//TCCR0 |= (1<<CS02);                 // Prescaler 256, 1024
	OCR0 = TIMER0_TOP_VALUE; // alle 1ms ein Compare Event 2do: zur Zeit aller 2ms
	
	//init ServoPWM
	TCCR1A |= (1<<WGM10);   // S.107ff FastPWM Modus, OCR1A = Top
	TCCR1A |= (1<<WGM11);
	TCCR1A |= (1<<COM1B1);  // Clear OCR1B on compare match, set at 0
	TCCR1B |= (1<<WGM12);
	TCCR1B |= (1<<WGM13);
	TCCR1B |= (1<<CS11);    // Prescaler 8
	OCR1A   = TIMER1_TOP_VALUE;
	OCR1B   = TIMER1_PULSE_SERVO_DOWN;
	
	//init Interrupts
	//MCUCSR |= (1<<ISC2);             // steigende Flanke von INT2 löst Interrupt aus
	MCUCSR &= ~(1<<ISC2);   // fallende Flanke von INT2 löst Interrupt aus
	GICR   |= (1<<INT2);    // Externer Interrupt (Taster) scharf
	TIMSK  |= (1<<OCIE0);   // Timer 0 Compare Interrupt scharf
	
	sei();                  // alle Interrupts scharf
	
	while(1)
	{
		if (flagINT2)                                    // Taster soeben gedrückt
		{
			_delay_ms(ENTPRELLZEIT);
			flagINT2 = 0;
			MCUCSR &= ~(1<<ISC2);   // muss man nochmal machen:  fallende Flanke von INT2 löst Interrupt aus
			GICR   |= (1<<INT2);    // Externer Interrupt (Taster) wieder scharf
		}
		
		if (run == 0)
		{
			run_ms = 0;
			run_s = 0;
			presetwert = (30*presetwert + ADCW) >> 5;  // TP-Filter für Poti, auf 9Bit halbiert, Ziehzeit 8min
		}
		
		uint16_t temp = (presetwert > run_s && run != 2) ? presetwert - run_s : 0;
		if (temp == 0 && presetwert != 0) run = 2;
		//PORTA |= TEST; // Debug Zeitmessung Start
		show(temp);
		//PORTA &= ~TEST; //Debug Zeitmessung Ende
		runservo();
		teetime();
	}
}

