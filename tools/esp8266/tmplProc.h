//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#ifndef __TMPL_PROC__
#define __TMPL_PROC__

// HTML template processor, searches keywords and calls callback
// inspired by: https://github.com/plapointe6/EspHtmlTemplateProcessor

#include "dbg.h"
#include <string.h>
#include <functional>
#include "ESPAsyncWebServer.h"

#define MAX_BUFFER_SIZE     256
#define MAX_KEY_LEN         20
enum { ST_NONE = 0, ST_BUF, ST_PROC, ST_START, ST_KEY };

typedef std::function<String(char* key)> TmplCb;

class tmplProc {
    public:
        tmplProc(AsyncWebServerRequest *request, uint32_t bufStartSize = 1000) {
            // Note: don't choose the bufStartSize to small. A too small start
            //       size will result in fractioned memory and maybe in a zero
            //       increase -> fail (Arduino - cbuf.cpp)
            mRequest  = request;
            mResponse = request->beginResponseStream("text/html", bufStartSize);
        }

        ~tmplProc() {}

        void process(const char* tmpl, const uint32_t tmplLen, TmplCb cb) {
        char* buf = new char[MAX_BUFFER_SIZE];
        char* p = buf;
        uint32_t len = 0, pos = 0, i = 0;
        uint8_t state = ST_BUF;
        bool complete = false;

        while (i < tmplLen) {
            switch (state) {
                default:
                    DPRINTLN(DBG_DEBUG, F("unknown state"));
                    break;
                case ST_BUF:
                    if(0 != i) {
                        buf[pos] = '\0';
                        mResponse->print(p);
                    }
                    pos = 0;
                    len = ((tmplLen - i) > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : (tmplLen - i);
                    if((len + i) == tmplLen)
                        complete = true;
                    memcpy_P(buf, &tmpl[i], len);
                    if(len < MAX_BUFFER_SIZE)
                        buf[len] = '\0';
                    p = buf;
                    state = ST_PROC;
                    break;

                case ST_PROC:
                    if(((pos + MAX_KEY_LEN) >= len) && !complete)
                        state = ST_BUF;
                    else if(buf[pos] == '{')
                        state = ST_START;
                    break;

                case ST_START:
                    if(buf[pos] == '#') {
                        if(pos != 0)
                            buf[pos-1] = '\0';
                        mResponse->print(p);
                        p = &buf[pos+1];
                        state = ST_KEY;
                    }
                    else
                        state = ST_PROC;
                    break;

                case ST_KEY:
                    if(buf[pos] == '}') {
                        buf[pos] = '\0';
                        mResponse->print((cb)(p));
                        p = &buf[pos+1];
                        state = ST_PROC;
                    }
                    break;
            }

            if(ST_BUF != state) {
                pos++;
                i++;
            }
        }

        mResponse->print(p);
        delete[] buf;
        mRequest->send(mResponse);
    }

    private:
        AsyncWebServerRequest *mRequest;
        AsyncResponseStream *mResponse;
};

#endif /*__TMPL_PROC__*/
