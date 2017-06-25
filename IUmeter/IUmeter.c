/*
 * PlasmaController.cpp
 *
 * Created: 18.06.2017 18:03:13
 *  Author: Serhg
 */ 

#include "conf.h"
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdbool.h>
#include "lcd-library.h"
#include "encoder.h"
#include "millis.h"

uint32_t timer_time;
uint32_t lostcycles = 0;

uint16_t voltage = 0;
uint16_t current = 0;
uint32_t summary_ah = 0;
uint32_t set_ah;

uint32_t enc_butt_press_time;
uint32_t enc_butt_press_time2;
uint32_t ah_calc_last_time = 0;

bool ah_alert_output = 0;
bool start_output = 0;
bool tim_led = 0;
bool timer_ena = 0;
uint8_t timer_state = 0;

uint16_t enc_butt_resolution = 1;
uint32_t ah_butt_resolution = 1;
bool enc_butt_resolution_change = 0;
uint8_t ah_butt_resolution_change = 0;
uint16_t abr_state;

uint8_t display_mutex;
uint8_t display_changed;

EMPTY_INTERRUPT (ADC_vect);

void init()
{
	wdt_enable(WDTO_500MS);
	wdt_reset();
	_delay_ms(50);
	lcdInit();
	wdt_reset();
	timer_time=eeprom_read_word(0) << 16;
	timer_time+=eeprom_read_word(3);
	uint8_t buff = eeprom_read_byte(6);
	if (buff == 1)
		timer_ena = 1;
	else eeprom_write_byte(6, (uint8_t)(1));
	set_ah = eeprom_read_byte(7)<< 16;
	set_ah += eeprom_read_byte(10);
	if ((timer_time>MAX_TIMER_TIME)||(timer_time<MIN_TIMER_TIME))
	{
		timer_time = 10;
		eeprom_write_word(0, (uint16_t)(0));
		eeprom_write_word(3, (uint16_t)(10));
	}
	if ((set_ah>999999))
	{
		set_ah = 50;
		eeprom_write_word(7, (uint16_t)(0));
		eeprom_write_word(10, (uint16_t)(500));
	}

	millis_init();
	millis_resume();

	//выход START
	SetBit(START_OUT_DDR, START_OUT);
	//нормально запитан
	SetBit(START_OUT_PORT, START_OUT);
	//диод активности
	SetBit(ACT_LED_DDR, ACT_LED);
	SetBit(ACT_LED_PORT, ACT_LED);
	//диод счетчика AH
	SetBit(AH_ALERT_DDR, ACT_LED);
	SetBit(AH_ALERT_PORT, ACT_LED);
	//диод таймера
	SetBit(TIM_LED_DDR, TIM_LED);
	SetBit(TIM_LED_PORT, TIM_LED);
	
	//вход INT0 - кнопка старт
	ClearBit(START_BUTT_DDR, START_BUTT);
	SetBit(START_BUTT_PORT, START_BUTT);
	//вход INT1 - кнопка энкодера
	ClearBit(TIMER_BUTT_DDR, TIMER_BUTT);
	SetBit(TIMER_BUTT_PORT, TIMER_BUTT);
	//вход INT1 - кнопка энкодера AH
	ClearBit(AH_BUTT_DDR, AH_BUTT);
	SetBit(AH_BUTT_PORT, AH_BUTT);

	ENC_InitEncoder();

	//customSumbolLoad();
	
	display_mutex = 0;
	display_changed = 3;
	
	set_sleep_mode( SLEEP_MODE_ADC );
	sleep_enable();

	sei();
}

