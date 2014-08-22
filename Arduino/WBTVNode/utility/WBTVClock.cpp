#include "WBTVNode.h"

#ifdef WBTV_ADV_MODE
struct WBTV_Time_t WBTVClock_Sys_Time;
/**
 *The current estimate of accumulated error in WBTVs system clock.
 *The error is measured in 2**16ths of a second, but it saturates at 4294967294 and does not roll over.
 *4294967295 is reserved for when the time has never been set.
 */

unsigned long WBTVClock_error = 4294967295ul; // The accumulated error estimator in seconds/2**16

//Assume an error per second of about 5000PPM, which is about 10 minutes in day,
//Which is approximately what one gets with ceramic resonators.

/**This is how much error you estimate to accumulate per second with the built in crystal.
 * The error is measured in parts per 2**16. The default is 2500 or about 4%
 * This is very conservative and the arduino's crystal is more like 0.1% or better.
 * However some other devices may have much more error.
 * The default is 4% becuase UARTs don't work well with more error than that so it's
 * A safe assumtion.
 */
unsigned int WBTVClock_error_per_second = 2500;

unsigned long WBTVClock_prevMillis = 0;

/*Manually set the WBTV Clock. time must be the current UNIX time number,
 *fraction is the fractional part of the time in 2**16ths of a second,
 *error_ms is the error in 2**16ths of second that you estimate your source of timing to contain.
 *Times entered by humans should always be suspect to be several minutes off.
*/
void WBTVClock_set_time(long long time, unsigned int fraction, unsigned long error)
{
    //If our estimated internal time accuracy is better than whatever this new
    //Time Source alledges, then avoid making the accuracy worse.
    if (error>WBTVClock_error)
    {
        return;
    }
    WBTVClock_prevMillis=millis();
    
    WBTVClock_prevMillis -= fraction >>6;
    //Now we add the fraction value divided by 2**12
    //To compensate for dividing by 64 being too much.
    WBTVClock_prevMillis += fraction >>12;
    WBTVClock_prevMillis += fraction >>13;
    WBTVClock_error = error; 
}

/**
 *Returns the current time as a struct, the integer part called seconds
 *and the 16 bit fraction part called time.
 */
struct WBTV_Time_t WBTVClock_get_time()
{
unsigned long temp;

temp = millis();
//This function based in part on the datetime library.
while(temp - WBTVClock_prevMillis >= 1000){
    WBTVClock_Sys_Time.seconds++;
    WBTVClock_prevMillis += 1000;
    
    //if the error gets close to the max we can hold, use the "Error to high to count" flag,
    //otherwise just increase the error counter.
    if (WBTVClock_error<4294900000ul)
    {
    WBTVClock_error+= WBTVClock_error_per_second;
    }
    else
    {
    WBTVClock_error = 4294967294ul;
    }
  }
  
  //(1000 * 65) + 500 is 2**16 to within a tenth of a percent.
  //Therefore (millis*65) + (millis/2) approximates a convrsion from
  //milliseconds to 2^16ths of a secons
  
  //But when the milliseconds value is 1000, the output will be low by 63
  //The average error is therefore 31.5, and is always low.
  
  //Therefore we must add 32, which in the average case will make it more accurate,
  //And in the worst case will make it less accurate by a very tiny amount.
  
  temp -= WBTVClock_prevMillis;
  WBTVClock_Sys_Time.fraction = ((temp*65)+(temp>>1))+32;
  return (WBTVClock_Sys_Time);
}
#endif

#ifdef WBTV_ADV_MODE
unsigned char WBTVNode::internalProcessMessage()
{
    unsigned long error_temp;
    if(headerTerminatorPosition == 4)
    {
    if (memcmp(message,"TIME",4)==0)

    {
        //If the exponent is bigger than 8 we can't store that big of number
        //So assume the error is too high to count and store the flag value.
        if ((*((signed char*) (message+17) ) ) > 8)
        {
            error_temp = (4294967294ul);
        }
        else
        {
        
            //Calculate error based on the floating point value given
            //In the time broadcast and the error we get from the poll rate.
            
            //If the error exponent is less than -16, that would require shifting
            //in the other direction. Instead of actually estimating that,
            //lets assume the error is the maximum you can represent with an exponent of -16
            //In the worst case this will make our estimate 4ms too high.
            if ((*(signed char*) (message+17)) >-16)
            {
                error_temp = *(unsigned char*) (message+18);
                
                //The fixed point value we are going for is measured in 2**16ths of a second
                //therefore, if a the exponent is -16, the multiplier and the output are
                //one and the same.
                error_temp = error_temp << ((*(signed char*) (message+17))+15);
            }
            else
            {
                error_temp=255;
            }
            //Message_time_error is in milliseconds, so we multiply by 64
            //to get approximate 2*16ths of a second.
            //Then we add msgtimeerror*2, to improve the aprroximation and make it conservative.
            //
            error_temp += ((unsigned long) message_time_error)<<6;
            error_temp += ((unsigned long) message_time_error)<<1;
            
            if (!(message_time_accurate))
            {
                //Message time accurate is true when the message arrival time
                //Is known to within a small range because the start byte was the only
                //byte there.
                //That means it must not have been there last time it was polled(or it
                //would have been processed), and s must have arrived between the last two polling
                //cycles. This means it could have arrived at any time at all.
                //So we tack on up to 5 minutes of extra error in that case.
                if (error_temp <4275306493ul)
                {
                   error_temp+= 19660800ul;
                }
                else
                {
                   error_temp = (4294967294ul);
                }
                
            }

    }
        
        //If the estimated accuracy of WBTV's internal clock is better than
        //The estimated accuracy of the new time value, ignore the new value
        //And keep the current estimate.
        if(error_temp <=  WBTVClock_error)
        {
            
            WBTVClock_prevMillis=message_start_time;
            WBTVClock_error = error_temp;
            //5 Accounts for the 5 bytes of
            WBTVClock_Sys_Time.seconds = *(long long*) (message+5);
            //Message+ten= 8 to skip over the seconds part, and 2 to get to the 2 most significant bytes
            //of the fractional portion(Because the WBTV time spec uses 32 bit fractions but this
            //Library only uses 16 bits for the fractional time.
            
            //This is basically approximate division by 65.53 to map
            //The fractional seconds to milliseconds.
            
            //We subtract this milliseconds value from prevMillis to
            //Attempt to make it equal to the millis value
            //exactly when the second rolled over.
            WBTVClock_prevMillis -= (*(unsigned int*) (message+15)) >>6;
            //Now we add the fraction value divided by 2**12
            //To compensate for dividing by 64 being too much.
            WBTVClock_prevMillis += (*(unsigned int*) (message+15)) >>12;
            WBTVClock_prevMillis += (*(unsigned int*) (message+15)) >>13;


            //15 not 13 because we only want the two most significant bytes because
            //arduino doesnt have ghz resolution
            

        }
        

    return (1);
}
}
return(0);
}

