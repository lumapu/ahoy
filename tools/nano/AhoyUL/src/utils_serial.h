
// 2022 mb for AHOY-UL (Arduino Nano, USB light IF)
// todo: make class

#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "dbg.h"

class SerialUtils {
   public:
    SerialUtils() {
        // some member vars here
    }

    ~SerialUtils() {}

    char *mSerBuffer;
    char *mTokens[MAX_SER_PARAM];                   // this pointer array is used to tokenize the mSerBuffer, the data are kept in serBuffer
    char *mStrOutBuf;
    uint8_t *mByteBuffer;

    void setup(uint8_t mMAX_IN_CHAR = MAX_SERBYTES, uint8_t mMAX_OUT_CHAR = MAX_STRING_LEN, uint8_t mNUM_CMD_PARA = MAX_SER_PARAM) {
        // mMAX_IN_CHAR = MAX_IN_CHAR;
        // mMAX_OUT_CHAR = MAX_OUT_CHAR;
        // mNUM_CMD_PARA = NUM_CMD_PARA;

        // allocate serial buffer space
        mSerBuffer = new char[mMAX_IN_CHAR + 1];  // one extra byte for \0 termination
        mByteBuffer = new uint8_t[mMAX_IN_CHAR / 2];
        mStrOutBuf = new char[mMAX_OUT_CHAR + 1];
        // char pointer array for string token
        //mTokens = new char*[mNUM_CMD_PARA];

        mSerBuffer[mMAX_IN_CHAR] = '\0';
        mStrOutBuf[mMAX_OUT_CHAR] = '\0';
    }

    char *getStrOutBuf() {
        return mStrOutBuf;
    }

    char *getInBuf() {
        return mSerBuffer;
    }

    char **getParamsBuf() {
        return mTokens;
    }

    /******************************************************************************************************
     * serial read function with timeout
     * With ATMEGA328p it can only detect 64byte at once (serial input buffer size) at the baudrate of 115200baud (~5ms per byte)
     */
    static int serReadUntil(char _ch, char *_str, int _len, uint16_t _TIMEOUT) {
        static volatile uint16_t _backupTO;
        static volatile int thislen;

        _backupTO = Serial.getTimeout();
        thislen = 0;

        Serial.setTimeout(_TIMEOUT);
        thislen = Serial.readBytesUntil(_ch, _str, _len);  // can only be used unto max serial input buffer of 64bytes
        _str[thislen] = '\0';                              // terminate char* str with '\0', data are available external now
        if (_len > 0) {
            DPRINT(DBG_DEBUG, F("ser Rx "));
            _DPRINT(DBG_DEBUG, _str);
            _DPRINT(DBG_DEBUG, F("  len "));
            _DPRINT(DBG_DEBUG, thislen);
        } else {
            DPRINT(DBG_DEBUG, F("ser TIMEOUT"));
        }
        Serial.setTimeout(_backupTO);
        return thislen;
    }  // end serReadUntil()

    static int c_remove(char _chrm, char *_str, int _len) {
        int _ir = 0;
        int _iw = 0;
        // _ir = 0;
        // _iw = 0;
        while (_str[_ir] && _ir < _len) {
            if (_str[_ir] != _chrm) {
                _str[_iw++] = _str[_ir];
            }
            _ir++;
        }
        _str[_iw] = '\0';
        return _iw;
    }  // end c_remove()

    static void c_replace(char _crpl, char _cnew, char *_str, int _len) {
        int _ir = 0;
        _ir = 0;
        while (_str[_ir] && _ir < _len) {
            if (_str[_ir] == _crpl) {
                _str[_ir] = _cnew;
            }
            _ir++;
        }
        return;
    }  // end c_replace

    static void c_lower(char *_str, int _len) {
        int _ir = 0;
        _ir = 0;
        while (_str[_ir] && _ir < _len) {
            if (_str[_ir] >= 'A' && _str[_ir] < 'Z') {
                _str[_ir] = (char)(_str[_ir] + 0x20);
            }
            _ir++;
        }
        return;
    }  // end c_lower

    // todo: use strtol(const char *str, char **endptr, int base) instead
    /**
     * converts 2 digits hex string to uint8_t byte, the high byte is FF in case of non-hex digit
     */
    /*
    static uint16_t x2b(char high, char low) {
      static volatile uint16_t onebyte=0;
      onebyte = 0;
      if(high >= '0' && high <= '9') onebyte = (uint16_t) high - '0';
      else if (high >='A' && high <= 'F') onebyte = (uint16_t) high - 'A' + 10;
      else if (high >='a' && high <= 'f') onebyte = (uint16_t) high - 'a' + 10;
      else return 0xF000;
      onebyte = onebyte << 4;

      if(low >= '0' && low <= '9') onebyte |= (uint16_t) low - '0';
      else if (low >='A' && low <= 'F') onebyte |= (uint16_t) low - 'A' + 10;
      else if (low >='a' && low <= 'f') onebyte |= (uint16_t) low - 'a' + 10;
      else return 0x0F00;

      return onebyte & 0x00FF;
    } //end x2i()
    */

