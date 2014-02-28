#ifndef _WBTVNode
#define _WBTVNode
#include <Arduino.h>
#include "protocol_definitions.h"
#include "HardwareSerial.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>

/*
This class represents one WBTV node. You must pass it: the adress of a Stream object(like a serial port),
 the pin that is connected to the bus(for arduino and msp430 should be the same as te RX pin in question,
 and a callback that gets (channel*,channellen,data*,datalen) when a new message has been recieved.
 
 call parseByte over each new byte that comes in the port, and call 
 */



class WBTVNode
{
public:
  void sendMessage(const unsigned char * channel, const unsigned char channellen, const unsigned char * data, const unsigned char datalen);
  void stringSendMessage(const char *channel, const char *data);
  void decodeChar(unsigned char chr);
  void service();
  WBTVNode( Stream *, int bus_sense_pin);
  WBTVNode( Stream *);
  
  void setBinaryCallback(
  void (*thecallback)(
  unsigned char *, 
  unsigned char , 
  unsigned char *, 
  unsigned char ));

  void setStringCallback(
  void (*thecallback)(
   char *, 
   char *));
  
  unsigned int MIN_BACKOFF = 1100;
  unsigned int MAX_BACKOFF = 1200;
  #ifdef ADV_MODE
  void sendTime();
  #endif
#ifdef RECORD_TIME
  unsigned long message_start_time;
  unsigned long lastServiced;
  unsigned int message_time_error;
  unsigned char message_time_accurate;
#endif
private:   
  //Pointer to the place to put the new char
  unsigned char recievePointer = 0;
  //Place to keep track of where the header stops and data begins
  unsigned char headerTerminatorPosition;
  //Used for the fletcher checksum
  unsigned char sumSlow,sumFast =0;
  //If the last char recieved was an unesaped escape, this is true
  unsigned char escape = 0;

  //If this frame is garbage, true(like if it is too long)
  unsigned char garbage = 0;

  //Buffer for the message
  unsigned char message[MAX_MESSAGE];

  unsigned char sensepin;
  unsigned char wiredor;
  
  void (*callback)(
  unsigned char *, 
  unsigned char , 
  unsigned char *, 
  unsigned char );
  
  void (*stringCallback)(
   char *,  
   char *);


  Stream *BUS_PORT;

  void updateHash(unsigned char chr);
  unsigned char writeWrapper(unsigned char chr);
  unsigned char escapedWrite(unsigned char chr);
  void waitTillICanSend();

  void dummyCallback(
  unsigned char * header, 
  unsigned char headerlen, 
  unsigned char * data, 
  unsigned char datalen);
  
  unsigned char internalProcessMessage();

};

struct WBTV_Time_t
{
    long long seconds;
    unsigned int fraction;
};

#ifdef ADV_MODE
struct WBTV_Time_t WBTVClock_get_time();
void WBTVClock_set(WBTVNode);
extern unsigned long WBTVClock_error;
extern unsigned int WBTVClock_error_per_second;
#define WBTV_CLOCK_UNSYNCHRONIZED 4294967295
#define WBTV_CLOCK_HIGH_ERROR 4294967294
#define WBTVClock_invalidate() WBTVClock_error_per_second = 4294967294
#endif

/**Read one of whatever data type from the pointer you give it, then increment
 *the pointer by the size of the data type.
 *Example:
 *read_increment(x,unsigned char)
 *will return the value under the pointer, then increment the pointer by one.
 */

#define read_interpret(ptr,type) (*(type*)((ptr=ptr+sizeof(type))-sizeof(type)))
#define get_byte_of_value(datum,index) (((unsinged char*)&datum)[index])
#endif

