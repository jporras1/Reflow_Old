//
//  heater.c
//  Index
//
//  Created by Javier Porras jr on 4/6/21.
//  Copyright © 2021 Javier Porras jr. All rights reserved.
//

#include "machine.h"

volatile float seconds = 0;  // this will get incremented once a second
volatile long count = 0;
volatile int lcdCount = 0;
volatile float temp = 0;

enum States state = OFF;

void initHeater(void){
    RELAY_DDR |= RELAY_MASK;
    RELAY_PORT &= ~RELAY_MASK;

    BUZZER_DDR |= BUZZER_MASK;
    BUZZER_PORT &= ~BUZZER_MASK;


    /*this is specific for the attiny84*/
    TCCR1A |= 0;                //This whole register is set to zeros,
    TCCR1B |= (1 << WGM12);     //Set this to enable CTC mode
    TCCR1B |= (1 << CS12) | (1 << CS10);        //setting the prescaler to 1024
    OCR1A = 244; //1,000,000[F_CPU] / (1024[prescaler] * 2 * (1Hz *2)
//        TIMSK1 |= (1 << OCIE1A);

    /*This is specific for the attiny84*/
    /*This is for the relay*/
    TCCR0A |= (1 << WGM00);     //setting the WGM mode to mode 7, Phase correct pwm
    TCCR0B |= (1 << WGM02);
    TCCR0B |= (1 << CS00);      //setting the prescaler to 1.
    //TCCR0A |= (1 << COM0A1) | (1 << COM0A0);    //enable toggle on pin OC0A
    
}

void action(void){
    started = true;
    state = 1;

    TIMSK1 |= (1 << OCIE1A);
    
    while (state != 0) {
        float tempTemperature = 0;
        float previousTemp = temp;
        
        analogRead();
        tempTemperature = (adc.value * 0.29); //analogRead();
        
        if (isnan(tempTemperature)){
            temp = previousTemp;
        }
        else{
            temp = tempTemperature;
        }
        
        LCD_clear();
        LCD_changeAddress(0x40);
        LCD_sendString("Temp: ");
        LCD_changeAddress(0x46);
        lcdPrintDouble((adc.value * 0.29));
        LCD_sendString("`C   ");
        LCD_changeAddress(0x4F);
        lcdPrintNumber((int)state);
        _delay_ms(50);
        
        switch (state) {
            case OFF:
                //set relay pin low.
                RELAY_PORT &= ~RELAY_MASK;
                TCCR1B &= ~WGM12;
                //return;
                
                break;
            case Preheating:
                //pwm relay pin to 75 (0-255) profile[0]
                // TODO: need to implement the pwm here.
                OCR0A = ((profiles[selectedProfile].preheatPWM) * 2.55);
                TCCR0A |= (1 << COM0A1) | (1 << COM0A0);    //enable toggle on pin OCR0A
                
                //check if temp >= profile[1], 140
                if (temp >= profiles[selectedProfile].preheatTemp) {
                    //if true, state = 2; and count = 0;
                    state = Soaking;
                    count = 0;
                    TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0));
                }
                break;
            case Soaking:
                //check if avgTemp < profile[1], 140, set relayPin high,
                if (temp < profiles[selectedProfile].preheatTemp) {
                    RELAY_PORT |= RELAY_MASK;
                } else {
                    // else set relayPin low
                    RELAY_PORT &= ~RELAY_MASK;
                }
                //check if seconds >= profile[2], 45, set state to 3.
                if (seconds >= profiles[selectedProfile].soakDuration) {
                    state = Reflowing;
                }
                break;
            case Reflowing:
                //pwm relay pin to 125, (0-255) profile[3]
                // TODO: need to implement the pwm here. Again.
                OCR0A = ((profiles[selectedProfile].reflowPWM) * 2.55);
                TCCR0A |= (1 << COM0A1) | (1 << COM0A0);    //enable toggle on pin OCR0A
                
                //check if avgTemp >= profile[4], 205, set state = 4, count = 0
                if (temp >= profiles[selectedProfile].reflowTemp) {
                    state = Cooling;
                    count = 0;
                    TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0));
                }
                break;
            case Cooling:
                //check if avgTemp < profile[4], 205, set realayPin high.
                if (temp < profiles[selectedProfile].reflowTemp) {
                    RELAY_PORT |= RELAY_MASK;
                } else {
                    //if false, set relayPin low.
                    RELAY_PORT &= ~RELAY_MASK;
                }
                //check if seconds >= profile[5], 20, set state = 0, set relayPin LOW.
                if (seconds >= profiles[selectedProfile].reflowDuration) {
                    state = OFF;
                    RELAY_PORT &= ~RELAY_MASK;
                }
                TCCR1B &= ~WGM12;
                break;
            default:
                RELAY_PORT &= ~RELAY_MASK; //set relayPin low,
                state = OFF;
                count = 0;
                TCCR1B &= ~WGM12;
                break;
        }
    }
    
}

void beep(void){
    BUZZER_PORT |= BUZZER_MASK;
    _delay_ms(300);
    BUZZER_PORT &= ~BUZZER_MASK;
}



ISR(TIM1_COMPA_vect){
    count ++;
    seconds = (float)count / 4;
}

