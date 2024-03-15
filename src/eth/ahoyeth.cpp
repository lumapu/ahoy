//-----------------------------------------------------------------------------
// 2023 Ahoy, https://www.mikrocontroller.net/topic/525778
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//-----------------------------------------------------------------------------

#if defined(ETHERNET)

#if defined(ESP32) && defined(F)
  #undef F
  #define F(sl) (sl)
#endif
#include "ahoyeth.h"
#include <ESPmDNS.h>

//-----------------------------------------------------------------------------
ahoyeth::ahoyeth()
{
    // WiFi.onEvent(ESP32_W5500_event);
}


//-----------------------------------------------------------------------------
void ahoyeth::setup(settings_t *config, uint32_t *utcTimestamp, OnNetworkCB onNetworkCB, OnTimeCB onTimeCB) {
    mConfig = config;
    mUtcTimestamp = utcTimestamp;
    mOnNetworkCB = onNetworkCB;
    mOnTimeCB = onTimeCB;
    mEthConnected = false;

    Serial.flush();
    WiFi.onEvent([this](WiFiEvent_t event, arduino_event_info_t info) -> void { this->onEthernetEvent(event, info); });

    Serial.flush();
    //#if defined(CONFIG_IDF_TARGET_ESP32S3)
    mEthSpi.begin(DEF_ETH_MISO_PIN, DEF_ETH_MOSI_PIN, DEF_ETH_SCK_PIN, DEF_ETH_CS_PIN, DEF_ETH_IRQ_PIN, DEF_ETH_RST_PIN);
    //#else
    //ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
    //ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, DEF_ETH_MOSI_PIN, ETH_PHY_TYPE, ETH_CLK_MODE);
    //#endif

    if(mConfig->sys.ip.ip[0] != 0) {
        IPAddress ip(mConfig->sys.ip.ip);
        IPAddress mask(mConfig->sys.ip.mask);
        IPAddress dns1(mConfig->sys.ip.dns1);
        IPAddress dns2(mConfig->sys.ip.dns2);
        IPAddress gateway(mConfig->sys.ip.gateway);
        if(!ETH.config(ip, gateway, mask, dns1, dns2))
            DPRINTLN(DBG_ERROR, F("failed to set static IP!"));
    }
}


//-----------------------------------------------------------------------------
bool ahoyeth::updateNtpTime(void) {
    DPRINTLN(DBG_DEBUG, F(__FUNCTION__)); Serial.flush();
    Serial.printf("ETH.linkUp()=%s\n", ETH.linkUp() ? "up" : "down");
    Serial.print("ETH.localIP()=");
    Serial.println(ETH.localIP());
    Serial.printf("Go on? %s\n", (!ETH.localIP()) ? "No..." : "Yes...");
    if (!ETH.localIP())
        return false;

    DPRINTLN(DBG_DEBUG, F("updateNtpTime: checking udp \"connection\"...")); Serial.flush();
    if (!mUdp.connected()) {
        DPRINTLN(DBG_DEBUG, F("updateNtpTime: About to (re)connect...")); Serial.flush();
        IPAddress timeServer;
        if (!WiFi.hostByName(mConfig->ntp.addr, timeServer))
            return false;

        if (!mUdp.connect(timeServer, mConfig->ntp.port))
            return false;

        DPRINTLN(DBG_DEBUG, F("updateNtpTime: Connected...")); Serial.flush();
        mUdp.onPacket([this](AsyncUDPPacket packet) {
            DPRINTLN(DBG_DEBUG, F("updateNtpTime: about to handle ntp packet...")); Serial.flush();
            this->handleNTPPacket(packet);
        });
    }

    DPRINTLN(DBG_DEBUG, F("updateNtpTime: prepare packet...")); Serial.flush();

    // set all bytes in the buffer to 0
    memset(mUdpPacketBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)

    mUdpPacketBuffer[0]   = 0b11100011;   // LI, Version, Mode
    mUdpPacketBuffer[1]   = 0;     // Stratum, or type of clock
    mUdpPacketBuffer[2]   = 6;     // Polling Interval
    mUdpPacketBuffer[3]   = 0xEC;  // Peer Clock Precision

    // 8 bytes of zero for Root Delay & Root Dispersion
    mUdpPacketBuffer[12]  = 49;
    mUdpPacketBuffer[13]  = 0x4E;
    mUdpPacketBuffer[14]  = 49;
    mUdpPacketBuffer[15]  = 52;

    //Send unicast
    DPRINTLN(DBG_DEBUG, F("updateNtpTime: send packet...")); Serial.flush();
    mUdp.write(mUdpPacketBuffer, sizeof(mUdpPacketBuffer));

    return true;
}

