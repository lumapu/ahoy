//-----------------------------------------------------------------------------
// 2024 Ahoy, https://github.com/lumpapu/ahoy
// Creative Commons - http://creativecommons.org/licenses/by-nc-sa/4.0/deed
//-----------------------------------------------------------------------------

#include <algorithm>
#include "syslog.h"

#ifdef ENABLE_SYSLOG

#define SYSLOG_MAX_PACKET_SIZE 256

DbgSyslog::DbgSyslog() : mSyslogBuffer{} {}

//-----------------------------------------------------------------------------
void DbgSyslog::setup(settings_t *config) {
    mConfig  = config;

    // Syslog callback overrides web-serial callback
    registerDebugCb(std::bind(&DbgSyslog::syslogCb, this, std::placeholders::_1)); // dbg.h
}

//-----------------------------------------------------------------------------
void DbgSyslog::syslogCb (String msg)
{
    if (!mSyslogIP.isSet()) {
        //  use WiFi.hostByName to  DNS lookup for IPAddress of syslog server
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.hostByName(SYSLOG_HOST,mSyslogIP);
        }
    }
    if (!mSyslogIP.isSet()) {
        return;
    }
    uint16_t msgLength = msg.length();
    uint16_t msgPos = 0;

    do {
        uint16_t charsToCopy = std::min(msgLength-msgPos,SYSLOG_BUF_SIZE - mSyslogBufFill);

        while (charsToCopy > 0) {
            mSyslogBuffer[mSyslogBufFill] = msg[msgPos];
            msgPos++;
            mSyslogBufFill++;
            charsToCopy--;
        }
        mSyslogBuffer[mSyslogBufFill] = '\0';

        bool isBufferFull = (mSyslogBufFill == SYSLOG_BUF_SIZE);
        bool isEolFound = false;
        if (mSyslogBufFill >= 2) {
            isEolFound = (mSyslogBuffer[mSyslogBufFill-2] == '\r' && mSyslogBuffer[mSyslogBufFill-1] == '\n');
        }
        // Get severity from input message
        if (msgLength >= 2) {
            if (':' == msg[1]) {
                switch(msg[0]) {
                    case 'E': mSyslogSeverity = PRI_ERROR; break;
                    case 'W': mSyslogSeverity = PRI_WARNING; break;
                    case 'I': mSyslogSeverity = PRI_INFO; break;
                    case 'D': mSyslogSeverity = PRI_DEBUG; break;
                    default:  mSyslogSeverity = PRI_NOTICE; break;
                }
            }
        }

        if (isBufferFull || isEolFound) {
            // Send mSyslogBuffer in chunks because mSyslogBuffer is larger than syslog packet size
            int packetStart = 0;
            int packetSize = 122; // syslog payload depends also on hostname and app
            char saveChar;
            if (isEolFound) {
                mSyslogBuffer[mSyslogBufFill-2]=0; // skip \r\n
            }
            while(packetStart < mSyslogBufFill) {
                saveChar = mSyslogBuffer[packetStart+packetSize];
                mSyslogBuffer[packetStart+packetSize] = 0;
                log(mConfig->sys.deviceName,SYSLOG_FACILITY, mSyslogSeverity, &mSyslogBuffer[packetStart]);
                mSyslogBuffer[packetStart+packetSize] = saveChar;
                packetStart += packetSize;
            }
            mSyslogBufFill = 0;
        }

    } while (msgPos < msgLength); // Message not completely processed

}

//-----------------------------------------------------------------------------
void DbgSyslog::log(const char *hostname, uint8_t facility, uint8_t severity, char* msg) {
    // The PRI value is an integer number which calculates by the following metric:
    uint8_t priority = (8 * facility) + severity;

    // This is a unit8 instead of a char because that's what udp.write() wants
    uint8_t buffer[SYSLOG_MAX_PACKET_SIZE];
    int len = snprintf(static_cast<char*>(buffer), SYSLOG_MAX_PACKET_SIZE, "<%d>%s %s: %s", priority, hostname, SYSLOG_APP, msg);
    //printf("syslog::log %s\n",mSyslogIP.toString().c_str());
    //printf("syslog::log %d %s\n",len,buffer);
    // Send the raw UDP packet
    mSyslogUdp.beginPacket(mSyslogIP, SYSLOG_PORT);
    mSyslogUdp.write(buffer, len);
    mSyslogUdp.endPacket();
}

#endif
