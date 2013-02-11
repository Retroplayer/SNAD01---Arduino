/*
 * SONIX SNAD01x Series ADC library for Arduino
 * Modified version of SoftI2CMaster written by Tod E. Kurt, http://todbot.com/blog/
 * Mod written by Scott McDonnell 01/2013
 */

#ifndef h
#define h

#include <inttypes.h>

#define _VERSION 0 // software version of this library

class SNAD01
{
private:
  // per object data
  uint8_t _DIO;
  uint8_t _CLK;
  uint8_t _START;
  uint8_t _DIOBitMask;
  uint8_t _CLKBitMask;
  uint8_t _STARTBitMask;
  
  volatile uint8_t *_DIOPortRegister;
  volatile uint8_t *_CLKPortRegister;
  volatile uint8_t *_STARTPortRegister;
  volatile uint8_t *_DIOPortModeRegister;
  volatile uint8_t *_CLKPortModeRegister;
  volatile uint8_t *_STARTPortModeRegister;
  volatile uint8_t *_DIOPinRegister;

  // private methods
  void setPins(uint8_t DIO, uint8_t CLK, uint8_t START);
  void writebit( uint8_t c );
  uint8_t readbit(void);
  void write( uint8_t c, uint8_t len );
  uint8_t read( );
  void PulseClock();
//  void sendCMD(uint8_t c);
//  void sendChan(uint8_t c);
//  void sendReg(uint8_t c);
  
  
public:
  // public methods
  SNAD01(uint8_t DIO, uint8_t CLK, uint8_t START);
  void init(uint8_t reg, uint8_t mode, uint8_t Wkup);
  void PWRDN();
  uint8_t Convert(uint8_t chan);
  uint8_t getDigital();
 
};

#endif
