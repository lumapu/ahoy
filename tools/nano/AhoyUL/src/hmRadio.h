//-----------------------------------------------------------------------------
// 2022 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

// 2022 mb modified for AHOY-UL (Hoymiles Arduino Nano, USB light IF)
// - RF handling and function sendPacket_raw() without automatic channel increment
// - loop() saves new packet fragments only to save space

#ifndef __RADIO_H__
#define __RADIO_H__

#include <RF24.h>
#include <RF24_config.h>
#include <SPI.h>

#include "CircularBuffer.h"
#include "config.h"
#include "crc.h"
#include "dbg.h"

#define DEFAULT_RECV_CHANNEL 3
#define SPI_SPEED 1000000
#define DUMMY_RADIO_ID ((uint64_t)0xDEADBEEF01ULL)
#define RF_CHANNELS 5
#define RF_LOOP_CNT 300

// used in main.cpp for response catching and data requests
#define TX_REQ_INFO 0X15
#define TX_REQ_DEVCONTROL 0x51
#define ALL_FRAMES 0x80
#define SINGLE_FRAME 0x81

const char *const rf24AmpPowerNames[] = {"MIN", "LOW", "HIGH", "MAX"};

//-----------------------------------------------------------------------------
// MACROS
//-----------------------------------------------------------------------------
#define CP_U32_LittleEndian(buf, v) ({ \
    uint8_t *b = buf;                  \
    b[0] = ((v >> 24) & 0xff);         \
    b[1] = ((v >> 16) & 0xff);         \
    b[2] = ((v >> 8) & 0xff);          \
    b[3] = ((v)&0xff);                 \
})

#define CP_U32_BigEndian(buf, v) ({ \
    uint8_t *b = buf;               \
    b[3] = ((v >> 24) & 0xff);      \
    b[2] = ((v >> 16) & 0xff);      \
    b[1] = ((v >> 8) & 0xff);       \
    b[0] = ((v)&0xff);              \
})

#define BIT_CNT(x) ((x) << 3)

//-----------------------------------------------------------------------------
// HM Radio class
//-----------------------------------------------------------------------------
template <uint8_t CE_PIN, uint8_t CS_PIN, class BUFFER, uint64_t DTU_ID = DTU_RADIO_ID>
class HmRadio {
    uint8_t mTxCh;
    uint8_t mRxCh;  // only dummy use
    uint8_t mTxChIdx;
    uint8_t mRfChLst[RF_CHANNELS];
    uint8_t mRxChIdx;
    uint16_t mRxLoopCnt;

    RF24 mNrf24;
    // RF24 mNrf24(CE_PIN, CS_PIN, SPI_SPEED);

    BUFFER *mBufCtrl;
    uint8_t mTxBuf[MAX_RF_PAYLOAD_SIZE];
    DevControlCmdType DevControlCmd;
    volatile bool mIrqRcvd;

    uint8_t last_rxdata[2];

   public:
    HmRadio() : mNrf24(CE_PIN, CS_PIN, SPI_SPEED) {
        // HmRadio() : mNrf24(CE_PIN, CS_PIN) {

        // Depending on the program, the module can work on 2403, 2423, 2440, 2461 or 2475MHz.
        // Channel List      2403, 2423, 2440, 2461, 2475MHz
        mRfChLst[0] = 03;
        mRfChLst[1] = 23;
        mRfChLst[2] = 40;
        mRfChLst[3] = 61;
        mRfChLst[4] = 75;

        mTxChIdx = 2;
        mRxChIdx = 4;

        mRxLoopCnt = RF_LOOP_CNT;
        mSendCnt = 0;
        mSerialDebug = true;
        mIrqRcvd = true;
    }

    ~HmRadio() {}

