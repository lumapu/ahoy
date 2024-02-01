//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - https://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#ifndef __PROTECTION_H__
#define __PROTECTION_H__
#pragma once

#include <array>
#include <cstdint>

#include "../config/config.h"
#include "../utils/helper.h"

class Protection {
    protected:
        Protection(const char *pwd) {
            mPwd = pwd;
            mLogoutTimeout = 0;
            mLoginIp.fill(0);

            // no password set - unlock
            if(pwd[0] == '\0')
                mProtected = false;
        }

    public:
        Protection(Protection &other) = delete;
        void operator=(const Protection &) = delete;

        static Protection* getInstance(const char *pwd) {
            if(nullptr == mInstance)
                mInstance = new Protection(pwd);
            return mInstance;
        }

        void tickSecond() {
            // auto logout
            if(0 != mLogoutTimeout) {
                if (0 == --mLogoutTimeout) {
                    if(mPwd[0] == '\0')
                        mProtected = true;
                }
            }
        }

        void lock(void) {
            mProtected = true;
            mLoginIp.fill(0);
        }

        void unlock(const char *clientIp) {
            mLogoutTimeout = LOGOUT_TIMEOUT;
            mProtected = false;
            ah::ip2Arr(static_cast<uint8_t*>(mLoginIp.data()), clientIp);
        }

        void resetLockTimeout(void) {
            mLogoutTimeout = LOGOUT_TIMEOUT;
        }

        bool isProtected(void) const {
            return mProtected;
        }

        bool isProtected(const char *clientIp) const {
            if(mProtected)
                return true;

            if(mPwd[0] == '\0')
                return false;

            std::array<uint8_t, 4> ip;
            ah::ip2Arr(static_cast<uint8_t*>(ip.data()), clientIp);
            for(uint8_t i = 0; i < 4; i++) {
                if(mLoginIp[i] != ip[i]) {
                    DPRINTLN(DBG_INFO, "ip nicht gleich!");
                    return true;
                }
            }

            return false;
        }

    protected:
        static Protection *mInstance;

    private:
        const char *mPwd;
        bool mProtected = true;
        uint16_t mLogoutTimeout = LOGOUT_TIMEOUT;
        std::array<uint8_t, 4> mLoginIp;
};

#endif /*__PROTECTION_H__*/
