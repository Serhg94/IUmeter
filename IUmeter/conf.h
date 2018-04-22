/*
 * IncFile1.h
 *
 * Created: 01.11.2016 0:27:42
 *  Author: Serhg
 */ 


#ifndef INCFILE1_H_
#define INCFILE1_H_


#define F_CPU 16000000UL

#define FAST_BLINK_TIMER_REMAIND 30

#define VOLTAGE_DIFF_TO_REFRASH 1
#define AMPER_DIFF_TO_REFRASH 2
#define I_SAMPLES_COUNT 40
#define I_LOW_LIMIT 10

#define ADC_REFRASH_DURATION 200
#define ADC_CYCLES 13.5*16+65

#define AH_CALCULATE_DIV 50000
#define RESOLUTION_BLINK_DURATION 10000
#define SET_AH_DISPLAY_DURATION 80000
#define TIMER_LED_BLINK_DURATION 10000
#define TIMER_LED_FASTBLINK_DURATION 5000
#define DISPLAY_REFRASH_DURATION 5000
#define SAVE_PARAMS_DURATION 50000

//99 часов
#define MAX_TIMER_TIME 359999
#define MIN_TIMER_TIME 10
#define MAX_FREQ 10
#define MIN_FREQ 0.2
#define MAX_D_TIME 90
#define MIN_D_TIME 10

#define START_IGNORE_TIME 250
#define PRESS_TIME 100
#define LONG_PRESS_TIME 1000

//кнопка запуска источника
#define START_BUTT_DDR DDRD
#define START_BUTT_PORT PORTD
#define START_BUTT_PIN PIND
#define START_BUTT 2
//выход на управление источником - бывших торч он аут
#define START_OUT_DDR DDRB
#define START_OUT_PORT PORTB
#define START_OUT 5
//выход на включение положительной диагонали - бывший вход измерения тока
#define POSITIVE_OUT_DDR DDRC
#define POSITIVE_OUT_PORT PORTC
#define POSITIVE_OUT 1
//выход на включение отрицательной диагонали - бывший вход измерения напряжения 
#define NEGATIVE_OUT_DDR DDRC
#define NEGATIVE_OUT_PORT PORTC
#define NEGATIVE_OUT 0

//нижний диод
#define TIM_LED_DDR DDRD
#define TIM_LED_PORT PORTD
#define TIM_LED 0
//средний диод
#define AH_ALERT_DDR DDRD
#define AH_ALERT_PORT PORTD
#define AH_ALERT 1
//верхних диод
#define ACT_LED_DDR DDRB
#define ACT_LED_PORT PORTB
#define ACT_LED 3

//кнопка энкодера установки ампер часов
#define AH_BUTT_DDR DDRD
#define AH_BUTT_PORT PORTD
#define AH_BUTT_PIN PIND
#define AH_BUTT 3
//кнопка энкодера установки времени таймера
#define TIMER_BUTT_DDR DDRB
#define TIMER_BUTT_PORT PORTB
#define TIMER_BUTT_PIN PINB
#define TIMER_BUTT 4


#define ENC_RIGHT_SPIN RIGHT_SPIN 
#define ENC_LEFT_SPIN LEFT_SPIN
//порт и выводы к которым подключен энкодер установки счетчика ампер часов
#define PORT_EncT 	PORTD
#define PIN_EncT 	PIND
#define DDR_EncT 	DDRD
#define Pin1_EncT 	4
#define Pin2_EncT 	5
//порт и выводы к которым подключен энкодер установки таймера
#define PORT_Enc	PORTD
#define PIN_Enc 	PIND
#define DDR_Enc 	DDRD
#define Pin1_Enc 	6
#define Pin2_Enc 	7
/*
	Здесь определяются выводы контроллера, подключенные к LCD. Выводы данных
	должны занимать один порт. Порядок выводов любой. Также на одном порту
	должны располагаться управляющие выводы. Тоже в любом порядке, не обяза-
	тельно подряд.
*/ 

#define LCDDATAPORT			PORTC					// Порт и пины,
#define LCDDATADDR			DDRC					// к которым подключены
#define LCDDATAPIN			PINC					// сигналы D4-D7.
#define LCD_D4				2
#define LCD_D5				3
#define LCD_D6				4
#define LCD_D7				5

#define LCDCONTROLPORT		PORTB					// Порт и пины,
#define LCDCONTROLDDR		DDRB					// к которым подключены
#define LCD_RS				0						// сигналы RS, RW и E.
#define LCD_RW				1
#define LCD_E				2




#endif /* INCFILE1_H_ */