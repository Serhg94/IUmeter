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
#include "timer_one.h"

enum REVERS_PARAMS { 
	NONE = 0, 
	TIMER_TIME = 1, 
	POS_TIME = 2, 
	POS_TIME_MS = 4, 
	NEG_TIME = 8, 
	NEG_TIME_MS = 16,  
	D_TIME = 32, 
	FREQ = 64
};
#define GROUP1_BEG POS_TIME
#define GROUP1_END NEG_TIME_MS
#define GROUP2_BEG D_TIME
#define GROUP2_END FREQ

enum STATES {OFF, POS_PERIOD, POS_DEAD, NEG_PERIOD, NEG_DEAD};

uint32_t timer_time;
uint32_t lostcycles = 0;
uint32_t save_param = 0;
uint8_t changed_params = NONE;

uint16_t voltage = 0;
uint16_t current = 0;
uint32_t summary_ah = 0;
uint32_t set_ah;

uint32_t enc_butt_press_time;
uint32_t enc_butt_press_time2;
uint32_t ah_calc_last_time = 0;

bool revers_enable = false;
volatile bool impuls_enable = false;
uint8_t frequency;
uint8_t d_time_percents;
uint32_t positive_period;
uint32_t negative_period; 

bool ah_alert_output = 0;
bool start_input = 0;
bool start_output = 0;
volatile bool positive_output = 0;
volatile bool negative_output = 0;
bool tim_led = 0;
bool timer_ena = 0;
uint8_t timer_state = 0;

uint16_t enc_butt_resolution = 1;
uint32_t ah_butt_resolution = 1;
bool enc_butt_resolution_change = 0;
uint8_t ah_butt_resolution_change = 0;
uint32_t abr_state;

uint8_t display_mutex;
uint8_t display_changed;
uint8_t current_screen = 1;
uint8_t current_param = NONE;

EMPTY_INTERRUPT (ADC_vect);

void disableOutput()
{
	if (impuls_enable)
	{
		ClearBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
		ClearBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	}
}

void enableOutput()
{
	if (impuls_enable)
	{
		ClearBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
		ClearBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
		if (positive_output) SetBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
		else if (negative_output) SetBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	}
}

