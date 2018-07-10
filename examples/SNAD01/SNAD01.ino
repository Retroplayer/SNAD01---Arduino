#include <SNAD01.h>

//Defines for attrib register
#define Analog 1
#define Digital 0

#define AllAnalog 0xFF
#define AllDigital 0x00


//Defines for WakeUp register
#define Enable 1
#define Disable 0
#define NoWkup 0x00
#define AllWkup 0xFF

//Defines for Control Register
//These are all possible, valid combinations, see datasheet
#define Pullups_Internal 0xa0
#define Pullups_External 0x90
#define Pulldowns_Internal 0x60
#define Pulldowns_External 0x50
#define Internal 0x20
#define External 0x10

//Create a new instance of class SNAD01
//and define the pins used for bit-banging
//adc = SNAD01(DIO pin, CLK pin, START pin)
SNAD01 adc = SNAD01(8,9,10);
uint8_t t=0;

float analog[8];
uint8_t digital[8];

void setup(){

  uint8_t reg;
  reg = External; //Set for external reference on REF pin, no pullups or pulldowns enabled

  //Set the registers in the SNAD01
  //adc.init(reg,attrib,wkup)
  adc.init(0x10,0xFF,0x00);//external reference, all analog, no wakeup

  Serial.begin(9600);

}


void loop(){
  unsigned char c;
  uint8_t res;

  if(t==8){
    t=0;
  }

  if(Serial.available()){
    c = Serial.read();
    if (c  == 'a'){
      for(t=0;t<8;t++){    
        if(t==7){
          Serial.write("Chan 7 = ");
          res = adc.Convert(t);
          analog[t] = (res*6) * (3.27 / 254.0); //Channel 7 is scaled 1/16th, so we scale it back up
          Serial.println(analog[t]);
        }
        else{
          res = adc.Convert(t);
          analog[t] = res * (3.27 / 254.0); //3.27 is my voltage in test configuration. Change to your system voltage

          Serial.write("Chan ");
          Serial.write(char(t+0x30));
          Serial.write(" = ");
          Serial.println(analog[t]);
        }

      }
    }
    else{

    if (c  == 'd')
    {
      Serial.println("Getting Digital");
      res = adc.getDigital();

     for(t=0;t<8;t++){
        digital[t] = bitRead(res,t);
        Serial.write("Chan ");
        Serial.write(char(t+0x30));
        Serial.write(" = ");
        Serial.println(digital[t]);

      }
    }
  }
  }
}

