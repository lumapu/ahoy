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
            mWebIp.fill(0);
            mApiIp.fill(0);
            mToken.fill(0);
        }

    public:
        Protection(Protection &other) = delete;
        void operator=(const Protection &) = delete;

        static Protection* getInstance(const char *pwd) {
            if(nullptr == mInstance)
                mInstance = new Protection(pwd);
            return mInstance;
        }

        void tickSecond() { // auto logout
            if(0 != mLogoutTimeout) {
                if (0 == --mLogoutTimeout) {
                    if(mPwd[0] != '\0')
                        lock(false);
                }
            }
        }

        void lock(bool fromWeb) {
            mWebIp.fill(0);
            if(fromWeb)
                return;

            mApiIp.fill(0);
            mToken.fill(0);
        }

        char *unlock(const char *clientIp, bool loginFromWeb) {
            mLogoutTimeout = LOGOUT_TIMEOUT;

            if(loginFromWeb)
                ah::ip2Arr(static_cast<uint8_t*>(mWebIp.data()), clientIp);
            else {
                ah::ip2Arr(static_cast<uint8_t*>(mApiIp.data()), clientIp);
                genToken();
            }

            return reinterpret_cast<char*>(mToken.data());
        }

        void resetLockTimeout(void) {
            if(0 != mLogoutTimeout)
                mLogoutTimeout = LOGOUT_TIMEOUT;
        }

        bool isProtected(const char *clientIp, const char *token, bool askedFromWeb) const {
            if(mPwd[0] == '\0') // no password set
                return false;

            if(askedFromWeb)
                return !isIdentical(clientIp, mWebIp);

            if(nullptr == token)
                return true;

            if('*' == token[0]) // call from WebUI
                return !isIdentical(clientIp, mWebIp);

            if(isIdentical(clientIp, mApiIp))
                return (0 != strncmp(token, mToken.data(), 16));

            return false;
        }

    private:
        void genToken() {
            mToken.fill(0);
            for(uint8_t i = 0; i < 16; i++) {
                mToken[i] = random(1, 35);
                // convert to ascii number 1-9 (zero isn't allowed) or upper
                // case character A-Z
                mToken[i] += (mToken[i] < 10) ? 0x30 : 0x37;
            }
        }

        bool isIdentical(const char *clientIp, const std::array<uint8_t, 4> cmp) const {
            std::array<uint8_t, 4> ip;
            ah::ip2Arr(static_cast<uint8_t*>(ip.data()), clientIp);
            for(uint8_t i = 0; i < 4; i++) {
                if(cmp[i] != ip[i])
                    return false;
            }
            return true;
        }

    protected:
        static Protection *mInstance;

    private:
        const char *mPwd;
        uint16_t mLogoutTimeout = 0;
        std::array<uint8_t, 4> mWebIp, mApiIp;
        std::array<char, 17> mToken;
};

#endif /*__PROTECTION_H__*/
