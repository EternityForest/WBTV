/*This is a very simple WBTV controlled WS2801 pixel driver, that will drive 50 RGB pixels, using the WBTV STAGE protocol. 
to set the start address, send a 16 bit nuber to the UUID that will be sent in a NOTE message when the device boots.
Be sure to give it a few minutes the first time you power it on, because it needs to generate it's UUID which could take a few minutes.
*/





#include <SPI.h>

#include <EEPROM.h>

unsigned char dummy;
#include <WBTVNode.h>
#include <Adafruit_WS2801.h>
#define absdif(x,y) (((x)>(y))?((x)-(y)):((y)-(x)))
//This is the WBTV Stage Protocol start index
unsigned short STARTADDR;
#define VALUE_COUNT 150
Adafruit_WS2801 strip = Adafruit_WS2801(50, 2, 3);

unsigned long startTime;
unsigned long lastFrameTime;
WBTVNode node = WBTVNode(&Serial);
//The nominal WBTV frame rate used to define fade times
#define FPS 48
//Here we have two lists. One of the current values for the channels, one fo the destination it is currently fading to, and one of th
//rate.
//When we call the update function, each value will be moved towards the destination by the rate
unsigned short current[150] = {255,5999,255,5999,255,255,255,255,255};
unsigned short destination[150] = {255,255,255,255,255,255,255,255,255};
unsigned short rate[150];
unsigned short xshort;
unsigned char i;
unsigned char uuid[16];
//Placeholder for UUID starts at 96
unsigned char msg[] = "WBTV STAGE2 to WS2801 device active, 200 RGB channels. To change start address, Send uint16 to:XXXXXXXXXXXXXXXX";


void setup() {
  strip.begin();
  
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
node.sendMessage((unsigned char*)"NOTE",4,msg,111);
}



void loop() {

  if (millis() > (lastFrameTime + (1000/FPS)))
  {
    lastFrameTime = millis();
    update();
  }
  output();

  node.service();
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
    node.stringSendMessage("NOTE","WBTV Stage2 Start Address Set");
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
  static unsigned char pwm;
  unsigned char r,g,b;
  unsigned char ind =0;
  uint32_t color;
  for(i=0;i<150;i+=3)
  {
    
    
      if((((current[i]>>8)<255)) &&  ((current[i]&255)>pwm))
      {
        r = (current[i]>>8)+1;
         
      }
      else
      {
         r = current[i]>>8;
      }
      
      if((((current[i+1]>>8)<255))&&((current[i+1]&255)>pwm))
      {
        g =(current[i+1]>>8)+1;

      }
      else
      {
        g = current[i+1]>>8;
      }
      
     if((((current[i+2]>>8)<255))&&((current[i+2]&255)>pwm))
      {
        b= (current[i+2]>>8)+1;
      }
      else
      {
        b = current[i+2]>>8;
        
      }
      
      //Number chosen based on a guess of what might look best.
      pwm += 167;
      color = Color(r,g,b);
      strip.setPixelColor(ind,color);
      ind+=1;
      
      
      

  }
  strip.show();
}




