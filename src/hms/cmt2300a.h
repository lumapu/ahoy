//-----------------------------------------------------------------------------
// 2023 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __CMT2300A_H__
#define __CMT2300A_H__

#include "esp32_3wSpi.h"

#define WORK_FREQ_KHZ       865000 // desired work frequency between DTU and
                                   // inverter in kHz
#define HOY_BASE_FREQ_KHZ   860000 // in kHz
#define HOY_MAX_FREQ_KHZ    923500 // 0xFE * 250kHz + Base_freq
#define HOY_BOOT_FREQ_KHZ   868000 // Hoymiles boot/init frequency after power up inverter
#define FREQ_STEP_KHZ       250    // channel step size in kHz
#define FREQ_WARN_MIN_KHZ   863000 // for EU 863 - 870 MHz is allowed
#define FREQ_WARN_MAX_KHZ   870000 // for EU 863 - 870 MHz is allowed

// detailed register infos from AN142_CMT2300AW_Quick_Start_Guide-Rev0.8.pdf

#define CMT2300A_MASK_CFG_RETAIN        0x10
#define CMT2300A_MASK_RSTN_IN_EN        0x20
#define CMT2300A_MASK_LOCKING_EN        0x20
#define CMT2300A_MASK_CHIP_MODE_STA     0x0F

#define CMT2300A_CUS_CMT10              0x09

#define CMT2300A_CUS_MODE_CTL           0x60    // [7] go_switch
                                                // [6] go_tx
                                                // [5] go_tfs
                                                // [4] go_sleep
                                                // [3] go_rx
                                                // [2] go_rfs
                                                // [1] go_stby
                                                // [0] n/a

#define CMT2300A_CUS_MODE_STA           0x61    // [3:0] 0x00 IDLE
                                                //       0x01 SLEEP
                                                //       0x02 STBY
                                                //       0x03 RFS
                                                //       0x04 TFS
                                                //       0x05 RX
                                                //       0x06 TX
                                                //       0x08 UNLOCKED/LOW_VDD
                                                //       0x09 CAL
#define CMT2300A_CUS_EN_CTL             0x62
#define CMT2300A_CUS_FREQ_CHNL          0x63

#define CMT2300A_CUS_IO_SEL             0x65    // [5:4] GPIO3
                                                //       0x00 CLKO
                                                //       0x01 DOUT / DIN
                                                //       0x02 INT2
                                                //       0x03 DCLK
                                                // [3:2]  GPIO2
                                                //       0x00 INT1
                                                //       0x01 INT2
                                                //       0x02 DOUT / DIN
                                                //       0x03 DCLK
                                                // [1:0]  GPIO1
                                                //       0x00 DOUT / DIN
                                                //       0x01 INT1
                                                //       0x02 INT2
                                                //       0x03 DCLK

#define CMT2300A_CUS_INT1_CTL           0x66    // [4:0] INT1_SEL
                                                //       0x00 RX active
                                                //       0x01 TX active
                                                //       0x02 RSSI VLD
                                                //       0x03 Pream OK
                                                //       0x04 SYNC OK
                                                //       0x05 NODE OK
                                                //       0x06 CRC OK
                                                //       0x07 PKT OK
                                                //       0x08 SL TMO
                                                //       0x09 RX TMO
                                                //       0x0A TX DONE
                                                //       0x0B RX FIFO NMTY
                                                //       0x0C RX FIFO TH
                                                //       0x0D RX FIFO FULL
                                                //       0x0E RX FIFO WBYTE
                                                //       0x0F RX FIFO OVF
                                                //       0x10 TX FIFO NMTY
                                                //       0x11 TX FIFO TH
                                                //       0x12 TX FIFO FULL
                                                //       0x13 STATE IS STBY
                                                //       0x14 STATE IS FS
                                                //       0x15 STATE IS RX
                                                //       0x16 STATE IS TX
                                                //       0x17 LED
                                                //       0x18 TRX ACTIVE
                                                //       0x19 PKT DONE

#define CMT2300A_CUS_INT2_CTL           0x67    // [4:0] INT2_SEL

#define CMT2300A_CUS_INT_EN             0x68    // [7] SL TMO EN
                                                // [6] RX TMO EN
                                                // [5] TX DONE EN
                                                // [4] PREAM OK EN
                                                // [3] SYNC_OK EN
                                                // [2] NODE OK EN
                                                // [1] CRC OK EN
                                                // [0] PKT DONE EN

