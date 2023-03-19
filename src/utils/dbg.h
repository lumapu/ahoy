//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
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

        inline void registerDebugCb(DBG_CB cb) {
            mCb = cb;
        }

        #ifndef DSERIAL
            #define DSERIAL Serial
        #endif

        //template <class T>
        inline void DBGPRINT(String str) { DSERIAL.print(str); if(NULL != mCb) mCb(str); }
        //template <class T>
        inline void DBGPRINTLN(String str) { DBGPRINT(str); DBGPRINT(F("\r\n")); }
        inline void DHEX(uint8_t b) {
            if( b<0x10 ) DSERIAL.print(F("0"));
            DSERIAL.print(b,HEX);
            if(NULL != mCb) {
                if( b<0x10 ) mCb(F("0"));
                mCb(String(b, HEX));
            }
        }

        inline void DBGHEXLN(uint8_t b) {
            DHEX(b);
            DBGPRINT(F("\r\n"));
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

#define DPRINT_INIT(level,text) ({\
    DPRINT(level,F(""));\
    DBGPRINT_TXT(text);\
})

#define DPRINTLN_TXT(level,text) ({\
    DPRINT(level,F(""));\
    DBGPRINTLN_TXT(text);\
})

#define DPRINTHEAD(level, id) ({\
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


// available text variables
#define TXT_ENQUCMD     6
#define TXT_INVSERNO    2
#define TXT_REQRETR     7
#define TXT_PPYDMAX    10
#define TXT_NOPYLD      1

#define TXT_GDEVINF     3
#define TXT_DEVCTRL     4
#define TXT_INCRALM     5
#define TXT_PPYDCMD     8
#define TXT_PPYDTXI     9


/*                    DBGPRINT(F(" power limit "));
DPRINTLN(DBG_DEBUG, F("Response from info request received"));

DBGPRINTLN(F("Response from devcontrol request received"));
                    DPRINT(DBG_DEBUG, F("fragment number zero received"));
                    DBGPRINT(F("has accepted power limit set point "));
                    DBGPRINT(F(" with PowerLimitControl "));
                    DPRINT(DBG_INFO, F("Payload (") + String(payloadLen) + "): ");
                    DPRINTLN(DBG_ERROR, F("plausibility check failed, expected ") + String(rec->pyldLen) + F(" bytes"));

                DPRINTLN(DBG_VERBOSE, F("incomlete, txCmd is 0x") + String(txCmd, HEX)); // + F("cmd is 0x") + String(cmd, HEX));
*/



#define DBGPRINT_TXT(text) ({\
    switch(text) {\
        case TXT_NOPYLD:   DBGPRINT(F("no Payload received! (retransmits: ")); break; \
        case TXT_INVSERNO: DBGPRINT(F("Requesting Inv SN ")); break; \
        case TXT_GDEVINF:  DBGPRINT(F("prepareDevInformCmd 0x")); break; \
        case TXT_DEVCTRL:  DBGPRINT(F("Devcontrol request 0x")); break; \
        case TXT_INCRALM:  DBGPRINT(F("alarm ID incremented to ")); break; \
        case TXT_ENQUCMD:  DBGPRINT(F("enqueuedCmd: 0x")); break; \
        case TXT_REQRETR:  DBGPRINT(F(" missing: Request Retransmit")); break; \
        case TXT_PPYDCMD:  DBGPRINT(F("procPyld: cmd:  0x")); break; \
        case TXT_PPYDTXI:  DBGPRINT(F("procPyld: txid: 0x")); break; \
        case TXT_PPYDMAX:  DBGPRINT(F("procPyld: max:  ")); break; \
        default:        ; break; \
    }\
})


// available text variables w. lf
#define TXT_BUILD       1
#define TXT_TIMEOUT     2
#define TXT_NOPYLD2     3
#define TXT_CRCERR      4
#define TXT_RSTPYLD     5
#define TXT_NULLREC     7
#define TXT_PREVSND     8
#define TXT_RESPLIM     9

//resetPayload

#define DBGPRINTLN_TXT(text) ({\
    switch(text) {\
        case TXT_TIMEOUT:  DBGPRINTLN(F("enqueued cmd failed/timeout"));  break; \
        case TXT_BUILD:    DBGPRINTLN(F("build")); break; \
        case TXT_NOPYLD2:  DBGPRINTLN(F("nothing received")); break; \
        case TXT_RSTPYLD:  DBGPRINTLN(F("resetPayload"));break; \
        case TXT_CRCERR:   DBGPRINTLN(F("CRC Error: Request Complete Retransmit")); break; \
        case TXT_NULLREC:  DBGPRINTLN(F("record is NULL!")); break; \
        case TXT_PREVSND:  DBGPRINTLN(F("Prevent retransmit on Restart / CleanState_LockAndAlarm...")); break; \
        case TXT_RESPLIM:  DBGPRINTLN(F("retransmit power limit")); break; \
        default:        ; break; \
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
