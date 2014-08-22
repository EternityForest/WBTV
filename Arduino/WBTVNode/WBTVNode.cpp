#include "WBTVNode.h"

/*
 *Instantiate a wired-OR WBTV node with CSMA, collision avoidance,
 *and collision detection. bus_sense_pin must be the RX pin, and
 *port must be a Serial or other stream object
 */
 WBTVNode::WBTVNode(Stream *port,int bus_sense_pin)
{
  BUS_PORT=port;
  sensepin = bus_sense_pin;
  wiredor = 1;
  
    MIN_BACKOFF = 1100;
    MAX_BACKOFF = 1200;
    recievePointer = 0;
    sumSlow=sumFast =0;
    escape = 0;
    garbage = 0;
    }

/*
 *This lets you use WBTV over full duplex connections like usb to serial.
 */
WBTVNode::WBTVNode(Stream *port)
{
BUS_PORT=port;
wiredor =0;

MIN_BACKOFF = 1100;
MAX_BACKOFF = 1200;
recievePointer = 0;
sumSlow=sumFast =0;
escape = 0;
garbage = 0;
    
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
void WBTVNode::setStringCallback( void (*thecallback)( char *,  char *))
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


#ifdef DUMMY_WBTV_STH
  //Sent dummy start code for reliabilty in case there was noise that looked like an WBTV_ESC.
  //This is really just there for paranoia reasons but it's a nice addition and it's only one more byte.
  if (!writeWrapper(WBTV_STH))
  {
    goto waiting;
  }
#endif

  if (!writeWrapper(WBTV_STH))//Sent start code
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
  if (!writeWrapper(WBTV_STX))
  {
    goto waiting;
  }

#ifdef WBTV_HASH_STX
  updateHash(WBTV_STX);
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

  if (!escapedWrite(sumFast))
  {
    goto waiting;
  }

  if (!escapedWrite(sumSlow))

  {
    goto waiting;
  }

  if (!writeWrapper(WBTV_EOT))
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
    if (BUS_PORT->available())
    {
      decodeChar(BUS_PORT->read());
    }
    
lastServiced = millis();

}

