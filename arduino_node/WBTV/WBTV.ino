

//The state of the bus when nothing is being sent.
#define BUS_IDLE_STATE 1
//The serial object thing(for devices with more than one)
#define BUS_PORT Serial
//The pin used to sense bus activity
//(in case some platform doesn't let you digitalread from an RX pin )
#define BUS_SENSE_PIN 0
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


//YOU MUST DEFINE THIS UNLESS YOU ARE USING DIRECT POINT-TO-POINT LIKE DOING IT OVER USB
//#define WIREDOR

//Pointer to the place to put the new char
unsigned char recievePointer = 0;
//Place to keep track of where the header stops and data begins
unsigned char lastByteOfHeader;
//Used for the fletcher checksum
unsigned char sumSlow,sumFast =0;
//If the last char recieved was an unesaped escape, this is true
unsigned char escape = 0;
//Last time the periodic message was sent
unsigned long timeLastSent;
//If this frame is garbage, true(like if it is too long)
unsigned char garbage = 0;
unsigned char a5val;
//Buffer for the message
unsigned char message[MAX_MESSAGE];

void updateHash(unsigned char);
void decodeChar(unsigned char);

unsigned char arrcmp(unsigned char * a, unsigned char * b, unsigned char len)
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


void setup()
{
  pinMode(BUS_SENSE_PIN,INPUT_PULLUP);
  pinMode(A5,INPUT);
  BUS_PORT.begin(9600);
}

void loop()
{
  if (BUS_PORT.available())
  {
    decodeChar(BUS_PORT.read());
  }
  
  if ((millis()-timeLastSent) >2500)
    {
      a5val = analogRead(A5);
      timeLastSent = millis();
      sendMessage((unsigned char *)"A5Value",7,&a5val,1);
    }

}


void processNewMessage(
const unsigned char * header, 
const unsigned char headerlen, 
const unsigned char * data, 
const unsigned char datalen)
{ 
    sendMessage((unsigned char *)"IGotAMessage!!!",15,data,datalen); 
}


unsigned char escapedWrite(unsigned char chr)
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
unsigned char writeWrapper(unsigned char chr)
{
  unsigned char x;
  unsigned long start;
#ifdef WIREDOR
  BUS_PORT.read();
#endif
  BUS_PORT.write(chr);
  start = millis();
#ifdef WIREDOR

  while (!BUS_PORT.available())
    {
      if ((millis()-start)>MAX_WAIT)
        {
          return 0;
        }
    }
    

  if (! (BUS_PORT.read()==chr))
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

void sendMessage(const unsigned char * channel, const unsigned char channellen, const unsigned char * data, const unsigned char datalen)
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



void updateHash(unsigned char chr)
{
  //This is a fletcher-256 hash. Not quite as good as a CRC, but pretty good.
  sumSlow += chr;
  sumFast += sumSlow; 
}

void decodeChar(unsigned char chr)
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
      
      //Hash the packet
      for (i=0; i< recievePointer-2;i++)

      {
        updateHash(message[i]);
      }
      
      //Check the hash
      if ((message[recievePointer-1]== sumFast) & (message[recievePointer-2]== sumSlow))
      {

        processNewMessage((unsigned char*)message , 
        lastByteOfHeader, 
        (unsigned char *)message+lastByteOfHeader,
        recievePointer-(lastByteOfHeader+2));
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



//Block until the bus is free
void waitTillICanSend()
{
  unsigned long start,time;
wait:
  start = millis();
  time = random(1 , 5);

  //While it has been less than the required time, just loop. Should the bus get un-idled in that time, totally restart.     
  while((millis()-start)<time)
  {
    //We directly read from the pin to determine if it is idle, that lets us catch the 
    if (!(digitalRead(BUS_SENSE_PIN)== BUS_IDLE_STATE))
    {
      goto wait;
    }
  }
}


