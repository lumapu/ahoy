//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#if defined(ETHERNET)
#ifndef __AHOYETH_H__
#define __AHOYETH_H__

#include <functional>

#include <Arduino.h>
#include <AsyncUDP.h>
#include <DNSServer.h>

#include "ethSpi.h"
#include "../utils/dbg.h"
#include "../config/config.h"
#include "../config/settings.h"

#include "AsyncWebServer_ESP32_W5500.h"


class app;

#define NTP_PACKET_SIZE 48

class ahoyeth {
    public: /* types */
        typedef std::function<void(bool)> OnNetworkCB;
        typedef std::function<void(bool)> OnTimeCB;

    public:
        ahoyeth();

        void setup(settings_t *config, uint32_t *utcTimestamp, OnNetworkCB onNetworkCB, OnTimeCB onTimeCB);
        bool updateNtpTime(void);

    private:
        void setupEthernet();

        void handleNTPPacket(AsyncUDPPacket packet);

        void welcome(String ip, String mode);

        void onEthernetEvent(WiFiEvent_t event, arduino_event_info_t info);

    private:
        #if defined(CONFIG_IDF_TARGET_ESP32S3)
        EthSpi mEthSpi;
        #endif
        settings_t *mConfig = nullptr;

        uint32_t *mUtcTimestamp;
        AsyncUDP mUdp; // for time server
        byte mUdpPacketBuffer[NTP_PACKET_SIZE];   // buffer to hold incoming and outgoing packets

        OnNetworkCB mOnNetworkCB;
        OnTimeCB mOnTimeCB;

};

#endif /*__AHOYETH_H__*/
#endif /* defined(ETHERNET) */
