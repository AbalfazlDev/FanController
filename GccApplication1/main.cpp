#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include "Tools/Framebuffer.h"
#include "Tools/icon.h"
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>


#define btn_right PD3
#define btn_left PD2
#define btn_center PD1
#define adc_filter_repeat 20

#define step_fan_speed 5
#define step_timer_speed 10

#define read_fan_pals PB0
#define fan_pwm PB1

#define read_temp_lm335 PC3

// for calculate fan rpm
uint8_t last_icr ;
uint8_t priod = 0;
bool is_ready_new_priod =false;

typedef enum {
	mode_manual,
	mode_auto
}operation_mode;

typedef enum{
	power_off_page,
	running_page,
	temp_warning_page,
	timer_page
}device_page;

typedef enum{
	power_off,
	normal_run,
	high_temp,
}device_status;

typedef enum {
	enable,
	disable
}timer_state;

Framebuffer fb;
uint16_t _adc_value;
uint16_t adc_values[adc_filter_repeat]={};

float _adc_voltage;
uint32_t temp_k;
int16_t temp_c;
uint8_t current_fan_frame;
device_page current_device_page = timer_page;
device_status current_device_status = normal_run;
uint8_t fan_speed = 20;
char  fan_speed_str [7] = "start";
operation_mode current_mode = mode_manual;
uint8_t keep_center_btn = 0;
uint32_t timer_millis = 0;
uint32_t off_timer = 0;
uint32_t off_time_set;
uint32_t millis_before_set_timer;
timer_state current_timer_state = disable;

void initialize_timer(void){
	TCCR2A = (1 << WGM21);
	TCCR2B = (1 << CS22);
	OCR2A = 125;
	TIMSK2 = (1 << OCIE2A);
	
	sei();
}