    void setup(config_t *config, BUFFER *ctrl) {
        DPRINT(DBG_VERBOSE, F("hmRadio.h: setup(CE_PIN: "));
        _DPRINT(DBG_VERBOSE, config->pinCe);
        _DPRINT(DBG_VERBOSE, F(", CS_PIN: "));
        _DPRINT(DBG_VERBOSE, config->pinCs);
        _DPRINT(DBG_VERBOSE, F(", SPI_SPEED: "));
        _DPRINT(DBG_VERBOSE, SPI_SPEED);
        _DPRINTLN(DBG_VERBOSE, F(")"));
        delay(100);

        pinMode(config->pinIrq, INPUT_PULLUP);
        mBufCtrl = ctrl;

        // mNrf24.begin(config->pinCe, config->pinCs);
        mNrf24.begin();
        mNrf24.setRetries(0, 0);
        mNrf24.setChannel(DEFAULT_RECV_CHANNEL);
        mNrf24.setDataRate(RF24_250KBPS);
        mNrf24.setCRCLength(RF24_CRC_16);
        mNrf24.setAutoAck(false);
        mNrf24.setPayloadSize(MAX_RF_PAYLOAD_SIZE);
        mNrf24.setAddressWidth(5);
        mNrf24.openReadingPipe(1, DTU_RADIO_ID);
        mNrf24.enableDynamicPayloads();

        // enable only receiving interrupts
        mNrf24.maskIRQ(true, true, false);

        DPRINT(DBG_VERBOSE, F("RF24 Amp Pwr: RF24_PA_"));
        _DPRINT(DBG_VERBOSE, rf24AmpPowerNames[config->amplifierPower]);
        mNrf24.setPALevel(config->amplifierPower & 0x03);
        mNrf24.startListening();

        if (DEBUG_LEVEL >= DBG_VERBOSE) {
            DPRINTLN(DBG_VERBOSE, F("Radio Config: "));
            mNrf24.printPrettyDetails();
        }

        mTxCh = setDefaultChannels();
        mRxCh = 0;  // only dummy

        if (!mNrf24.isChipConnected()) {
            DPRINT(DBG_WARN, F("WARNING! your NRF24 module can't be reached, check the wiring"));
        }
    }

    /**
     * writes to circular packet_t buffer on Rx interrupt data
     */
    bool loop(void) {
        volatile bool _status;
        volatile uint8_t dlen;
        _status = false;
        DISABLE_IRQ;
        if (mIrqRcvd) {
            mIrqRcvd = false;
            bool tx_ok, tx_fail, rx_ready;
            mNrf24.whatHappened(tx_ok, tx_fail, rx_ready);  // resets the IRQ pin to HIGH
            RESTORE_IRQ;
            uint8_t pipe;  //, len;
            packet_t *p;
            while (mNrf24.available(&pipe)) {
                DPRINT(DBG_DEBUG, F("RxAvail "));
                _DPRINT(DBG_DEBUG, pipe);
                if (!mBufCtrl->full()) {
                    _status = true;
                    p = mBufCtrl->getFront();  // init pointer address with circular buffer space
                    p->rfch = mRxCh;
                    p->plen = mNrf24.getPayloadSize();
                    // p->plen = mNrf24.getDynamicPayloadSize();   //is not the real RF payload fragment (packet) length, calculate later
                    if (p->plen < 0) {
                        DPRINT(DBG_ERROR, F("Rx plen false"));
                        mNrf24.flush_rx();  // drop the packet and leave
                        return false;
                    } else if (p->plen > MAX_RF_PAYLOAD_SIZE) {
                        p->plen = MAX_RF_PAYLOAD_SIZE;
                    }  // end if-else
                    mNrf24.read(p->data, p->plen);

                    // only store new packets, try to get the CRC part only for matching
                    //  PACKET_BUFFER_SIZE can be reduced to max number of expected packets, e.g. 6...8 for 4 channel inverter (payload of 62 bytes)
                    dlen = p->data[0];  // get decoded data len
                    dlen = dlen >> 2;
                    if (dlen < MAX_RF_PAYLOAD_SIZE) {
                        if (p->data[dlen - 2] == last_rxdata[0] && p->data[dlen - 1] == last_rxdata[1]) {
                            _DPRINT(DBG_DEBUG, F("-> skip"));
                            continue;
                        }  // if()
                        _DPRINT(DBG_DEBUG, F("-> take"));
                        last_rxdata[0] = p->data[dlen - 2];
                        last_rxdata[1] = p->data[dlen - 1];
                    }  // end if()
                    mBufCtrl->pushFront(p);

                } else
                    break;
            }  // end while()
            DPRINT(DBG_DEBUG, F("Rx buf "));
            _DPRINT(DBG_DEBUG, mBufCtrl->available());
            mNrf24.flush_rx();  // drop the packet
        }
        RESTORE_IRQ;
        return _status;
    }  // end loop()