void displayRefrash()
{
	if (display_changed)
	if (!display_mutex)
	{
		char stringOne[17];
		char stringTwo[17];
		uint32_t time;
		if (timer_state>0)
			time = timer_time - millis()/1000;
		else
			time = timer_time;
		uint32_t hours = (time/3600);
		time = time - hours*3600;
		uint16_t min = (time/60);
		uint8_t sec = time - min*60;
		uint32_t summary_tr;
		if (ah_butt_resolution_change>=1)
			summary_tr = set_ah;
		else
			summary_tr = summary_ah;
		//мигание элементов времени таймера
		sprintf(stringOne, "I=%03uA U=%02uV A*h", current, voltage);
		if ((enc_butt_resolution_change)&&(enc_butt_resolution == 1))
			sprintf(stringTwo, "%02lu:%02u:    %06lu", hours, min, summary_tr);
		else if ((enc_butt_resolution_change)&&(enc_butt_resolution == 60))
			sprintf(stringTwo, "%02lu:  :%02d  %06lu", hours, sec, summary_tr);
		else if ((enc_butt_resolution_change)&&(enc_butt_resolution == 3600))
			sprintf(stringTwo, "  :%02u:%02d  %06lu", min, sec, summary_tr);
		else 
			sprintf(stringTwo, "%02lu:%02u:%02d  %06lu", hours, min, sec, summary_tr);
		if (ah_butt_resolution_change>0)
		{
			stringTwo[9] = '>';
			//мигание цифрой в числе установленных ампер часов
			if (ah_butt_resolution_change==2)
			switch (ah_butt_resolution)
			{
				case 1:
					stringTwo[15] = ' ';
					break;
				case 10:
					stringTwo[14] = ' ';
					break;
				case 100:
					stringTwo[13] = ' ';
					break;
				case 1000:
					stringTwo[12] = ' ';
					break;
				case 10000:
					stringTwo[11] = ' ';
					break;
				case 100000:
					stringTwo[10] = ' ';
			}
		}
		stringOne[16]=0;
		stringTwo[16]=0;
		if (!lcdGotoXY(0,0))
		{
			lcdInit();
			//customSumbolLoad();
			display_changed = 1;
		}
		lcdPuts(stringOne);
		if (!lcdGotoXY(1,0))
		{
			lcdInit();
			//customSumbolLoad();
			display_changed = 1;
		}
		lcdPuts(stringTwo);
		display_changed=0;
	}
}

void ChangeTimeResolution()
{
	if (timer_state>0) return;
	if (enc_butt_resolution == 3600)
		enc_butt_resolution = 1;
	else enc_butt_resolution = enc_butt_resolution*60;
	enc_butt_resolution_change = 1;
	display_changed|=128;
}

void ChangeAhResolution()
{
	if (ah_butt_resolution == 100000)
	ah_butt_resolution = 1;
	else ah_butt_resolution = ah_butt_resolution*10;
	ah_butt_resolution_change = 2;
	abr_state = 0;
	display_changed|=128;
}

void accountForADC() {
	lostcycles += ADC_CYCLES;
	while (lostcycles >= F_CPU/1000) 
	{ 
		millis_add(1);
		lostcycles -= F_CPU/1000;
	}
}

int analogRead(int An_pin)
{
	ADMUX=An_pin;   
	ADCSRA=B11000100;	//B11000111-125kHz B11000110-250kHz
	ADCSRA |= (1 << ( ADIE ));
	//_delay_us(10);	                                                                      
	
	do sleep_cpu();
	while( ( (ADCSRA & (1<<ADSC)) != 0 ) );
	ADCSRA &= ~ (1 << ( ADIE ));
	An_pin = ADCL;
	int An = ADCH; 
	accountForADC();
	return (An<<8) + An_pin;
}

void periodicProcess()
{
	static uint16_t led_state;
	static uint16_t ah_led_state;
	static uint16_t meter_state;
	static uint16_t ebr_state;
	static uint16_t disp_state = 0;
	
	//мигнем выбранным разрешением изменения времени таймера
	if (enc_butt_resolution_change==0)
		ebr_state = 0;
	else
	{
		ebr_state++;
		if (ebr_state >= RESOLUTION_BLINK_DURATION)
		{
			enc_butt_resolution_change = 0;
			ebr_state = 0;
			display_changed|=128;
		}
	}
	
	//покажем изменяемое время ампер часов
	if (ah_butt_resolution_change==0)
		abr_state = 0;	
	else
	{
		abr_state++;
		if ((abr_state >= RESOLUTION_BLINK_DURATION)&&(abr_state < SET_AH_DISPLAY_DURATION))
		{
			ah_butt_resolution_change = 1;
			display_changed|=128;
		}
		else if (abr_state >= SET_AH_DISPLAY_DURATION)
		{
			ah_butt_resolution_change = 0;
			abr_state = 0;
			display_changed|=128;
		}
	}
	
	if (start_output==0)
		meter_state = 0;
	//считаем ампер часы
	else
	{
		meter_state++;
		if (meter_state >= AH_CALCULATE_DIV)
		{
			static double diff = 0;
			diff += (((double)(millis()-ah_calc_last_time))/3600000)*current;
			if (diff>0)
			{
				uint32_t udiff = diff;
				summary_ah += udiff;
				diff -= udiff;
			}
			ah_calc_last_time = millis();
			meter_state = 0;
			display_changed|=64;
		}
	}
	
	if (timer_state==0)
		led_state = 0;
	//если диоду таймера надо мигать - мигаем
	else 
	{
		led_state ++;
		if (((led_state >= TIMER_LED_FASTBLINK_DURATION)&&(timer_state==2))||
		((led_state >= TIMER_LED_BLINK_DURATION)&&(timer_state==1)))
		{
			led_state = 0;
			if (tim_led)
				tim_led = 0;
			else
				tim_led = 1;
		}
	}
	
	if ((set_ah!=0)&&(start_output))
	//если диоду оповещения о превышении ампер часов надо мигать - мигаем
	{
		if (set_ah <= summary_ah)
		{
			ah_led_state ++;
			if (ah_led_state >= TIMER_LED_FASTBLINK_DURATION)
			{
				ah_led_state = 0;
				if (ah_alert_output)
					ah_alert_output = 0;
				else
					ah_alert_output = 1;
			}
		}
	}
	
	//периодически обновляем содержимое дисплея
	disp_state++;
	if (disp_state == DISPLAY_REFRASH_DURATION)
	{
		disp_state = 0;
		displayRefrash();
	}
}

