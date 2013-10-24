
#include "wbtv.h"
WBTVNode node(&Serial,0);
//The pin used to sense bus activity
//(in case some platform doesn't let you digitalread from an RX pin )
#define BUS_SENSE_PIN 0
  unsigned char a5val;
    //Last time the periodic message was sent
  unsigned long timeLastSent;
void setup()
{

  pinMode(A5,INPUT);
  Serial.begin(9600);
  node.setBinaryCallback(&processNewMessage);
  delay(2500);
  node.stringSendMessage("test~   ","string");
}

void loop()
{
  if (Serial.available())
  {
    node.decodeChar(Serial.read());
  }

  if ((millis()-timeLastSent) >2500)
  {
    a5val = analogRead(A5);
    timeLastSent = millis();
    node.sendMessage((unsigned char *)"A5Value",7,&a5val,1);
  }

}


void processNewMessage(
unsigned char * header, 
unsigned char headerlen, 
unsigned char * data, 
unsigned char datalen)
{ 
  node.sendMessage((unsigned char *)"IGotAMessage!!!",15,data,datalen); 
}

