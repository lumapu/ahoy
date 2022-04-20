#ifndef __HM1200_DECODE__
#define __HM1200_DECODE__

typedef struct {
    double u;
    double i;
    double p;
    uint16_t y_d; // yield day
    double y_t; // yield total
} ch_t;

typedef struct {
    ch_t ch_dc[4];
    ch_t ch_ac;
    double temp;
    double pct;
    double acFreq;
} hoyData_t;

class hm1200Decode {
    public:
        hm1200Decode() {
            memset(&mData, 0, sizeof(hoyData_t));
        }
        ~hm1200Decode() {}

        void convert(uint8_t buf[], uint8_t len) {
            switch(buf[0]) {
                case 0x01: convCmd01(buf, len); break;
                case 0x02: convCmd02(buf, len); break;
                case 0x03: convCmd03(buf, len); break;
                case 0x84: convCmd84(buf, len); break;
                default: break;
            }
        }


        hoyData_t mData;

    private:
        void convCmd01(uint8_t buf[], uint8_t len) {
            mData.ch_dc[0].u   = ((buf[ 3] << 8)  | buf[ 4]) / 10.0f;
            mData.ch_dc[0].i   = ((buf[ 5] << 8)  | buf[ 6]) / 100.0f;
            mData.ch_dc[1].i   = ((buf[ 7] << 8)  | buf[ 8]) / 100.0f;
            mData.ch_dc[0].p   = ((buf[ 9] << 8)  | buf[10]) / 10.0f;
            mData.ch_dc[1].p   = ((buf[11] << 8)  | buf[12]) / 10.0f;
            mData.ch_dc[0].y_t = ((buf[13] << 24) | (buf[14] << 16)
                                | (buf[15] << 8)  | buf[16]) / 1000.0f;
        }


        void convCmd02(uint8_t buf[], uint8_t len) {
            mData.ch_dc[1].y_t = ((buf[ 1] << 24) | (buf[ 2] << 16)
                                | (buf[ 3] << 8)  | buf[ 4]) / 1000.0f;
            mData.ch_dc[0].y_d = ((buf[ 5] << 8)  | buf[ 6]);
            mData.ch_dc[1].y_d = ((buf[ 7] << 8)  | buf[ 8]);
            mData.ch_dc[1].u   = ((buf[ 9] << 8)  | buf[10]) / 10.0f;
            mData.ch_dc[2].i   = ((buf[11] << 8)  | buf[12]) / 100.0f;
            mData.ch_dc[3].i   = ((buf[13] << 8)  | buf[14]) / 100.0f;
            mData.ch_dc[2].p   = ((buf[15] << 8)  | buf[16]) / 10.0f;
        }


        void convCmd03(uint8_t buf[], uint8_t len) {
            mData.ch_dc[3].p   = ((buf[ 1] << 8)  | buf[ 2]) / 10.0f;
            mData.ch_dc[2].y_t = ((buf[ 3] << 24) | (buf[4] << 16)
                                | (buf[ 5] << 8)  | buf[ 6]) / 1000.0f;
            mData.ch_dc[3].y_t = ((buf[ 7] << 24) | (buf[8] << 16)
                                | (buf[ 9] << 8)  | buf[10]) / 1000.0f;
            mData.ch_dc[2].y_d = ((buf[11] << 8)  | buf[12]);
            mData.ch_dc[3].y_d = ((buf[13] << 8)  | buf[14]);
            mData.ch_ac.u      = ((buf[15] << 8)  | buf[16]) / 10.0f;
        }


        void convCmd84(uint8_t buf[], uint8_t len) {
            mData.acFreq   = ((buf[ 1] << 8) | buf[ 2]) / 100.0f;
            mData.ch_ac.p  = ((buf[ 3] << 8) | buf[ 4]) / 10.0f;
            mData.ch_ac.i  = ((buf[ 7] << 8) | buf[ 8]) / 100.0f;
            mData.pct      = ((buf[ 9] << 8) | buf[10]) / 10.0f;
            mData.temp     = ((buf[11] << 8) | buf[12]) / 10.0f;
        }

        // private member variables
};

#endif /*__HM1200_DECODE__*/
