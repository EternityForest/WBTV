#ifndef _WBTVNode
#define _WBTVNode
#include <Stream.h>
#include "utility/protocol_definitions.h"
#include "HardwareSerial.h"
#include "utility/WBTVRand.h"
#include "utility/WBTVClock.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#if defined(ENERGIA) 
#include <Energia.h>
#else
#include <Arduino.h>
#endif
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
  
  unsigned int MIN_BACKOFF;
  unsigned int MAX_BACKOFF;
  #ifdef WBTV_ADV_MODE
  void sendTime();
  #endif
#ifdef WBTV_RECORD_TIME
  unsigned long message_start_time;
  unsigned long lastServiced;
  unsigned int message_time_error;
  unsigned char message_time_accurate;
#endif
private:   
  //Pointer to the place to put the new char
  unsigned char recievePointer;
  //Place to keep track of where the header stops and data begins
  unsigned char headerTerminatorPosition;
  //Used for the fletcher checksum
  unsigned char sumSlow,sumFast;
  //If the last char recieved was an unesaped escape, this is true
  unsigned char escape;

  //If this frame is garbage, true(like if it is too long)
  unsigned char garbage;

  //Buffer for the message
  unsigned char message[WBTV_MAX_MESSAGE];

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
  void inline handle_end_of_message();

  void dummyCallback(
  unsigned char * header, 
  unsigned char headerlen, 
  unsigned char * data, 
  unsigned char datalen);
  
  unsigned char internalProcessMessage();

};



/**Read one of whatever data type from the pointer you give it, then increment
 *the pointer by the size of the data type.
 *Example:
 *read_increment(x,unsigned char)
 *will return the value under the pointer, then increment the pointer by one.
 */

#define read_interpret(ptr,type) (*(type*)((ptr=ptr+sizeof(type))-sizeof(type)))
#define get_byte_of_value(datum,index) (((unsinged char*)&datum)[index])
#endif