#define CMT2300A_CUS_FIFO_CTL           0x69    // [7] TX DIN EN
                                                // [6:5] TX DIN SEL
                                                //     0x00 SEL GPIO1
                                                //     0x01 SEL GPIO2
                                                //     0x02 SEL GPIO3
                                                // [4] FIFO AUTO CLR DIS
                                                // [3] FIFO TX RD EN
                                                // [2] FIFO RX TX SEL
                                                // [1] FIFO MERGE EN
                                                // [0] SPI FIFO RD WR SEL

#define CMT2300A_CUS_INT_CLR1           0x6A // clear interrupts Bank1
#define CMT2300A_CUS_INT_CLR2           0x6B // clear interrupts Bank2
#define CMT2300A_CUS_FIFO_CLR           0x6C

#define CMT2300A_CUS_INT_FLAG           0x6D    // [7] LBD FLG
                                                // [6] COL ERR FLG
                                                // [5] PKT ERR FLG
                                                // [4] PREAM OK FLG
                                                // [3] SYNC OK FLG
                                                // [2] NODE OK FLG
                                                // [1] CRC OK FLG
                                                // [0] PKT OK FLG

#define CMT2300A_CUS_RSSI_DBM           0x70

#define CMT2300A_GO_SWITCH              0x80
#define CMT2300A_GO_TX                  0x40
#define CMT2300A_GO_TFS                 0x20
#define CMT2300A_GO_SLEEP               0x10
#define CMT2300A_GO_RX                  0x08
#define CMT2300A_GO_RFS                 0x04
#define CMT2300A_GO_STBY                0x02
#define CMT2300A_GO_EEPROM              0x01

#define CMT2300A_STA_IDLE               0x00
#define CMT2300A_STA_SLEEP              0x01
#define CMT2300A_STA_STBY               0x02
#define CMT2300A_STA_RFS                0x03
#define CMT2300A_STA_TFS                0x04
#define CMT2300A_STA_RX                 0x05
#define CMT2300A_STA_TX                 0x06
#define CMT2300A_STA_EEPROM             0x07
#define CMT2300A_STA_ERROR              0x08
#define CMT2300A_STA_CAL                0x09

#define CMT2300A_INT_SEL_TX_DONE        0x0A

#define CMT2300A_MASK_TX_DONE_FLG       0x08
#define CMT2300A_MASK_PKT_OK_FLG        0x01

// default CMT parameters
static uint8_t cmtConfig[0x60] PROGMEM {
    // 0x00 - 0x0f -- RSSI offset +- 0 and 13dBm
    0x00, 0x66, 0xEC, 0x1C, 0x70, 0x80, 0x14, 0x08,
    0x11, 0x02, 0x02, 0x00, 0xAE, 0xE0, 0x35, 0x00,
    // 0x10 - 0x1f
    0x00, 0xF4, 0x10, 0xE2, 0x42, 0x20, 0x0C, 0x81,
    0x42, 0x32, 0xCF, 0x82, 0x42, 0x27, 0x76, 0x12, // 860MHz as default
    // 0x20 - 0x2f
    0xA6, 0xC9, 0x20, 0x20, 0xD2, 0x35, 0x0C, 0x0A,
    0x9F, 0x4B, 0x29, 0x29, 0xC0, 0x14, 0x05, 0x53,
    // 0x30 - 0x3f
    0x10, 0x00, 0xB4, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x12, 0x1E, 0x00, 0xAA, 0x06, 0x00, 0x00, 0x00,
    // 0x40 - 0x4f
    0x00, 0x48, 0x5A, 0x48, 0x4D, 0x01, 0x1D, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xC3, 0x00, 0x00, 0x60,
    // 0x50 - 0x5f
    0xFF, 0x00, 0x00, 0x1F, 0x10, 0x70, 0x4D, 0x06,
    0x00, 0x07, 0x50, 0x00, 0x42, 0x0C, 0x3F, 0x7F //  - TX 13dBm
};


enum {CMT_SUCCESS = 0, CMT_ERR_SWITCH_STATE, CMT_ERR_TX_PENDING, CMT_FIFO_EMPTY, CMT_ERR_RX_IN_FIFO};

template<class SPI>
class Cmt2300a {
    typedef SPI SpiType;
    public:
        Cmt2300a() {}

        void setup(uint8_t pinSclk, uint8_t pinSdio, uint8_t pinCsb, uint8_t pinFcsb) {
            mSpi.setup(pinSclk, pinSdio, pinCsb, pinFcsb);
            init();
        }

        void setup() {
            mSpi.setup();
            init();
        }

