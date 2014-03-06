#include "WBTVNode.h"

//This sketch listens for one byte messages on the channel "brightness"
//And uses the one byte value to set the PWM for pin 13 which has an LED on it on the leonardo.

WBTVNode usb(&Serial);


void setup()
{
  delay(500);
  Serial.begin(9600);
  usb.setBinaryCallback(&onMessageFromComputer);
  pinMode(13,OUTPUT);
}

void loop()
{
  usb.service();
  
 }


void onMessageFromComputer(unsigned char * channel, unsigned char  clength, unsigned char * data, unsigned char dlength)
{
unsigned char p;
if (memcmp(channel,"brightness",clength)==0)
{
p = read_interpret(data,unsigned char);
analogWrite(13,p);
}
}