//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

//
// 2022: mb, AHOY-src modified for AHOY-UL (Hoymiles, USB light IF) for  Arduino Nano (also works on ESP8266 without WIFI/BT)
// The HW is same as described in AHOY project.
// The Hoymiles inverter (e.g. HM800) is accessed via NRF24L01+.
// The interface is USB2serial for input and output.
// There are two modes of operation:
// - automode: one REQUEST message is polled periodically and decoded payload is given by serial-IF (@57600baud), some comfig inputs possible
// - smac-mode: -> The hoymiles specific REQUEST messages must be given as input via serial-IF (@57600baud)
//              <- The full sorted RESPONSE is given to the serial-IF with as smac-packet (to be used with python, fhem, etc.) 
// 

#include <Arduino.h>
#include <printf.h>
#include <stdint.h>
#include <stdio.h>

#include <RF24.h>
#include <RF24_config.h>
#include <SPI.h>

#include "CircularBuffer.h"
#include "config.h"
#include "dbg.h"
#include "defines.h"
#include "hmDefines.h"
#include "hmRadio.h"
#include "utils_serial.h"

typedef CircularBuffer<packet_t, PACKET_BUFFER_SIZE> BufferType;
typedef HmRadio<DEF_RF24_CE_PIN, DEF_RF24_CS_PIN, BufferType> RadioType;
// typedef Inverter<float> InverterType;
// typedef HmSystem<RadioType, BufferType, MAX_NUM_INVERTERS, InverterType> HmSystemType;

// declaration of functions
static void resetSystem(void);
static void loadDefaultConfig(config_t *_mconfig);
// utility function
static int availableMemory(void);
static void swap_bytes(uint8_t *, uint32_t);
static uint32_t swap_bytes(uint32_t);
static bool check_array(uint8_t *, uint8_t *, uint8_t);
// low level packet payload and inverter handling
static uint8_t getInvID(invPayload_t *, uint8_t, uint8_t *);
static uint8_t getNumInv(invPayload_t *, uint8_t);
static bool copyPacket2Payload(invPayload_t *, uint8_t, packet_t *, bool);
static bool savePayloadfragment(invPayload_t *, packet_t *, uint8_t, uint8_t);
static bool processPayload(invPayload_t *, config_t *, bool);
static uint8_t checkPayload(invPayload_t *);
static bool resetPayload(invPayload_t *);
// output and decoding
static bool returnMACPackets(invPayload_t *);
static uint8_t returnPayload(invPayload_t *, uint8_t *, uint8_t);
static void decodePayload(uint8_t, uint8_t *, uint8_t, uint32_t, char*, uint8_t, uint16_t);
//interrupt handler
static void handleIntr(void);

// sysConfig_t mSysConfig;
static config_t mConfig;
static BufferType packet_Buffer;
// static char mVersion[12];

static volatile uint32_t mTimestamp;
// static uint16_t mSendTicker;
// static uint8_t mSendLastIvId;

static invPayload_t mPayload[MAX_NUM_INVERTERS];
// static uint32_t mRxFailed;
// static uint32_t mRxSuccess;
// static uint32_t mFrameCnt;
// static uint8_t mLastPacketId;

// serial
static volatile uint16_t mSerialTicker;

// timer
// static uint32_t mTicker;
// static uint32_t mRxTicker;

// static HmSystemType *mSys;
static RadioType hmRadio;
// static uint8_t radio_id[5];                       //todo: use the mPayload[].id field   ,this defines the radio-id (domain) of the rf24 transmission, will be derived from inverter id
static uint64_t radio_id64 = 0ULL;

#define P(x) (__FlashStringHelper *)(x)                             // PROGMEM-Makro for variables
static const char COMPILE_DATE[] PROGMEM = {__DATE__};
static const char COMPILE_TIME[] PROGMEM = {__TIME__};
static const char NAME[] PROGMEM = {DEF_DEVICE_NAME};

#define USER_PAYLOAD_MAXLEN 128
static uint8_t user_payload[USER_PAYLOAD_MAXLEN];  // used for simple decoding and output only
static uint32_t user_pl_ts = 0;

#define MAX_STRING_LEN 51
static char strout[MAX_STRING_LEN];                             //global string buffer for sprintf, snprintf, snprintf_P outputs

