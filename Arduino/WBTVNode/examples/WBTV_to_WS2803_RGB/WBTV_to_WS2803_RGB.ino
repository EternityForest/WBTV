/*

 This drives a ws2803 to paralell the fist 6, middle 6, and last 6 channels for driving RGB LEDs.
 to set the start address, send a 16 bit nuber to the UUID that will be sent in a NOTE message when the device boots.
 Be sure to give it a few minutes the first time you power it on, because it needs to generate it's UUID which could take a few minutes.
 
 This sketch provided as-is.
 */





#include <SPI.h>

#include <EEPROM.h>

unsigned char dummy;
#include <WBTVNode.h>
#include <Adafruit_WS2801.h>
#define absdif(x,y) (((x)>(y))?((x)-(y)):((y)-(x)))
//This is the WBTV Stage Protocol start index
unsigned short STARTADDR;
#define VALUE_COUNT 3
//It thinks the ws2803 is 6 rgb pixels.
Adafruit_WS2801 strip = Adafruit_WS2801(6, 2, 3);

unsigned long startTime;
unsigned long lastFrameTime;
WBTVNode node = WBTVNode(&Serial,0);
//The nominal WBTV frame rate used to define fade times
#define FPS 48
//Here we have two lists. One of the current values for the channels, one fo the destination it is currently fading to, and one of th
//rate.
//When we call the update function, each value will be moved towards the destination by the rate
unsigned short current[3] = {250,500,0};
unsigned short destination[3] = {0,0,0};
unsigned short rate[3] = {10,10,10};
unsigned short xshort;
unsigned char i;
unsigned char uuid[16];
//Placeholder for UUID starts at 96
unsigned char msg[] = "WBTV STAGE2 to WS2801 device active, 200 RGB channels. To change start address, Send uint16 to:XXXXXXXXXXXXXXXX";


void setup() {
  strip.begin();
  strip.setPixelColor(2,Color(10,10,10));
  strip.show();
  Serial.begin(115200);

  delay(1000);
  node.setBinaryCallback(parse_command);

  //node.stringSendMessage("NOTE","RGB Controller Booting");


  //UUID INITIALIZATION

  //Totally arbitrary magic values meaning a UUID has been generated.
  //Basically, if the magic value is missing, generate a new UUID for self
  //Using the temperature sensor as a source of randomness.


  if ( ( EEPROM.read(16) != 57) || (EEPROM.read(17) != 38))
  {
    for (i=0;i<16;i++)
    {
      WBTV_get_entropy();
      EEPROM.write(i,WBTV_urand_byte());
    }
    EEPROM.write(16,57);
    EEPROM.write(17,38);
  }
  for (i=0;i<16;i++)
  {
    uuid[i]= EEPROM.read(i);
  }

  //Copy our UUID into the proper slot, and send the message, so we know where to send the message to.
  memcpy(msg+95,uuid,16);
  STARTADDR = EEPROM.read(19);
  STARTADDR= STARTADDR<<8;
  STARTADDR+=EEPROM.read(18);
  node.sendMessage((unsigned char*)"NOTE",4,msg,111);
}



void loop() {

  if (millis() > (lastFrameTime + (1000/FPS)))
  {
    lastFrameTime = millis();
    update();
  }
  output();

  node.serviceAll();
}


