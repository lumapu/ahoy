//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------


#ifndef __DBG_H__
#define __DBG_H__
#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif

//-----------------------------------------------------------------------------
// available levels
#define DBG_ERROR       1
#define DBG_WARN        2
#define DBG_INFO        3
#define DBG_DEBUG       4
#define DBG_VERBOSE     5


//-----------------------------------------------------------------------------
// globally used level
#define DEBUG_LEVEL DBG_INFO       //adapt DEBUG_LEVEL to your needs

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
    #define PERR(str) DBGPRINT(F("\nE: ")); DBGPRINT(str);
    #define PERRLN(str) DBGPRINT(F("\nE: ")); DBGPRINTLN(str);
    #define _PERR(str) DBGPRINT(str);
    #define _PERRLN(str) DBGPRINTLN(str);
    #define _PERRHEX(str) DHEX(str);
#else
    //no output
    #define PERR(str)
    #define PERRLN(str)
    #define _PERR(str)
    #define _PERRLN(str)
    #define _PERRHEX(str) 
#endif

#if DEBUG_LEVEL >= DBG_WARN
    #define PWARN(str) DBGPRINT(F("\nW: ")); DBGPRINT(str);
    #define PWARNLN(str) DBGPRINT(F("\nW: ")); DBGPRINTLN(str);
    #define _PWARN(str) DBGPRINT(str);
    #define _PWARNLN(str) DBGPRINTLN(str);
    #define _PWARNHEX(str) DHEX(str);
#else
    //no output
    #define PWARN(str)
    #define PWARNLN(str)
    #define _PWARN(str) 
    #define _PWARNLN(str)
    #define _PWARNHEX(str)
#endif

#if DEBUG_LEVEL >= DBG_INFO
    #define PINFO(str) DBGPRINT(F("\nI: ")); DBGPRINT(str);
    #define PINFOLN(str) DBGPRINT(F("\nI: ")); DBGPRINTLN(str);
    #define _PINFO(str) DBGPRINT(str);
    #define _PINFOLN(str) DBGPRINTLN(str);
    #define _PINFOHEX(str) DHEX(str);
#else
    //no output
    #define PINFO(str)
    #define PINFOLN(str)
    #define _PINFO(str)
    #define _PINFOLN(str)
    #define _PINFOHEX(str)
#endif

#if DEBUG_LEVEL >= DBG_DEBUG
    #define PDBG(str) DBGPRINT(F("\nD: ")); DBGPRINT(str);
    #define PDBGLN(str) DBGPRINT(F("D: ")); DBGPRINTLN(str);
    #define _PDBG(str) DBGPRINT(str);
    #define _PDBGLN(str) DBGPRINTLN(str);
    #define _PDBGHEX(str) DHEX(str);
#else
    //no output
    #define PDBG(str)
    #define PDBGLN(str)
    #define _PDBG(str)
    #define _PDBGLN(str)
    #define _PDBGHEX(str)
#endif

#if DEBUG_LEVEL >= DBG_VERBOSE
    #define PVERB(str) DBGPRINT(F("\nV: ")); DBGPRINT(str);
    #define PVERBLN(str) DBGPRINT(F("\nV: ")); DBGPRINTLN(str);
    #define _PVERB(str) DBGPRINT(str);
    #define _PVERBLN(str) DBGPRINTLN(str);
    #define _PVERBHEX(str) DHEX(str);
#else
    #define PVERB(str)
    #define PVERBLN(str)
    #define _PVERB(str)
    #define _PVERBLN(str)
    #define _PVERBHEX(str)
#endif



//external methods

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

#define _DPRINT(level, str) ({\
    switch(level) {\
        case DBG_ERROR: _PERR(str);  break; \
        case DBG_WARN:  _PWARN(str); break; \
        case DBG_INFO:  _PINFO(str); break; \
        case DBG_DEBUG: _PDBG(str);  break; \
        default:        _PVERB(str); break; \
    }\
})

#define _DPRINTLN(level, str) ({\
    switch(level) {\
        case DBG_ERROR: _PERRLN(str);  break; \
        case DBG_WARN:  _PWARNLN(str); break; \
        case DBG_INFO:  _PINFOLN(str); break; \
        case DBG_DEBUG: _PDBGLN(str);  break; \
        default:        _PVERBLN(str); break; \
    }\
})

//2022-10-30: added hex output
#define _DPRINTHEX(level, str) ({\
    switch(level) {\
        case DBG_ERROR: _PERRHEX(str);  break; \
        case DBG_WARN:  _PWARNHEX(str); break; \
        case DBG_INFO:  _PINFOHEX(str); break; \
        case DBG_DEBUG: _PDBGHEX(str);  break; \
        default:        _PVERBHEX(str); break; \
    }\
})

#endif /*__DBG_H__*/
