#include "WBTVNode.h"

//This sketch uses the WBTV protocol to periodically send an analog reading.


//Create A WBTVNode object called node, that uses the direct leonardo serial.
WBTVNode node(&Serial);

unsigned char a5val;
//Last time the periodic message was sent
unsigned long timeLastSent;

void setup()
{
  pinMode(A5,INPUT);
  Serial.begin(9600);
}

void loop()
{
 
  //If it has been long enough since the last one, send another message with the analog value
  if ((millis()-timeLastSent) >1000)
  {
    a5val = analogRead(A5);
    timeLastSent = millis();
    node.sendMessage((unsigned char *)"A5Value",7,&a5val,1);
  }

}

