//The state of the bus when nothing is being sent.
#define BUS_IDLE_STATE 1

//The maximum time to wait and snoop bus before transmitting. must be at least one char time
#define MAX_WAIT 5

//How much space to resserve for the message buffer
#define MAX_MESSAGE 64

//Protocol symbol constants for STart of Header, STart of Text,
//End of Transmission, and ESCape.
#define STH '!'
#define STX '~'
#define EOT '\n'
#define ESC '\\'


#define HASH_STX

//Comment this to disable recording the packet arrival times.
#define RECORD_TIME

//Comment this to disable automatic handling of certain messages.
//If left enabled, TIME broadcasts will be handled automatically for you,
//And millisecond level time access functions will be provided.
#define ADV_MODE

//Continually seed arduino's internal RNG with data from the
//Exact contents and arrival times of packets. This is very fast because
//We are already calculating the hash of every packet.
//However, you might want to comment this out if you want a specific sequence
//of random values and you don't want WBTV to mess with it.
//Uncomment this to enable seeding the RNG, comment to disable.
//#define WBTV_SEED_ARDUINO_RNG

//Enable the internal RNG. The WBTV_rand,WBTV_doRand, etc functions will not work if this is disabled.
//If this is enabled, an internal 32 bit modiied keyed marsaglia xorshit rng will be used as an entropy pool.
//Whenever it is accessed, the micros() value will be used to reseed, and the micros value also acts as the key,
//affecting the random sequence, so the actual period should be much much longer in practice and if there is
//any user input or network traffic at all than the random values should be of a fairly high quality, though
//definately not up to any kind of cryptographic standards at all.

//Also, if this is enabled and WBTV_SEED_ARDUINO_RNG is disables, WBTV won't touch the arduino's built in RNG at all.
//As a matter of fact, you probably shouldn't either, because the WBTV version is much faster and self seeding.
#define WBTV_ENABLE_RNG
