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