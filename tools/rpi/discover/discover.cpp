/* based on "gettingstarted.cpp" by 2bdy5 */

/**
 * "PING" all known microinverters (serial numbers) on all
 * known channels.
 * Use a "known good" master (DTU) address.
 * Keep track of the inverters as they frequency-hop.
 * 
 * Test this tool by setting up an instance of "gettingstarted.cpp"
 * with address setting '0' (default) as a test receiver.
 */
#include <ctime>       // time()
#include <iostream>    // cin, cout, endl
#include <iomanip>
#include <string>      // string, getline()
#include <vector>
#include <sstream>
#include <time.h>      // CLOCK_MONOTONIC_RAW, timespec, clock_gettime()
#include <RF24/RF24.h> // RF24, RF24_PA_LOW, delay()

#include <time.h>
#include <stdlib.h>

#include "common.hpp"

using namespace std;

// connection to our radio board
RF24 radio(22, 0, 1000000);
// See http://nRF24.github.io/RF24/pages.html for more information on usage


/** Ping the given address.
 * @returns true if we received a reply, otherwise false.
 */
bool doPing(int ch, string src, string dst)
{
    const int payloadsize = 12;
    char payload[20];
    radio.enableDynamicPayloads();
    radio.setChannel(ch);

    radio.setPALevel(RF24_PA_MAX); // RF24_PA_MAX is default.
    radio.setDataRate(RF24_250KBPS);

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(1, (const uint8_t *)src.c_str());
        // ...not that this matters for simple ping/ack

    radio.stopListening();                                          // put radio in TX mode

     // set the TX address of the RX node into the TX pipe
    radio.openWritingPipe((const uint8_t *)dst.c_str());

    // We need to modify the payload every time otherwise recipients
    // will detect packets as 'duplicates' and silently ignore them
    // (although they will still auto-ack them).
    unsigned int r = rand();  
    snprintf(payload, sizeof(payload), "ping%08ud", r);

    bool report = radio.write(payload, payloadsize);

    if (report) {
        // payload was delivered
        return true;
    }
    return false;  // no reply received
}


int main(int argc, char** argv) 
{
    srand(time(NULL));   // Initialization, should only be called once.

    if (!radio.begin()) {
        cout << "radio hardware is not responding!!" << endl;
        return 0; // quit now
    }

    if(!radio.isPVariant())
    {
    	printf("not nRF24L01+\n");
        return 0;
    }

    if(!radio.isChipConnected())
    {
    	printf("not connected\n");
        return 0;
    }

    // TODO 
    // we probably want
    // - dynamic payloads (check in rf logs)
    // - do we need/want "custom ack payloads"?
    // - use isAckPayloadAvailable() once we've actually contacted an inverter successfully!

    //radio.printPrettyDetails();

    // well-known valid DTU serial number
    // just in case the inverter only responds to addresses
    // that fulfil certain requirements.
    //string masteraddr = serno2shockburstaddrbytes(99912345678);
    string masteraddr = serno2shockburstaddrbytes(999970535453);


    // serial numbers of all inverters that we are trying to find
    vector<string> dstaddrs;
    dstaddrs.push_back(string("1Node"));
    dstaddrs.push_back(string("2Node"));
    dstaddrs.push_back(serno2shockburstaddrbytes(114174608145));
    dstaddrs.push_back("\x45\x81\x60\x74\x01");
    dstaddrs.push_back(serno2shockburstaddrbytes(114174608177));

    // channels that we will scan
    vector<int> channels{1, 3, 6, 9, 11, 23, 40, 41, 61, 75, 76, 99};

    for(auto & ch : channels)
    {
        cout << "ch " << setw(2) << ch << " ";
        for(auto & a : dstaddrs)
        {
            cout << prettyPrintAddr(a);
            bool success = doPing(ch, masteraddr, a);
            if(success) {
                cout << " XXX";
            } else {
                cout << "  - ";
            }
            cout << "  " << flush;
            //delay(10);
        }
        cout << endl;
    }

    //radio.printPrettyDetails();
    return 0;
}

