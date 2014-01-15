#include "WBTVNode.h"
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

//Constructor for a wired or(listen before transmit) version
WBTVNode::WBTVNode(Stream *port,int bus_sense_pin)
{
  BUS_PORT=port;
  sensepin = bus_sense_pin;
  wiredor = 1;
}

//Constructor for a full duplex version
WBTVNode::WBTVNode(Stream *port)
{
  BUS_PORT=port;
  wiredor =0;
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
  if(wiredor)
  {
    waitTillICanSend();
  }
  //Reset the checksum engine
  sumSlow = sumFast =0;

  //Basically, send bytes, if we get interfered with, go back to waiting


#ifdef DUMMY_STH
  //Sent dummy start code for reliabilty in case there was noise that looked like an ESC.
  //This is really just there for paranoia reasons but it's a nice addition and it's only one more byte.
  if (!writeWrapper(STH))
  {
    goto waiting;
  }
#endif

  if (!writeWrapper(STH))//Sent start code
  {
    goto waiting;
  }
  //Send header(escaped)
  for (i = 0; i<channellen;i++)
  {
    updateHash(channel[i]); ///Update the hash with every header byte we send
    if (!escapedWrite(channel[i]))
    {
      goto waiting;

    }
  }

  //Send the start-of-message(end of header)
  if (!writeWrapper(STX))
  {
    goto waiting;
  }

#ifdef HASH_STX
  updateHash(STX);
#endif

  //Send the message(escaped)
  for (i = 0; i<datalen;i++)
  {
    updateHash(data[i]); //Hash the message
    if (!escapedWrite(data[i]))
    {
      goto waiting;

    }
  }

  //Send the two bytes of the checksum, After doing the proper fletcher derivation.
  //When the reciever hashses the message along with the checksum,the result should be 0.
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

//Note that one hash engine gets used for sending and recieving. This works for now because we calc the hash all at once after we are
//done recieving
//i.e. hashing a recieved packet is atomic
void WBTVNode::updateHash(unsigned char chr)
{
  //This is a fletcher-256 hash. Not quite as good as a CRC, but pretty good.
  sumSlow += chr;
  sumFast += sumSlow; 
}

void WBTVNode::service()
{
    if (! BUS_PORT->available())
    {
        return;
    }
    else
    {
      decodeChar(BUS_PORT->read());
    }
}

//Process one incoming char
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
      headerTerminatorPosition = 0; //We need to set this to zero to recoginze missing data sections, they will look like 0 len headers because this wont move.
      garbage = 0;
      return;
    }


    //Handle the division between header and text
    if (chr == STX)
    {
      //If this isn't 0, the default, then that would indicate a message with multiple segments,
      //which simply can't be handled by this library as it, so they are ignored.
      //CHANGE THIS IF YOU WANT TO ADD SUPPORT FOR MULTI-SEGMENT MESSAGES
      if (headerTerminatorPosition)
      {
        garbage =  1;
      }

      headerTerminatorPosition = recievePointer;
      message[recievePointer] = 0; //Null terminator between header and data makes string callbacks work
      recievePointer ++;
      return;

    }

    //Handle end of packet
    if (chr == EOT)
    {
      unsigned char i;
      if (garbage)//If this packet was garbage, throw it away
      {
        return;
      }

      if (!headerTerminatorPosition)
        //If the headerTerminatorPosition is 0, then we either have a zero length header, or we never recieved a end-of header char.
        //I never specifically wrote that 0-len headers are illegal in the spec. but maybe I should. Otherwise this is a bug [ ]
      {
        return;
      }

      //not possible to be a valid message becuse len(checksum) = 2
      if(recievePointer <2)
      {
        return;
      }
      
      //Reset the hash state to calculate a new hash
      sumFast=sumSlow=0;

      //Hash the packet
      for (i=0; i< recievePointer-2;i++)

      {
        //Ugly slow hack to deal with the empty space we put in
        if(! (i==headerTerminatorPosition))
        {
          updateHash((message[i]));
        }

#ifdef HASH_STX
        else
        {
          updateHash('~');
        }
#endif

      }

      //Check the hash
      if ((message[recievePointer-1]== sumFast) & (message[recievePointer-2]== sumSlow))
      {
        message[recievePointer-2] = 0; //Null terminator for people using the string callbacks.
        if(callback)
        {
          callback((unsigned char*)message ,
          headerTerminatorPosition,
          (unsigned char *)message+headerTerminatorPosition+1, //The plus one accounts for the null terminator we put in
          recievePointer-(headerTerminatorPosition+1)); //Would be plus 2 for the checksum but we have the null byte inserted so it's plus 1
        }
        else
        {
          stringCallback((unsigned char*)message ,
          (unsigned char *)message+headerTerminatorPosition);
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
  BUS_PORT->read();
  BUS_PORT->write(chr);
  if (wiredor)
  {
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
  }
  //Not wiredor, always return 1 because there is no way for this to be interfered with
  else 
  {
    return 1;
  }
}

unsigned char WBTVNode::escapedWrite(unsigned char chr)
{
  unsigned char x;

  x = 1;

  //If chr is a special character, escape it first
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

  //If the escape succeded(or there was no escape), then send the char, and return it's sucess value
  if (x)
  {
    return writeWrapper(chr);
  }
  //Else return a failure
  return 0;

}

//Block until the bus is free.
//It does this by waiting a random time and making sure there are no transitions in that period
//the collision detection and immediate retry means that
//even a relatively high rate of collisions should not cause problems.

//A cool side effect of this is that in many cases nodes will be able to "wait out" noise on the bus
void WBTVNode::waitTillICanSend()
{
  unsigned long start,time;
wait:
  start = micros();
  time = random(MIN_BACKOFF , MAX_BACKOFF);

  //While it has been less than the required time, just loop. Should the bus get un-idled in that time, totally restart.
  // The performance of the loop is really pooptastic, because  digital reads take 1us of so and there is a divide operation in the 
  //micros() 
  //implementation. Still, this should arbitrate well enough.
  while( (micros()-start) < time)
  {
    //UNROLL LOOP X4 TO INCREASE CHACE OF CATCHING FAST PULSES
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== BUS_IDLE_STATE))
    {
      goto wait;
    }
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== BUS_IDLE_STATE))
    {
      goto wait;
    }
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== BUS_IDLE_STATE))
    {
      goto wait;
    }
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== BUS_IDLE_STATE))
    {
      goto wait;
    }
  }
}










