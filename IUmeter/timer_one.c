#ifndef TIMERONE_cpp
#define TIMERONE_cpp

#include "timer_one.h"


unsigned int pwmPeriod;
unsigned char clockSelectBits;
char oldSREG;       
void (*isrOverCallback)();
void (*isrCompareCallback)();


ISR(TIMER1_OVF_vect)          // interrupt service routine that wraps a user defined function supplied by attachInterrupt
{
  isrOverCallback();
}

ISR(TIMER1_COMPA_vect)          // interrupt service routine that wraps a user defined function supplied by attachInterrupt
{
	isrCompareCallback();
}

void initialize(long microseconds)
{
  TCCR1A = 0;                 // clear control register A 
  TCCR1B = (1<<WGM13);        // set mode 8: phase and frequency correct pwm, stop the timer
  setPeriod(microseconds);
}


void setPeriod(long microseconds)		// AR modified for atomic access
{
  
  long cycles = (F_CPU / 2000000) * microseconds;                                // the counter runs backwards after TOP, interrupt is at BOTTOM so divide microseconds by 2
  if(cycles < RESOLUTION)              clockSelectBits = (1<<CS10);              // no prescale, full xtal
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = (1<<CS11);              // prescale by /8
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = (1<<CS11) | (1<<CS10);  // prescale by /64
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = (1<<CS12);              // prescale by /256
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = (1<<CS12) | (1<<CS10);  // prescale by /1024
  else        cycles = RESOLUTION - 1, clockSelectBits = (1<<CS12) | (1<<CS10);  // request was out of bounds, set as maximum
  
  oldSREG = SREG;				
  cli();							// Disable interrupts for 16 bit register access
  ICR1 = pwmPeriod = cycles;                                          // ICR1 is TOP in p & f correct pwm mode
  SREG = oldSREG;
  
  TCCR1B &= ~((1<<CS10) | (1<<CS11) | (1<<CS12));
  TCCR1B |= clockSelectBits;                                          // reset clock select register, and starts the clock
}

void setDuty(int duty)
{
  unsigned long dutyCycle = pwmPeriod;
  
  dutyCycle *= duty;
  dutyCycle >>= 10;
  
  oldSREG = SREG;
  cli();
  OCR1A = dutyCycle;
  SREG = oldSREG;
}

void attachOverInterrupt(void (*isr)())
{
  isrOverCallback = isr;                                       // register the user's callback with the real ISR
  TIMSK1 |= (1<<TOIE1);                                     // sets the timer overflow interrupt enable bit										
}

void attachCompareInterrupt(void (*isr)())
{
	isrCompareCallback = isr;                                       // register the user's callback with the real ISR
	TIMSK1 |= (1<<OCIE1A);    
}

void detachInterrupt()
{
  TIMSK1 &= ~(1<<TOIE1);     
  TIMSK1 &= ~(1<<OCIE1A);                                // clears the timer overflow interrupt enable bit 
															// timer continues to count without calling the isr
}

void resume()				// AR suggested
{ 
  TCCR1B |= clockSelectBits;
}

void stop()
{
  TCCR1B &= ~((1<<CS10) | (1<<CS11) | (1<<CS12));          // clears all clock selects bits
}

#endif