    void handleIntr(void) {
        DPRINT(DBG_DEBUG, F(" i"));
        mIrqRcvd = true;
    }

    uint8_t setDefaultChannels(void) {
        // DPRINTLN(DBG_VERBOSE, F("hmRadio.h:setDefaultChannels"));
        mTxChIdx = 2;  // Start TX with 40
        mRxChIdx = 4;  // Start RX with 75
        return mRfChLst[mTxChIdx];
    }

    void sendControlPacket(uint8_t *_radio_id, uint8_t cmd, uint16_t *data) {
        //DPRINTLN(DBG_INFO, F("hmRadio:sendControlPacket"));
        sendCmdPacket(_radio_id, TX_REQ_DEVCONTROL, ALL_FRAMES, false);  // 0x80 implementation as original DTU code
        int cnt = 0;
        mTxBuf[10] = cmd;  // cmd --> 0x0b => Type_ActivePowerContr, 0 on, 1 off, 2 restart, 12 reactive power, 13 power factor
        mTxBuf[10 + (++cnt)] = 0x00;
        if (cmd >= ActivePowerContr && cmd <= PFSet) {
            mTxBuf[10 + (++cnt)] = ((data[0] * 10) >> 8) & 0xff;  // power limit
            mTxBuf[10 + (++cnt)] = ((data[0] * 10)) & 0xff;       // power limit
            mTxBuf[10 + (++cnt)] = ((data[1]) >> 8) & 0xff;       // setting for persistens handlings
            mTxBuf[10 + (++cnt)] = ((data[1])) & 0xff;            // setting for persistens handling
        }
        // crc control data
        uint16_t crc = Hoymiles::crc16(&mTxBuf[10], cnt + 1);
        mTxBuf[10 + (++cnt)] = (crc >> 8) & 0xff;
        mTxBuf[10 + (++cnt)] = (crc)&0xff;
        // crc over all
        cnt += 1;
        mTxBuf[10 + cnt] = Hoymiles::crc8(mTxBuf, 10 + cnt);

        sendPacket(_radio_id, mTxBuf, 10 + (++cnt), true);
    }

    void sendTimePacket(uint8_t *_radio_id, uint8_t cmd, uint32_t ts, uint16_t alarmMesId) {
        // DPRINTLN(DBG_VERBOSE, F("hmRadio.h:sendTimePacket"));
        sendCmdPacket(_radio_id, TX_REQ_INFO, ALL_FRAMES, false);
        mTxBuf[10] = cmd;  // cid
        mTxBuf[11] = 0x00;
        CP_U32_LittleEndian(&mTxBuf[12], ts);  //?? adapt for atmega/nano
        // dumpBuf("  ", &mTxBuf[12], 4);

        if (cmd == RealTimeRunData_Debug || cmd == AlarmData) {
            mTxBuf[18] = (alarmMesId >> 8) & 0xff;
            mTxBuf[19] = (alarmMesId)&0xff;
        } else {
            mTxBuf[18] = 0x00;
            mTxBuf[19] = 0x00;
        }
        uint16_t crc = Hoymiles::crc16(&mTxBuf[10], 14);
        mTxBuf[24] = (crc >> 8) & 0xff;
        mTxBuf[25] = (crc)&0xff;
        mTxBuf[26] = Hoymiles::crc8(mTxBuf, 26);

        sendPacket(_radio_id, mTxBuf, 27, true);
    }

    void sendCmdPacket(uint8_t *_radio_id, uint8_t mid, uint8_t pid, bool calcCrc = true) {
        // DPRINTLN(DBG_VERBOSE, F("hmRadio.h:sendCmdPacket"));
        memset(mTxBuf, 0, MAX_RF_PAYLOAD_SIZE);
        mTxBuf[0] = mid;  // message id
        // CP_U32_BigEndian(&mTxBuf[1], (invId  >> 8));           // this must match with the given invID format, big endian is needed because the id from the plate is entered big endian
        memcpy(&mTxBuf[1], &_radio_id[1], 4);         // copy 4bytes of radio_id to right possition at txbuffer
        CP_U32_BigEndian(&mTxBuf[5], (DTU_ID >> 8));  // needed on Arduino nano
        mTxBuf[9] = pid;
        if (calcCrc) {
            mTxBuf[10] = Hoymiles::crc8(mTxBuf, 10);
            sendPacket(_radio_id, mTxBuf, 11, false);
        }
    }