void parse_command(unsigned char *header, unsigned char headerlen, unsigned char * data, unsigned char datalen)
{
  void *endbyte = data+datalen;
  unsigned char cmd,len,i,x;
  unsigned long offset;
  unsigned char time;

  //If a packet gets sent to our UUID, then we use it to set our start address.
  if (memcmp(header,uuid,headerlen)==0)
  {
    EEPROM.write(18,read_interpret(data,unsigned char));
    EEPROM.write(19,read_interpret(data,unsigned char));
    node.sendMessage((unsigned char*)"NOTE",4,(unsigned char*)"WBTV Stage2 Start Address Set",29);
  STARTADDR = EEPROM.read(19);
  STARTADDR= STARTADDR<<8;
  STARTADDR+=EEPROM.read(18);
  }

  if (memcmp(header,"STAGE",headerlen))
  {
    return;
  }

  while(data<endbyte)
  {
    cmd = read_interpret(data,unsigned char);

    //Set lights command
    if (cmd==0)
    {
      //Find an offset such that when we add it to an absolute value number it converts it to our local value numbering.
      offset=read_interpret(data,unsigned short)-STARTADDR;

      len=read_interpret(data,unsigned char);

      //Go over all the values specified in the command
      for(i=0;i<len;i++)
      {
        //Read every value
        x = read_interpret(data,unsigned char);
        //Only if this value is within our local range, do we actually use it to set a light.
        if(((i+offset)>=0)&&((i+offset)<VALUE_COUNT))
        {
          destination[(i+offset)] = x*256;
          current[(i+offset)] = x*256;
        }
      }
    }

    //Fade lights command
    else if (cmd==1)
    {
      //Find an offset such that when we add it to an absolute value number it converts it to our local value numbering.
      offset=read_interpret(data,unsigned short)-STARTADDR;

      len=read_interpret(data,unsigned char);
      time=read_interpret(data,unsigned char);
      for(i=0;i<len;i++)
      {
        x = read_interpret(data,unsigned char);
        //Only if this value is within our local range, do we actually use it to set a light.
        //If i plus offset is negative, the value is before the start, if i plus offset is greater than the top of our range, its past the end of what we
        //pay attention to.
        if(((i+offset)>=0) &&( (i+offset)<VALUE_COUNT))
        {
          destination[i+offset] = x*256;
          rate[i+offset] = absdif(current[i+offset],(unsigned int)x*256)/time;
        }
      }
    }

    //Set lights command, 16 bit edition.
    if (cmd==2)
    {
      //Find an offset such that when we add it to an absolute value number it converts it to our local value numbering.
      offset=read_interpret(data,unsigned short)-STARTADDR;

      len=read_interpret(data,unsigned char);

      //Go over all the values specified in the command
      for(i=0;i<len;i++)
      {
        //Read every value
        xshort = read_interpret(data,unsigned short);
        //Only if this value is within our local range, do we actually use it to set a light.
        if(((i+offset)>=0)&&((i+offset)<VALUE_COUNT))
        {
          destination[(i+offset)] = xshort;
          current[(i+offset)] = xshort;
        }
      }
    }

    //Fade lights command
    else if (cmd==3)
    {
      //Find an offset such that when we add it to an absolute value number it converts it to our local value numbering.
      offset=read_interpret(data,unsigned short)-STARTADDR;

      len=read_interpret(data,unsigned char);
      time=read_interpret(data,unsigned char);
      for(i=0;i<len;i++)
      {
        xshort = read_interpret(data,unsigned short);
        //Only if this value is within our local range, do we actually use it to set a light.
        //If i plus offset is negative, the value is before the start, if i plus offset is greater than the top of our range, its past the end of what we
        //pay attention to.
        if(((i+offset)>=0) &&( (i+offset)<VALUE_COUNT))
        {
          destination[i+offset] = xshort;
          rate[i+offset] = absdif(current[i+offset],(unsigned int)xshort)/time;
        }
      }
    }



  }
}





void update()
{
  unsigned char i;
  for (i=0;i<15;i++)
  {
    if ( (current[i]) < destination[i])
    {
      if( ((current[i]+rate[i]) <= (destination[i])) && ((current[i]+rate[i]) > current[i]))
      {
        current[i]=current[i]+rate[i];
      }
      else
      {
        current[i]= destination[i];
      }
    }

    else if ( (current[i]) > destination[i])
    {
      if( ((current[i]-rate[i]) >= (destination[i])) && ((current[i]-rate[i]) < current[i]))
      {
        current[i]=current[i]-rate[i];
      }
      else
      {
        current[i]=destination[i];
      }
    }
  }
}


uint32_t Color(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}


//Update the actual strip to reflect what the current color array settings.
void output()
{
  unsigned short r,g,b;
  unsigned char pwm;
  
  unsigned char channelLeds[6];

  r = current[0]/42;
  
  if((current[0]%42)>pwm)
  {
    r+=1;
  }

  for(i=0;i<6;i+=1)
  {
    channelLeds[i]=0;
    if (r>255)
    {
      channelLeds[i] = 255;
      r-=255;
    }
    else
    {
      channelLeds[i] =r;
      r=0;
    }

  }

  strip.setPixelColor(0,Color(channelLeds[0],channelLeds[1],channelLeds[2]));
  strip.setPixelColor(1,Color(channelLeds[3],channelLeds[4],channelLeds[5]));


  g = current[1]/42;
    if((current[1]%42)>pwm)
  {
    g+=1;
  }
  for(i=0;i<6;i+=1)
  {
    channelLeds[i]=0;
    if (g>255)
    {
      channelLeds[i] = 255;
      g-=255;
    }
    else
    {
      channelLeds[i] =g;
      g=0;
    }

  }

  strip.setPixelColor(2,Color(channelLeds[0],channelLeds[1],channelLeds[2]));
  strip.setPixelColor(3,Color(channelLeds[3],channelLeds[4],channelLeds[5]));

  b = current[2]/42;
    if((current[2]%42)>pwm)
  {
    b+=1;
  }

  for(i=0;i<6;i+=1)
  {
    channelLeds[i]=0;
    if (b>255)
    {
      channelLeds[i] = 255;
      b-=255;
    }
    else
    {
      channelLeds[i] =b;
      b=0;
    }

  }

  strip.setPixelColor(4,Color(channelLeds[0],channelLeds[1],channelLeds[2]));
  strip.setPixelColor(5,Color(channelLeds[3],channelLeds[4],channelLeds[5]));

pwm=(pwm+25)%42;
  strip.show();
}





