//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __DBG_H__
#define __DBG_H__
#if defined(F) && defined(ESP32)
  #undef F
  #define F(sl) (sl)
#endif

#include <functional>
//-----------------------------------------------------------------------------
// available levels
#define DBG_ERROR       1
#define DBG_WARN        2
#define DBG_INFO        3
#define DBG_DEBUG       4
#define DBG_VERBOSE     5

//#define LOG_MAX_MSG_LEN 100


//-----------------------------------------------------------------------------
// globally used level
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DBG_INFO
#endif

#ifdef ARDUINO
  #include "Arduino.h"
#endif

#ifdef NDEBUG
    #define DBGPRINT(str)
    #define DBGPRINTLN(str)
#else
    #ifdef ARDUINO
        #define DBG_CB std::function<void(String)>
        extern DBG_CB mCb;
        extern bool mDebugEn;

        inline void registerDebugCb(DBG_CB cb) {
            mCb = cb;
        }

        #ifndef DSERIAL
            #define DSERIAL Serial
        #endif

        inline void setDebugEn(bool en) {
            mDebugEn = en;
        }

        //template <class T>
        inline void DBGPRINT(String str, bool ser = true) { if(ser && mDebugEn) DSERIAL.print(str); if(NULL != mCb) mCb(str); }
        //template <class T>
        inline void DBGPRINTLN(String str, bool ser = true) { DBGPRINT(str); DBGPRINT(F("\r\n")); }
        inline void DHEX(uint8_t b, bool ser = true) {
            if(ser && mDebugEn) {
                if( b<0x10 ) DSERIAL.print(F("0"));
                DSERIAL.print(b,HEX);
            }
            if(NULL != mCb) {
                if( b<0x10 ) mCb(F("0"));
                mCb(String(b, HEX));
            }
        }

        inline void DBGHEXLN(uint8_t b, bool ser = true) {
            DHEX(b, ser);
            DBGPRINT(F("\r\n"), ser);
        }
        /*inline void DHEX(uint16_t b) {
            if( b<0x10 ) DSERIAL.print(F("000"));
            else if( b<0x100 ) DSERIAL.print(F("00"));
            else if( b<0x1000 ) DSERIAL.print(F("0"));
            DSERIAL.print(b, HEX);
            if(NULL != mCb) {
                if( b<0x10 ) mCb(F("000"));
                else if( b<0x100 ) mCb(F("00"));
                else if( b<0x1000 ) mCb(F("0"));
                mCb(String(b, HEX));
            }
        }
        inline void DHEX(uint32_t b) {
            if( b<0x10 ) DSERIAL.print(F("0000000"));
            else if( b<0x100 ) DSERIAL.print(F("000000"));
            else if( b<0x1000 ) DSERIAL.print(F("00000"));
            else if( b<0x10000 ) DSERIAL.print(F("0000"));
            else if( b<0x100000 ) DSERIAL.print(F("000"));
            else if( b<0x1000000 ) DSERIAL.print(F("00"));
            else if( b<0x10000000 ) DSERIAL.print(F("0"));
            DSERIAL.print(b, HEX);
            if(NULL != mCb) {
                if( b<0x10 ) mCb(F("0000000"));
                else if( b<0x100 ) mCb(F("000000"));
                else if( b<0x1000 ) mCb(F("00000"));
                else if( b<0x10000 ) mCb(F("0000"));
                else if( b<0x100000 ) mCb(F("000"));
                else if( b<0x1000000 ) mCb(F("00"));
                else if( b<0x10000000 ) mCb(F("0"));
                mCb(String(b, HEX));
            }
        }*/
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

#define DPRINT_IVID(level, id) ({\
    DPRINT(level, F("(#")); DBGPRINT(String(id)); DBGPRINT(F(") "));\
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


/*class ahoyLog {
    public:
        ahoyLog() {}

        inline void logMsg(uint8_t lvl, bool newLine, const char *fmt, va_list args) {
            snprintf(mLogBuf, LOG_MAX_MSG_LEN, fmt, args);
            DSERIAL.print(mLogBuf);
            if(NULL != mCb)
                mCb(mLogBuf);
            if(newLine) {
                DSERIAL.print(F("\r\n"));
                if(NULL != mCb)
                    mCb(F("\r\n"));
            }
        }

        inline void logError(const char *fmt, ...) {
            #if DEBUG_LEVEL >= DBG_ERROR
                va_list args;
                va_start(args, fmt);
                logMsg(DBG_ERROR, true, fmt, args);
                va_end(args);
            #endif
        }

        inline void logWarn(const char *fmt, ...) {
            #if DEBUG_LEVEL >= DBG_WARN
                va_list args;
                va_start(args, fmt);
                logMsg(DBG_ERROR, true, fmt, args);
                va_end(args);
            #endif
        }

        inline void logInfo(const char *fmt, ...) {
            #if DEBUG_LEVEL >= DBG_INFO
                va_list args;
                va_start(args, fmt);
                logMsg(DBG_ERROR, true, fmt, args);
                va_end(args);
            #endif
        }

    private:
        char mLogBuf[LOG_MAX_MSG_LEN];
        DBG_CB mCb = NULL;
};*/

#endif /*__DBG_H__*/