    static uint16_t x2b(char *_hexbyte) {
        uint16_t onebyte = 0;
        if (_hexbyte[0] >= '0' && _hexbyte[0] <= '9')
            onebyte = (uint16_t)_hexbyte[0] - '0';
        else if (_hexbyte[0] >= 'A' && _hexbyte[0] <= 'F')
            onebyte = (uint16_t)_hexbyte[0] - 'A' + 10;
        else if (_hexbyte[0] >= 'a' && _hexbyte[0] <= 'f')
            onebyte = (uint16_t)_hexbyte[0] - 'a' + 10;
        else
            return 0xF000;
        onebyte = onebyte << 4;

        if (_hexbyte[1] >= '0' && _hexbyte[1] <= '9')
            onebyte |= (uint16_t)_hexbyte[1] - '0';
        else if (_hexbyte[1] >= 'A' && _hexbyte[1] <= 'F')
            onebyte |= (uint16_t)_hexbyte[1] - 'A' + 10;
        else if (_hexbyte[1] >= 'a' && _hexbyte[1] <= 'f')
            onebyte |= (uint16_t)_hexbyte[1] - 'a' + 10;
        else
            return 0x0F00;

        return onebyte & 0x00FF;
    }  // end x2i()

    /*********************************************************************************************************************************************************************
     *
     *
     * faster serial buffer reading function with timeout, value parsing after reading; function works for more than 64byte UART buffer, only for baudrates <= 57600baud
     * also 57600baud works with atmega 8Mhz as well
     */
    static int serBlockRead_ms(char *buf, size_t _lenMAX = MAX_SERBYTES, size_t _TIMEOUT_ms = 100) {
        volatile size_t _rlen;
        volatile size_t _len;
        volatile size_t _backupTO = Serial.getTimeout();

        Serial.setTimeout(_TIMEOUT_ms);
        // fast block reading works for max buffer size e.g. 120bytes @ 57600baud
        // if (Serial.available()) {                             //no need to wait for available
        _rlen = Serial.readBytes(buf, _lenMAX);
        //}
        buf[_rlen] = '\0';
        Serial.setTimeout(_backupTO);
        return _rlen;
    }  // end serReadBytes_ms()

    /**
     *
     *
     *
     *
     *
     * eval the serial command smac<txchannel>:<data>:rc<rxchannel>:, e.g. smac:ch03:958180....:rc40: and puts it to the packet_t structure
     * the data payload is addressed directly to the RF interface with the given channels
     *
     * !!! _serdata is changed by strtok(), it's unsafe to be used afterwards in another parsing function  !!!
     */

    // static boolean eval_uart_smac_request(char *_serdata, uint8_t _slen, packet_t *packet, uint8_t *rxch) {
    //     char *p = NULL;
    //     char *m = NULL;

    //     if (_slen < 10) {
    //         DPRINT(DBG_ERROR, F("slen low "));
    //         _DPRINT(DBG_ERROR, _slen);
    //         return false;
    //     }

    //     _slen = c_remove(' ', _serdata, _slen);
    //     c_lower(_serdata, _slen);
    //     p = strtok(_serdata, ":");
    //     m = strstr(p, "mac");
    //     if (m == NULL) return false;
    //     p = strtok(NULL, ":");
    //     m = strstr(p, "ch");
    //     if (m) {
    //         m += 2;
    //         packet->rfch = atoi(m);
    //         DPRINT(DBG_INFO, F("smac_txch "));
    //         _DPRINT(DBG_DEBUG, packet->rfch);
    //         p = strtok(NULL, ":");
    //         // next section
    //         DPRINT(DBG_INFO, F("smac_data "));
    //         _DPRINT(DBG_DEBUG, p);
    //         _DPRINT(DBG_DEBUG, F(",  len "));
    //         _DPRINT(DBG_DEBUG, strlen(p));
    //         uint8_t _i = 0;
    //         for (_i = 0; _i < strlen(p); _i += 2) {
    //             packet->data[_i / 2] = (uint8_t) x2b(&p[_i]);
    //         }  // end for()
    //         packet->plen = _i / 2;
    //         // eval rx channel input
    //         p = strtok(NULL, ":");
    //         m = strstr(p, "rc");
    //         if (m) {
    //             m += 2;
    //             *rxch = atoi(m);
    //         } else {
    //             *rxch = 0;
    //         }  // end if()
    //         DPRINT(DBG_INFO, F("smac_rxch "));
    //         _DPRINT(DBG_DEBUG, *rxch);
    //         return true;
    //     } else {
    //         DPRINT(DBG_ERROR, F("smac ERROR"));
    //         return false;
    //     }  // end if()
    // }      // end eval_uart_cmd()



