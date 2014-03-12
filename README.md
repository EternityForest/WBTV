WBTV
====

pseudo-pubsub for embedded devices using wired-OR. Under heavy development, may or may not work at any given
moment. API may change. Protocl may change. Should be finalized within a few weeks.

##Protocol Spec Documents
The "official" version is WBTV_specification.odt. All other versions are generated from this file.
Files in "development" represent possible future versions and as such have no bearing on the actual protocol spcification.

##Protocol Summary

The protocol works over wired-OR busses, such as a single wire pulled to 5V and driven with open collector outputs, or CAN bus hardware.

The protocol is based on the concept of posting "messages" to "channels", similar to how CANBus attatches message IDs to messages, or how message-oriented middleware works.

The message structure is very simple

    !!<channel>~<data>CS\n

Every message begins with an exclamation point, followed by the channel, than a tilde, than the actual data of the message, then a two-byte checksum(more on that later), followed by a newline as an end of message indicator.

Generally an extra exclamation point is sent before the "real" start of message on the off chance that there was noise on the bus that looked like an escape character.

If a tilde, an exclamation point, or a newline needs to appear in the data,channel name, or checksum, it can simply be escaped by using a backslash. Backslashes also escape themselves.

WBTV is multi-drop through a mechanism called carrier-sense multiple access. When a device wants to send, it first listens to ensure the bus is not busy. While sending, it monitors the bus to detect interference. When interference is detected, both devices back off for a random period and retry.

The use of control characters as delimiters rather than length bytes makes cancelling and retrying a failed transmission much simpler and faster as there is no need to wait for a timeout period to elapse.

The actual documentation includes information about reserved channel names that add compatibility, such as a standard way to send the current time over the bus, and ways of automatic device discovery are in the works.

##Arduino Library

An Arduino library which should work with any Arduino clone or compatible board also has been provied, supporting multiple interfaces on one board, wired-OR and full duplex links, random number generation, and automatic handling of TIME messages.

The decision to include random numbers was based on tight integration with WBTV so as to gather entropy from network traffic,
and because WBTV requires random numbers for network timing and backoffs, and the avr-libc generator is several hundred bytes larger than the builtin RNG that WBTV has. User access is provided to the entropy pool to allow access to the gathered entropy and reduce the sketch size.

TIME handling is included because sending a TIME message is a low level action requiring millisecond level timing for the best results.

###Electrical Connections

To use with a single wire wired-or bus as you would I2C or such,
Pull up the bus wire with a 4.7k or so resistor, connect all your TX pins to the bus
using a diode(facing the TX pins), to make them act as pen collector ouputs.
Connect your RX pins directly to the bus.

To use with a simple point-to-point connection, connect the RX of the first board to the TX of the other just like any other serial connection. You can also use WBTV in a one-way link.

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
###Arduino lib API(Incomplete documentation)

####Creating WBTV Objects
The library supports multiple interfaces, each associated with a stream.

####WBTVNode(stream *)
Represents one direct point to point WBTV packet connection.
Used for e.g. the leonardo's USB to serial. This assumes a full duplex
channel and doesn't do any of the wired-OR or CSMA/CD stuff.

####WBTVNode(stream *, pin#)
Creates a WBTV node for accessing a bus. The pin number must be the RX pin.
This pin is used for collision avoidance and detection.


####The Built in Entropy Pool
WBTVNode maintains an internal 32-bit modified XORshift RNG which may be faster than the RNG functions on your platform.
Whenever a new packet arrives, the packet arrival time, and the checksum of the packet is mixed into the state.
Every time you request a random number, the exact micros() value of the request is mixed in. Therefore the random numbers depends on the complete history of arriving messages, and the exact time at which you request the number(which is affected by the entire history of program execution, interrupts, etc), making them likely suitable for anything except cryptographic purposes.

The entropy pool is global, as are all functions associated with it, and is shared by all interfaces.

Note once again that the marsaglia XORShift generators are NOT cryptographically secure in any way, and that WBTV by default only uses 32 bits of PRNG state. The period of an XORShift-32 genrator is normall 2^32. However because we mix in the current micros() value with every single request, there really isn't a period that can be calculated easily and even if there was, it would change from sketch to sketch.

In the file utility/protocol_definitions you can change the internal RNG to a 64 bit, or some other options.

####WBTV_rand(unsigned long max)
Return a random number from the WBTV internal entropy pool from 0 to max inclusive.

####WBTV_rand(long max, long min)
Return a random integer between min and max inclusive.

####WBTV_doRand([long seed])
Mix the current micros() value into the entropy pool. You can call this when something that happens with
unpredictable timing occurs. You can also supply the optional seed parameter to mix in additional entropy.
I don't think there is any way calling this function with bad data or at the wrong time can hurt anything.
You might be able to produce high-quality numbers by mixing in a lot of entropy for each byte you read.

This function is the same as the internal RNG iteration function.

####WBTV_rawrand()
Returns a raw 32 bit random value straight from the LFSR.

####WBTV_urand_byte()
Return a single random byte.

####The WBTV Internal Clock
WBTV Maintains an estimate of the current UTC time of day by keeping track of TIME messages. TIME messages are not passed
to user callbacks as the decoding of them is fairly complex.

