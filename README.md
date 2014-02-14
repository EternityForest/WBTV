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

####WBTVClock_get_time()

Returns a struct with two fields, one is a 64 bit unix timestamp, the other a
32 bit unsigned binary fraction part of the current time.
The WBTV clock is automatically set by incoming TIME messages, and can be manally set.


####WBTVClock_set_time(long long time, uint16_t fraction, uint32_t error)
Set the WBTV internal clock by passing the current UNIX time number as a long long,
the fractional part of the time in seconds/2**16, and the estimated error of the time source
in seconds/2**16. Should the error be too great to represent in 32 bits, use 4294967294.
Human entered times should be suspect and should have an estimated error of at least several minutes.

If you really need accurate time, use a GPS reciever to set the clock.

####WBTV_Clock_error_per_second
This 16 bit value represents the estimated accuracy of the internal oscillator.
It is in parts per 2**16, and defaults to 2500, or 4%.
Suggested conservative values based on your crystal:
    0.5% = 350(Arduino
    250ppm = 20
    100ppm = 10
    20ppm = 3
    10ppm or better = 1;

####WBTVClock_invalidate()
Tell WBTV that it's current time is inaccurate. Used to force manual setting.
This is just a macro that sets the error.

####WBTVClock_error

The error estimate of the clock in seconds/2**16
The special value 4294967295 means totally unsynchronized,
and 4294967294 means synchronized but the error is greater than can be represented.

You do not need to do anything to synchronize the clock, WBTVNode objects
automatically keep track of TIME broadcasts. TIME messages contain their
own estimated error, so a device will ignore TIME messages that do not advertise
at least as much accuracy as the current internal estimate.
(based on the error in the initial setting plus 4% of the time elapsed since being set)

Crystals are almost always better than 4%, but we are conservative and we want to account for
devices using the internal resonator.

####WBTV_CLOCK_UNSYNCHRONIZED
Macro for 4294967295, which means the clock has never been synchronized.

####WBTV_CLOCK_HIGH_ERROR
Macro for 4294967294, meaning the error is too high to count(Greater than 20 minutes or so)
    
####WBTVNode(stream *)
Represents one direct point to point WBTV packet connection.
Used for e.g. the leonardo's USB to serial. This assumes a full duplex
channel and doesn't do any of the wired-OR or CSMA/CD stuff.

####WBTVNode(stream *, pin#)
Creates a WBTV node for accessing a bus. The pin number must be the RX pin.
This pin is used for collision avoidance and detection.

####WBTVNode.sendMessage(byte * channel, byte channellen, byte * data, byte datalen)
Send a message that my contain NULs by supplying a channel and a length

####WBTVNode.stringSendMessage(char * channel, char * data)
Same as sendMessage, but uses null terminated strings instead of pointer-length pairs.

####WBTVNode.setStringCallback(f)
Set the callback to handle new messages.
f is a function pointer to a function taking two char *'s , the channel and the data.

####WBTVNode.setBinaryCallback(f)
Set the binary callback. f takes (unsigned char * channel, unsigned char channellen, unsigned char* data, unsigned char datalen)
Note there may only be one callback at a time. string and binary callbacks both delete whatever callback was already there.

####read_interpret(unsigned char*, type)
This is a macro you may find useful when parsing the contents of a message.
You pass it a pointer and a type, and it takes the first sizeof(type) bytes from the pointer
and interprets them as type and returns that, then it increments the pointer by that that many bytes.

This is very useful for when you have a message that contains several values of different types.

Example:
    x = read_interpret(ptr, float)

Will take the first 4 bytes at the pointer, interpret them as a float and return that, and then
increment the pointer by four.

This lets you treat a pointer as a stream of various different types.

    
