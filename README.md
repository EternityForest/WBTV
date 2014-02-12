WBTV
====

pseudo-pubsub for embedded devices using wired-OR. Under heavy development, may or may not work at any given
moment. API may change. Protocl may change. Should be finalized within a few weeks.


##Arduino Library

###Electrical Connections

To use with a single wire wired-or bus as you would I2C or such,
Pull up the bus wire with a 4.7k or so resistor, connect all your TX pins to the bus
using a diode(facing the TX pins), to make them act as pen collector ouputs.
Connect your RX pins directly to the bus.

###SW Example
    #include "WBTVNode.h"
    
    //Create a WBTV node object(Yes, you can have multiple)
    //You must tell it what pin is used for RX, because it must be able to sample
    //The level of the line for it's collision avoidance.
    
    WBTVNode uart(&Serial,0);
    
    void send()
    {
        //This line sends the message "Hi" to a channel called "Everyone"
        uart.stringSendMessage("Everyone","Hi");
    }
    
    void setup()
    {
        Serial.begin(9600);
        uart.setStringCallback(&callback)
        
    }
    void callback(char *channel, char *message)
        {
            //Do something with the message you just got here.
            //The message and data will be two null terminated strings.
        }
###API(Incomplete documentation)

WBTVClock_get_time()

Returns a struct with two fields, one is a 64 bit unix timestamp, the other a
32 bit unsigned binary fraction part of the current time.

WBTVClock_error

The error estimate of the clock in seconds/2**16
The special value 4294967295 means totally unsynchronized,
and 4294967294 means synchronized but the error is greater than can be represented.

You do not need to do anything to synchronize the clock, WBTVNode objects
automatically keep track of TIME broadcasts. TIME messages contain their
own estimated error, so a device will ignore TIME messages that do not advertise
at least as much accuracy as the current internal estimate
(based on the error in the initial setting plus 0.5% of the time elapsed since being set)
    
WBTVNode(stream *)
Represents one direct point to point WBTV packet connection.
Used for e.g. the leonardo's USB to serial. This assumes a full duplex
channel and doesn't do any of the wired-OR or CSMA/CD stuff.

WBTVNode(stream *, pin#)
Creates a WBTV node for accessing a bus. The pin number must be the RX pin.
This pin is used for collision avoidance and detection.


WBTVNode.sendMessage(byte * channel, byte channellen, byte * data, byte datalen)
    Send a message that my contain NULs by supplying a channel and a length
    

    