void initialize_micro(void){
	// start ADC for lm335
	ADMUX = (1<<REFS0);
	ADCSRA = (1<<ADEN) |(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
	
	// initialize btns
	DDRD &=~( (1 << btn_left)  | (1 << btn_right) );
	PORTD |= (1 << btn_left)  |(1 << btn_right);
	
	DDRD &=~(1<<btn_center);
	PORTD |=(1<<btn_center);
	
	DDRB &= ~(1<<read_fan_pals);
	PORTB |= (1<<read_fan_pals); //pull up
	//TCCR1B = (1 <<CS10); //Clock Select bit 0 & Timer/Counter Control Register
	TIMSK1 = (1 <<ICIE1); // Input Capture Interrupt Enable
	
	DDRB |= (1<<fan_pwm);
	TCCR1A = (1 << COM1A1) | (1 << WGM10);  // Clear on match, Fast PWM 8-bit
	TCCR1B = (1 << WGM12)  | (1 << CS11) | (1 << CS10); //Fast PWM 8-bit? prescaler=64
	OCR1A = 128;
	initialize_timer();
	// 	EICRA |= (1<<ISC01) | (1<<ISC11);
	// 	EICRA &= ~((1<<ISC00) | (1 <<ISC10));
	// 	EIMSK = (1 <<INT0) | (1 <<INT1);
	sei();
}

ISR(TIMER1_CAPT_vect){
	uint8_t now = ICR1;
	priod = now - last_icr;
	last_icr = now;
	is_ready_new_priod = true;
}

ISR(TIMER2_COMPA_vect){
	timer_millis++;
}

uint32_t millis(void){
	uint32_t temp;
	cli();
	temp = timer_millis;
	sei();
	return temp;
}

uint16_t ADC_Read(uint8_t channel){
	channel &= 0X07;
	ADMUX = (ADMUX &0XF8) | channel;
	
	ADCSRA |= (1<<ADSC) ;
	while(ADCSRA &(1<<ADSC));
	
	return ADC;
}

uint16_t ADC_Read_with_filter(uint8_t channel){
	uint16_t sum = 0;
	uint8_t i;
	_delay_ms(30);
	
	for (i = adc_filter_repeat - 1; i > 0; i--) {
		adc_values[i] = adc_values[i - 1];
		sum += adc_values[i];
	}
	adc_values[0] = ADC_Read(3);
	sum += adc_values[0];
	
	return sum /adc_filter_repeat;
}

void set_fan_adnimation_frame() {
	
	//1 to 10;
	//current_fan_clock +=1;
	//fan_speed = 11-fan_speed;
	//int remaining = current_fan_clock/fan_speed;
	//if(remaining == 1){
	current_fan_frame ++;
	//current_fan_clock = 0;
	
	if(current_fan_frame == fan_frame)
	current_fan_frame = 0;
	//}
}

void set_fan_speed(uint8_t speed){
	uint8_t pwm = speed * 255 / 100;
	OCR1A = pwm;
}

void update_status(){
	if(temp_c >45)
	{
		current_device_status = high_temp;
		current_device_page = temp_warning_page;
	}
	else if(fan_speed==0){
		current_device_status = power_off;
		current_device_page = power_off_page;
	}
	else if(current_device_status == power_off)
	{
		fan_speed = 0;
		current_device_page = power_off_page;
	}
	else if(current_device_page !=timer_page)
	{
		current_device_status = normal_run;
		current_device_page = running_page;
	}
	
	
	// change string status
	if (fan_speed <= 40) {
		strcpy(fan_speed_str,"low");
	}
	else if (fan_speed <= 60) {
		strcpy(fan_speed_str,"medium");
	}
	else if (fan_speed < 90) {
		strcpy(fan_speed_str,"fast");;
	}
	else {
		strcpy(fan_speed_str,"turbo");;
	}
	
	switch(current_mode){
		case mode_auto:
		
		if(temp_c<10)
		fan_speed = 0;
		else if(temp_c <=40)
		fan_speed = ( 10 * temp_c /3) -33;
		else
		fan_speed = 100;
		break;
		
		case mode_manual:
		current_mode = mode_manual;
		
		break;
	}
}

float cal_voltage(uint8_t channel){
	float adc_value =  ADC_Read_with_filter(channel);
	return( adc_value * 5.0)/1023.0 ;
}

void read_temp (){
	temp_k =cal_voltage(3) * 100 +132;
	temp_c = temp_k - 273;
}

// uint8_t Cal_Fan_RPM(){
// 	if(is_ready_new_priod){
// 		is_ready_new_priod = false
//
// 	}
// }

void Update_Screen()
{
	fb.clear();
	fb.drawString(0,53,"TEMP",1);
	fb.drawHLine(0, 49, 128);
	
	fb.drawUnsignedNumber(temp_k,40,53,1);
	fb.drawChar(60,51,'k',1);
	
	fb.drawNumber(temp_c,83,53,1);
	fb.drawChar(98,51,'c',1);
	
	if(( (millis()/500) % 2 == 0) & (current_timer_state==enable)) fb.drawChar(118,52,'T',1);
	
	switch(current_device_page){
		case running_page:
		
		set_fan_adnimation_frame();
		fb.drawBitmap(arr_fan[current_fan_frame],25,25,3,22);
		fb.drawString(34,32,fan_speed_str,1);
		
		switch(current_mode){
			case mode_auto:
			fb.drawString(98,32,"auto",1);
			break;
			case mode_manual:
			fb.drawString(89,32,"manual",1);
			break;
		}
		fb.drawString(7,11,"SPEED",2);
		fb.drawUnsignedNumber(fan_speed,80,13,1);
		fb.drawBitmap(arrow_right,9,9,101,12);
		fb.drawBitmap(arrow_left,9,9,62,12);
		
		//fb.drawString(7,0,"PRM",1);
		//for test
		fb.drawFloat(_adc_value,5,0,0,1);
		fb.drawFloat(ADC_Read(3),5,50,0,1);
		fb.drawFloat(_adc_voltage*100,5,95,0,1);
		
		// 	fb.drawUnsignedNumber(current_fan_clock %10,5,10,1);
		// 	fb.drawUnsignedNumber(current_fan_clock,5,1,1);
		break;
		
		case temp_warning_page:
		fb.drawBitmap(bmp_warnig,28,28,1,15);
		fb.drawString(50,28,"High temp",1);
		fb.drawString(1,5,"running at full power",1);
		break;
		
		case  power_off_page:
		fb.drawString(51,28,"Power off",1);
		fb.drawBitmap(bmp_power_off,25,25,9,15);
		fb.drawString(1,0,"Right button to start",1);
		break;
		
		case timer_page:
		fb.drawString(0,32,"Uptime",1);
		
		uint16_t uptime_device = millis() /1000;
		
		//for one digit
		fb.drawUnsignedNumber(uptime_device/3600,55,32,1);
		fb.drawChar(63,32,'h',1);
		
		//for one digit
		fb.drawUnsignedNumber(uptime_device /60,84,32,1);
		fb.drawChar(92,32,'m',1);
		
		// for two digit
		fb.drawUnsignedNumber(uptime_device % 60,109,32,1);
		fb.drawChar(122,32,'s',1);
		
		fb.drawString(23,16,"auto off timer",1);
		
		if( ((millis()/250) % 2 ==0) & (current_timer_state==enable)) fb.drawBitmap(arrow_left,9,9,2,0); // toggle arrow
		

		fb.drawUnsignedNumber(off_timer/3600,25,0,1);
		fb.drawChar(39,0,'h',1);
		
		fb.drawUnsignedNumber(off_timer / 60,66,0,1);
		fb.drawChar(80,0,'m',1);
		
		fb.drawUnsignedNumber(off_timer % 60,100,0,1);
		fb.drawChar(114,0,'s',1);
		
		
		
		break;
		
	}
	
	
	fb.show();
}

void check_right_btn(void)
{
	while((PIND & (1<<btn_right)) == 0)
	{
		if(current_device_status == power_off) current_device_status= normal_run;

		if(current_device_page==timer_page)
		{
			off_timer +=step_timer_speed;
		}
		else
		{
			fan_speed += step_fan_speed ;
			if (fan_speed > 100)
			fan_speed =100;
			current_mode = mode_manual;
		}
		Update_Screen();
		_delay_ms(30);
	}
}

void check_left_btn(void)
{
	while((PIND & (1<<btn_left)) == 0)
	{
		if(current_device_page==timer_page)
		{
			if(off_timer>step_timer_speed) off_timer -= step_timer_speed;
			else off_timer = 0;
			current_timer_state = disable;
		}
		else
		{
			if (fan_speed > step_fan_speed) fan_speed -=  step_fan_speed;
			else fan_speed =0;
			current_mode = mode_manual;
		}
		Update_Screen();
		_delay_ms(30);
	}
}

void check_center_btn(void)
{
	if((PIND & (1<<btn_center))==0)
	{
		_delay_ms(300);
		
		if((PIND & (1<<btn_center))==0)
		{
			if (current_device_page == timer_page) current_device_page = running_page;
			else current_device_page = timer_page;
			Update_Screen();
			_delay_ms(700);
		}
		else
		{
			if(current_device_page ==timer_page)
			{
				millis_before_set_timer = millis();
				off_time_set = off_timer *1000; //tmep
				current_timer_state = enable;
				current_device_page = running_page; // to to running page
			}
			else // in running page
			{
				if(current_mode == mode_auto)
				current_mode = mode_manual;
				else if(current_mode == mode_manual)
				current_mode = mode_auto;
			}
		}
		
		
	}
}

void update_off_timer_status(void){
	if((current_timer_state ==enable) & off_timer <= 0)
	{
		current_device_status = power_off;
		current_timer_state = disable;
	}
	
	if((current_timer_state == enable) & off_timer >0){
		off_timer =(off_time_set - (millis() - millis_before_set_timer)) /1000;
	}
}

int main(void) {
	initialize_micro();

	while (1) {
		read_temp ();
		update_status();
		
		check_right_btn();
		check_left_btn();
		check_center_btn();
		update_off_timer_status();
		
		
		set_fan_speed(fan_speed);


		
		Update_Screen();
	}

	return 0;
}