//Send the current time as estimated in the internal clock
void WBTVNode::sendTime()
{
    unsigned char i;
    unsigned long temp;
    signed char count;
    struct WBTV_Time_t t;
    start:
    sumFast=sumSlow =0;
    waitTillICanSend();
    
    //Send the start byte.
    if (writeWrapper('!'))
        {
            //If we sent the initial start byte sucessful
            //Get the current time at which the ! was sent
            t = WBTVClock_get_time();
        }
    else
    {
        goto  start;
    }
    
    //Send and hash each byte of the channel name.
    //We use writewrapper and not escapedwrite TIME
    if(!writeWrapper('T'))
       {
        goto start;
       }
     if(!writeWrapper('I'))
       {
        goto start;
       }
        if(!writeWrapper('M'))
       {
        goto start;
       }
        if(!writeWrapper('E'))
       {
        goto start;
       }
       updateHash('T');updateHash('I');updateHash('M');updateHash('E');
    
    //Send the divider
    if(!writeWrapper('~'))
       {
        goto start;
       }
    
    //If the protocol definition says to include the ~ in the hash, update it.
    #ifdef WBTV_HASH_STX
    updateHash('~');
    #endif
    
    
    //Sent the 64 byte seconds count.
    for (i=0;i<8;i++)
    {
        if(!escapedWrite(((const unsigned char *)(& t.seconds))[i]))
        {
            //Go back and retry if we get interfered with.
            goto start;
        }
        updateHash(((const unsigned char *)(& t.seconds))[i]);
    }
    
    //We don't know what the two least significant bytes of the 32 bit fraction are, because we only use 16 bits
    //internally
    //So we just send 0.5 times the possible range.
    if(!escapedWrite(0))
        {
            goto start;
        }
        updateHash(0);
        
    if(!escapedWrite(127))
        {
            goto start;
        }
        updateHash(127);
        
    //Send the two most significant bytes of the fraction part of the time.
    for (i=0;i<2;i++)
    {
        if(!escapedWrite(((unsigned char *)(& t.fraction))[i]))
        {
            goto start;
        }
        updateHash(((unsigned char *)(& t.fraction))[i]);
    }
    
    //If the error is too high to count, assume that it could be any crazy insane number.
    //Like perhaps older than the earth....
    if(WBTVClock_error >= 4294967294ul)
    {
      
        if(!escapedWrite(127))
        {
            goto start;
        }
        updateHash(127);
        
        if(!escapedWrite(255))
        {
            goto start;
        }
        updateHash(255);
    }
    
    else
    {
        temp = WBTVClock_error;
        count =-16;
        
        //We repeatedly divide by 2, until we get to 255 or less.
        //This is equal to dividing the count by 2 to the n where n is the smallest
        //number that produces a result less than 256.
        //We keep track of the number of divides.
        
        
        //Our error count was originally in 2 to the 16ths of a second.
        //Therefore, if we have only one of those ticks, our error
        //should be one times 2^-16, so we start the initial count off at -16.
        //For every time we divide, we increase the count by one.
        while (temp > 255)
        {
            count ++;
            temp = temp>>1;
            
        }
    
    //We send the count of divides
    if(!escapedWrite(count))
        {
            goto start;
        }
        updateHash(count);
        
    //Now we send the actual value
    if(!escapedWrite(temp&0xff))
        {
            goto start;
        }
        updateHash(temp&0xff);
    }
    
    //and now the hash
        if(!escapedWrite(sumFast))
        {
            goto start;
        }
        
        if(!escapedWrite(sumSlow))
        {
            goto start;
        }

        if(!writeWrapper('\n'))
        {
            goto start;
        }
}

#endif
