#include "WBTVNode.h"

//This sketch interfaces with real wired-or bus devices from a usb port using an arduino leonardo.


//Basically it uses the leonardo as a usb to serial device that handles the super fast timing that a normal
//usb to serial device can't. Just connect your leonardo's RX pin to the bus, and the TX pin through a diode(facing TX)
//and pull up the bus. Or, connect RX and TX to a CAN tranciever.


//This application also demonstrates TIME broadcast functionality.
//The LED on the leonardo will flash at the top of every second.
//The device will send the current time out of both of it's ports every 5 seconds.

//Create A WBTVNode object called usb, that uses the direct leonardo serial.
WBTVNode usb(&Serial);

//Tell it that the pin to be used for directly sampling the bus voltage is 0, i.e.RX on leonardo
WBTVNode uart(&Serial1,0);
unsigned long lastTime;


void setup()
{
  delay(500);
  Serial.begin(9600);
  
  //This line sets the speed of the hardware network. Change if needed.
  Serial1.begin(9600);
  
  usb.setBinaryCallback(&onMessageFromComputer);
  uart.setBinaryCallback(&onMessageFromUART);
  pinMode(13,OUTPUT);
  
}

void loop()
{
  usb.service();
  uart.service();
  
  
  //If the clock has been set by a TIME broadcast
  if(WBTVClock_error < 4294967295)
 {
    if (millis()-lastTime >5000)
    {
        usb.stringSendMessage("CONV","Status: Online");

      usb.sendTime();
      uart.sendTime();
      lastTime = millis();
    }
 }

//PPS output on digital pin 13. The LED will blink once per second.
//Note that when the time is set there may be hordes of jitter.
//Also note that the pulses will still be output even if the clock is not synchronized.
//This is really just a cool light show added in to test the accuracy of the internal clock.
 if(WBTVClock_get_time().fraction < 2000)
 {
   digitalWrite(13,HIGH);
 }
 else
 {
   digitalWrite(13,LOW);
 }
}

void onMessageFromComputer(unsigned char * channel, unsigned char  clength, unsigned char * data, unsigned char dlength)
{
  uart.sendMessage(channel,clength,data,dlength);
}

void onMessageFromUART(unsigned char * channel, unsigned char  clength, unsigned char * data, unsigned char dlength)
{
  usb.sendMessage(channel,clength,data,dlength);
}
