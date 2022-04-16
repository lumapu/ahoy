#include "app.h"

#include "html/h/index_html.h"
extern String setup_html;

//-----------------------------------------------------------------------------
app::app() : Main() {
    uint8_t wrAddr[6];
    mRadio = new RF24(RF24_CE_PIN, RF24_CS_PIN);

    mRadio->begin();
    mRadio->setAutoAck(false);
    mRadio->setRetries(0, 0);

    mHoymiles = new hoymiles();
    mEep->read(ADDR_HOY_ADDR, mHoymiles->mAddrBytes, HOY_ADDR_LEN);
    mHoymiles->serial2RadioId();

    mBufCtrl = new CircularBuffer(mBuffer, PACKET_BUFFER_SIZE);

    mSendCnt = 0;
    mSendTicker = new Ticker();
}


//-----------------------------------------------------------------------------
app::~app(void) {

}


//-----------------------------------------------------------------------------
void app::setup(const char *ssid, const char *pwd, uint32_t timeout) {
    Main::setup(ssid, pwd, timeout);

    mWeb->on("/",       std::bind(&app::showIndex, this));
    mWeb->on("/setup",  std::bind(&app::showSetup, this));
    mWeb->on("/save ",  std::bind(&app::showSave, this));

    initRadio();

    mSendTicker->attach_ms(1000, std::bind(&app::sendTicker, this));
}


//-----------------------------------------------------------------------------
void app::loop(void) {
    Main::loop();

    if(!mBufCtrl->empty()) {
        uint8_t len, rptCnt;
        NRF24_packet_t *p = mBufCtrl->getBack();

        //mHoymiles->dumpBuf("RAW", p->packet, PACKET_BUFFER_SIZE);

        if(mHoymiles->checkCrc(p->packet, &len, &rptCnt)) {
            // process buffer only on first occurrence
            if((0 != len) && (0 == rptCnt)) {
                mHoymiles->dumpBuf("Payload", p->packet, len);
                // @TODO: do analysis here
            }
        }
        else {
            if(p->packetsLost != 0) {
                Serial.println("Lost packets: " + String(p->packetsLost));
            }
        }
        mBufCtrl->popBack();
    }
}


//-----------------------------------------------------------------------------
void app::handleIntr(void) {
    uint8_t lostCnt = 0, pipe, len;
    NRF24_packet_t *p;

    DISABLE_IRQ;

    while(mRadio->available(&pipe)) {
        if(!mBufCtrl->full()) {
            p = mBufCtrl->getFront();
            memset(p->packet, 0xcc, MAX_RF_PAYLOAD_SIZE);
            p->timestamp = micros(); // Micros does not increase in interrupt, but it can be used.
            p->packetsLost = lostCnt;
            len = mRadio->getPayloadSize();
            if(len > MAX_RF_PAYLOAD_SIZE)
                len = MAX_RF_PAYLOAD_SIZE;

            mRadio->read(p->packet, len);
            mBufCtrl->pushFront(p);
            lostCnt = 0;
        }
        else {
            bool tx_ok, tx_fail, rx_ready;
            if(lostCnt < 255)
                lostCnt++;
            mRadio->whatHappened(tx_ok, tx_fail, rx_ready); // reset interrupt status
            mRadio->flush_rx(); // drop the packet
        }
    }

    RESTORE_IRQ;
}


//-----------------------------------------------------------------------------
void app::initRadio(void) {
    mRadio->setChannel(DEFAULT_RECV_CHANNEL);
    mRadio->setDataRate(RF24_250KBPS);
    mRadio->disableCRC();
    mRadio->setAutoAck(false);
    mRadio->setPayloadSize(MAX_RF_PAYLOAD_SIZE);
    mRadio->setAddressWidth(5);
    mRadio->openReadingPipe(1, DTU_RADIO_ID);

    // enable only receiving interrupts
    mRadio->maskIRQ(true, true, false);

    // Use lo PA level, as a higher level will disturb CH340 serial usb adapter
    mRadio->setPALevel(RF24_PA_MAX);
    mRadio->startListening();

    Serial.println("Radio Config:");
    mRadio->printPrettyDetails();
}