//Process one incoming char
void WBTVNode::decodeChar(unsigned char chr)
{
  //Set the garbage flag if we get a message that is too long
  if (recievePointer > WBTV_MAX_MESSAGE)
  {        
    garbage = 1;
    recievePointer = 0;
  }

  //Handle the special chars
  if (!escape)
  {
    if (chr == WBTV_STH)
    {
      #ifdef WBTV_RECORD_TIME
        //Keep track of when the msg started, or else time sync won't work.
        message_start_time = millis();
        
        //If the new byte is the only byte, then it must have arrived
        //At some point between the last time it was polled and now.
        //For best average performance, we assume it arrived at the halfway point.
        //If it is not the only byte it cannon be trusted, so message time
        //accurate must be set to 0
        
        message_start_time -= ((message_start_time-lastServiced)>>1);
        
        message_time_error = message_start_time-lastServiced;
        //If there is another byte in the stream, then consider the arrival time invalid. 
        if (BUS_PORT->available())
        {
            message_time_accurate = 0;
        }
        else
        {
             message_time_accurate = 1;
        }
      #endif
      
      
      //an unescaped start of header byte resets everything. 
      recievePointer = 0;
      headerTerminatorPosition = 0; //We need to set this to zero to recoginze missing data sections, they will look like 0 len headers because this wont move.
      garbage = 0;
      
      #ifdef WBTV_SEED_ARDUINO_RNG
      randomSeed(micros()+random(100000);
      #endif
      
      #ifdef WBTV_ENABLE_RNG
      //doRand Automatically uses the current micros() value.
      WBTV_doRand();
      #endif

      return;
    }

    //Handle the division between header and text
    if (chr == WBTV_STX)
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
    if (chr == WBTV_EOT)
    {
        handle_end_of_message();
        return;
    }
    
    //Handle an unescaped escape
    if ( chr == WBTV_ESC)
    {
        escape = 1;
        return; 
    }
  }
//If we got this far, it means that we are either escaped or that the character was not a control char.
escape = 0;
message[recievePointer] = chr;
recievePointer ++;

}


void inline WBTVNode::handle_end_of_message()
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

#ifdef WBTV_HASH_STX
        else
        {
          updateHash('~');
        }
#endif
      }

      //Check the hash
      if ((message[recievePointer-1]==sumSlow) && (message[recievePointer-2]== sumFast))
      {
        #ifdef WBTV_ADV_MODE
        //Check if this is a time() message.
        //This function is part of wbtvclock
        if (internalProcessMessage())
        {
        return;
        }
        #endif
        message[recievePointer-2] = 0; //Null terminator for people using the string callbacks.
        
        //If there is a callback set up, use it.
        if(callback)
        {
          callback((unsigned char*)message ,
          headerTerminatorPosition,
          (unsigned char *)message+headerTerminatorPosition+1, //The plus one accounts for the null terminator we put in
          recievePointer-(headerTerminatorPosition+1)); //Would be plus 2 for the checksum but we have the null byte inserted so it's plus 1
        }
        else
        {
            //If there are any NUL bytes in the header, just return.
            //Presumably nobody would register a string callback
            //that listens on a channel with 0s in its data
            //but the channel needs to be checked to prevent against
            //channels that start with the name of the string channel
            //and then a null.
            for(i=0;i<headerTerminatorPosition;i++)
            {
                if (message[i] ==0)
                    {
                        return;
                    }
            }
            //Veriied that the channel name is safe. now we hand it off to the callback
            if (stringCallback)
            {
              stringCallback((char*)message ,
              (char *)message+headerTerminatorPosition);
            }
        }
      }
      #ifdef WBTV_SEED_ARDUINO_RNG
      randomSeed(sumSlow+random(100000);
      #endif
      
      #ifdef WBTV_ENABLE_RNG
      WBTV_doRand(sumSlow);
      #endif
}

unsigned char WBTVNode::writeWrapper(unsigned char chr)
{
  unsigned long start;
  BUS_PORT->read();
  BUS_PORT->write(chr);

  if (wiredor)
  {
    start = micros();
    while (!BUS_PORT->available())
    {
      if ((micros()-start)>WBTV_MAX_WAIT)
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
  if (chr == WBTV_STH)
  {
    x = writeWrapper(WBTV_ESC); 
  }
  if (chr == WBTV_STX)
  {
    x= writeWrapper(WBTV_ESC); 
  }
  if (chr == WBTV_EOT)
  {
    x = writeWrapper(WBTV_ESC); 
  }
  if (chr == WBTV_ESC)
  {
    x =writeWrapper(WBTV_ESC); 
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
  #ifdef WBTV_ENABLE_RNG
  time = WBTV_rand(MIN_BACKOFF,MAX_BACKOFF);
  #else
  time = random(MIN_BACKOFF , MAX_BACKOFF);
  #endif
  

  //While it has been less than the required time, just loop. Should the bus get un-idled in that time, totally restart.
  // The performance of the loop is really pooptastic, because  digital reads take 1us of so and there is a divide operation in the 
  //micros() 
  //implementation. Still, this should arbitrate well enough.
  while( (micros()-start) < time)
  {
    //UNROLL LOOP X4 TO INCREASE CHACE OF CATCHING FAST PULSES
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== WBTV_BUS_IDLE_STATE))
    {
      goto wait;
    }
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== WBTV_BUS_IDLE_STATE))
    {
      goto wait;
    }
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== WBTV_BUS_IDLE_STATE))
    {
      goto wait;
    }
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(sensepin)== WBTV_BUS_IDLE_STATE))
    {
      goto wait;
    }
  }
}
