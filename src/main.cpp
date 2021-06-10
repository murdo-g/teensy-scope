#include <Arduino.h>
#include "sh1106.h"
#include <Metro.h>
#include <ADC.h>
#include <ADC_util.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioInputAnalog         adc1;           //xy=221,265


/* Hardware Wemos D1 mini SPI pins
 D5 GPIO13   CLK         - D0 pin OLED display
 D7 GPIO11   MOSI (DOUT) - D1 pin OLED display
 D1 GPIO0    RST         - RST pin OLED display
 D2 GPIO1    DC          - DC pin OLED
 D8 GPIO10   CS / SS     - CS pin OLED display
*/

// Pin definitions for SPI
// ESP8266
#define OLED_RESET  0   // RESET
#define OLED_DC     1   // Data/Command
#define OLED_CS     10  // Chip select
#define OLED_SCK    13  // Clock
#define OLED_MOSI   11  // MOSI

int sensorPin = A2;
int preVal = 0;
int timeOut = 0;
int timeBase = 5000; //us
float timediv =  timeBase/128;
bool clkFlag = false, bufFlag = false;

// Uncomment one of the following based on OLED type
SH1106 display(true, OLED_SCK, OLED_MOSI, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI

ADC *adc = new ADC(); // adc object;


unsigned long cMicros = 0;
unsigned long pMicros = 0;
unsigned long cMillis = 0;
unsigned long pMillis = 0;
float offset = 0;
float currentVal = 0;
float buffer[1024] = {};
int count = 0;
int startPos = 0;
int timerCount = 0;

// Sawtooth sawtooth(440);



void setup() {
    Serial.begin(9600);
    display.init();
    display.displayOn();
    display.drawGrid();
    display.drawSine(0);
    display.display();

    adc->adc0->setAveraging(4);                                         // set number of averages
    adc->adc0->setResolution(8);                                        // set bits of resolution
    adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED); // change the conversion speed
    adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);     // change the sampling speed
}

void loop() {
    /* Read clock */
    cMillis = millis();
    // cMicros = micros();
    // int inVal = analogRead(sensorPin);
    // if((preVal < 450) && (inVal > 450)) {
    //     clkFlag = true;
    // }

    // if(clkFlag){
    // if((cMillis - pMillis)>200){
    // Sample every 1.5ms
        // if((cMicros - pMicros) > 1000){
        //     pMicros = micros();
        //     buffer[count] = (adc->adc0->analogRead(sensorPin)/5);
        //     count ++;
        //     if(count >= 128) {
        //         clkFlag = false;
        //         bufFlag = true;
        //         count = 0;
        //     }
        // }
    // }
    int centre = 0;
    
    if(!bufFlag){
        for(int x=0; x<256; x++){
            buffer[x] = (adc->adc0->analogRead(sensorPin)/5);
            delayMicroseconds(timediv);
        }

        // Zero detection
        
        for(int x=64; x<196; x++){
            if((buffer[x] < 30) && (buffer[x+1] > 30)){
                centre = x;
                timeOut = cMillis;
            }
        }

        bufFlag=true;
    }

    if(((cMillis - pMillis)>50) || bufFlag){
        pMillis = millis();
        
        

        if(centre!=0){
            display.clear();
            for(int column = 0; column < 127; column++){
                display.setPixel(column, (64-buffer[centre-64+column]));
                display.drawLine(column, (64-buffer[centre-64+column]), column+1, (64-buffer[centre-64+column+1]));
            }
            display.drawGrid();
            display.updateDisplay();
            bufFlag = false;
        }
        else if((cMillis - timeOut) > 250){
            display.clear();
            timerCount ++;
            for(int column = 0; column < 128; column++){
                display.setPixel(column, 64-buffer[column]);
            }
            display.drawGrid();
            display.updateDisplay();
            if(timerCount > 10){
                timerCount = 0;
                bufFlag = false;
            }
        }
        
    }



    // cMillis = millis();
    // cMicros = micros();
    // int inVal = analogRead(sensorPin);

    // if((cMicros - pMicros) > timediv) {
    //     pMicros = micros();
    //     for (int x=512; x>0; x--){
    //         buffer[x] = buffer[x-1];
    //     }
    //     buffer[0] = (analogRead(sensorPin)/10) - 15;
    // }

    // if((cMillis - pMillis)>20){
    //     for(count = 0; count < 384; count ++) {
    //         if((buffer[count]<40) && (buffer[count+1]>40)){
    //             startPos = count;
    //         }
    //     }
    //     display.clear();
    //     for(int column = 0; column < 128; column++){
    //         display.setPixel(column, buffer[startPos + column]);
    //     }
    //     display.updateDisplay();
    // }

    // preVal = inVal;
}