The WBTV internal clock maintains an estimate of the accumulated error. When a TIME message arrives, it is ignored unless it advertises equal or better accuracy to the internal current estimate.

The clock uses an internal 1ms resolution based on the millis() value.

The WBTV Clock, like the entropy pooling, is global and shared by all interfaces.

####WBTVClock_get_time()

Returns a struct called a WBTV_Time_t with two fields, one is a 64 bit unix timestamp, the other a
32 bit unsigned binary fraction part of the current time.
The WBTV clock is automatically set by incoming TIME messages, and can be manally set.

####WBTVNode.sendTime()

Send a standard WBTV TIME message containing th current internal clock value and the current
internal error estimate. It is probably a bad idea to send TIME messages with the normal sendMessage API.
Instead, set the clock and then use sendTime.

####WBTVClock_set_time(long long time, uint16_t fraction, uint32_t error)
Set the WBTV internal clock by passing the current UNIX time number as a long long,
the fractional part of the time in seconds/2**16, and the estimated error of the time source
in seconds/2**16. Should the error be too great to represent in 32 bits, use 4294967294.
Human entered times should be suspect and should have an estimated error of at least several minutes.

If you really need accurate time, use a GPS reciever to set the clock.

Note that this function will do absolutely nothing if error>current internal error estimate.
Use WBTVClock_invalidate() first to force setting the time.

####WBTV_Clock_error_per_second
This 16 bit value represents the estimated accuracy of the internal oscillator.
It is in parts per 2**16, and defaults to 2500, or 4%, but you can write a smallr value if your oscillator is better.
Suggested conservative values based on your crystal

    0.5% = 350(Common ceramic resonator)
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

####WBTV Core Functions
These are the functions dealing with sending and recieving messages.

####WBTVNode.service()
Tell the node to check the serial buffer, process up to one character,
and when a complete message is recieved, either pass it off to the registered callback
or, if it is a TIME message, use it to set the internal clock.
This will block either only for microseconds while processing one byte,
or for as long as the callback takes when processing a full message. 

####WBTVNode.sendMessage(byte * channel, byte channellen, byte * data, byte datalen)
Send a message that my contain NULs by supplying a channel and a length.
Note that this is a blocking function when used with wired-OR busses,
and the time cannot be predicted without knowing the full list of devices on the bus.
Heavily loaded networks may block for a long time, and if the termination resistor fails and nothing pulls the bus up,
this function may block indefinately.These drawbacks are considered acceptable in the low risk/non critical, low speed, consumer type applications WBTV was intended for.

This function, when used with WBTV over point to point(full duplex) links, should not block for any longer than one would expect a similar Serial.print statement to block for. 

####WBTVNode.stringSendMessage(char * channel, char * data)
Same as sendMessage, but uses null terminated strings instead of pointer-length pairs. Blocks(or doesn't block) in the same way as the binary version.


####WBTVNode.setStringCallback(f)
Set the callback to handle new messages.
f is a function pointer to a function taking two char *'s , the channel and the data. If you use a string callback, any message with a NUL byte in the channel name will simply be dropped and ignored. Messages with NULs in the actuall data may still get through and appear truncated to any function depending on the null terminator. 

####WBTVNode.setBinaryCallback(f)
Set the binary callback. f takes (unsigned char * channel, unsigned char channellen, unsigned char* data, unsigned char datalen)

Note there may only be one callback at a time. string and binary callbacks both delete whatever callback was already there.

####WBTVNode.MIN_BACKOFF and MAX_BACKOFF
The minimum and maximum times to wait before sending a message in microseconds.
MIN_BACKOFF needs to be at least 1 byte-time at whatever baud rate you run at.
MAX_BACKOFF should be at least few dozen microseconds larger than MIN_BACKOFF for arbitration to function properly.

These default to 1100 and 1200, for operation at 9600 baud.
If your baud rate is higher you should change these or sending a message might get interuptd a lot and take a long time.

####Useful Macros

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

####Untested Stuff

This stuff might go through API changes, not work, or dissapear entirely later.

#### WBTV_true_rand()
This function is available on 32u4, 168, 328, and certain MSP430 boards with energia.
It will read the temperature sensor a few hundred times to add about 1 byte of entropy to the entropy pool.

##Python Library

The python library really just consists of one file, wbtv.py. Copy it where you need it and import it.
Note that USB-to-serial was not designed for the ultra fast timing the arbitration requires. This library
can only listen in on networks, and interface with WBTV point to point(full duplex) devices because of this.

A sketch is included in the arduino examples directory that acts as a repeater, bridging between a wired-OR bus and a PC serial port, but it only runs on devices with two or more hardware interfaces like the Leonardo.

The wbtv module has the following:

###wbtv.Node(port,rate)

Class representing one serial interface.

###Node.poll()
Read the port, polling for new messages. Returns all new messages as a list of tuples, where the first element is a bytearray of the channel and the second element is a bytearray of the message.

###Node.send(channel, data)
Send a message to channel containing data. strings, bytes, and bytearrays are acceptable.

###Node.sendTime(accuracy)
Send a TIME broadcast. Accuracy is the accuracy to advertise, in maximum seconds of error.
