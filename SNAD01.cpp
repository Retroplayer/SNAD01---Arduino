/*
 * SONIX SNAD01x Series ADC Library for Arduino
 * Modified version of SoftI2CMaster by Tod E. Kurt, http://todbot.com/blog/
 * Mod written by scott McDonnell 01/2013
 */

#include "Arduino.h"
#include "pins_arduino.h"
#include "SNAD01.h"

#include <avr/delay.h>
#include <string.h>

#define  bitdelay 5

//Define commands (in binary)
#define cmd_reg 0x60
#define cmd_pwdn 0x00
#define cmd_wkup 0x40
#define cmd_attrib 0x20
#define cmd_conv 0x80
#define cmd_dig 0xa0




//_XXXPortModeRegister = DDR register
//_XXXPortRegister = Port data register
//_XXXPinRegister = Input Register

//macro for setting DIO pin HIGH
#define DIO_hi()                            \
    digitalWrite(_DIO, HIGH);  

//macro for setting DIO pin Low
#define DIO_lo()                            \
    digitalWrite(_DIO, LOW);  

//Macro for setting START pin HIGH    
#define START_hi()                            \
    digitalWrite(_START, HIGH);
    
//Macro for setting START pin Low     
#define START_lo()                            \
    digitalWrite(_START, LOW);  

//Macro for setting DIO as input
#define DIO_in(){                            \
    digitalWrite(_DIO, LOW);  								\
    pinMode(_DIO, INPUT_PULLUP); \
   }
    
//Macro for setting DIO pin as output
#define DIO_out()                            \
		digitalWrite(_DIO,LOW); \
    pinMode(_DIO, OUTPUT);
    
//
// Constructor
//

//Usage: ADC = SNAD01(2,3,4); DIO = pin 2, CLK = pin 3, START = pin 4
SNAD01::SNAD01(uint8_t DIO, uint8_t CLK, uint8_t START) 
{
  setPins(DIO, CLK, START);
  
  pinMode(_START,OUTPUT);
  digitalWrite(_START, HIGH);
  
  pinMode(_CLK, OUTPUT);
  digitalWrite(_CLK, LOW);
  
  digitalWrite(_DIO,LOW);
  pinMode(_DIO,INPUT);
}

void SNAD01::setPins(uint8_t DIO, uint8_t CLK, uint8_t START)
{
	//Define pins for CLK, START, DIO
	
  uint8_t port;

  _DIO = DIO;
  _CLK = CLK;
  _START = START;

  _DIOBitMask = digitalPinToBitMask(DIO);
  _CLKBitMask = digitalPinToBitMask(CLK);
  _STARTBitMask = digitalPinToBitMask(START);
  
  port = digitalPinToPort(CLK);
  _CLKPortRegister     = portOutputRegister(port);
  _CLKPortModeRegister = portModeRegister(port);

  port = digitalPinToPort(DIO);
  _DIOPortRegister     = portOutputRegister(port);//Port Register
  _DIOPortModeRegister = portModeRegister(port); //DDR register
  _DIOPinRegister =      portInputRegister(port);//Pin Register (input)

    port = digitalPinToPort(START);
  _STARTPortRegister     = portOutputRegister(port);
  _STARTPortModeRegister = portModeRegister(port);

}

//Usage: ADC.SNAD_init(reg,attrib,wkup);
void SNAD01::init(uint8_t reg, uint8_t attrib, uint8_t wkup)
{

//Setup the control register
START_lo();
write(cmd_reg, 3);
write(reg,4);
START_hi();
DIO_in();
delayMicroseconds(bitdelay);

//Setup the attributes register (1:analog, 0:digital)
START_lo();
write(cmd_attrib,3);
write(attrib,8);
START_hi();
DIO_in();
delayMicroseconds(bitdelay);

//Setup the Wakeup feature (0:disable, 1:enable)
START_lo();
write(cmd_wkup,3);
write(wkup,8);
START_hi();
DIO_in();
delayMicroseconds(bitdelay);

}

void SNAD01::PWRDN(){
	
//Prepare for power down (set RF and MB bits to 0)
START_lo();
write(cmd_reg,3);
write(0x0,4); //need some way of setting only the RF and MB bits... hmm
START_hi();
DIO_in();

_delay_us(bitdelay);

//send cmd for powerdown (0x000b)
START_lo();
write(cmd_pwdn,3);
START_hi();
DIO_in();

}

//--------------------------------------------------------------------
//Get Readings (Digital and Analog Routines)
//--------------------------------------------------------------------
uint8_t SNAD01::Convert(uint8_t chan){
//Send command for analog conversion
//read one byte and return byte	

uint8_t res;

START_lo();
write(cmd_conv,3);

//PulseClock();
//PulseClock();

chan =chan << 5;

write(chan,4);

PulseClock();


res = read();

START_hi();
DIO_in();

return res;
	
}

uint8_t SNAD01::getDigital(){
//send command for digital reading
//read one byte, return byte
uint8_t res;

START_lo();
write(cmd_dig, 3);

res=read();

digitalWrite(_START,HIGH);

return res;
}

void SNAD01::write(uint8_t c, uint8_t length){

	uint8_t i;

	DIO_out();
	
	for (i = 0; i < length; i++)  {
			digitalWrite(_DIO, !!(c & (1 << (7 - i))));
			PulseClock();		
	}
}

uint8_t SNAD01::read(){
uint8_t c=0;
uint8_t i;

pinMode(_DIO,INPUT_PULLUP);

	for (i = 0; i < 8; ++i) {
		digitalWrite(_CLK, HIGH);
			c |= digitalRead(_DIO) << (7 - i);
		digitalWrite(_CLK, LOW);
		
	}
	
return c;
}

void SNAD01::PulseClock(){ 
	digitalWrite(_CLK, HIGH); 
	_delay_us(bitdelay); 
	digitalWrite(_CLK, LOW);
}