void encoderProcess()
{		
	//обработка вращения энкодера установки ампер часов
	int rotation = ENC_PollEncoderT();
	if (rotation==RIGHT_SPIN)
	if (set_ah>0)
	{
		if (set_ah<ah_butt_resolution) set_ah = 0;
		else set_ah-=ah_butt_resolution;
		display_changed|=2;
		ah_butt_resolution_change = 1;
		abr_state = 0;
		eeprom_write_word(7, (uint16_t)(set_ah & 0xFFFF));
		eeprom_write_word(10, (uint16_t)(set_ah & 0xFFFF0000 >> 16));
	}
	if (rotation==LEFT_SPIN)
	if (set_ah<999999)
	{
		if (set_ah+ah_butt_resolution>999999) return;
		set_ah+=ah_butt_resolution;
		display_changed|=2;
		ah_butt_resolution_change = 1;
		abr_state = 0;
		eeprom_write_word(7, (uint16_t)(set_ah & 0xFFFF));
		eeprom_write_word(10, (uint16_t)(set_ah & 0xFFFF0000 >> 16));
	}
	// энкодер установки времени таймера
	if (timer_state>0) return;
	rotation = ENC_PollEncoder();
	if (rotation==RIGHT_SPIN)
	if (timer_time>MIN_TIMER_TIME) 
	{
		if (timer_time-MIN_TIMER_TIME<enc_butt_resolution) return;
		timer_time = timer_time - enc_butt_resolution;
		display_changed|=2;
		eeprom_write_word(0, (uint16_t)(timer_time & 0xFFFF));
		eeprom_write_word(3, (uint16_t)(timer_time & 0xFFFF0000 >> 16));
	}
	if (rotation==LEFT_SPIN)
	if (timer_time<MAX_TIMER_TIME) 
	{
		if (timer_time+enc_butt_resolution>MAX_TIMER_TIME) return;
		timer_time = timer_time + enc_butt_resolution;
		eeprom_write_word(0, (uint16_t)(timer_time & 0xFFFF));
		eeprom_write_word(3, (uint16_t)(timer_time & 0xFFFF0000 >> 16));
		display_changed|=2;
	}
}

void logicProcess()
{
	//переход на быстрое мигание после того, как остается менее 30 сек
	if ((timer_state==1)&&(millis()/1000>=timer_time-FAST_BLINK_TIMER_REMAIND))
		timer_state = 2;
	//если диоду таймера не надо мигать - управляем им отсюда
	if ((timer_state==0)&&(timer_ena==1))
		tim_led = 1;
	if (timer_ena==0) tim_led = 0;
	//выключаем источник после таймаута
	if ((timer_state>0)&&(millis()/1000>=timer_time))
	{
		timer_state = 0;
		start_output = 0;
		display_changed|=8;
	}
	//если таймер включен - обновляем каждую секунду
	if (timer_state>0) display_changed|=16;
}

void setStates()
{
	if (start_output) 
	{
		ClearBit(START_OUT_PORT, START_OUT);
		SetBit(ACT_LED_PORT, ACT_LED);
	}
	else
	{
		SetBit(START_OUT_PORT, START_OUT);
		ClearBit(ACT_LED_PORT, ACT_LED);
	}
	if (ah_alert_output) SetBit(AH_ALERT_PORT, AH_ALERT);
	else ClearBit(AH_ALERT_PORT, AH_ALERT);
	if (tim_led) SetBit(TIM_LED_PORT, TIM_LED);	
	else ClearBit(TIM_LED_PORT, TIM_LED);	
}