///////////////////////////////////////////////////////////////////
void setup() {
    // Serial.begin(115200);
    Serial.begin(57600);
    printf_begin();
    Serial.flush();
    Serial.print(P(NAME));
    Serial.print(F("\ncompiled "));
    Serial.print(P(COMPILE_DATE));
    Serial.print(F(" "));
    Serial.print(P(COMPILE_TIME));

    mSerialTicker = 0xffff;
    resetSystem();                              // reset allocated mPayload buffer
    loadDefaultConfig(&mConfig);                // fills the mConfig parameter with values
    strout[MAX_STRING_LEN-1] = '\0';                                  //string termination
    

    // todo: loadEEconfig() from Flash, like Inverter-ID, power setting
    // mSys = new HmSystemType();
    // mSys->setup(&mConfig);

    DPRINT(DBG_INFO, F("freeRAM ")); _DPRINT(DBG_INFO,availableMemory());
    delay(2000);
    hmRadio.setup(&mConfig, &packet_Buffer);
    attachInterrupt(digitalPinToInterrupt(DEF_RF24_IRQ_PIN), handleIntr, FALLING);

    // prepare radio domain ID
    // radio_id[0] = (uint8_t) 0x01;
    // swap_bytes( &radio_id[1], (uint32_t)IV1_RADIO_ID );

    // assign inverter ID to the payload structure
    mPayload[0].invId[0] = (uint8_t)0x01;
    swap_bytes(&mPayload[0].invId[1], (uint32_t)IV1_RADIO_ID);
    mPayload[0].invType = (uint16_t)(IV1_RADIO_ID >> 32);                                                 //keep just upper 6 and 5th byte (e.g.0x1141) of interter plate id
    
    // hmRadio.dumpBuf("\nsetup InvID ", &mPayload[0].invId[0], 5, DBG_DEBUG);

    // alternativly radio-id
    //radio_id64 = (uint64_t)(swap_bytes((uint32_t)IV1_RADIO_ID)) << 8 | 0x01;

    // todo: load Inverter decoder depending on InvID

}  // end setup()

// volatile static uint32_t current_millis = 0;
static volatile uint32_t timer1_millis = 0L;  // general loop timer
static volatile uint32_t timer2_millis = 0L;  // send Request timer
static volatile uint32_t lastRx_millis = 0L;
#define ONE_SECOND      (1000L)
#define ONE_MINUTE      (60L * ONE_SECOND)
#define QUARTER_HOUR    (15L * ONE_MINUTE)
#define SEND_INTERVAL_ms        (ONE_SECOND * SEND_INTERVAL)
#define MIN_SEND_INTERVAL_ms    (ONE_SECOND * MIN_SEND_INTERVAL)
static uint8_t c_loop = 0;
static volatile int sread_len = 0;  // UART read length
// static volatile bool rxRdy = false;                         //will be set true on first receive packet during sending interval, reset to false before sending
static uint8_t inv_ix;
static bool payload_used[MAX_NUM_INVERTERS];
// static bool saveMACPacket = false;  // when true the the whole MAC packet with address is kept, remove the MAC for CRC calc
static bool automode = true;
static bool doDecode = false;
static bool showMAC = true;
static volatile uint32_t polling_inv_msec = SEND_INTERVAL_ms;
static volatile uint16_t tmp16 = 0;
static uint8_t tmp8 = 0;
static uint32_t min_SEND_SYNC_msec = MIN_SEND_INTERVAL_ms;

