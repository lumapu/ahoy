//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __DBG_H__
#define __DBG_H__

//-----------------------------------------------------------------------------
// available levels
#define DBG_ERROR       1
#define DBG_WARN        2
#define DBG_INFO        3
#define DBG_DEBUG       4
#define DBG_VERBOSE     5


//-----------------------------------------------------------------------------
// globally used level
#define DEBUG_LEVEL DBG_INFO

#ifdef ARDUINO
  #include "Arduino.h"
#endif

#ifdef NDEBUG
    #define DBGPRINT(str)
    #define DBGPRINTLN(str)
#else
    #ifdef ARDUINO
        #ifndef DSERIAL
            #define DSERIAL Serial
        #endif

        template <class T>
        inline void DBGPRINT(T str) { DSERIAL.print(str); }
        template <class T>
        inline void DBGPRINTLN(T str) { DBGPRINT(str); DBGPRINT(F("\r\n")); }
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
#endif


#if DEBUG_LEVEL >= DBG_ERROR
    #define PERR(str) DBGPRINT(F("E: ")); DBGPRINT(str);
    #define PERRLN(str) DBGPRINT(F("E: ")); DBGPRINTLN(str);
#else
    #define PERR(str)
    #define PERRLN(str)
#endif

#if DEBUG_LEVEL >= DBG_WARN
    #define PWARN(str) DBGPRINT(F("W: ")); DBGPRINT(str);
    #define PWARNLN(str) DBGPRINT(F("W: ")); DBGPRINTLN(str);
#else
    #define PWARN(str)
    #define PWARNLN(str)
#endif

#if DEBUG_LEVEL >= DBG_INFO
    #define PINFO(str) DBGPRINT(F("I: ")); DBGPRINT(str);
    #define PINFOLN(str) DBGPRINT(F("I: ")); DBGPRINTLN(str);
#else
    #define PINFO(str)
    #define PINFOLN(str)
#endif

#if DEBUG_LEVEL >= DBG_DEBUG
    #define PDBG(str) DBGPRINT(F("D: ")); DBGPRINT(str);
    #define PDBGLN(str) DBGPRINT(F("D: ")); DBGPRINTLN(str);
#else
    #define PDBG(str)
    #define PDBGLN(str)
#endif

#if DEBUG_LEVEL >= DBG_VERBOSE
    #define PVERB(str) DBGPRINT(F("V: ")); DBGPRINT(str);
    #define PVERBLN(str) DBGPRINT(F("V: ")); DBGPRINTLN(str);
#else
    #define PVERB(str)
    #define PVERBLN(str)
#endif

#define DPRINT(level, str) ({\
    switch(level) {\
        case DBG_ERROR: PERR(str);  break; \
        case DBG_WARN:  PWARN(str); break; \
        case DBG_INFO:  PINFO(str); break; \
        case DBG_DEBUG: PDBG(str);  break; \
        default:        PVERB(str); break; \
    }\
})

#define DPRINTLN(level, str) ({\
    switch(level) {\
        case DBG_ERROR: PERRLN(str);  break; \
        case DBG_WARN:  PWARNLN(str); break; \
        case DBG_INFO:  PINFOLN(str); break; \
        case DBG_DEBUG: PDBGLN(str);  break; \
        default:        PVERBLN(str); break; \
    }\
})

#endif /*__DBG_H__*/
