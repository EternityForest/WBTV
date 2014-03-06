#include <stdint.h>
#include <stdio.h>
#include "WBTVNode.h"

static uint32_t y;

void WBTV_doRand()
{
    y^=micros();
    y^=y<<2;y^=y>>7;y^=y<<7;
}

void WBTV_doRand(uint32_t seed)
{
    y^=y<<2;y^=y>>7;y^=y<<7;
    y^=micros()+seed;
}

uint32_t WBTV_rawrand()
{
    WBTV_doRand();
    return y;
}

unsigned long WBTV_rand(unsigned long min, unsigned long max)
{
    WBTV_doRand();
    return(((y&0xFFFF)%((max+1)-min))+min);
}

long WBTV_rand(long min,long max)
{
    WBTV_doRand();
    return(((y&0xFFFF)%((max+1)-min))+min);
}

unsigned char WBTV_rand(unsigned char min, unsigned char max)
{
    WBTV_doRand();
    return(((y&0xFF)%((max+1)-min))+min);
}

unsigned int WBTV_rand(unsigned int min, unsigned int max)
{
    WBTV_doRand();
    return(((y&0xFFFF)%((max+1)-min))+min);
}

int WBTV_rand(int min, int max)
{
    WBTV_doRand();
    return(((y&0xFFFF)%((max+1)-min))+min);
}

unsigned int WBTV_rand(unsigned int max)
{
    WBTV_doRand();
    return((y&0xFFFF)%(max+1));
}

unsigned char WBTV_rand(unsigned char max)
{
    WBTV_doRand();
    return((y&0xFF)%(max+1));
}

unsigned char WBTV_urand_byte()
{
    WBTV_doRand();
    return(y&255);
}