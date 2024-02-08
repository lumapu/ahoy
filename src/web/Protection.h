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
        explicit Protection(const char *pwd) {
            mPwd = pwd;
            mLogoutTimeout = 0;
            mLoginIp.fill(0);
            mToken.fill(0);

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
                    if(mPwd[0] != '\0')
                        mProtected = true;
                }
            }
        }

        void lock(void) {
            mProtected = true;
            mLoginIp.fill(0);
        }

        char *unlock(const char *clientIp) {
            mLogoutTimeout = LOGOUT_TIMEOUT;
            mProtected = false;
            ah::ip2Arr(static_cast<uint8_t*>(mLoginIp.data()), clientIp);
            genToken();

            return reinterpret_cast<char*>(mToken.data());
        }

        void resetLockTimeout(void) {
            if(0 != mLogoutTimeout)
                mLogoutTimeout = LOGOUT_TIMEOUT;
        }

        bool isProtected(void) const {
            return mProtected;
        }

        bool isProtected(const char *clientIp, const char *token) const {
            if(isProtected(clientIp))
                return true;

            if(0 == mToken[0]) // token is zero
                return true;

            return (0 != strncmp(token, mToken.data(), 16));
        }

        bool isProtected(const char *clientIp) const {
            if(mProtected)
                return true;

            if(mPwd[0] == '\0')
                return false;

            std::array<uint8_t, 4> ip;
            ah::ip2Arr(static_cast<uint8_t*>(ip.data()), clientIp);
            for(uint8_t i = 0; i < 4; i++) {
                if(mLoginIp[i] != ip[i])
                    return true;
            }

            return false;
        }

    private:
        void genToken() {
            mToken.fill(0);
            for(uint8_t i = 0; i < 16; i++) {
                mToken[i] = random(1, 35);
                if(mToken[i] < 10)
                    mToken[i] += 0x30; // convert to ascii number 1-9 (zero isn't allowed)
                else
                    mToken[i] += 0x37; // convert to ascii upper case character
            }
        }

    protected:
        static Protection *mInstance;

    private:
        const char *mPwd;
        bool mProtected = true;
        uint16_t mLogoutTimeout = 0;
        std::array<uint8_t, 4> mLoginIp;
        std::array<char, 17> mToken;
};

#endif /*__PROTECTION_H__*/
