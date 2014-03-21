#ifndef __WBTV_RAND_HEADER__
#define __WBTV_RAND_HEADER__
void WBTV_doRand();
void WBTV_doRand(uint32_t seed);
uint32_t WBTV_rawrand();
unsigned long WBTV_rand(unsigned long min, unsigned long max);
long WBTV_rand(long min,long max);
unsigned char WBTV_rand(unsigned char min, unsigned char max);
unsigned int WBTV_rand(unsigned int min, unsigned int max);
int WBTV_rand(int min, int max);
unsigned int WBTV_rand(unsigned int max);
unsigned char WBTV_rand(unsigned char max);
unsigned char WBTV_urand_byte();
int WBTV_rand(int max);
long WBTV_rand(long max);
float WBTV_rand(float max);
float WBTV_rand(double max);
float WBTV_rand(float min,float max);
float WBTV_rand(double min,double max);
float WBTV_rand(long min,float max);
float WBTV_rand(float min,long max);
float WBTV_rand(int min,float max);
float WBTV_rand(float min,int max);
float WBTV_rand(int min,double max);
float WBTV_rand(double min,int max);
float WBTV_rand(long min,double max);
float WBTV_rand(double min,long max);
float WBTV_rand(unsigned char min,float max);
float WBTV_rand(float min,unsigned char max);
float WBTV_rand_float();

#ifdef  __AVR_ATmega32U4__
#define WBTV_HW_ENTROPY
#elif defined(__MSP430G2452__) || defined(__MSP430G2553__) || defined(__MSP430G2231__) 
#define WBTV_HW_ENTROPY
#elif defined( __AVR_ATmega328P__) || defined( __AVR_ATmega168P__) || defined( __AVR_ATmega328__) || defined( __AVR_ATmega168__)
#define WBTV_HW_ENTROPY
#endif

#ifdef WBTV_HW_ENTROPY
void WBTV_get_entropy();
#endif

#endif