        // call as often as possible
        void loop() {
            if(mTxPending) {
                if(CMT2300A_MASK_TX_DONE_FLG == mSpi.readReg(CMT2300A_CUS_INT_CLR1)) {
                    if(cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY)) {
                        mTxPending = false;
                        goRx();
                    }
                }
            }
        }

        uint8_t goRx(void) {
            if(mTxPending)
                return CMT_ERR_TX_PENDING;

            if(mInRxMode)
                return CMT_SUCCESS;

            mSpi.readReg(CMT2300A_CUS_INT1_CTL);
            mSpi.writeReg(CMT2300A_CUS_INT1_CTL, CMT2300A_INT_SEL_TX_DONE);

            uint8_t tmp = mSpi.readReg(CMT2300A_CUS_INT_CLR1);
            if(0x08 == tmp) // first time after TX a value of 0x08 is read
                mSpi.writeReg(CMT2300A_CUS_INT_CLR1, 0x04);
            else
                mSpi.writeReg(CMT2300A_CUS_INT_CLR1, 0x00);

            if(0x10 == tmp)
                mSpi.writeReg(CMT2300A_CUS_INT_CLR2, 0x10);
            else
                mSpi.writeReg(CMT2300A_CUS_INT_CLR2, 0x00);

            mSpi.writeReg(CMT2300A_CUS_FIFO_CTL, 0x02);
            mSpi.writeReg(CMT2300A_CUS_FIFO_CLR, 0x02);
            mSpi.writeReg(0x16, 0x0C); // [4:3]: RSSI_DET_SEL, [2:0]: RSSI_AVG_MODE

            if(!cmtSwitchStatus(CMT2300A_GO_RX, CMT2300A_STA_RX))
                return CMT_ERR_SWITCH_STATE;

            mInRxMode = true;

            return CMT_SUCCESS;
        }

        uint8_t getRx(uint8_t buf[], uint8_t *rxLen, uint8_t maxlen, int8_t *rssi) {
            if(mTxPending)
                return CMT_ERR_TX_PENDING;

            if(0x1b != (mSpi.readReg(CMT2300A_CUS_INT_FLAG) & 0x1b))
                return CMT_FIFO_EMPTY;

            // receive ok (pream, sync, node, crc)
            if(!cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY))
                return CMT_ERR_SWITCH_STATE;

            mSpi.readFifo(buf, rxLen, maxlen);
            *rssi = mSpi.readReg(CMT2300A_CUS_RSSI_DBM) - 128;

            if(!cmtSwitchStatus(CMT2300A_GO_SLEEP, CMT2300A_STA_SLEEP))
                return CMT_ERR_SWITCH_STATE;

            if(!cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY))
                return CMT_ERR_SWITCH_STATE;

            mInRxMode   = false;
            mCusIntFlag = mSpi.readReg(CMT2300A_CUS_INT_FLAG);

            return CMT_SUCCESS;
        }

        uint8_t tx(uint8_t buf[], uint8_t len) {
            if(mTxPending)
                return CMT_ERR_TX_PENDING;

            if(mInRxMode) {
                mInRxMode = false;
                if(!cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY))
                    return CMT_ERR_SWITCH_STATE;
            }

            mSpi.writeReg(CMT2300A_CUS_INT1_CTL, CMT2300A_INT_SEL_TX_DONE);

            // no data received
            mSpi.readReg(CMT2300A_CUS_INT_CLR1);
            mSpi.writeReg(CMT2300A_CUS_INT_CLR1, 0x00);
            mSpi.writeReg(CMT2300A_CUS_INT_CLR2, 0x00);

            mSpi.writeReg(CMT2300A_CUS_FIFO_CTL, 0x07);
            mSpi.writeReg(CMT2300A_CUS_FIFO_CLR, 0x01);

            mSpi.writeReg(0x45, 0x01);
            mSpi.writeReg(0x46, len); // payload length

            mSpi.writeFifo(buf, len);

            if(0xff != mRqstCh) {
                mCurCh = mRqstCh;
                mRqstCh = 0xff;
                mSpi.writeReg(CMT2300A_CUS_FREQ_CHNL, mCurCh);
            }

            if(!cmtSwitchStatus(CMT2300A_GO_TX, CMT2300A_STA_TX))
                return CMT_ERR_SWITCH_STATE;

            // wait for tx done
            mTxPending = true;

            return CMT_SUCCESS;
        }

        // initialize CMT2300A, returns true on success
        bool reset(void) {
            mSpi.writeReg(0x7f, 0xff); // soft reset
            delay(30);

            if(!cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY))
                return false;

            mSpi.writeReg(CMT2300A_CUS_MODE_STA, 0x52);
            mSpi.writeReg(0x62, 0x20);
            if(mSpi.readReg(0x62) != 0x20)
                return false; // not connected!

            for(uint8_t i = 0; i < 0x60; i++) {
                mSpi.writeReg(i, cmtConfig[i]);
            }


            mSpi.writeReg(CMT2300A_CUS_IO_SEL, 0x20); // -> GPIO3_SEL[1:0] = 0x02

            // interrupt 1 control selection to TX DONE
            if(CMT2300A_INT_SEL_TX_DONE != mSpi.readReg(CMT2300A_CUS_INT1_CTL))
                mSpi.writeReg(CMT2300A_CUS_INT1_CTL, CMT2300A_INT_SEL_TX_DONE);

            // select interrupt 2
            if(0x07 != mSpi.readReg(CMT2300A_CUS_INT2_CTL))
                mSpi.writeReg(CMT2300A_CUS_INT2_CTL, 0x07);

            // interrupt enable (TX_DONE, PREAM_OK, SYNC_OK, CRC_OK, PKT_DONE)
            mSpi.writeReg(CMT2300A_CUS_INT_EN, 0x3B);

            mSpi.writeReg(0x64, 0x64);

            if(0x00 == mSpi.readReg(CMT2300A_CUS_FIFO_CTL))
                mSpi.writeReg(CMT2300A_CUS_FIFO_CTL, 0x02); // FIFO_MERGE_EN

            if(!cmtSwitchStatus(CMT2300A_GO_SLEEP, CMT2300A_STA_SLEEP))
                return false;

            delayMicroseconds(95);

            if(!cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY))
                return false;

            if(!cmtSwitchStatus(CMT2300A_GO_SLEEP, CMT2300A_STA_SLEEP))
                return false;

            if(!cmtSwitchStatus(CMT2300A_GO_STBY, CMT2300A_STA_STBY))
                return false;

            //switchDtuFreq(WORK_FREQ_KHZ);

            return true;
        }

        inline uint8_t freq2Chan(const uint32_t freqKhz) {
            if((freqKhz % FREQ_STEP_KHZ) != 0) {
                DPRINT(DBG_WARN, F("switch frequency to "));
                DBGPRINT(String(freqKhz));
                DBGPRINT(F("kHz not possible!"));
                return 0xff; // error
                // apply the nearest frequency
                //freqKhz = (freqKhz + FREQ_STEP_KHZ/2) / FREQ_STEP_KHZ;
                //freqKhz *= FREQ_STEP_KHZ;
            }

            if((freqKhz < HOY_BASE_FREQ_KHZ) || (freqKhz > HOY_MAX_FREQ_KHZ))
                return 0xff; // error

            if((freqKhz < FREQ_WARN_MIN_KHZ) || (freqKhz > FREQ_WARN_MAX_KHZ))
                DPRINTLN(DBG_WARN, F("Desired frequency is out of EU legal range! (863 - 870MHz)"));

            return (freqKhz - HOY_BASE_FREQ_KHZ) / FREQ_STEP_KHZ;
        }

        inline void switchChannel(uint8_t ch) {
            mRqstCh = ch;
        }

        inline uint32_t getFreqKhz(void) {
            if(0xff != mRqstCh)
                return HOY_BASE_FREQ_KHZ + (mRqstCh * FREQ_STEP_KHZ);
            else
                return HOY_BASE_FREQ_KHZ + (mCurCh * FREQ_STEP_KHZ);
        }

    private:
        void init() {
            mTxPending  = false;
            mInRxMode   = false;
            mCusIntFlag = 0x00;
            mCnt        = 0;
            mRqstCh     = 0xff;
            mCurCh      = 0x20;
        }

        // CMT state machine, wait for next state, true on success
        bool cmtSwitchStatus(uint8_t cmd, uint8_t waitFor, uint16_t cycles = 40) {
            mSpi.writeReg(CMT2300A_CUS_MODE_CTL, cmd);
            while(cycles--) {
                yield();
                delayMicroseconds(10);
                if(waitFor == (getChipStatus() & waitFor))
                    return true;
            }
            //Serial.println("status wait for: " + String(waitFor, HEX) + " read: " + String(getChipStatus(), HEX));
            return false;
        }

        inline bool switchDtuFreq(const uint32_t freqKhz) {
            uint8_t toCh = freq2Chan(freqKhz);
            if(0xff == toCh)
                return false;

            switchChannel(toCh);

            return true;
        }

        inline uint8_t getChipStatus(void) {
            return mSpi.readReg(CMT2300A_CUS_MODE_STA) & CMT2300A_MASK_CHIP_MODE_STA;
        }

        SpiType mSpi;
        uint8_t mCnt;
        bool mTxPending;
        bool mInRxMode;
        uint8_t mCusIntFlag;
        uint8_t mRqstCh, mCurCh;
};

#endif /*__CMT2300A_H__*/