/////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
void loop() {
    // the inverter id/ix I shall be handled in a loop if more than one inverter is registered for periodic polling
    inv_ix = 0;

    // counting the time reference of seconds
    if (millis() - timer1_millis >= ONE_SECOND) {
        timer1_millis = millis();
        mTimestamp += 1;
        c_loop++;
        if (c_loop > 15) {
            c_loop = 0;
            // DPRINT(DBG_VERBOSE, F("loop().."));
            DPRINT(DBG_DEBUG, millis());
            _DPRINT(DBG_DEBUG, F(" loop:freeRAM "));
            _DPRINT(DBG_DEBUG, availableMemory());
        }  // end if
    }

    // query serial-IF for some control and data to be send via RF (cmd: sMAC:chXX:... see eval_uart_smac_cmd() format)
    if (Serial.available()) {
        // wait char
        inSer = Serial.read();
        delay(10);
        switch (inSer) {
            case (char)'s': {
                // sending smac commands which are send to the inverter, same format as from inverter, see details in funct. eval_uart_smac_cmd(...)
                //e.g. "smac:ch03:958180....:rc40:" -- ch03 for Tx channel, <raw data with crc8>, rc40 for rx channel 40, if rx channel is left then default tx_ix+2
                DPRINT(DBG_INFO, F("s OK"));                                                // OK needed only for my simple python at-terminal
                static packet_t rfTX_packet;
                static uint8_t rxch;
                ser_buffer[0] = '\0';
                automode = false;
                sread_len = serReadBytes_ms(ser_buffer, MAX_SERBYTES, 2000);
                if (eval_uart_smac_cmd(ser_buffer, sread_len, &rfTX_packet, &rxch)) {
                    // send on Tx channel and receive on Rx channel
                    if (rxch == 0) {
                        // if rxchannel not given, then set automatically
                        rxch = hmRadio.getRxChannel(rfTX_packet.rfch);
                    }
                    hmRadio.setRxChanIdx(hmRadio.getChanIdx(rxch));

                    // compare inv-id from packet data with all registerd inv-id of the payload_t struct array
                    inv_ix = getInvID(mPayload, MAX_NUM_INVERTERS, &rfTX_packet.data[0]);
                    if (inv_ix != 0xFF) {
                        DPRINT(DBG_DEBUG, F("match, inv_ix "));
                        _DPRINT(DBG_DEBUG, inv_ix);
                        payload_used[inv_ix] = !resetPayload(&mPayload[inv_ix]);
                        mPayload[inv_ix].isMACPacket = true;                             //MAC must be enabled to show the full MAC packet, no need for user_payload only
                        mPayload[inv_ix].receive = false;
                        hmRadio.sendPacket_raw(&mPayload[0].invId[0], &rfTX_packet, rxch);          // 2022-10-30: byte array transfer working
                        mPayload[inv_ix].requested = true;
                    } else {
                        // no matching inverter, do nothing
                        inv_ix = 0;
                    }
                }
                break;
            }  // end case s

            case (char)'c': {
                // todo: scan all channels for 1bit RSSI value and print result
                Serial.print(F("\nc OK "));
                hmRadio.scanRF();
                break;
            }
            
            case (char)'z': {
                // todo: register new Inverter ID in payload_t array via "z:<5bytes>:<crc>:", save to eeprom
                // todo: query via z1? for first inverter
                break;
            }

            case (char)'a': {
                //enable automode with REQ polling interval via a10 => 10sec, a100 => 100sec or other range 5....3600sec
                automode = true;
                sread_len = serReadBytes_ms(ser_buffer, MAX_SERBYTES, 2000);
                polling_inv_msec = eval_uart_decimal_val("auto polling msec ", ser_buffer, sread_len, 5, 3600, ONE_SECOND);
                break;
            }  // end case a

            case (char)'d': {
                // trigger decoding, enable periodic decoding via "d1" and disable via "d0"
                Serial.print(F("\nd OK "));
                // simple decoding
                decodePayload(TX_REQ_INFO + 0x80, &user_payload[0], 42, user_pl_ts, &strout[0], MAX_STRING_LEN, mPayload[inv_ix].invType);
                sread_len = serReadBytes_ms(ser_buffer, MAX_SERBYTES, 2000);
                doDecode = (bool)eval_uart_decimal_val("decoding ", ser_buffer, sread_len, 0, 255, 1);
                break;
            }  // end case d

            case (char)'m': {
                // enable/disable show MACmessages via "m0" or "d1"
                Serial.print(F("\nm OK "));
                sread_len = serReadBytes_ms(ser_buffer, MAX_SERBYTES, 2000);
                showMAC = (bool)eval_uart_decimal_val("showMAC ", ser_buffer, sread_len, 0, 255, 1);
                break;
            }  // end case m

            case (char)'i': {
                // query current radio information
                DPRINT(DBG_INFO, F("i OK"));
                hmRadio.print_radio_details();
                break;
            }

            case (char)'t': {
                // set the time sec since Jan-01 1970 (UNIX epoch time) as decimal String value e.g. "t1612345678:" for Feb-03 2021 9:47:58
                // timestamp is only used for sending packet timer, but not for the timing of Tx/Rx scheduling etc...
                sread_len = serReadBytes_ms(ser_buffer, MAX_SERBYTES, 2000);
                mTimestamp = eval_uart_decimal_val("time set ", ser_buffer, sread_len, 12 * 3600, 0xFFFFFFFF, 1);


                break;
            } // end case t
        }  // end switch-case
    }      // end if serial...

    // automode RF-Tx-trigger
    if (automode) {
        //slow down the sending if no Rx for long time
        if (millis() - lastRx_millis > QUARTER_HOUR) {
            min_SEND_SYNC_msec = QUARTER_HOUR;
        } else {
            min_SEND_SYNC_msec = MIN_SEND_INTERVAL_ms;
        }

        // normal sending request interval
        // todo: add queue of cmds or schedule simple device control request for power limit values
        if ( millis() - timer2_millis >= min_SEND_SYNC_msec && ((millis() - lastRx_millis > polling_inv_msec) || millis() < 60000) ) {
            timer2_millis = millis();
            // DISABLE_IRQ;
            // todo: handle different inverter via inv_id
            payload_used[inv_ix] = !resetPayload(&mPayload[inv_ix]);
            mPayload[inv_ix].isMACPacket = true;
            mPayload[inv_ix].receive = false;
            hmRadio.sendTimePacket(&mPayload[inv_ix].invId[0], 0x0B, mTimestamp, 0x0000);
            mPayload[inv_ix].requested = true;
            // RESTORE_IRQ;
        }
    }

    // RF-Rx-loop raw reading
    // receives rf data and writes data into circular buffer (packet_Buffer)
    hmRadio.loop();

    // eval RF-Rx raw data (one single entry per loop to keep receiving new messages from one inverter (inverter domain is set via invID)
    if (!packet_Buffer.empty()) {
        packet_t *p;
        p = packet_Buffer.getBack();
        if (hmRadio.checkPaketCrc(&p->data[0], &p->plen, p->rfch)) {
            // process buffer only on first occurrence
            DPRINT(DBG_INFO, F("RX Ch"));
            if (p->rfch < 10) _DPRINT(DBG_INFO, F("0"));
            _DPRINT(DBG_INFO, p->rfch);
            _DPRINT(DBG_INFO, F(" "));
            _DPRINT(DBG_INFO, p->plen);
            _DPRINT(DBG_INFO, F("B | "));
            hmRadio.dumpBuf(DBG_INFO, NULL, p->data, p->plen);
            // mFrameCnt++;

            if (p->plen) {
                // no need to get the inv_ix, because only desired inverter will answer, payload buffer is CRC protected
                // inv_ix = getInvID(mPayload, MAX_NUM_INVERTERS, &p->data[0]);
                if (inv_ix >= 0 && inv_ix < MAX_NUM_INVERTERS) {
                    if (copyPacket2Payload(&mPayload[inv_ix], inv_ix, p, mPayload[inv_ix].isMACPacket)) {
                        lastRx_millis = millis();
                    }
                }
            }                     // end if(plen)
        }                         // end if(checkPacketCRC)
        packet_Buffer.popBack();  // remove last entry after eval and print
        DPRINT(DBG_DEBUG, F("Records in p_B "));
        _DPRINT(DBG_DEBUG, packet_Buffer.available());
    }  // end if()

    // handle output of data if ready
    // after min 500msec and all packets shall be copied successfully to mPayload structure, run only if requested and some data received after REQ-trigger
    if ((millis() - timer2_millis >= 500) && packet_Buffer.empty() && mPayload[inv_ix].requested && mPayload[inv_ix].receive) {
        // process payload some sec after last sending
        if (false == payload_used[inv_ix]) {
            if (processPayload(&mPayload[inv_ix], &mConfig, true)) {
                // data valid and complete

                if (mPayload[inv_ix].isMACPacket && showMAC) {
                    returnMACPackets(&mPayload[inv_ix]);
                } 
                payload_used[inv_ix] = true;
                tmp8 = returnPayload(&mPayload[inv_ix], &user_payload[0], USER_PAYLOAD_MAXLEN);
                user_pl_ts = mPayload[inv_ix].ts;

                if (tmp8 == 42 && doDecode) {
                    decodePayload(mPayload[inv_ix].txId, &user_payload[0], tmp8, user_pl_ts, &strout[0], MAX_STRING_LEN, mPayload[inv_ix].invType);
                }
            }
        }
    }

}  // end loop()
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
static void resetSystem(void) {
    mTimestamp = 1665740000;  // sec since 1970 Jan-01, is Oct-14 2022 ~09:33
    // mTicker = 0;
    // mRxTicker = 0;
    // mSendLastIvId = 0;
    memset(mPayload, 0, (MAX_NUM_INVERTERS * sizeof(invPayload_t)));
    // mRxFailed = 0;
    // mRxSuccess = 0;
    // mFrameCnt = 0;
    // mLastPacketId = 0x00;
    // mSerialTicker = 0xffff;
}

