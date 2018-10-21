/*
 * .h
 *
 * Created: 21.10.2018 22:52
 *  Author: Serhg
 */ 
#ifndef TIMERONE_h
#define TIMERONE_h

#include <avr/io.h>
#include <avr/interrupt.h>
#include "conf.h"

#define RESOLUTION 65536


void initialize(long microseconds);
void stop();
void resume();
unsigned long read();
void attachOverInterrupt(void (*isr)());
void attachCompareInterrupt(void (*isr)());
void detachInterrupt();
void setPeriod(long microseconds);
void setDuty(int duty);


#endif