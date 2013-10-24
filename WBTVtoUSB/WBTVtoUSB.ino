unsigned char message[64];
unsigned char recievePointer = 0;
unsigned char lastByteOfHeader;
unsigned char sumSlow,sumFast =0;
unsigned char escape = 0;
unsigned long timeLastSent;
unsigned char garbage = 0;
#define BUS_IDLE_STATE 1
#define BUS_SENSE_PIN 2
#define MAX_WAIT 5
#define MAX_MESSAGE 62
#define STH '!'
#define STX '~'
#define EOT '\n'
#define ESC '\\'
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
  pinMode(1,OUTPUT);
  Serial.begin(9600);
  Serial1.begin(9600);
}

void loop()
{
  if (Serial1.available())
    {
      Serial.write(Serial1.read());
    }
    
  if (Serial.available())
    {
      decodeChar(Serial.read());
    }

}


void processNewMessage(
const unsigned char * header, 
const unsigned char headerlen, 
const unsigned char * data, 
const unsigned char datalen)
{ 
    sendMessage(header,headerlen,data,datalen); 
}


unsigned char escapedWrite(unsigned char chr)
{
  unsigned char x;

  x =1;
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
  unsigned long start;
  Serial1.write(chr);
  start = millis();
  
  while (!Serial1.available())
    {
      if((millis()-start)>MAX_WAIT)
        {
          return 0;
    }
    }

  if (! (chr == Serial1.read()))
  {
    return 0;
  }
  else
  {
    return 1;
  }


}

void sendMessage(const unsigned char * channel, const unsigned char channellen, const unsigned char * data, const unsigned char datalen)
{
  unsigned char i;

waiting:
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
    

    if (chr == STX)
    {
      lastByteOfHeader = recievePointer;
      return;
    }

    if (chr == EOT)
    {
      unsigned char i;
      if (garbage)
        {
            return;
        }
      sumFast = sumSlow = 0;
      for (i=0; i< (recievePointer-2);i++)

      {
        updateHash(message[i]);
      }
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



void waitTillICanSend()
{
  unsigned long start,time;
wait:
  start = millis();
  time = random(1 , MAX_WAIT);

  //While it has been less the required time, just loop. Should the bus get un-idled in that time, totally restart.     
  while((millis()-start)<time)
  {
    if (!(digitalRead(BUS_SENSE_PIN)== BUS_IDLE_STATE))
    {
      goto wait;
    }
  }
  Serial.print("sending\n");
}