//-----------------------------------------------------------------------------
static void loadDefaultConfig(config_t *_mconfig) {
    DPRINT(DBG_VERBOSE, F("loadDefaultCfg .."));
    // memset(&mSysConfig, 0, sizeof(sysConfig_t));
    memset(_mconfig, 0, sizeof(config_t));
    // snprintf(mVersion, 12, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    // snprintf(mSysConfig.deviceName, DEVNAME_LEN, "%s", DEF_DEVICE_NAME);

    // nrf24
    _mconfig->sendInterval = SEND_INTERVAL;
    _mconfig->maxRetransPerPyld = DEF_MAX_RETRANS_PER_PYLD;
    _mconfig->pinCs = DEF_RF24_CS_PIN;
    _mconfig->pinCe = DEF_RF24_CE_PIN;
    _mconfig->pinIrq = DEF_RF24_IRQ_PIN;
    _mconfig->amplifierPower = DEF_AMPLIFIERPOWER & 0x03;

    // serial
    _mconfig->serialInterval = SERIAL_INTERVAL;
    _mconfig->serialShowIv = true;
    _mconfig->serialDebug = true;
}




// free RAM check for debugging. SRAM for ATmega328p = 2048Kb.
static int availableMemory() {
    // Use 1024 with ATmega168
    volatile int size = 2048;
    byte *buf;
    while ((buf = (byte *)malloc(--size)) == NULL)
        ;
    free(buf);
    return size;
}

