#ifndef	encoder_h
#define	encoder_h
#include <avr/io.h>
#include "conf.h"
//_________________________________________

//______________________
#define RIGHT_SPIN 0x01 
#define LEFT_SPIN 0xff

void ENC_InitEncoder(void);
unsigned char ENC_PollEncoder(void);
unsigned char ENC_PollEncoderT(void);
#endif  //encoder_h
