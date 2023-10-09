//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#include "CommQueue.h"
#include <Arduino.h>
#include "../utils/crc.h"

class Communication : public CommQueue<> {
    public:
        void setup(uint32_t *timestamp) {
            mTimestamp = timestamp;
        }

        void loop() {
            get([this](bool valid, const queue_s *q) {
                if(!valid)
                    return; // empty

                switch(mState) {
                    case States::IDLE:
                        setTs(mTimestamp);
                        q->iv->radio->prepareDevInformCmd(q->iv, q->cmd, q->ts, q->iv->alarmLastId, false);
                        lastMillis = millis();
                        lastFound = false;
                        mState = States::WAIT;
                        break;

                    case States::WAIT:
                        if((millis()-lastMillis) < 500)
                            return;
                        mState = States::CHECK_FRAMES;
                        break;

                    case States::CHECK_FRAMES:
                        if(!q->iv->radio->loop())
                            break; // radio buffer empty

                        while(!q->iv->radio->mBufCtrl.empty()) {
                            packet_t *p = &q->iv->radio->mBufCtrl.front();
                            q->iv->radio->mBufCtrl.pop();

                            if(!checkIvSerial(&p->packet[1], q->iv))
                                continue; // inverter ID incorrect

                            q->iv->radioStatistics.frmCnt++;

                            if (p->packet[0] == (TX_REQ_INFO + ALL_FRAMES))  // response from get information command
                                parseFrame(p);
                            else if (p->packet[0] == (TX_REQ_DEVCONTROL + ALL_FRAMES)) // response from dev control command
                                parseDevCtrl(p);

                            yield();
                        }
                        break;
                }
            });
        }

        void gotData() {
            setDone();
        }

    private:
        inline bool checkIvSerial(uint8_t buf[], Inverter<> *iv) {
            uint8_t tmp[4];
            CP_U32_BigEndian(tmp, iv->radioId.u64 >> 8);
            for(uint8_t i = 0; i < 4; i++) {
                if(tmp[i] != buf[i])
                    return false;
            }
            return true;
        }

        inline bool checkFrameCrc(uint8_t buf[], uint8_t len) {
            return (ah::crc8(buf, len - 1) == buf[len-1]);
        }

        inline void parseFrame(packet_t *p) {
            uint8_t *frameId = &p->packet[9];
            if(0x00 == *frameId)
                return; // skip current packet
            if((*frameId & 0x7f) > MAX_PAYLOAD_ENTRIES)
                return; // local storage is to small for id

            if(!checkFrameCrc(p->packet, p->len))
                return; // CRC8 is wrong, frame invalid

            if((*frameId & ALL_FRAMES) == ALL_FRAMES) {
                maxFrameId = (*frameId & 0x7f);
                if(*frameId > 0x81)
                    lastFound = true;
            }

            frame_t *f = &mLocalBuf[(*frameId & 0x7f) - 1];
            memcpy(f->buf, &p->packet[10], p->len-11);
            f->len  = p->len - 11;
            f->rssi = p->rssi;
        }

        inline void parseDevCtrl(packet_t *p) {
            //if((p->packet[12] == ActivePowerContr) && (p->packet[13] == 0x00))
        }

    private:
        enum class States : uint8_t {
            IDLE, WAIT, CHECK_FRAMES
        };

        typedef struct {
            uint8_t buf[MAX_RF_PAYLOAD_SIZE];
            uint8_t len;
            int8_t rssi;
        } frame_t;

    private:
        States mState = States::IDLE;
        uint32_t *mTimestamp;
        uint32_t lastMillis;
        std::array<frame_t, MAX_PAYLOAD_ENTRIES> mLocalBuf;
        bool lastFound;
        uint8_t maxFrameId;
};

#endif /*__COMMUNICATION_H__*/