    bool checkPaketCrc(uint8_t buf[], uint8_t *len, uint8_t rxCh) {
        // DPRINTLN(DBG_VERBOSE, F("hmRadio.h:checkPaketCrc"));

        // will be returned by pointer value
        *len = (buf[0] >> 2);

        // DPRINT(DBG_INFO, String(F("RX crc_in  ")) + String(*len) + String(F("B Ch")) + String(rxCh) + String(F(" | ")));
        // dumpBuf(NULL, buf, *len, DBG_DEBUG);

        if (*len > (MAX_RF_PAYLOAD_SIZE - 2))
            *len = MAX_RF_PAYLOAD_SIZE - 2;

        for (uint8_t i = 1; i < (*len + 1); i++) {
            buf[i - 1] = (buf[i] << 1) | (buf[i + 1] >> 7);
        }

        // DPRINT(DBG_INFO, String(F("RX crc_out ")) + String(*len) + String(F("B Ch")) + String(rxCh) + String(F(" | ")));
        // dumpBuf(NULL, buf, *len, DBG_DEBUG);

        volatile uint8_t crc = Hoymiles::crc8(buf, *len - 1);
        volatile bool valid = (crc == buf[*len - 1]);

        DPRINT(DBG_DEBUG, F("RX crc-valid "));
        _DPRINT(DBG_DEBUG, valid);
        return valid;
    }

    bool switchRxCh(uint16_t addLoop = 0) {
        DPRINT(DBG_DEBUG, F("hmRadio.h:switchRxCh: try"));
        // DPRINTLN(DBG_VERBOSE, F("R"));

        mRxLoopCnt += addLoop;
        if (mRxLoopCnt != 0) {
            mRxLoopCnt--;
            DISABLE_IRQ;
            mNrf24.stopListening();
            mNrf24.setChannel(getRxNxtChannel());
            mNrf24.startListening();
            RESTORE_IRQ;
        }
        return (0 == mRxLoopCnt);  // receive finished
    }

    /**
     * Hex string output of uint8_t array
     */
    void dumpBuf(const char *info, uint8_t buf[], uint8_t len) {
        if (NULL != info) DBGPRINT(info);
        for (uint8_t i = 0; i < len; i++) {
            DHEX(buf[i]);
            if (i % 10 == 0) DBGPRINT(" ");
        }  // end for()
    }

    /**
     * Hex output with debug-flag dependancy
     */
    void dumpBuf(int _thisdebug, const char *info, uint8_t buf[], uint8_t len) {
        // DPRINTLN(DBG_VERBOSE, F("hmRadio.h:dumpBuf"));
        if (_thisdebug <= DEBUG_LEVEL) {
            dumpBuf(info, buf, len);
        }  // end if(debug)
        // DBGPRINTLN("");
    }

    bool isChipConnected(void) {
        // DPRINTLN(DBG_VERBOSE, F("hmRadio.h:isChipConnected"));
        return mNrf24.isChipConnected();
    }

    uint32_t mSendCnt;
    bool mSerialDebug;

