#include "WBTVNode.h"

//This sketch interfaces with real wired-or bus devices from a usb port.
//Basically it uses the leonardo as a usb to serial device that handles the super fast timing that a normal
//usb to serial device can't. Just connect your leonardo's RX pin to the bus, and the TX pin through a diode(facing TX)
//and pull up the bus. Or, connect RX and TX to a CAN tranciever.


//Create A WBTVNode object called usb, that uses the direct leonardo serial.
WBTVNode usb(&Serial);

//Tell it that the pin to be used for directly sampling the bus voltage is 0, i.e.RX on leonardo
WBTVNode uart(&Serial1,0);



void setup()
{
  Serial.begin(9600);
    usb.setBinaryCallback(&onMessageFromComputer);
    uart.setBinaryCallback(&onMessageFromUART);

}

void loop()
{

}

void onMessageFromComputer(unsigned char * channel, unsigned char  clength, unsigned char * data, unsigned char dlength)
{
    uart.send(channel,clength,data,dlength);
}

void onMessageFromUART(unsigned char * channel, unsigned char  clength, unsigned char * data, unsigned char dlength)
{
    usb.send(channel,clength,data,dlength);
}
