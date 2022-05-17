#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef NDEBUG
  #define DPRINT(str)
  #define DPRINTLN(str)
#else

#ifndef DSERIAL
  #define DSERIAL Serial
#endif

  template <class T>
  inline void DPRINT(T str) { DSERIAL.print(str); }
  template <class T>
  inline void DPRINTLN(T str) { DPRINT(str); DPRINT(F("\r\n")); }
  inline void DHEX(uint8_t b) {
    if( b<0x10 ) DSERIAL.print('0');
    DSERIAL.print(b,HEX);
  }
  inline void DHEX(uint16_t b) {
    if( b<0x10 ) DSERIAL.print(F("000"));
    else if( b<0x100 ) DSERIAL.print(F("00"));
    else if( b<0x1000 ) DSERIAL.print(F("0"));
    DSERIAL.print(b,HEX);
  }
  inline void DHEX(uint32_t b) {
    if( b<0x10 ) DSERIAL.print(F("0000000"));
    else if( b<0x100 ) DSERIAL.print(F("000000"));
    else if( b<0x1000 ) DSERIAL.print(F("00000"));
    else if( b<0x10000 ) DSERIAL.print(F("0000"));
    else if( b<0x100000 ) DSERIAL.print(F("000"));
    else if( b<0x1000000 ) DSERIAL.print(F("00"));
    else if( b<0x10000000 ) DSERIAL.print(F("0"));
    DSERIAL.print(b,HEX);
  }
#endif

#endif /*__DEBUG_H__*/