static void swap_bytes(uint8_t *_buf, uint32_t _v) {
    _buf[0] = ((_v >> 24) & 0xff);
    _buf[1] = ((_v >> 16) & 0xff);
    _buf[2] = ((_v >> 8) & 0xff);
    _buf[3] = ((_v)&0xff);
    return;
}

static uint32_t swap_bytes(uint32_t _v) {
    volatile uint32_t _res;
    _res = ((_v >> 24) & 0x000000ff) | ((_v >> 8) & 0x0000ff00) | ((_v << 8) & 0x00ff0000) | ((_v << 24) & 0xff000000);
    return _res;
}


//////////////////  handle payload function, maybe later as own class //////////////////
/**
 * compares the two arrays and returns true when equal
 */
static bool check_array(uint8_t *invID, uint8_t *rcvID, uint8_t _len) {
    uint8_t i;
    for (i = 0; i < _len; i++) {
        if (invID[i] != rcvID[i]) return false;
    }
    return true;
}

/**
 * gets the payload index that matches to the received ID, in case of no match 0xFF is returned
 */
static uint8_t getInvID(invPayload_t *_payload, uint8_t _pMAX, uint8_t *rcv_data) {
    uint8_t i;
    for (i = 0; i < _pMAX; i++) {
        // comparison starts at index 1 of invID, because index zero contains the pipe, only 4 bytes are compared
        if (check_array(&_payload->invId[1], &rcv_data[1], 4)) return i;
        _payload++;
    }
    return (uint8_t)0xFF;
}

