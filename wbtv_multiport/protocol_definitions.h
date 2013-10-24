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
//Define if potocol is running over wired or bus and not direct full duplex point to point
//#define WIREDOR