    /**
     * 
     * 
     * 
     * takes numerical value of char* serdata with some checks and factor, define MIN and MAX value as response
    */
    static uint32_t uart_eval_decimal_val(const __FlashStringHelper *_info, char *_serdata, uint8_t _len, uint16_t inMIN, uint32_t inMAX, int outFACTOR) {
        volatile uint32_t _tmp32 = 0L;

        DISABLE_IRQ;
        if ((_len > 0) && (_len < 12)) {
            //_serdata[_len] = '\0';
            _tmp32 = atol(_serdata);
            if (_tmp32 < inMIN) {
                _tmp32 = inMIN;
            } else if (_tmp32 > inMAX) {
                _tmp32 = inMAX;
            }
            _tmp32 = outFACTOR * _tmp32;
        }
        RESTORE_IRQ;
        
        if (_info != NULL) {
            DPRINT(DBG_INFO, _info);
            _DPRINT(DBG_INFO, _tmp32);
        }
        
        return _tmp32;
    }  // end eval_simple_cmd()


    /**
     *
     *
     *
     * generic uart command parsing
     * return number of char* with start address of each token, usually first is the cmd, _tok** only referes to addresses of mSerBuffer,
     */
    uint8_t read_uart_cmd_param(char **_tok, const uint8_t _numTok = MAX_SER_PARAM) {
        char *_p = NULL;
        char *_m = NULL;
        volatile uint8_t _plen;
        volatile uint8_t _rlen;
        bool _res = false;

        _rlen = serBlockRead_ms(mSerBuffer);
        _rlen = c_remove(' ', mSerBuffer, _rlen);
        c_replace('\n', '\0', mSerBuffer, _rlen);
        c_replace('\r', '\0', mSerBuffer, _rlen);
        c_lower(mSerBuffer, _rlen);

        _p = strtok(mSerBuffer, ":");

        // // check cmdstr header
        // _m = strstr(_p, _matchstr);
        // if (_m == NULL) {
        //     DPRINT(DBG_ERROR, F("wrong cmd "));
        //     return 0xff;
        // }

        volatile uint8_t _i = 0;
        for (_i = 0; _i < _numTok; _i++) {
            if (_p) {
                _tok[_i] = _p;
                DPRINT(DBG_DEBUG, F("para"));
                _DPRINT(DBG_DEBUG, _i);
                _DPRINT(DBG_DEBUG, F(" "));
                _DPRINT(DBG_DEBUG, _tok[_i]);
            } else {
                break;
            }
            _p = strtok(NULL, ":");
        }  // end for()
        return _i;
    }// end read_uart_cmd_param()

    /**
     * 
     * 
     * 
     * parses the uart smac command parameter and writes into the mac-packet sending structure
     */
    boolean uart_cmd_smac_request_parsing(char **_cmd_params, uint8_t _numPara, packet_t *packet, uint8_t *rxch) {
        
        //example cmd "smac:ch03:958180....:rc40:"
        if (_numPara < 2) {
            DPRINT(DBG_ERROR, F(" numPara low"));
            _DPRINT(DBG_ERROR, _numPara);
            return false;
        }

        //parse tx channel
        char* m;
        m = strstr(_cmd_params[1], "ch");
        if(!m) {
            DPRINT(DBG_ERROR, F("no chan"));
            return false;
        }
        m += 2;
        packet->rfch = atoi(m);                                     //sending channel for request
        
        //copy the bytes-string into byte array 
        uint8_t _i = 0;
        for (_i = 0; _i < strlen(_cmd_params[2]); _i += 2) {
            packet->data[_i / 2] = (uint8_t)x2b(&_cmd_params[2][_i]);
        }  // end for()
        packet->plen = _i / 2;

        if (DBG_DEBUG <= DEBUG_LEVEL) {
            Serial.print(F("\ndata "));
            print_bytes( packet->data, packet->plen, "", true);
        }
        
        // parse rx channel input
        *rxch = 0;
        if (_numPara == 3) {
            m = strstr(_cmd_params[3], "rc");
            if(m) {
                m += 2;
                *rxch = atoi(m);
            } // end if(m)
        }  // end if()

        DPRINT(DBG_DEBUG, F("rxch ")); _DPRINT(DBG_DEBUG, *rxch);
        return true;
    }// end uart_cmd_smac_request_parser()


