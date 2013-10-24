#include "protocol_definitions.h"
#include <Arduino.h>
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

  WBTVNode( Stream *, int bus_sense_pin);

  void setBinaryCallback(
  void (*thecallback)(
  unsigned char *, 
  unsigned char , 
  unsigned char *, 
  unsigned char ));

  void setStringCallback(
  void (*thecallback)(
  unsigned char *, 
  unsigned char *));

private:
  //Pointer to the place to put the new char
  unsigned char recievePointer = 0;
  //Place to keep track of where the header stops and data begins
  unsigned char lastByteOfHeader;
  //Used for the fletcher checksum
  unsigned char sumSlow,sumFast =0;
  //If the last char recieved was an unesaped escape, this is true
  unsigned char escape = 0;

  //If this frame is garbage, true(like if it is too long)
  unsigned char garbage = 0;

  //Buffer for the message
  unsigned char message[MAX_MESSAGE];

  unsigned char sensepin;
  
  void (*callback)(
  unsigned char *, 
  unsigned char , 
  unsigned char *, 
  unsigned char );
  
  void (*stringCallback)(
  unsigned char *,  
  unsigned char *);


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

};


