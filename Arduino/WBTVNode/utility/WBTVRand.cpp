#include <stdint.h>
#include "utility/WBTVRand.h"
#include "WBTVNode.h"

#ifdef WBTV_USE_XORSHIFT32
static uint32_t y;
void WBTV_doRand()
{
    y^=micros();
    y^=y<<2;y^=y>>7;y^=y<<7;
}
void WBTV_doRand(uint32_t seed)
{
    y^=micros()+seed;
    y^=y<<2;y^=y>>7;y^=y<<7;
}
#endif

#ifdef WBTV_USE_XORSHIFT64
static uint64_t y=88172645463325252LL;
void WBTV_doRand()
{
    y^=micros();
    y^=(y<<1); y^=(y>>7); y^=(y<<44);
}

void WBTV_doRand(uint32_t seed)
{
    y^=micros()+seed;
    y^=(y<<1); y^=(y>>7); y^=(y<<44);
}
#endif

uint32_t WBTV_rawrand()
{
    WBTV_doRand();
    return (uint32_t)y;
}

unsigned long WBTV_rand(unsigned long min, unsigned long max)
{
    WBTV_doRand();
    return(((uint32_t)y%((max+1)-min))+min);
}

long WBTV_rand(long min,long max)
{
    WBTV_doRand();
    return(((uint32_t)y%((max+1)-min))+min);
}

unsigned char WBTV_rand(unsigned char min, unsigned char max)
{
    WBTV_doRand();
    return(((uint32_t)y%((max+1)-min))+min);
}

unsigned int WBTV_rand(unsigned int min, unsigned int max)
{
    WBTV_doRand();
    return(((uint32_t)y%((max+1)-min))+min);
}

int WBTV_rand(int min, int max)
{
    WBTV_doRand();
    return(((uint32_t)y%((max+1)-min))+min);
}

unsigned int WBTV_rand(unsigned int max)
{
    WBTV_doRand();
    return((uint32_t)y%(max+1));
}

unsigned char WBTV_rand(unsigned char max)
{
    WBTV_doRand();
    return((uint32_t)y%(max+1));
}

float WBTV_rand_float()
{
    WBTV_doRand();
    return((float)4294967295/y);

}

unsigned char WBTV_urand_byte()
{
    WBTV_doRand();
    //Add together bytes 0 and 2 to produce our final output byte.
    //No, I do not know why this improves statistical performance.
    //I heard stories of people multiplying the output by a constant.
    //That was too slow IMHO so I XORed two bytes together and it works.
    return(((uint32_t)y>>16)+(uint32_t)y);
}


#if defined( __AVR_ATmega32U4__) || defined(__MSP430G2452__) || defined(__MSP430G2553__) || defined(__MSP430G2231__) || defined( __AVR_ATmega328P__) || defined( __AVR_ATmega168P__) || defined( __AVR_ATmega328__) || defined( __AVR_ATmega168__)
//On supported platforms, gather enough entrop from the temperature sensor to generate one random byte.
//Use this followed by urand_byte when you need a high qualit random byte, or simply call this when you have time
//To add in entropy. May block for several seconds, true entropy is slow to generate.

unsigned int getTemperature()
{

#ifdef  __AVR_ATmega32U4__
unsigned char oldADCSRA = ADCSRA;
unsigned char oldADMUX = ADMUX;
unsigned char oldADCSRB = ADCSRB;

  //disable ADC...now new values can be written in MUX register
  ADCSRA &= ~(1 << ADEN);   
  // Set MUX to use on-chip temperature sensor
  ADMUX = (1 << MUX0) | (1 << MUX1) | (1 << MUX2);
  ADCSRB =  (1 << MUX5);   // MUX 5 bit part of ADCSRB

  ADCSRB |=  (1 << ADHSM);   // High speed mode

  // Set Voltage Reference to internal 2.56V reference with external capacitor on AREF pin
  ADMUX |= (1 << REFS1) | (1 << REFS0);

  // Enable ADC conversions
  ADCSRA |= (1 << ADEN);

  ADCSRA |= (1 << ADSC);

  while(!(ADCSRA & (1 << ADIF)));
  ADCSRA |= (1 << ADIF);

  delayMicroseconds(5);

  //Leave things like we found them
  ADMUX = oldADMUX;
  ADCSRB = oldADCSRB;
  

  return ADC;
#elif defined(__MSP430G2452__) || defined(__MSP430G2553__) || defined(__MSP430G2231__) 
  return analogRead(TEMPSENSOR);

#elif defined( __AVR_ATmega328P__) || defined( __AVR_ATmega168P__) || defined( __AVR_ATmega328__) || defined( __AVR_ATmega168__)
  unsigned int wADC;
  
  unsigned char oldADCSRA = ADCSRA;
unsigned char oldADMUX = ADMUX;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC

  delay(20);            // wait for voltages to become stable.

  ADCSRA |= _BV(ADSC);  // Start the ADC

    // Detect end-of-conversion
  while (bit_is_set(ADCSRA,ADSC));

  // Reading register "ADCW" takes care of how to read ADCL and ADCH.
  wADC = ADCW;
  
  //Leave things like we found them
  ADMUX = oldADMUX;
  
  return(wADC);
#endif
}

void WBTV_true_rand()
{
    unsigned char last =0;
    unsigned char changes =0;
    unsigned char temp = 0;
    unsigned char sames =0;
    while((changes<12)|(sames<12))
    {
     temp = getTemperature();
     if(!(temp==last))
     {
        changes++;
        last=temp;
     }
     else
     {
        sames++;
     }
     WBTV_doRand(temp);
    }
}
#endif