    /**
     * send the buf content to the tx-channel, the rx-channel is switched to two higher index
     */
    uint8_t sendPacket_raw(uint8_t *_radio_id, packet_t *_p, uint8_t _rxch) {
        // void sendPacket_raw(uint64_t _radio_id64, packet_t *_p, uint8_t _rxch) {
        // DPRINT(DBG_DEBUG, F("sendPacket_raw"));
        uint8_t _arc = 0;

        DPRINT(DBG_INFO, F("TXraw Ch"));
        if (_p->rfch) _DPRINT(DBG_INFO, F("0"));
        _DPRINT(DBG_INFO, _p->rfch);
        _DPRINT(DBG_INFO, F(" "));
        _DPRINT(DBG_INFO, _p->plen);
        _DPRINT(DBG_INFO, F("B | "));
        dumpBuf(DBG_INFO, NULL, _p->data, _p->plen);

        DISABLE_IRQ;
        mNrf24.stopListening();
        mNrf24.setChannel(_p->rfch);
        // mTxCh = getTxNxtChannel();                           // prepare Tx channel for next packet
        DPRINT(DBG_DEBUG, F("RF24 addr "));
        dumpBuf(DBG_DEBUG, NULL, _radio_id, 5);
        delay(25);  // wait serial data sending before RF transmission, otherwise missing first RF packet
        /*
        if(mSerialDebug) {
            DPRINT(DBG_INFO, "TX   iv-ID 0x");
            DHEX((uint32_t)(_radio_id64>>32)); Serial.print(F(" ")); DHEX((uint32_t)(_radio_id64));
        }
        */
        // mNrf24.openWritingPipe(_radio_id64);                                               // 2022-10-17: mb, addr taken from buf (must be transformed to big endian)
        mNrf24.openWritingPipe(_radio_id);  // 2022-10-17: mb, addr taken from _p->data must be transformed to big endian,
        mNrf24.setCRCLength(RF24_CRC_16);
        mNrf24.enableDynamicPayloads();
        mNrf24.setAutoAck(true);
        mNrf24.setRetries(3, 15);  // 3*250us and 15 loops -> 11.25ms
        mNrf24.write(_p->data, _p->plen);
        //_arc = mNrf24.getARC();

        // Try to avoid zero payload acks (has no effect)
        mNrf24.openWritingPipe(DUMMY_RADIO_ID);  // TODO: why dummy radio id?, deprecated
        // mNrf24.setChannel(getRxChannel(_p->rfch));                                      // switch to Rx channel that matches to Tx channel
        setRxChanIdx(getChanIdx(_rxch));
        mNrf24.setChannel(_rxch);  // switch to Rx channel
        mNrf24.setAutoAck(false);
        mNrf24.setRetries(0, 0);
        mNrf24.disableDynamicPayloads();
        mNrf24.setCRCLength(RF24_CRC_DISABLED);
        mNrf24.startListening();
        RESTORE_IRQ;

        _DPRINT(DBG_INFO, F(" ->ARC ")); _DPRINT(DBG_INFO, _arc);
        DPRINT(DBG_INFO, "RX Ch");
        if (_rxch < 10) _DPRINT(DBG_INFO, F("0"));
        _DPRINT(DBG_INFO, _rxch);
        _DPRINT(DBG_INFO, " wait");
        return _arc;
    }

    uint8_t getRxChannel(uint8_t _txch) {
        uint8_t ix;
        ix = getChanIdx(_txch);
        ix = (ix + 2) % RF_CHANNELS;
        return mRfChLst[mRxChIdx];
    }

    uint8_t getRxChan() {
        return mRfChLst[mRxChIdx];
    }

    uint8_t getChanIdx(uint8_t _channel) {
        // uses global channel list
        static uint8_t ix;
        for (ix = 0; ix < RF_CHANNELS; ix++) {
            if (_channel == mRfChLst[ix]) break;
        }
        return ix;
    }

    void setRxChanIdx(uint8_t ix) {
        mRxChIdx = ix;
    }

    void print_radio_details() {
        mNrf24.printPrettyDetails();
    }

    /**
     * scans all channel for 1bit rdp value (1 == >=-64dBm, 0 == <-64dBm)
     */
    void scanRF(void) {
        bool _rdp = 0;
        DISABLE_IRQ;
        DPRINT(DBG_INFO, F("RDP[0...124] "));
        for (uint8_t _ch = 0; _ch < 125; _ch++) {
            if (_ch % 10 == 0) _DPRINT(DBG_INFO, " ");
            mNrf24.setChannel(_ch);
            mNrf24.startListening();
            _DPRINT(DBG_INFO, mNrf24.testRPD());
        }
        RESTORE_IRQ;
    }

