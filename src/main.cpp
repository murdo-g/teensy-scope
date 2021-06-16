#include <Arduino.h>
#include "sh1106.h"
#include <ADC.h>
#include <IntervalTimer.h>

#define BUFFER_SIZE 1024

// Pin definitions for SPI
// ESP8266
#define OLED_RESET 0 // RESET
#define OLED_DC 1    // Data/Command
#define OLED_CS 10   // Chip select
#define OLED_SCK 13  // Clock
#define OLED_MOSI 11 // MOSI

// Uncomment one of the following based on OLED type
SH1106 display(true, OLED_SCK, OLED_MOSI, OLED_RESET, OLED_DC, OLED_CS); // FOR SPI

const int readPinIn = A0;
const int readPinExt = A1;

int timebase = 1000;
int period = 20; //us

const int refreshPeriod = 30000; // us

ADC *adc = new ADC(); // adc object

IntervalTimer timer0, timer1;

uint8_t in_buffer_0[BUFFER_SIZE];
int in_buffer_0_count = 0;
uint8_t in_buffer_1[BUFFER_SIZE];
int in_buffer_1_count = 0;
bool in_buffer0Ready = true;

uint8_t ext_buffer_0[BUFFER_SIZE];
int ext_buffer_0_count = 0;
uint8_t ext_buffer_1[BUFFER_SIZE];
int ext_buffer_1_count = 0;
bool ext_buffer0Ready = true;

elapsedMillis latchTimeout;
elapsedMillis timebaseUpdate = 0;

int startTimerValue0 = 0, startTimerValue1 = 0;

bool lfoMode = true;
float lfoBuffer[100];
int lfoBufferCount = 0;
elapsedMillis lfoTimer = 0;
float lfoMax = -3, lfoMin = 3;

void updateDisplayLFO(uint8_t *buffer)
{
    if (lfoTimer > 50)
    {
        float value = 0;
        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            value += buffer[i];
        }
        value /= BUFFER_SIZE;
        value = map(float(value), 0, 256, -3.2, 3.2);
        // Serial.println(value);

        // float absValue = fabs(value);
        if (value > lfoMax)
            lfoMax = value;
        if (value < lfoMin)
            lfoMin = value;
        float centre = lfoMax - (lfoMax - lfoMin) / 2;

        lfoTimer = 0;
        lfoBuffer[lfoBufferCount] = -(value - centre);
        lfoBufferCount++;
        //float voltage = map(float(value), 0, 256, -3.3, 3.3);

        display.clear();
        for (int column = 1; column < lfoBufferCount; column++)
        {
            display.drawLine(column, (32 + (lfoBuffer[column - 1]) * 31 / (lfoMax - centre)),
                             column + 1, (32 + lfoBuffer[column] * 31 / (lfoMax - centre)));
        }
        display.drawLFOGrid(32 + 32 * (centre / 2));

        display.drawString(95, 0, String(lfoMax, 2) + "V");
        display.drawString(95, 25, String(value, 2) + "V");
        display.drawString(95, 50, String(lfoMin, 2) + "V");
        display.updateDisplay();
    }
    if (lfoBufferCount > 90)
    {
        lfoBufferCount = 0;
        for (int i = 0; i < 90; i++)
        {
            lfoBuffer[i] = 0;
        }
    }
}

void updateDisplay(uint8_t *buffer, uint8_t *ext_buffer)
{
    int centre = 0;
    display.clear();

    for (int x = 128; x < (BUFFER_SIZE - 128); x++)
    {
        float value = ext_buffer[x] * 0.2;
        float value2 = ext_buffer[x + 1] * 0.2;
        if ((value < 30) && (value2 > 30))
        {
            centre = x;
            latchTimeout = 0;
            break;
        }
    }
    if (centre != 0)
    {
        for (int column = 0; column < 127; column++)
        {
            display.drawLine(column, (64 - buffer[centre - 128 + column * 2] / 4), column + 1, (64 - buffer[centre - 128 + (column + 1) * 2] / 4));
        }
        display.drawGrid();
        display.updateDisplay();
    }
    else if (latchTimeout > 250)
    {
        for (int column = 0; column < 127; column++)
        {
            display.drawLine(column, (64 - buffer[64 + column] / 5), column + 1, (64 - buffer[64 + column + 1] / 5));
        }
        display.drawGrid();
        display.updateDisplay();
    }
}

