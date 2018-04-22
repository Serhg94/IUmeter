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

enum { NONE = 0, TIMER_TIME = 1, DEAD_TIME = 2, D_TIME = 4, FREQ = 8};
enum {OFF, POS_PERIOD, POS_DEAD, NEG_PERIOD, NEG_DEAD};

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

const bool revers_enable = true;
const uint32_t dead_time = 60000;
const uint32_t positive_period = 14400000;
const uint32_t negative_period = 14400000; 
uint8_t out_state = OFF;
uint32_t last_state_time = 0;

bool ah_alert_output = 0;
bool start_input = 0;
bool start_output = 0;
bool positive_output = 0;
bool negative_output = 0;
bool tim_led = 0;

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

void init()
{
	wdt_enable(WDTO_500MS);
	wdt_reset();
	_delay_ms(50);
	lcdInit();
	wdt_reset();

	set_ah = eeprom_read_dword(7);
	
	if ((set_ah>999999))
	{
		set_ah = 50;
		eeprom_write_dword(7, (uint32_t)(500));
	}

	millis_init();
	millis_resume();

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
			if (start_input){
				uint32_t time;
				switch (out_state){
					case POS_PERIOD:
						time = (positive_period - millis() + last_state_time)/1000;
						break;
					case POS_DEAD:
						time = (dead_time + 1 - millis() + last_state_time)/1000;
						break;
					case NEG_PERIOD:
						time = (negative_period - millis() + last_state_time)/1000;
						break;
					case NEG_DEAD:
						time = (dead_time + 1 - millis() + last_state_time)/1000;
						break;
				}
				// Если время больше того, что может влезть на экран - убираем его)
				if (time > 359999)
					time = 0;

				uint32_t hours = (time/3600);
				time = time - hours*3600;
				uint16_t min = (time/60);
				uint8_t sec = time - min*60;
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
	}	
}

void logicProcess()
{
	static bool lcd_restart_once = false;
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
					lcd_restart_once = true;
				break;
				case POS_PERIOD:
					if (millis() - last_state_time >= positive_period){
						last_state_time = millis();
						out_state = POS_DEAD;
						positive_output = false;
						start_output = false;
						lcd_restart_once = true;
					}
				break;
				case POS_DEAD:
					if (millis() - last_state_time >= dead_time+1){
						last_state_time = millis();
						out_state = NEG_PERIOD;
						negative_output = true;
						start_output = true;
						lcd_restart_once = true;
					}
				break;
				case NEG_PERIOD:
					if (millis() - last_state_time >= negative_period){
						last_state_time = millis();
						out_state = NEG_DEAD;
						negative_output = false;
						start_output = false;
						lcd_restart_once = true;
					}
				break;
				case NEG_DEAD:
					if (millis() - last_state_time >= dead_time+1){
						last_state_time = millis();
						out_state = POS_PERIOD;
						positive_output = true;
						start_output = true;
						lcd_restart_once = true;
					}
				break;
			}
		}
		display_changed|=16;
	}
	else {
		start_output = false;
		positive_output = false;
		negative_output = false;
	}
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
	}
	if (positive_output) SetBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
	else ClearBit(POSITIVE_OUT_PORT, POSITIVE_OUT);
	if (negative_output) SetBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	else ClearBit(NEGATIVE_OUT_PORT, NEGATIVE_OUT);
	
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
		// Если источник не запущен - меряем значение коррекции, которое будем вычитать из полученного во время работы.
		else
		{
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
			// запоминаем  длительность последней стадии
			last_state_time = millis() - last_state_time;
		}
		else
		{
			start_input = 1;
			// обнуляем счетчик и возвращаем запомненную длительность последней стадии
			millis_reset();
			millis_add(last_state_time);
			last_state_time = 0;
			// возвращаем запущенный источник, если он был в соответствующем состоянии
			switch (out_state){
				case POS_PERIOD:
					positive_output = true;
					start_output = true;
					break;
				case NEG_PERIOD:
					negative_output = true;
					start_output = true;
					break;
			}
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

}	

inline void LongPressLeft()
{

}

inline void LongPressRight()
{
	if (current_screen == 1)
		summary_ah = 0;
}

inline void PressLeft()
{

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