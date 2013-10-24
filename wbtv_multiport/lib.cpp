#include "wbtv.h"
static unsigned char arrcmp(unsigned char * a, unsigned char * b, unsigned char len)
{
  unsigned char i;
  for (i = 0; i<len;i++)
  {
    if ( ! (a[i] == b[i]))
    {
      return 0;
    } 
  }
  return 1;
}


WBTVNode::WBTVNode(Stream *port,int bus_sense_pin)
{
  BUS_PORT=port;
  sensepin = bus_sense_pin;
}

void WBTVNode::dummyCallback(
unsigned char * header, 
unsigned char headerlen, 
unsigned char * data, 
unsigned char datalen)
{
  //Does absoulutly nothing
}


void WBTVNode::setBinaryCallback(
void (*thecallback)(
unsigned char *, 
unsigned char , 
unsigned char *, 
unsigned char ))
{
  callback = thecallback;
}

/*Set a function taking two strings as input to be called when a packet arrives*/
void WBTVNode::setStringCallback(
void (*thecallback)(
unsigned char *, 
unsigned char *))
{
  stringCallback = thecallback;
  callback = 0;
}

void WBTVNode::stringSendMessage(const char *channel, const char *data)
{
  sendMessage((const unsigned char *)channel,strlen(channel),(const unsigned char *) data,strlen(data));
}

/*Given a channel as pointer to unsigned char, channel length, data, and data length, send it out the serial port.*/
void WBTVNode::sendMessage(const unsigned char * channel, const unsigned char channellen, const unsigned char * data, const unsigned char datalen)
{
  unsigned char i;

  //If at any time an error is found, go back here to retry
waiting:
  //Wait till the bus is free
  waitTillICanSend();

  sumSlow = sumFast =0;
  if (!writeWrapper(STH))
  {
    goto waiting;
  }

  for (i = 0; i<channellen;i++)
  {
    updateHash(channel[i]);
    if (!escapedWrite(channel[i]))
    {
      goto waiting;

    }
  }

  if (!writeWrapper(STX))
  {
    goto waiting;
  }

  for (i = 0; i<datalen;i++)
  {
    updateHash(data[i]);
    if (!escapedWrite(data[i]))
    {
      goto waiting;

    }
  }

  if (!escapedWrite(sumSlow))
  {
    goto waiting;
  }

  if (!escapedWrite(sumFast))
  {
    goto waiting;
  }

  if (!writeWrapper(EOT))
  {
    goto waiting;
  }


}

void WBTVNode::updateHash(unsigned char chr)
{
  //This is a fletcher-256 hash. Not quite as good as a CRC, but pretty good.
  sumSlow += chr;
  sumFast += sumSlow; 
}

void WBTVNode::decodeChar(unsigned char chr)
{

  //Set the garbage flag if we get a message that is too long
  if (recievePointer > MAX_MESSAGE)
  {        
    garbage = 1;
    recievePointer = 0;
  }

  //Handle the special chars
  if (!escape)
  {
    if (chr == STH)
    {
      //an unescaped start of header byte resets everything. 
      recievePointer = 0;
      garbage = 0;
      return;
    }


    //Handle the division between header and text
    if (chr == STX)
    {
      lastByteOfHeader = recievePointer;
      message[recievePointer] = 0; //Null terminator between header and data makes string callbacks work
      recievePointer ++;
      return;
    }

    //Handle end of packet
    if (chr == EOT)
    {
      unsigned char i;
      if (garbage)
      {
        return;
      }

      //Set the hash to starting position
      sumFast = sumSlow = 0;
      
      //not possible to be a valid message becuse len(checksum) = 2
      if(recievePointer <2)
      {
        return;
      }
      //Hash the packet
      for (i=0; i< recievePointer-2;i++)

      {
        //Ugly slow hack to deal with the empty space we put in
        if(! (i==lastByteOfHeader))
        {
        updateHash((message[i]));
        }
      }

      //Check the hash
      if ((message[recievePointer-1]== sumFast) & (message[recievePointer-2]== sumSlow))
      {
        message[recievePointer-1] = 0; //Null terminator for people using the string callbacks.
        if(callback)
        {
          callback((unsigned char*)message , 
          lastByteOfHeader, 
          (unsigned char *)message+lastByteOfHeader+1, //The plus one accounts for the null terminator we put in
          recievePointer-(lastByteOfHeader+3)); //Would be plus 2 for the checksum but we have the null byte inserted so it's plus 1
        }
        else
        {
          stringCallback((unsigned char*)message , 
          (unsigned char *)message+lastByteOfHeader+1); //The plus one accounts for the null terminator we put in
        }
      }

      return;
    }
  }

  if ( chr == ESC)
  {
    if (!escape)
    {
      escape = 1;
      return; 
    }
    else
    {
      escape = 0;
      message[recievePointer] = ESC;
      recievePointer ++;
      return;
    }
  }
  else
  {
    escape =0;
  }
  message[recievePointer] = chr;
  recievePointer++;


}

unsigned char WBTVNode::writeWrapper(unsigned char chr)
{
  unsigned char x;
  unsigned long start;
#ifdef WIREDOR
  BUS_PORT->read();
#endif
  BUS_PORT->write(chr);
#ifdef WIREDOR
  start = millis();
  while (!BUS_PORT->available())
  {
    if ((millis()-start)>MAX_WAIT)
    {
      return 0;
    }
  }


  if (! (BUS_PORT->read()==chr))
  {
    return 0;
  }
  else
  {
    return 1;
  }
#endif

#ifndef WIREDOR
  return 1;
#endif
}

unsigned char WBTVNode::escapedWrite(unsigned char chr)
{
  unsigned char x;

  x = 1;
  if (chr == STH)
  {
    x = writeWrapper(ESC); 
  }
  if (chr == STX)
  {
    x= writeWrapper(ESC); 
  }
  if (chr == EOT)
  {
    x = writeWrapper(ESC); 
  }
  if (chr == ESC)
  {
    x =writeWrapper(ESC); 
  }


  if (x)
  {
    return writeWrapper(chr);
  }
  return 0;

}

//Block until the bus is free
void WBTVNode::waitTillICanSend()
{
  unsigned long start,time;
wait:
  start = millis();
  time = random(1 , 5);

  //While it has been less than the required time, just loop. Should the bus get un-idled in that time, totally restart.     
  while((millis()-start)<time)
  {
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== BUS_IDLE_STATE))
    {
      goto wait;
    }
  }
}




