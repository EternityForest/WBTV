#include <WBTVNode.h>

void setup()
{
    delay(250);
    Serial.begin(9600);
    delay(250);
}

//Loop around in circles, sending bytes repeatadly.
void loop()
{
    Serial.write(WBTV_urand_byte());
    //Mix in A0 into the state. This does not replace the current state but XORs
    //A0 into it. It also XORs the current micros value into the state.
    WBTV_doRand(analogRead(A0));
}