void readStates()
{
	static uint16_t samples[I_SAMPLES_COUNT];
	static uint8_t iter = 0;
	static uint16_t adc_state = 0;
	adc_state++;
	if (adc_state == ADC_REFRASH_DURATION)
	{
		if (iter>=I_SAMPLES_COUNT)
		{
			uint32_t new_I = 0;
			for (int s=0; s < I_SAMPLES_COUNT; s++ )
			new_I += samples[s];
			new_I = new_I/I_SAMPLES_COUNT;
			if (current - new_I>AMPER_DIFF_TO_REFRASH)
			{
				current = new_I;
				display_changed|=16;
			}
			iter = 0;
		}
		else
		{
			// Меряем на пине PC1 относительного внутреннего ИОН-а 2.56
			samples[iter] = analogRead(193)*4.296875;
			iter++;
		}
		
		//Uвых =  UвхR2/(R1 + R2)
		//U=(опорное напряжение*значение АЦП*коэффициент делителя)/число разрядов АЦП
		// Меряем на пине РС0 относительно напряжения питания
		// делитель - 2.2кОм/8.2кОм
		uint16_t new_V = (25*analogRead(64))/1024;
		if (voltage - new_V>VOLTAGE_DIFF_TO_REFRASH)
		{
			voltage = new_V;
			display_changed|=16;
		}
		
		adc_state = 0;
	}

	static bool start_state = false;
	if (millis()<START_IGNORE_TIME)
	return;
	if ((START_BUTT_PIN & (1 << START_BUTT))&&(start_state))
	{
		start_state = false;
	}
	else if ((!(START_BUTT_PIN & (1 << START_BUTT)))&&(!start_state))
	{
		start_state = true;
		millis_reset();
		if (start_output)
		{
			start_output = 0;
			timer_state = 0;
			ah_calc_last_time = 0;
		}
		else
		{
			start_output = 1;
			if (timer_ena)
				timer_state = 1;
		}
		display_changed|=4;
	}

	//// Меряем на пине PC1 относительного внутреннего ИОН-а 2.56
	//uint32_t new_I = analogRead(193)*4.296875;
	//if (current - new_I>AMPER_DIFF_TO_REFRASH)
	//{
		//current = new_I;
		//display_changed|=16;
	//}
	
	////Uвых =  UвхR2/(R1 + R2)
	////U=(опорное напряжение*значение АЦП*коэффициент делителя)/число разрядов АЦП
	//// Меряем на пине РС0 относительно напряжения питания
	//// делитель - 2.2кОм/8.2кОм
	//uint16_t new_V = (25*analogRead(64))/1024;
	//if (voltage - new_V>VOLTAGE_DIFF_TO_REFRASH)
	//{
		//voltage = new_V;
		//display_changed|=16;
	//}
}
	
void encoderButtStates()
{	
	static bool enc_state = false;
	static bool press_flag = false;
	if ((millis()-enc_butt_press_time>LONG_PRESS_TIME)&&(press_flag))
	{
		press_flag = false;
		if (timer_ena)
		{
			timer_ena = 0;
			timer_state = 0;
		}
		else
		{
			timer_ena = 1;
			//если включить таймер при включенном источнике - начинаем сразу считать
			if (start_output)
			{
				millis_reset();
				timer_state = 1;
			}
		}
		eeprom_write_byte(6, timer_ena);
	}
	if ((TIMER_BUTT_PIN & (1 << TIMER_BUTT))&&(enc_state))
	{
		if ((millis()-enc_butt_press_time>PRESS_TIME)&&(millis()-enc_butt_press_time<LONG_PRESS_TIME))
			ChangeTimeResolution();
		enc_state = false;
		press_flag = false;
	}
	else if ((!(TIMER_BUTT_PIN & (1 << TIMER_BUTT)))&&(!enc_state))
	{
		enc_butt_press_time = millis();
		enc_state = true;
		press_flag = true;
	}
	
	static bool enc_state2 = false;
	static bool press_flag2 = false;
	if ((millis()-enc_butt_press_time2>LONG_PRESS_TIME)&&(press_flag2))
	{
		press_flag2 = false;
		summary_ah = 0;
	}
	if ((AH_BUTT_PIN & (1 << AH_BUTT))&&(enc_state2))
	{
		if ((millis()-enc_butt_press_time2>PRESS_TIME)&&(millis()-enc_butt_press_time2<LONG_PRESS_TIME))
			ChangeAhResolution();
		enc_state2 = false;
		press_flag2 = false;
	}
	else if ((!(AH_BUTT_PIN & (1 << AH_BUTT)))&&(!enc_state2))
	{
		enc_butt_press_time2 = millis();
		enc_state2 = true;
		press_flag2 = true;
	}
}

int main(void)
{
	init();
	
	while(1)
	{
		wdt_reset();
		encoderProcess();
		encoderButtStates();
		periodicProcess();
		readStates();
		logicProcess();
		setStates();
	}
}