//-----------------------------------------------------------------------------
void ahoyeth::handleNTPPacket(AsyncUDPPacket packet) {
    char       buf[80];

    memcpy(buf, packet.data(), sizeof(buf));

    unsigned long highWord = word(buf[40], buf[41]);
    unsigned long lowWord = word(buf[42], buf[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    *mUtcTimestamp = secsSince1900 - 2208988800UL; // UTC time
    DPRINTLN(DBG_INFO, "[NTP]: " + ah::getDateTimeStr(*mUtcTimestamp) + " UTC");
    mOnTimeCB(true);
}

//-----------------------------------------------------------------------------
void ahoyeth::welcome(String ip, String mode) {
    DBGPRINTLN(F("\n\n--------------------------------"));
    DBGPRINTLN(F("Welcome to AHOY!"));
    DBGPRINT(F("\npoint your browser to http://"));
    DBGPRINT(ip);
    DBGPRINTLN(mode);
    DBGPRINTLN(F("to configure your device"));
    DBGPRINTLN(F("--------------------------------\n"));
}

void ahoyeth::onEthernetEvent(WiFiEvent_t event, arduino_event_info_t info) {
    DPRINTLN(DBG_VERBOSE, F("[ETH]: Got event..."));
    switch (event) {
#if ( ( defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2) ) && ( ARDUINO_ESP32_GIT_VER != 0x46d5afb1 ) )
    // For breaking core v2.0.0
    // Why so strange to define a breaking enum arduino_event_id_t in WiFiGeneric.h
    // compared to the old system_event_id_t, now in tools/sdk/esp32/include/esp_event/include/esp_event_legacy.h
    // You can preserve the old enum order and just adding new items to do no harm
    case ARDUINO_EVENT_ETH_START:
        DPRINTLN(DBG_INFO, F("\nETH Started"));
        //set eth hostname here
        if(String(mConfig->sys.deviceName) != "")
            ETH.setHostname(mConfig->sys.deviceName);
        else
            ETH.setHostname("ESP32_W5500");
        break;

    case ARDUINO_EVENT_ETH_CONNECTED:
        DPRINTLN(DBG_INFO, F("ETH Connected"));
        break;

    case ARDUINO_EVENT_ETH_GOT_IP:
        if (!mEthConnected) {
            DPRINT(DBG_INFO, F("ETH MAC: "));
            #if defined (CONFIG_IDF_TARGET_ESP32S3)
            DBGPRINT(mEthSpi.macAddress());
            #else
            DBGPRINT(ETH.macAddress());
            #endif
            DBGPRINT(F(", IPv4: "));
            DBGPRINTLN(String(ETH.localIP()));

            if (ETH.fullDuplex()) {
                DPRINTLN(DBG_INFO, F("FULL_DUPLEX, "));
            } else {
                DPRINTLN(DBG_INFO, F("HALF_DUPLEX, "));
            }

            DPRINT(DBG_INFO, String(ETH.linkSpeed()));
            DBGPRINTLN(F("Mbps"));

            mEthConnected = true;
            mOnNetworkCB(true);
        }
        if (!MDNS.begin(mConfig->sys.deviceName)) {
            DPRINTLN(DBG_ERROR, F("Error setting up MDNS responder!"));
        } else {
            DBGPRINT(F("[WiFi] mDNS established: "));
            DBGPRINT(mConfig->sys.deviceName);
            DBGPRINTLN(F(".local"));
        }
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      DPRINTLN(DBG_INFO, "ETH Disconnected");
      mEthConnected = false;
      mUdp.close();
      mOnNetworkCB(false);
      break;

    case ARDUINO_EVENT_ETH_STOP:
      DPRINTLN(DBG_INFO, "\nETH Stopped");
      mEthConnected = false;
      mUdp.close();
      mOnNetworkCB(false);
      break;

#else

    // For old core v1.0.6-
    // Core v2.0.0 defines a stupid enum arduino_event_id_t, breaking any code for ESP32_W5500 written for previous core
    // Why so strange to define a breaking enum arduino_event_id_t in WiFiGeneric.h
    // compared to the old system_event_id_t, now in tools/sdk/esp32/include/esp_event/include/esp_event_legacy.h
    // You can preserve the old enum order and just adding new items to do no harm
    case SYSTEM_EVENT_ETH_START:
        DPRINTLN(DBG_INFO, F("\nETH Started"));
        //set eth hostname here
        if(String(mConfig->sys.deviceName) != "")
            ETH.setHostname(mConfig->sys.deviceName);
        else
            ETH.setHostname("ESP32_W5500");
        break;

    case SYSTEM_EVENT_ETH_CONNECTED:
        DPRINTLN(DBG_INFO, F("ETH Connected"));
        break;

    case SYSTEM_EVENT_ETH_GOT_IP:
        if (!ESP32_W5500_eth_connected) {
            DPRINT(DBG_INFO, F("ETH MAC: "));
            DBGPRINT(ETH.macAddress());
            DBGPRINT(F(", IPv4: "));
            DBGPRINTLN(ETH.localIP());

            if (ETH.fullDuplex()) {
                DPRINTLN(DBG_INFO, F("FULL_DUPLEX, "));
            } else {
                DPRINTLN(DBG_INFO, F("HALF_DUPLEX, "));
            }

            DPRINT(DBG_INFO, ETH.linkSpeed());
            DBGPRINTLN(F("Mbps"));

            ESP32_W5500_eth_connected = true;
            mOnNetworkCB(true);
        }
        if (!MDNS.begin(mConfig->sys.deviceName)) {
            DPRINTLN(DBG_ERROR, F("Error setting up MDNS responder!"));
        } else {
            DBGPRINT(F("[WiFi] mDNS established: "));
            DBGPRINT(mConfig->sys.deviceName);
            DBGPRINTLN(F(".local"));
        }
        break;

    case SYSTEM_EVENT_ETH_DISCONNECTED:
        DPRINT(DBG_INFO, F("ETH Disconnected"));
        ESP32_W5500_eth_connected = false;
        mUdp.close();
        mOnNetworkCB(false);
        break;

    case SYSTEM_EVENT_ETH_STOP:
        DPRINT(DBG_INFO, F("ETH Stopped"));
        ESP32_W5500_eth_connected = false;
        mUdp.close();
        mOnNetworkCB(false);
        break;
#endif

    default:

      break;
  }

}

#endif /* defined(ETHERNET) */