    /**
     *
     *
     * parsing values of adds inverter cmd, preprare to the list of registered inverters
     *
     */
    bool uart_cmd_add_inverter_parsing(char **_cmd_params, uint8_t _nump, uint8_t *_invIX_p, uint16_t *_invType_p, uint8_t *invID5) {
        // volatile uint8_t _plen;
        // volatile uint8_t _rlen;
        bool _res = false;
        char strtmp5[5] = {'0', '0', '0', '0', '\0'};

        if (_nump != 0xFF && _nump > 1) {
            DPRINT(DBG_INFO, _cmd_params[0]);  // para1, used as index
            DPRINT(DBG_INFO, _cmd_params[1]);  // para2, the full inverter id of the plate

            if (strlen(&(_cmd_params[0][0])) > 0) {
                *_invIX_p = (uint8_t)atoi(_cmd_params[0]);  //*invIX_p must point to value storage outside of function!!!!!
                //_rlen = *_invIX_p;
                DPRINT(DBG_INFO, F(" InvIx "));
                _DPRINT(DBG_INFO, *_invIX_p);
                delay(10);

                if ((*_invIX_p) < MAX_NUM_INVERTERS ) {
                    if (strlen(&_cmd_params[1][0]) == 12) {
                        memcpy(strtmp5, &_cmd_params[1][0], 4);  // copy first 4 char into strtmp5
                        *_invType_p = strtol(strtmp5, NULL, 16);
                        DPRINT(DBG_INFO, F(" InvType "));
                        if (DBG_INFO) {
                            sprintf(mStrOutBuf, "0x%04X", *_invType_p);
                            Serial.print(mStrOutBuf);
                        }
                        delay(10);

                        invID5[0] = 0x01;  // lowest byte val is used for nrf24 pipe id of 5byte addr, not visible, but needed for nrf24 addr
                        for (volatile uint8_t i = 1; i < 5; i++) {
                            memcpy(strtmp5, &(_cmd_params[1][2 * i + 2]), 2);  // copy two digits used for one byte
                            strtmp5[2] = '\0';                                 // str termination
                            invID5[i] = strtol(strtmp5, NULL, 16);             // convert 2char to one byte
                        }

                        
                        if (DBG_INFO <= DEBUG_LEVEL) {
                            DPRINT(DBG_INFO, F(" InvID5  "));
                            print_bytes(&invID5[0], 5, "", true);
                        }
                        _res = true;

                    } else {
                        DPRINT(DBG_ERROR, F(" invID len fail"));
                        delay(10);
                    }  // end if strlen(token[1])

                } else {
                    DPRINT(DBG_ERROR, F(" invIx high"));
                    delay(10);
                }  // end if(invIX)
            }
        }
        return _res;
    }  // end uart_inverter_add()

    /**
     *
     *
     */
    static bool uart_cmd_del_inverter_parsing(char **_cmd_params, uint8_t _nump, uint8_t *_invIX_p) {
        bool _res = false;

        if (_nump != 0xFF && _nump > 0) {
            DPRINT(DBG_INFO, _cmd_params[1]);  // para1, index0 is cmd itself

            if (strlen(_cmd_params[1]) > 0) {
                *_invIX_p = (uint8_t)atoi(_cmd_params[1]);  //*invIX_p must point to value storage outside of function, like mSerBuffer !!!!!
                //_rlen = *_invIX_p;
                if ((*_invIX_p) < MAX_NUM_INVERTERS) {
                    DPRINT(DBG_INFO, F(" InvIx del "));
                    _DPRINT(DBG_INFO, *_invIX_p);
                    _res = true;
                }
            }  // end if(strlen(_cmd_params[1]))
        }      // end if(_plen...)
        return _res;
    }  // end uart_inverter_del_check()

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // output functions
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     *
     *
     *
     *
     * prints the byte array either as 3 digit uint8_t or 2digit hex string
     */
    void print_bytes(uint8_t *_buf, uint8_t _blen, const char *_sep, bool _asHex) {
        volatile uint8_t _i = 0;

        if (_asHex) {
            Serial.print(F("hex "));
        }

        while (_i < _blen) {
            if (_asHex) {
                sprintf(mStrOutBuf, "%02X%s", _buf[_i], _sep);
                Serial.print(mStrOutBuf);
            } else {
                sprintf(mStrOutBuf, "%03d%s", _buf[_i], _sep);
                Serial.print(mStrOutBuf);
            }
            _i++;
        }  // end while()
    }      // end print_bytes

   private:
};
