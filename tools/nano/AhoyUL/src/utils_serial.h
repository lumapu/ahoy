
//2022 mb for AHOY-UL (Arduino Nano, USB light IF)
//todo: make class

#include <stdint.h>
#include <stdio.h>
#include <Arduino.h>
#include "dbg.h"


//declaration of functions
static int serReadUntil(char, char*, int, uint16_t);
static int c_remove(char, char*, int);
static void c_replace(char, char, char*, int);
//static uint8_t* serGetHex(char*, int8_t*, uint16_t);
//static uint16_t x2b(char, char);
static uint16_t x2b(char *);
static int serReadTo_ms(char, char*, byte, uint16_t);
static int serReadBytes_ms(char *, byte, uint16_t );
static boolean eval_uart_smac_cmd(char*, uint8_t, packet_t*, uint8_t*);
static uint32_t eval_uart_single_val(char*, char*, uint8_t, uint8_t, uint8_t, int);


#define MAX_SERBYTES 150                                 //max buffer length serial
static char ser_buffer[MAX_SERBYTES+1];                  //one extra byte for \0 termination
static uint8_t byte_buffer[MAX_SERBYTES/2];              
static char inSer;
static int ser_len;



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
    thislen = Serial.readBytesUntil(_ch, _str, _len);                                                       //can only be used unto max serial input buffer of 64bytes
    _str[thislen] = '\0';                                                                                   //terminate char* str with '\0', data are available external now
    if( _len > 0 ) {
        DPRINT( DBG_DEBUG, F("ser Rx ")); _DPRINT( DBG_DEBUG, _str); _DPRINT( DBG_DEBUG, F("  len ")); _DPRINT( DBG_DEBUG, thislen);
    } else {
        DPRINT( DBG_DEBUG, F("ser TIMEOUT"));
    }
    Serial.setTimeout(_backupTO);
    return thislen;
}//end serReadUntil()



static int c_remove(char _chrm, char* _str, int _len) {
    static int _ir = 0;
    static int _iw = 0;
    _ir = 0;             //! must be initialized here explicitly with zero, instanciation above is only done once ! 
    _iw = 0;
    while (_str[_ir] && _ir < _len) {
        if (_str[_ir]!=_chrm) {   
            _str[_iw++] = _str[_ir];
        }
        _ir++;       
    }
    _str[_iw]='\0';
    return _iw;
}//end c_remove()

static void c_replace(char _crpl, char _cnew, char* _str, int _len) {
    int _ir = 0;
    _ir = 0;
    while (_str[_ir] && _ir < _len) {
        if (_str[_ir] ==_crpl) {   
            _str[_ir] = _cnew;
        }
        _ir++;       
    }
    return;
}//end c_replace

static void c_lower(char* _str, int _len) {
    int _ir = 0;
    _ir = 0;
    while (_str[_ir] && _ir < _len) {
        if (_str[_ir] >= 'A' && _str[_ir] < 'Z') {   
            _str[_ir] = (char) (_str[_ir] + 0x20);
        }
        _ir++;       
    }
    return;
}//end c_lower



//todo: use strtol(const char *str, char **endptr, int base) instead
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
  uint16_t onebyte=0;
  if(_hexbyte[0] >= '0' && _hexbyte[0] <= '9') onebyte = (uint16_t)_hexbyte[0] - '0';
  else if (_hexbyte[0] >='A' && _hexbyte[0] <= 'F') onebyte = (uint16_t) _hexbyte[0] - 'A' + 10;
  else if (_hexbyte[0] >='a' && _hexbyte[0] <= 'f') onebyte = (uint16_t) _hexbyte[0] - 'a' + 10;
  else return 0xF000;
  onebyte = onebyte << 4;
  
  if(_hexbyte[1] >= '0' && _hexbyte[1] <= '9') onebyte |= (uint16_t)_hexbyte[1] - '0';
  else if (_hexbyte[1] >='A' && _hexbyte[1] <= 'F') onebyte |= (uint16_t)_hexbyte[1] - 'A' + 10;
  else if (_hexbyte[1] >='a' && _hexbyte[1] <= 'f') onebyte |= (uint16_t)_hexbyte[1] - 'a' + 10;
  else return 0x0F00;

  return onebyte & 0x00FF;
} //end x2i()


