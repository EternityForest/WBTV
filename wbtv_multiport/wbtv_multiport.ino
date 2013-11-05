
#include "wbtv.h"

//This sketch uses the WBTV protocol to periodically send an analog reading.

WBTVNode node(&Serial);
unsigned char a5val;
//Last time the periodic message was sent
unsigned long timeLastSent;

void setup()
{

  pinMode(A5,INPUT);
  Serial.begin(9600);
  //Here we say that when we get a new packet, we want to pass it processNewMessage
  //If we know there are no null bytes in the messages we want, we could use string callbacks,
  //Which don't have the length args and get null terminators instead
  //Note that with those, ABCD\x00 will look the same as ABCD\x00EFG to most string handling functions
  node.setBinaryCallback(&processNewMessage);
  delay(2500);
  //Send a message, just for a test
  node.stringSendMessage("test~   ","string");
}

void loop()
{
  //While there is serial data available, we process it with decodeChar
  if (Serial.available())
  {
    node.decodeChar(Serial.read());
  }
 
  //If it has been long enough since the last one, send another message with the analog value
  if ((millis()-timeLastSent) >2500)
  {
    a5val = analogRead(A5);
    timeLastSent = millis();
    node.sendMessage((unsigned char *)"A5Value",7,&a5val,1);
  }

}

///When we get a new message, we want to echo it's data on the igitamessage channel
void processNewMessage(
unsigned char * header, 
unsigned char headerlen, 
unsigned char * data, 
unsigned char datalen)
{ 
  node.sendMessage((unsigned char *)"IGotAMessage!!!",15,data,datalen); 
}