static uint8_t getNumInv(invPayload_t *_payload, uint8_t _pMAX) {
    uint8_t i;
    for (i = 0; i < _pMAX; i++) {
        // check if invID is not set on first two entries, shall be sufficient for now
        if (_payload->invId[1] == 0x00 && _payload->invId[2] == 0x00) break;
        _payload++;
    }
    return i;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// low level payload packet handling
//
//

/**
 * clearing of the current inverter data structure, except the ID and typedef
 */
static bool resetPayload(invPayload_t *_payload) {
    // static uint8_t inv_ix;
    uint8_t backup_inv_id[5];
    uint16_t backup_inv_type;
    DPRINT(DBG_VERBOSE, F("resetPayload invid "));
    hmRadio.dumpBuf(DBG_VERBOSE, NULL, &_payload->invId[0], 5);
    memcpy(backup_inv_id, &_payload->invId[0], 5);
    backup_inv_type = _payload->invType;
    // nv_ix = getInvID(_payload, _pMAX, *invID);
    memset(_payload, 0, sizeof(invPayload_t));
    memcpy(&_payload->invId[0], backup_inv_id, 5);
    _payload->invType = backup_inv_type;
    return true;
}

/**
 * copies data (response of REQ_INFO message) from Rx packet buffer into payload structure per inverter
 */
static bool copyPacket2Payload(invPayload_t *_payload, uint8_t _invx, packet_t *_p, bool _keepMAC_packet) {
    // check whether inverter ID is registered and save data per registered inverter, but for TX_REQ_INFO only
    bool _res;
    uint8_t ix;
    uint8_t pid;

    _res = false;
    DPRINT(DBG_DEBUG, F("copyPacket2Payload invx "));
    _DPRINT(DBG_DEBUG, _invx);

    pid = _p->data[9];
    // check first if all RF data shall be kept, e.g. for transfer via uart-if as MAC packets
    ix = 10;  // start of real payload without address overhead
    _payload->isMACPacket = _keepMAC_packet;
    if (_keepMAC_packet) {
        ix = 0;
    }

    if ((pid & 0x7F) > MAX_PAYLOAD_ENTRIES) {
        DPRINT(DBG_ERROR, F(" high index "));
        _DPRINT(DBG_ERROR, pid & 0x7F);
        return false;
    }  // end

    // parsing of payload responses, only keeping the real payload data
    if (_p->data[0] == (TX_REQ_INFO + 0x80)) {
        // eval response from get information command 0x15
        DPRINT(DBG_DEBUG, F(" resp getinfo "));
        _DPRINT(DBG_DEBUG, F("  pid "));
        _DPRINT(DBG_DEBUG, pid & 0x7F);
        if (pid == 0x00) {
            _DPRINT(DBG_DEBUG, F(" ignore"));
        } else {
            // store regular fragments only
            _res = savePayloadfragment(_payload, _p, pid, ix);
        }  // end if-else(*pid == 0x00)
    }      // end if(TX_REQ_INFO +0x80)

    // check and save dev control
    else if (_p->data[0] == (TX_REQ_DEVCONTROL + 0x80)) {
        // response from dev control request
        DPRINT(DBG_DEBUG, F(" resp devcontrol"));

        switch (_p->data[12]) {
            case ActivePowerContr:
                _DPRINTLN(DBG_INFO, F(" ActivePowerContr"));
                break;
            default:
                _DPRINTLN(DBG_INFO, F(" default data[12] handling"));
                _res = savePayloadfragment(_payload, _p, pid, ix);
                break;
        }  // end switch-case

    } else {
        // handle all unknown Request response fragments
        _res = savePayloadfragment(_payload, _p, pid, ix);
    }  // end else-if

    return _res;
}  // end savePayload()


/**
 * saves copies one received payload-packet into inverter memory structure  
 */
static bool savePayloadfragment(invPayload_t *_payload, packet_t *_p, uint8_t _pid, uint8_t _ix) {
    volatile bool _res = false;
    if ((_pid & 0x7F) < MAX_PAYLOAD_ENTRIES) {
        _res = true;
        memcpy(&_payload->data[(_pid & 0x7F) - 1][0], &_p->data[_ix], _p->plen - _ix - 1);
        _payload->len[(_pid & 0x7F) - 1] = _p->plen - _ix - 1;
        _payload->rxChIdx = _p->rfch;
        _payload->ts = millis();
        _payload->txId = _p->data[0];
        _payload->receive = _res;                  // indicates that a packet was received at least once per Request iteration

        // handle last packet additionally
        if ((_pid & 0x80) == 0x80) {
            _DPRINT(DBG_DEBUG, F(" fragm.end"));
            if ((_pid & 0x7f) > _payload->maxPackId) {
                _payload->maxPackId = (_pid & 0x7f);  // set maxPackId > 0 as indication that last fragment was detected
                _DPRINT(DBG_DEBUG, F(" maxPID "));
                _DPRINT(DBG_DEBUG, _payload->maxPackId);
            }
        }  // end if((*pid & 0x80)...)
    } else {
        _DPRINT(DBG_ERROR, F(" pid out of range"));
    }  // end if(MAX_PAYLOAD_ENTRIES)
    return _res;
}  // end savePayload fragment


/**
 * process the received payload and handles retransmission request for one inverter
 */
static bool processPayload(invPayload_t *_payload, config_t *_mconfig, bool retransmit) {
    // uses gloabal send timer2_millis
    //  for one inverter only at a given time only, but payload structure array can hold more inverter
    DPRINT(DBG_VERBOSE, F("processPayload"));
    bool _res;
    uint8_t _reqfragment;

    _res = 0x00;
    //_payload->complete = false;

    _reqfragment = checkPayload(_payload);
    if (_reqfragment == 0xff) {
        return false;
    }

    if (_reqfragment > 0x80) {
        if (retransmit) {
            if (_payload->retransmits < _mconfig->maxRetransPerPyld) {
                _payload->retransmits++;
                DPRINT(DBG_DEBUG, F(" req fragment "));
                _DPRINTHEX(DBG_DEBUG, (uint8_t)(_reqfragment & 0x7f));
                hmRadio.sendCmdPacket(_payload->invId, TX_REQ_INFO, _reqfragment, true);
                timer2_millis = millis();  // set sending timer2
                // leaves with false to quickly receive again
                // so far the Tx and Rx frequency is handled globally, it shall be individually per inverter ID?
            }
        }
    } else {
        // all data valid
        _res = true;
        //_payload->complete = true;
    }  // end if-else()
    return _res;

}  // end processPayload()


/**
 * checks the paypload of all received packet-fragments via CRC16 and length
 * returns:
 *       - 0x00 == OK if all fragments received and CRC OK,
 *       - PID + 0x81 can be used directly to request the first next missing packet
 *       - 0xFF in case of CRC16 failure and next missing packet would be bigger than payload->maxPacketId
 */
static uint8_t checkPayload(invPayload_t *_payload) {
    // DPRINT(DBG_VERBOSE, F("checkPayload "));
    volatile uint16_t crc = 0xffff, crcRcv = 0x0000;
    uint8_t i;
    uint8_t ixpl;  // index were payload starts in each MAC packet

    if (_payload->maxPackId > MAX_PAYLOAD_ENTRIES) {
        _payload->maxPackId = MAX_PAYLOAD_ENTRIES;
    }

    ixpl = 0;
    if (_payload->isMACPacket) {
        ixpl = 10;
    }

    // try to get the next missing packet via len detection
    if (_payload->maxPackId == 0) {
        for (i = 0; i < MAX_PAYLOAD_ENTRIES; i++) {
            if (_payload->len[i] == 0) break;
        }
    } else {
        // check CRC over all entries
        for (i = 0; i < _payload->maxPackId; i++) {
            if (_payload->len[i] > 0) {
                DPRINT(DBG_VERBOSE, F(" checkPL "));
                _DPRINT(DBG_VERBOSE, _payload->len[i] - ixpl);
                _DPRINT(DBG_VERBOSE, F("B "));
                hmRadio.dumpBuf(DBG_VERBOSE, NULL, &_payload->data[i][ixpl], _payload->len[i] - ixpl);

                if (i == (_payload->maxPackId - 1)) {
                    crc = Hoymiles::crc16(&_payload->data[i][ixpl], _payload->len[i] - ixpl - 2, crc);
                    crcRcv = (_payload->data[i][_payload->len[i] - 2] << 8) | (_payload->data[i][_payload->len[i] - 1]);
                } else
                    crc = Hoymiles::crc16(&_payload->data[i][ixpl], _payload->len[i] - ixpl, crc);
            } else {
                // entry in range with len == zero
                //--> request retransmit with this index
                break;
            }
        }

        if (crc == crcRcv) {
            // success --> leave here
            DPRINT(DBG_DEBUG, F(" checkPL -> CRC16 OK"));
            return (uint8_t)0x00;


        } else {
            if ((_payload->maxPackId > 0) && (i >= _payload->maxPackId)) {
                DPRINT(DBG_ERROR, F("  crc    "));
                _DPRINTHEX(DBG_ERROR, crc);
                DPRINT(DBG_ERROR, F("  crcRcv "));
                _DPRINTHEX(DBG_ERROR, crcRcv);
                // wrong CRC16 over all packets, must actually never happen for correct packets, must be programming bug
                DPRINT(DBG_ERROR, F("  cPL wrong req. "));
                _DPRINTHEX(DBG_ERROR, (uint8_t)(i + 0x81));
                _payload->receive = false;                              //stop further eval
                return (uint8_t)0xFF;                                   
            }
        }  // end if-else
    }      // end if-else

    return (uint8_t)(i + 0x81);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// output and decoding functions
//
//
/**
 *  output of sorted packets with MAC-header, all info included 
 */
static bool returnMACPackets(invPayload_t *_payload) {
    for (uint8_t i = 0; i < (_payload->maxPackId); i++) {
        Serial.print(F("\nrMAC:ch"));
        if (_payload->rxChIdx < 10) Serial.print(F(" "));
        Serial.print(_payload->rxChIdx);
        Serial.print(F(": "));
        hmRadio.dumpBuf(NULL, &_payload->data[i][0], _payload->len[i]);
    }  // end for()
    Serial.print(F(":rt"));
    Serial.print(_payload->retransmits);
    Serial.print(F(":"));
    return true;
}

/**
 *  output of pure user payload message
 */
static uint8_t returnPayload(invPayload_t *_payload, uint8_t *_user_payload, uint8_t _ulen) {
    // iv->ts = mPayload[iv->id].ts;
    memset(_user_payload, 0, _ulen);
    static uint8_t _offs, _ixpl, _len;
    _offs = 0;
    _ixpl = 0;
    if (_payload->isMACPacket) {
        _ixpl = 10;  // index of position of payload start after pid
    }                // end if()

    for (uint8_t i = 0; i < (_payload->maxPackId); i++) {
        memcpy(&_user_payload[_offs], &_payload->data[i][_ixpl], (_payload->len[i] - _ixpl - 1));
        _offs += (_payload->len[i] - _ixpl);
    }  // end for()

    _offs -= 2;
    Serial.print(F("\nrPayload("));
    Serial.print(_offs);
    Serial.print(F("): "));
    hmRadio.dumpBuf(NULL, _user_payload, _offs);
    Serial.print(F(":"));
    return _offs;
}  // end if returnPaylout


/**
 * simple decoding of 2ch HM-inverter only
 */
static void decodePayload(uint8_t _cmd, uint8_t *_user_payload, uint8_t _ulen, uint32_t _ts, char* _strout, uint8_t _strlen, uint16_t _invtype) {
    volatile uint32_t _val = 0L;
    byteAssign_t _bp;
    volatile uint8_t _x;
    volatile uint8_t _end;
    volatile uint8_t _dot_val;
    volatile float _fval;
    
    //_str80[80] = '\0';
    snprintf_P(_strout, _strlen, PSTR("\ndata age: %d sec"), (millis() - _ts)/1000);
    Serial.print(_strout);
    snprintf_P(_strout, _strlen, PSTR("\nInvertertype %Xxxxxxxxx "), _invtype);
    Serial.print(_strout);

    if (_cmd == 0x95 and _ulen == 42) {
        //!!! simple HM600/700/800 2ch decoding for cmd=0x95 only !!!!
        for (_x = 0; _x < HM2CH_LIST_LEN; _x++) {
            // read values from given positions in payload
            _bp = hm2chAssignment[_x];
            _val = 0L;
            _end = _bp.start + _bp.num;

            snprintf_P(_strout, _strlen, PSTR("\nHM800/ch%02d/"), _bp.ch);
            Serial.print(_strout);
            //Serial.print(F("\nHM800/ch"));
            //Serial.print(_bp.ch);
            //Serial.print(F("/"));
            //strncpy_P(_strout, (PGM_P)pgm_read_word(&(PGM_fields[_bp.fieldId])), _strlen);               // read from PROGMEM array into RAM, works for A-Nano, not for esp8266
            //snprintf_P(_strout, );
            strncpy_P(_strout, &(PGM_fields[_bp.fieldId][0]), _strlen);
            Serial.print(_strout);
            Serial.print(F(": "));

            if (CMD_CALC != _bp.div && _bp.div != 0) {
                do {
                    _val <<= 8;
                    _val |= _user_payload[_bp.start];
                } while (++_bp.start != _end);
                _fval = _val / (float)_bp.div;

                if (_bp.unitId == UNIT_NONE) {
                    Serial.print(_val);
                    continue;
                }
                Serial.print(_fval,2);                                            //arduino nano does not sprintf.. float values, but print() does
                //Serial.print(units[_bp.unitId]);
                strncpy_P(_strout, &PGM_units[_bp.unitId][0], _strlen);
                Serial.print(_strout);

            } else {
                // do calculations
                Serial.print(F("not yet"));
            }
        }  // end for()
    } else {
        Serial.print(F("NO DECODER "));
        Serial.print(_cmd, HEX);
    }
}




///////////////////////////////////////////////////////////
//
// Interrupt handling
//
#if defined(ESP8266) || defined(ESP32) 
IRAM_ATTR void handleIntr(void) {
    hmRadio.handleIntr();
}
#else
static void handleIntr(void) {
    hmRadio.handleIntr();
}
#endif
///////////////////////////////////////////////////////////