/*********************************************************************************************************************************************************************
 * faster serial buffer reading function with timeout, value parsing after reading; function works for more than 64byte UART buffer, only for baudrates <= 57600baud
 */
static int serReadBytes_ms(char *buf, byte _lenMAX, uint16_t TIMEOUT_ms) {
    volatile uint32_t _startTime = 0L;
    volatile uint8_t _thislen;
    volatile int _len;
    volatile uint16_t _backupTO;

    _thislen = 0;
    _len = 0;
    _backupTO = Serial.getTimeout();
    Serial.setTimeout(100);
    _startTime = millis();
    
    //fast reading loop of full buffer,     
    while(Serial.available()) {
        _thislen = Serial.readBytes(&buf[_len], _lenMAX - _len);
        _len += _thislen;
    } //end while()
    buf[_len] = '\0';
    Serial.setTimeout(_backupTO);

    return _len;
}//end serReadBytes_ms()


/**
 * eval the serial command smac<txchannel>:<data>:rc<rxchannel>:, e.g. s:ch03:958180....:rc40: and puts it to the packet_t structure
 * the data payload is addressed directly to the RF interface with the given channels
*/
static boolean eval_uart_smac_cmd(char *_serdata, uint8_t _slen, packet_t *packet, uint8_t *rxch) {
    char *p = NULL;
    char *m = NULL;

    if(_slen<10) {
        DPRINT(DBG_ERROR, F("slen low")); _DPRINT(DBG_ERROR, _slen); _DPRINT(DBG_ERROR, F(" ERROR"));
        return false;
    }

    _slen = c_remove(' ', _serdata, _slen);
    c_lower(_serdata, _slen);
    p = strtok(_serdata, ":");
    m = strstr(p, "mac");
    if(m==NULL) return false;
    p = strtok(NULL, ":");
    m = strstr(p, "ch");
    if (m) {
        m += 2;
        packet->rfch = atoi(m);
        DPRINT(DBG_DEBUG, F("smac_txch ")); _DPRINT(DBG_DEBUG, packet->rfch);
        p = strtok(NULL, ":");
        // next section
        DPRINT(DBG_DEBUG, F("smac_data ")); _DPRINT(DBG_DEBUG, p); _DPRINT(DBG_DEBUG, F(",  len ")); _DPRINT(DBG_DEBUG, strlen(p));
        uint8_t _i = 0;
        for (_i = 0; _i < strlen(p); _i += 2) {
            packet->data[_i / 2] = (uint8_t)x2b(&p[_i]);
        }  // end for()
        packet->plen = _i / 2;
        //eval rx channel input
        p = strtok(NULL, ":");
        m = strstr(p, "rc");
        if (m) {
            m += 2;
            *rxch = atoi(m);
        } else {
            *rxch = 0;
        }//end if()
        DPRINT(DBG_DEBUG, F("smac_rxch ")); _DPRINT(DBG_DEBUG, *rxch);
        return true;
    } else {
        DPRINT(DBG_ERROR, F("smac ERROR"));
        return false;
    }//end if()
}//end eval_uart_cmd()


static uint32_t eval_uart_decimal_val(const char* _info, char* _serdata, uint8_t _len, uint32_t inMIN, uint32_t inMAX, int outFACTOR) {
    volatile uint32_t _tmp32;
    
    DISABLE_IRQ;
    _tmp32 = inMIN * outFACTOR;
    if ((_len > 1) && (_len < 12)) {
        _serdata[_len] = '\0';
        _tmp32 = atol(_serdata);
        if ((_tmp32 <= inMIN) && (_tmp32 >= inMAX)) {
            _tmp32 = inMIN;
        }
        _tmp32 = outFACTOR * _tmp32;
    }
    RESTORE_IRQ;
    DPRINT(DBG_INFO, _info); 
    _DPRINT(DBG_INFO,_tmp32);
    DPRINT(DBG_INFO, F("OK"));
    return _tmp32;
}   //end eval_simple_cmd()