//-----------------------------------------------------------------------------
void app::sendPacket(uint8_t buf[], uint8_t len) {
    DISABLE_IRQ;

    mRadio->stopListening();

#ifdef CHANNEL_HOP
    mRadio->setChannel(mHoymiles->getNxtChannel());
#else
    mRadio->setChannel(mHoymiles->getDefaultChannel());
#endif

    mRadio->openWritingPipe(mHoymiles->mRadioId);
    mRadio->setCRCLength(RF24_CRC_16);
    mRadio->enableDynamicPayloads();
    mRadio->setAutoAck(true);
    mRadio->setRetries(3, 15);

    mRadio->write(buf, len);

    // Try to avoid zero payload acks (has no effect)
    mRadio->openWritingPipe(DUMMY_RADIO_ID);

    mRadio->setAutoAck(false);
    mRadio->setRetries(0, 0);
    mRadio->disableDynamicPayloads();
    mRadio->setCRCLength(RF24_CRC_DISABLED);

    mRadio->setChannel(DEFAULT_RECV_CHANNEL);
    mRadio->startListening();

    RESTORE_IRQ;
}


//-----------------------------------------------------------------------------
void app::sendTicker(void) {
    uint8_t size = 0;
    if((mSendCnt % 6) == 0)
        size = mHoymiles->getTimePacket(mSendBuf, mTimestamp);
    else if((mSendCnt % 6) == 5)
        size = mHoymiles->getCmdPacket(mSendBuf, 0x15, 0x81);
    else if((mSendCnt % 6) == 4)
        size = mHoymiles->getCmdPacket(mSendBuf, 0x15, 0x80);
    else if((mSendCnt % 6) == 3)
        size = mHoymiles->getCmdPacket(mSendBuf, 0x15, 0x83);
    else if((mSendCnt % 6) == 2)
        size = mHoymiles->getCmdPacket(mSendBuf, 0x15, 0x82);
    else if((mSendCnt % 6) == 1)
        size = mHoymiles->getCmdPacket(mSendBuf, 0x15, 0x84);

    Serial.println("sent packet: #" + String(mSendCnt));
    dumpBuf(mSendBuf, size);
    sendPacket(mSendBuf, size);

    mSendCnt++;
}


//-----------------------------------------------------------------------------
void app::showIndex(void) {
    String html = index_html;
    html.replace("{DEVICE}", mDeviceName);
    html.replace("{VERSION}", mVersion);
    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showSetup(void) {
    // overrides same method in main.cpp

    String html = setup_html;
    html.replace("{SSID}", mStationSsid);
    // PWD will be left at the default value (for protection)
    // -> the PWD will only be changed if it does not match the placeholder "{PWD}"

    char addr[20] = {0};
    sprintf(addr, "%02X:%02X:%02X:%02X:%02X:%02X", mHoymiles->mAddrBytes[0], mHoymiles->mAddrBytes[1], mHoymiles->mAddrBytes[2], mHoymiles->mAddrBytes[3], mHoymiles->mAddrBytes[4], mHoymiles->mAddrBytes[5]);
    html.replace("{HOY_ADDR}", String(addr));

    html.replace("{DEVICE}", String(mDeviceName));
    html.replace("{VERSION}", String(mVersion));

    mWeb->send(200, "text/html", html);
}


//-----------------------------------------------------------------------------
void app::showSave(void) {
    saveValues(true);
}


//-----------------------------------------------------------------------------
void app::saveValues(bool webSend = true) {
    Main::saveValues(false); // general configuration

    if(mWeb->args() > 0) {
        char *p;
        char addr[20] = {0};
        uint8_t i = 0;

        memset(mHoymiles->mAddrBytes, 0, 6);
        mWeb->arg("hoy_addr").toCharArray(addr, 20);

        p = strtok(addr, ":");
        while(NULL != p) {
            mHoymiles->mAddrBytes[i++] = strtol(p, NULL, 16);
            p = strtok(NULL, ":");
        }

        mEep->write(ADDR_HOY_ADDR, mHoymiles->mAddrBytes, HOY_ADDR_LEN);

        if((mWeb->arg("reboot") == "on"))
            showReboot();
        else {
            mWeb->send(200, "text/html", "<!doctype html><html><head><title>Setup saved</title><meta http-equiv=\"refresh\" content=\"0; URL=/setup\"></head><body>"
                "<p>saved</p></body></html>");
        }
    }
    else {
        mWeb->send(200, "text/html", "<!doctype html><html><head><title>Error</title><meta http-equiv=\"refresh\" content=\"3; URL=/setup\"></head><body>"
            "<p>Error while saving</p></body></html>");
    }
}


//-----------------------------------------------------------------------------
void app::dumpBuf(uint8_t buf[], uint8_t len) {
    for(uint8_t i = 0; i < len; i ++) {
        if((i % 8 == 0) && (i != 0))
            Serial.println();
        Serial.print(String(buf[i], HEX) + " ");
    }
    Serial.println();
}