void init()
{
	wdt_enable(WDTO_500MS);
	wdt_reset();
	_delay_ms(50);
	lcdInit();
	wdt_reset();
	
	// Читаем параметры работы из энергонезависимой памяти - если там мусор - выставляем значения по умолчанию
	timer_time=eeprom_read_dword(0);
	uint8_t buff = eeprom_read_byte(6);
	if (buff == 1)
		timer_ena = 1;
	else if (buff == 0)
		timer_ena = 0;
	else eeprom_write_byte(6, (uint8_t)(0));
	set_ah = eeprom_read_dword(7);
	if ((timer_time>MAX_TIMER_TIME)||(timer_time<MIN_TIMER_TIME))
	{
		timer_time = 10;
		eeprom_write_dword(0, (uint32_t)(10));
	}
	if ((set_ah>999999))
	{
		set_ah = 50;
		eeprom_write_dword(7, (uint32_t)(500));
	}
	buff = eeprom_read_byte(12);
	if (buff == 1)
		revers_enable = 1;
	else if (buff == 0)
		revers_enable = 0;
	else eeprom_write_byte(12, (uint8_t)(0));
	
	buff = eeprom_read_byte(13);
	if (buff == 1)
		impuls_enable = 1;
	else if (buff == 0)
		impuls_enable = 0;
	else eeprom_write_byte(13, (uint8_t)(0));
	
	frequency=((uint8_t)eeprom_read_word(14));
	if ((frequency>MAX_FREQ)||(frequency<MIN_FREQ))
	{
		frequency = 1;
		eeprom_write_byte(14, (uint8_t)(1));
	}
	
	d_time_percents=eeprom_read_byte(17);
	if ((d_time_percents>MAX_D_TIME)||(d_time_percents<MIN_D_TIME))
	{
		d_time_percents = 50;
		eeprom_write_byte(17, (uint8_t)(50));
	}
	
	positive_period = eeprom_read_dword(19);
	if ((positive_period>MAX_PERIOD)||(positive_period<MIN_PERIOD))
	{
		positive_period = 10;
		eeprom_write_dword(19, (uint32_t)(10));
	}
	
	negative_period = eeprom_read_dword(23);
	if ((negative_period>MAX_PERIOD)||(negative_period<MIN_PERIOD))
	{
		negative_period = 10;
		eeprom_write_dword(23, (uint32_t)(10));
	}


	millis_init();
	millis_resume();
	
	attachCompareInterrupt(disableOutput);
	attachOverInterrupt(enableOutput);
	initialize(1000 / frequency);
	setDuty(d_time_percents*10);

	//выход START
	SetBit(START_OUT_DDR, START_OUT);
	//нормально распитан
	ClearBit(START_OUT_PORT, START_OUT);
	//выход положительной диагонали
	SetBit(POSITIVE_OUT_DDR, POSITIVE_OUT);
	SetBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
	//выход отрицательной диагонали
	SetBit(NEGATIVE_OUT_DDR, NEGATIVE_OUT);
	SetBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	//диод активности
	SetBit(ACT_LED_DDR, ACT_LED);
	SetBit(ACT_LED_PORT, ACT_LED);
	//диод счетчика AH
	SetBit(AH_ALERT_DDR, AH_ALERT);
	SetBit(AH_ALERT_PORT, AH_ALERT);
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
		if (current_screen == 1)
		{
			uint32_t summary_tr;
			if (ah_butt_resolution_change>=1)
				summary_tr = set_ah;
			else
				summary_tr = summary_ah;
			//мигание элементов времени таймера
			sprintf(stringOne, "I=%03uA U=%02uV A*h", current, voltage);
			if (timer_ena){
				uint32_t time;
				if (timer_state>0)
					time = timer_time - millis()/1000;
				else
					time = timer_time;
				// Если время больше того, что может влезть на экран - убираем его)
				if (time > 359999)
					time = 0;
				uint32_t hours = (time/3600);
				time = time - hours*3600;
				uint16_t min = (time/60);
				uint8_t sec = time - min*60;
				if ((enc_butt_resolution_change)&&(enc_butt_resolution == 1))
					sprintf(stringTwo, "%02lu:%02u:    %06lu", hours, min, summary_tr);
				else if ((enc_butt_resolution_change)&&(enc_butt_resolution == 60))
					sprintf(stringTwo, "%02lu:  :%02d  %06lu", hours, sec, summary_tr);
				else if ((enc_butt_resolution_change)&&(enc_butt_resolution == 3600))
					sprintf(stringTwo, "  :%02u:%02d  %06lu", min, sec, summary_tr);
				else
					sprintf(stringTwo, "%02lu:%02u:%02d  %06lu", hours, min, sec, summary_tr);
			}
			else
			{
				sprintf(stringTwo, "--:--:--  %06lu", summary_tr);
			}
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
		}
		else
		{
			uint16_t pos_h = positive_period / 1000;
			uint16_t pos_l = positive_period % 1000;
			uint16_t neg_h = negative_period / 1000;
			uint16_t neg_l = negative_period % 1000;
			sprintf(stringOne, "P%03u%03ums D=%02u%% ", pos_h, pos_l, d_time_percents);
			sprintf(stringTwo, "N%03u%03ums F=%02ukH", neg_h, neg_l, frequency);
			if (enc_butt_resolution_change && current_param)
			switch (current_param){
				case POS_TIME:
				sprintf(stringOne, "P   %03ums D=%02u%% ", pos_l, d_time_percents);
				break;
				case POS_TIME_MS:
				sprintf(stringOne, "P%03u   ms D=%02u%% ", pos_h, d_time_percents);
				break;
				case NEG_TIME:
				sprintf(stringTwo, "N   %03ums F=%02ukH", neg_l, frequency);
				break;
				case NEG_TIME_MS:
				sprintf(stringTwo, "N%03u   ms F=%02ukH", neg_h, frequency);
				break;
				case FREQ:
				sprintf(stringTwo, "N%03u%03ums F=  kH", neg_h, neg_l);
				break;
				case D_TIME:
				sprintf(stringOne, "P%03u%03ums D=  %% ", pos_h, pos_l);
				break;
			}
			if (!revers_enable)
			{
				stringOne[1] = '-';
				stringOne[2] = '-';
				stringOne[3] = '-';
				stringOne[4] = '-';
				stringOne[5] = '-';
				stringOne[6] = '-';
				stringTwo[1] = '-';
				stringTwo[2] = '-';
				stringTwo[3] = '-';
				stringTwo[4] = '-';
				stringTwo[5] = '-';
				stringTwo[6] = '-';
			}
			if (!impuls_enable)
			{
				stringOne[12] = '-';
				stringOne[13] = '-';
				stringTwo[12] = '-';
				stringTwo[13] = '-';
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
	else 
		enc_butt_resolution = enc_butt_resolution*60;
	enc_butt_resolution_change = 1;
	display_changed|=128;
}

void ChangeAhResolution()
{
	if (ah_butt_resolution == 100000)
	ah_butt_resolution = 1;
	else ah_butt_resolution = ah_butt_resolution*10;
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
			ah_butt_resolution = 1;
			abr_state = 0;
			display_changed|=128;
			eeprom_update_dword(7, set_ah);
		}
	}
	
	if (start_input==0)
		meter_state = 0;
	//считаем ампер часы
	else
	{
		meter_state++;
		if (meter_state >= AH_CALCULATE_DIV)
		{
			static double diff = 0;
			diff += (((double)(millis()-ah_calc_last_time))/3600000)*(double)current;
			if (diff>0)
			{
				uint32_t udiff = (uint32_t)diff;
				summary_ah += udiff;
				diff -= (double)udiff;
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
	
	if ((set_ah!=0)&&(start_input))
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
		else
		//тушим диод тревоги, если подняли установленное значение ампер часов
			ah_alert_output = 0;
	}
	//тушим диод тревоги, если источник не запущен
	else 
		ah_alert_output = 0;
	
	//периодически обновляем содержимое дисплея
	disp_state++;
	if (disp_state == DISPLAY_REFRASH_DURATION)
	{
		disp_state = 0;
		displayRefrash();
	}
	
	//при изменении времени - сохраняем время не сразу
	if (save_param > 0)
	{
		save_param++;
		if (save_param == SAVE_PARAMS_DURATION)
		{
			save_param = 0;
			if (changed_params & TIMER_TIME){
				eeprom_update_dword(0, timer_time);
			}
			if ((changed_params & POS_TIME) || (changed_params & POS_TIME_MS)){
				eeprom_write_dword(19, positive_period);
			}
			if ((changed_params & NEG_TIME) || (changed_params & NEG_TIME_MS)){
				eeprom_write_dword(23, negative_period);
			}
			if (changed_params & D_TIME){
				eeprom_update_byte(17, d_time_percents);
			}
			if (changed_params & FREQ){
				eeprom_update_word(14, (uint8_t)(frequency));
			}
			changed_params = NONE;
		}
	}
}

void encoderProcess()             
{		
	//обработка вращения энкодера установки ампер часов
	int rotation = ENC_PollEncoderT();
	if (current_screen == 1){
		if (rotation==ENC_LEFT_SPIN)
		if (set_ah>0)
		{
			if (set_ah<ah_butt_resolution) set_ah = 0;
			else set_ah-=ah_butt_resolution;
			display_changed|=2;
			ah_butt_resolution_change = 1;
			abr_state = 0;
		}
		if (rotation==ENC_RIGHT_SPIN)
		if (set_ah<999999)
		{
			if (set_ah+ah_butt_resolution>999999) return;
			set_ah+=ah_butt_resolution;
			display_changed|=2;
			ah_butt_resolution_change = 1;
			abr_state = 0;
		}
		// энкодер установки времени таймера
		if (timer_state>0) return;
	}
	else if (current_screen == 2){
		if (revers_enable == false && impuls_enable == false) return;
		switch (current_param){
			case POS_TIME:
			if (rotation==ENC_RIGHT_SPIN){
				if (positive_period < MAX_PERIOD-1000){
					positive_period+=1000;
					changed_params |= POS_TIME;
					save_param = 1;
					display_changed|=2;
				}
			}
			else if (rotation==ENC_LEFT_SPIN){
				if (positive_period > MIN_PERIOD+1000){
					positive_period-=1000;
					changed_params |= POS_TIME;
					save_param = 1;
					display_changed|=2;
				}
			}
			break;
			case POS_TIME_MS:
			if (rotation==ENC_RIGHT_SPIN){
				if (positive_period <= MAX_PERIOD){
					positive_period++;
					changed_params |= POS_TIME_MS;
					save_param = 1;
					display_changed|=2;
				}
			}
			else if (rotation==ENC_LEFT_SPIN){
				if (positive_period >= MIN_PERIOD){
					positive_period--;
					changed_params |= POS_TIME_MS;
					save_param = 1;
					display_changed|=2;
				}
			}
			break;
			case NEG_TIME:
			if (rotation==ENC_RIGHT_SPIN){
				if (negative_period <= MAX_PERIOD-1000){
					negative_period+=1000;
					changed_params |= NEG_TIME;
					save_param = 1;
					display_changed|=2;
				}
			}
			else if (rotation==ENC_LEFT_SPIN){
				if (negative_period >= MIN_PERIOD+1000){
					negative_period-=1000;
					changed_params |= NEG_TIME;
					save_param = 1;
					display_changed|=2;
				}
			}
			break;
			case NEG_TIME_MS:
			if (rotation==ENC_RIGHT_SPIN){
				if (negative_period < MAX_PERIOD){
					negative_period++;
					changed_params |= NEG_TIME_MS;
					save_param = 1;
					display_changed|=2;
				}
			}
			else if (rotation==ENC_LEFT_SPIN){
				if (negative_period > MIN_PERIOD){
					negative_period--;
					changed_params |= NEG_TIME_MS;
					save_param = 1;
					display_changed|=2;
				}
			}
			break;
			case FREQ:
			if (rotation==ENC_RIGHT_SPIN){
				if (frequency < MAX_FREQ){
					frequency+= 1;
					setPeriod(1000 / frequency);
					setDuty(d_time_percents*10);
					changed_params |= FREQ;
					save_param = 1;
					display_changed|=2;
				}
			}
			else if (rotation==ENC_LEFT_SPIN){
				if (frequency > MIN_FREQ){
					frequency-= 1;
					setPeriod(1000 / frequency);
					setDuty(d_time_percents*10);
					changed_params |= FREQ;
					save_param = 1;
					display_changed|=2;
				}
			}
			break;
			case D_TIME:
			if (rotation==ENC_RIGHT_SPIN){
				if (d_time_percents < MAX_D_TIME){
					d_time_percents++;
					setDuty(d_time_percents*10);
					changed_params |= D_TIME;
					save_param = 1;
					display_changed|=2;
				}
			}
			else if (rotation==ENC_LEFT_SPIN){
				if (d_time_percents > MIN_D_TIME){
					d_time_percents--;
					setDuty(d_time_percents*10);
					changed_params |= D_TIME;
					save_param = 1;
					display_changed|=2;
				}
			}
			break;
		}
	}
	rotation = ENC_PollEncoder();
	if (current_screen == 1){		
		if (rotation==ENC_LEFT_SPIN)
		if (timer_time>MIN_TIMER_TIME) 
		{
			if (timer_time-MIN_TIMER_TIME<enc_butt_resolution) return;
			timer_time = timer_time - enc_butt_resolution;
			display_changed|=2;
			changed_params |= 1;
			save_param = 1;
		}
		if (rotation==ENC_RIGHT_SPIN)
		if (timer_time<MAX_TIMER_TIME) 
		{
			if (timer_time+enc_butt_resolution>MAX_TIMER_TIME) return;
			timer_time = timer_time + enc_butt_resolution;
			save_param = 1;
			changed_params |= 1;
			display_changed|=2;
		}
	}
	else if (current_screen == 2){
		if (revers_enable == false && impuls_enable == false) return;
		if (rotation==ENC_LEFT_SPIN){
			if (current_param == GROUP1_BEG && !impuls_enable)
				current_param = GROUP1_END;
			else if (current_param == GROUP1_BEG && impuls_enable)
				current_param = GROUP2_END;
			else if (current_param == GROUP2_BEG && !revers_enable)
				current_param = GROUP2_END;
			else
				current_param /= 2;
			enc_butt_resolution_change = true;
			display_changed|=2;
		}
		else if (rotation==ENC_RIGHT_SPIN){
			if (current_param == GROUP1_END && !impuls_enable)
				current_param = GROUP1_BEG;
			else if (current_param == GROUP2_END && revers_enable)
				current_param = GROUP1_BEG;
			else if (current_param == GROUP2_END && !revers_enable)
				current_param = GROUP2_BEG;
			else
				current_param *= 2;
			enc_butt_resolution_change = true;
			display_changed|=2;
		}
	}	
}

void logicProcess()
{
	static bool lcd_restart_once = false;
	static uint8_t out_state = OFF;
	static uint32_t last_state_time = 0;
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
		start_input = 0;
		display_changed|=8;
	}
	//если включен реверс - надо не просто включить источник, а еще и управлять направлением
	if (start_input)
	{
		if (revers_enable)
		{
			if (lcd_restart_once && millis() - last_state_time  >= PRESS_TIME)
			{
				lcdInit();
				//customSumbolLoad();
				display_changed = 1;
				lcd_restart_once = false;
			}
			switch (out_state){
				case OFF:
					last_state_time = millis();
					out_state = POS_PERIOD;
					positive_output = true;
					start_output = true;
				break;
				case POS_PERIOD:
					if (millis() - last_state_time >= positive_period){
						last_state_time = millis();
						out_state = NEG_PERIOD;
						negative_output = true;
						positive_output = false;
						start_output = true;
						//lcd_restart_once = true;
					}
				break;
				/*case POS_DEAD:
					if (millis() - last_state_time >= 1){
						last_state_time = millis();
						out_state = NEG_PERIOD;
						negative_output = true;
						start_output = true;
						lcd_restart_once = true;
					}
				break;*/
				case NEG_PERIOD:
					if (millis() - last_state_time >= negative_period){
						last_state_time = millis();
						out_state = POS_PERIOD;
						positive_output = true;
						negative_output = false;
						start_output = true;
						//lcd_restart_once = true;
					}
				break;
				/*case NEG_DEAD:
					if (millis() - last_state_time >= 1){
						last_state_time = millis();
						out_state = POS_PERIOD;
						positive_output = true;
						start_output = true;
						lcd_restart_once = true;
					}
				break;*/
			}
		}
		else {
			start_output = true;
			out_state = OFF;
			last_state_time = 0;
			positive_output = true;
			negative_output = false;
		}
	}
	else {
		start_output = false;
		out_state = OFF;
		last_state_time = 0;
		positive_output = false;
		negative_output = false;
	}
	//если таймер включен - обновляем каждую секунду
	if (timer_state>0) display_changed|=16;
}

void setStates()
{
	if (start_output) 
	{
		SetBit(START_OUT_PORT, START_OUT);
		SetBit(ACT_LED_PORT, ACT_LED);
	}
	else
	{
		ClearBit(START_OUT_PORT, START_OUT);
		ClearBit(ACT_LED_PORT, ACT_LED);
		ClearBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
		ClearBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	}
	if (!impuls_enable){
		if (positive_output) SetBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
		else ClearBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
		if (negative_output) SetBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
		else ClearBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	}
	if (ah_alert_output) SetBit(AH_ALERT_PORT, AH_ALERT);
	else ClearBit(AH_ALERT_PORT, AH_ALERT);
	if (tim_led) SetBit(TIM_LED_PORT, TIM_LED);	
	else ClearBit(TIM_LED_PORT, TIM_LED);	
}

void readStates()
{
	static uint16_t samples[I_SAMPLES_COUNT];
	//static uint16_t correction = 0;
	static uint8_t iter = 0;
	static uint16_t adc_state = 0;
	adc_state++;
	if (adc_state == ADC_REFRASH_DURATION)
	{
		//если источник запущен и сейчас не мертвое время - меряем значения
		if ((positive_output)||(negative_output)){
			if (iter>=I_SAMPLES_COUNT)
			{
				uint32_t new_I = 0;
				//сортируем массив намерянных значений чтобы найти медиану
				for (int s=0; s < I_SAMPLES_COUNT-1; s++ )
					for (int s1=s+1; s1 < I_SAMPLES_COUNT; s1++ )
						if (samples[s]>samples[s1])
						{
							uint16_t buff = samples[s1];
							samples[s1] = samples[s];
							samples[s] = buff;
						}
				iter = I_SAMPLES_COUNT/2;
				new_I = samples[iter];
				if (voltage == 0) new_I = 0;
				if ((new_I < I_LOW_LIMIT) || (current - new_I>AMPER_DIFF_TO_REFRASH))
				{
					if (new_I < 999)
						current = new_I;
					else
						current = 0;
					
					display_changed|=16;
				}
				iter = 0;
			}
			else
			{
				//АДС6 (70) - ток
				// 2В - 100А
				samples[iter] = (analogRead(70))/4;
				// 2В - 400А
				//samples[iter] = (analogRead(65)-correction);
				iter++;
			}
		}
		else
		{
			// Если источник не запущен - меряем значение коррекции, которое будем вычитать из полученного во время работы.
			//correction = analogRead(70);
			current = 0;
		}
		adc_state = 0;
	}
	if (adc_state == ADC_REFRASH_DURATION/2)
	{
		//АДС7 (71) - напряжение
		uint16_t new_V = (analogRead(71))/10.24;
		if (voltage - new_V>VOLTAGE_DIFF_TO_REFRASH)
		{
			if (new_V <= VOLTAGE_DIFF_TO_REFRASH) voltage = 0;
			else voltage = new_V;
			display_changed|=16;
		}
	}

	static bool start_state = false;
	static uint32_t butt_press_time = 0;
	static bool press_flag = false;
	if ((millis()-butt_press_time>PRESS_TIME)&&(press_flag))
	{
		press_flag = false;
		ah_calc_last_time = 0;
		if ((START_BUTT_PIN & (1 << START_BUTT)))
		{
			start_input = 0;
			timer_state = 0;
		}
		else
		{
			start_input = 1;
			millis_reset();
			if (timer_ena)
				timer_state = 1;
		}
		display_changed|=4;

	}
	if ((START_BUTT_PIN & (1 << START_BUTT))&&(start_state))
	{
		start_state = false;
		butt_press_time = millis();
		press_flag = true;
	}
	else if ((!(START_BUTT_PIN & (1 << START_BUTT)))&&(!start_state))
	{
		butt_press_time = millis();
		start_state = true;
		press_flag = true;
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
	
inline void LongPressBoth()
{
	// не переходим на второй экран если источник в работе
	//if (start_input) 
	//	return;
	if (current_screen == 1){
		current_screen = 2;
		current_param = 2;
	}
	else{
		current_screen = 1;
		current_param = NONE;
	}
	display_changed |= 128;
}	

inline void LongPressLeft()
{
	if (current_screen == 1){
		if (timer_ena)	{
			timer_ena = 0;
			display_changed|=16;
			timer_state = 0;
		}
		else{
			timer_ena = 1;
			display_changed|=16;
			//если включить таймер при включенном источнике - начинаем сразу считать
			if (start_input){
				millis_reset();
				ah_calc_last_time = 0;
				timer_state = 1;
			}
		}
		eeprom_update_byte(6, timer_ena);
	}
	else{
		if (revers_enable){
			revers_enable = false;
			current_param = D_TIME;
			eeprom_update_byte(12, (uint8_t)(0));
		}
		else {
			revers_enable = true;
			eeprom_update_byte(12, (uint8_t)(1));
		}
		display_changed |= 16;
	}
}

inline void LongPressRight()
{
	if (current_screen == 1)
		summary_ah = 0;
	else {
		if (impuls_enable){
			impuls_enable = false;
			current_param = POS_TIME;
			eeprom_update_byte(13, (uint8_t)(0));
		}
		else {
			impuls_enable = true;
			eeprom_update_byte(13, (uint8_t)(1));
		}
		display_changed |= 16;
	}
}

inline void PressLeft()
{
	if (current_screen == 1)
		ChangeTimeResolution();
	else {
		if (!revers_enable && !impuls_enable) return;
		if (current_param == GROUP2_BEG && !revers_enable) current_param = GROUP2_END;
		else if (current_param == GROUP1_BEG && !impuls_enable) current_param = GROUP1_END;
		else if (current_param == GROUP1_BEG && impuls_enable) current_param = GROUP2_END;
		else current_param /= 2;
		enc_butt_resolution_change = true;
		display_changed|=2;
	}
}

inline void PressRight()
{	
	if (current_screen == 1){
		//если в момент нажатия на кнопку энкодера показывается текущее значение ампер часов
		// - не меняем разрешение, а только показываем установленное значение
		if (ah_butt_resolution_change == 0){
			ah_butt_resolution_change = 1;
			display_changed|=128;
		}
		else {
			ah_butt_resolution_change = 2;
			ChangeAhResolution();
		}
	}
	else {
		if (!revers_enable && !impuls_enable) return;
		if (current_param == GROUP2_END && revers_enable) current_param = GROUP1_BEG;
		else if (current_param == GROUP2_END && !revers_enable) current_param = GROUP2_BEG;
		else if (current_param == GROUP1_END && !impuls_enable) current_param = GROUP1_BEG;
		else current_param *= 2;
		enc_butt_resolution_change = true;
		display_changed|=2;
	}
}
	
void encoderButtStates()
{	
	static bool enc_state = false;
	static bool press_flag = false;
	static bool enc_state2 = false;
	static bool press_flag2 = false;
	uint32_t d_time1 = millis()-enc_butt_press_time;
	uint32_t d_time2 = millis()-enc_butt_press_time2;
	if ((d_time1>LONG_PRESS_TIME)&&(press_flag))
	{
		press_flag = false;
		if ((d_time2>PRESS_TIME)&&(press_flag2))
		{
			press_flag2 = false;
			LongPressBoth();
			return;
		}
		LongPressLeft();
	}
	if ((TIMER_BUTT_PIN & (1 << TIMER_BUTT))&&(enc_state))
	{
		if ((d_time1>PRESS_TIME)&&(d_time1<LONG_PRESS_TIME))
			PressLeft();
		enc_state = false;
		press_flag = false;
	}
	else if ((!(TIMER_BUTT_PIN & (1 << TIMER_BUTT)))&&(!enc_state))
	{
		enc_butt_press_time = millis();
		enc_state = true;
		press_flag = true;
	}
	
	if ((d_time2>LONG_PRESS_TIME)&&(press_flag2))
	{
		press_flag2 = false;
		if ((d_time1>PRESS_TIME)&&(press_flag))
		{
			press_flag = false;
			LongPressBoth();
			return;
		}
		LongPressRight();
	}
	if ((AH_BUTT_PIN & (1 << AH_BUTT))&&(enc_state2))
	{
		if ((d_time2>PRESS_TIME)&&(d_time2<LONG_PRESS_TIME))
			PressRight();
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