   private:
    /**
     * That is the actual send function
     */
    bool sendPacket(uint8_t *_radio_id, uint8_t buf[], uint8_t len, bool clear = false, bool doSend = true) {
        // DPRINT(DBG_DEBUG, F("hmRadio.h:sendPacket"));
        //bool _tx_ok = 0;  // could also query the num of tx retries as uplink quality indication, no lib interface seen yet
        uint8_t _arc = 0;
            

        // DPRINTLN(DBG_VERBOSE, "sent packet: #" + String(mSendCnt));
        // dumpBuf("SEN ", buf, len);
        if (mSerialDebug) {
            DPRINT(DBG_INFO, F("TX Ch"));
            if (mRfChLst[mTxChIdx] < 10) _DPRINT(DBG_INFO, F("0"));
            _DPRINT(DBG_INFO, mRfChLst[mTxChIdx]);
            _DPRINT(DBG_INFO, F(" "));
            _DPRINT(DBG_INFO, len);
            _DPRINT(DBG_INFO, F("B | "));
            dumpBuf(DBG_INFO, NULL, buf, len);

            DPRINT(DBG_VERBOSE, F("TX iv-ID  0x"));
            dumpBuf(DBG_VERBOSE, NULL, _radio_id, 5);
            // DPRINT(DBG_INFO, "TX iv-ID  0x");
            // DHEX((uint32_t)(invId>>32)); Serial.print(F(" ")); DHEX((uint32_t)(invId));
            delay(20);  // wait serial data sending before RF transmission, otherwise missing first RF packet
        }

        DISABLE_IRQ;
        mNrf24.stopListening();
        if (clear)
            mRxLoopCnt = RF_LOOP_CNT;
        mNrf24.setChannel(mRfChLst[mTxChIdx]);
        mTxCh = getTxNxtChannel();  // prepare Tx channel for next packet

        // mNrf24.openWritingPipe(invId);                                                            // TODO: deprecated, !!!! invID must be given in big endian uint8_t*
        mNrf24.openWritingPipe(_radio_id);
        mNrf24.setCRCLength(RF24_CRC_16);
        mNrf24.enableDynamicPayloads();
        mNrf24.setAutoAck(true);
        mNrf24.setRetries(3, 15);                                                            // 3*250us and 15 loops -> 11.25ms, I guess Hoymiles has disabled autoack, thus always max loops send
        if (doSend) mNrf24.write(buf, len);                                                  // only send in case of send-flag true, _tx_ok seems to be always false
        //_arc = mNrf24.getARC();                                                              // is always 15, hoymiles receiver might have autoack=false

        // Try to avoid zero payload acks (has no effect)
        mNrf24.openWritingPipe(DUMMY_RADIO_ID);                                                         // TODO: why dummy radio id?, deprecated
        // mRxChIdx = 0;
        mNrf24.setChannel(mRfChLst[mRxChIdx]);  // switch to Rx channel that matches to Tx channel
        mRxCh = mRfChLst[mRxChIdx];
        mNrf24.setAutoAck(false);
        mNrf24.setRetries(0, 0);
        mNrf24.disableDynamicPayloads();
        mNrf24.setCRCLength(RF24_CRC_DISABLED);
        mNrf24.startListening();
        RESTORE_IRQ;

        if (mSerialDebug) {
            //_DPRINT(DBG_INFO, F(" ->ARC ")); _DPRINT(DBG_INFO, _arc);

            DPRINT(DBG_VERBOSE, "RX Ch");
            if (mRfChLst[mRxChIdx] < 10) _DPRINT(DBG_VERBOSE, F("0"));
            _DPRINT(DBG_VERBOSE, mRfChLst[mRxChIdx]);
            _DPRINT(DBG_VERBOSE, F(" wait"));
            getRxNxtChannel();  // prepare Rx channel for next packet
        }

        mSendCnt++;
        return true;
    }

    uint8_t getTxNxtChannel(void) {
        if (++mTxChIdx >= RF_CHANNELS)
            mTxChIdx = 0;
        // DPRINT(DBG_DEBUG, F("next TX Ch"));
        //_DPRINT(DBG_DEBUG, mRfChLst[mTxChIdx]);
        return mRfChLst[mTxChIdx];
    }

    uint8_t getRxNxtChannel(void) {
        if (++mRxChIdx >= RF_CHANNELS)
            mRxChIdx = 0;
        // PRINT(DBG_DEBUG, F("next RX Ch"));
        //_DPRINT(DBG_DEBUG, mRfChLst[mRxChIdx]);
        return mRfChLst[mRxChIdx];
    }

    void setChanIdx(uint8_t _txch) {
        mTxChIdx = getChanIdx(_txch);
        mRxChIdx = (mTxChIdx + 2) % RF_CHANNELS;
    }

    uint8_t getRxChannel(void) {
        return mRxCh;
    }

};  // end class

#endif /*__RADIO_H__*/