void timer0_callback(void)
{
    adc->adc0->startSingleRead(readPinIn);
}

void timer1_callback(void)
{
    adc->adc0->startSingleRead(readPinExt);
}

void adc0_isr()
{
    uint8_t pin = ADC::sc1a2channelADC0[ADC0_SC1A & ADC_SC1A_CHANNELS];

    if (pin == A0)
    {
        uint16_t adc_val = adc->adc0->readSingle();
        if (!in_buffer0Ready)
        {
            if (in_buffer_0_count < BUFFER_SIZE)
            {
                in_buffer_0[in_buffer_0_count] = adc_val;
                in_buffer_0_count++;
                if (in_buffer_0_count == BUFFER_SIZE)
                {
                    in_buffer_0_count = 0;
                    in_buffer0Ready = true;
                }
            }
        }

        else
        {
            if (in_buffer_1_count < BUFFER_SIZE)
            {
                in_buffer_1[in_buffer_1_count] = adc_val;
                in_buffer_1_count++;
            }
            if (in_buffer_1_count == BUFFER_SIZE)
            {
                in_buffer0Ready = false;
                in_buffer_1_count = 0;
            }
        }
    }

    if (pin == A1)
    {
        uint16_t adc_val = adc->adc0->readSingle();
        if (!in_buffer0Ready)
        {
            if (ext_buffer_0_count < BUFFER_SIZE)
            {
                ext_buffer_0[in_buffer_0_count] = adc_val;
                ext_buffer_0_count++;
                if (ext_buffer_0_count == BUFFER_SIZE)
                {
                    ext_buffer_0_count = 0;
                    ext_buffer0Ready = true;
                }
            }
        }

        else
        {
            if (ext_buffer_1_count < BUFFER_SIZE)
            {
                ext_buffer_1[in_buffer_1_count] = adc_val;
                ext_buffer_1_count++;
            }
            if (ext_buffer_1_count == BUFFER_SIZE)
            {
                ext_buffer0Ready = false;
                ext_buffer_1_count = 0;
            }
        }
    }

    // restore ADC config if it was in use before being interrupted by the analog timer
    if (adc->adc0->adcWasInUse)
    {
        // restore ADC config, and restart conversion
        adc->adc0->loadConfig(&adc->adc0->adc_config);
        // avoid a conversion started by this isr to repeat itself
        adc->adc0->adcWasInUse = false;
    }
}

void setup()
{
    Serial.begin(9600);
    delay(1000);

    display.init();
    display.displayOn();

    display.drawGrid();
    display.updateDisplay();

    pinMode(readPinIn, INPUT);
    pinMode(readPinExt, INPUT);

    adc->adc0->setAveraging(8);                                           // set number of averages
    adc->adc0->setResolution(8);                                          // set bits of resolution
    adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED); // change the conversion speed
    adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);     // change the sampling speed

    startTimerValue0 = timer0.begin(timer0_callback, 15);

    delayMicroseconds(12);

    startTimerValue1 = timer1.begin(timer1_callback, 15);

    adc->adc0->enableInterrupts(adc0_isr);

    delay(500);
}

int value = 0;
char c = 0;

void loop()
{

    if (lfoMode)
    {
        updateDisplayLFO(in_buffer_0);
    }

    else
    {
        if (in_buffer0Ready)
        {
            updateDisplay(in_buffer_0, ext_buffer_0);
        }
        if (!in_buffer0Ready)
        {
            updateDisplay(in_buffer_1, ext_buffer_1);
        }
        delayMicroseconds(refreshPeriod);
    }
}