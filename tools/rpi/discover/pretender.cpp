/* based on "gettingstarted.cpp" by 2bdy5 */

/**
 * Behave like we expect a Hoymiles microinverter to behave.
 */
#include <ctime>       // time()
#include <iostream>    // cin, cout, endl
#include <iomanip>
#include <string>      // string, getline()
#include <vector>
#include <sstream>
#include <time.h>      // CLOCK_MONOTONIC_RAW, timespec, clock_gettime()
#include <RF24/RF24.h> // RF24, RF24_PA_LOW, delay()

using namespace std;

#include "common.hpp"

// Generic:
RF24 radio(22, 0);
// See http://nRF24.github.io/RF24/pages.html for more information on usage


/** Receive forever
 */
void receiveForever(int ch, string myaddr)
{
    uint8_t buf[30];

    radio.enableDynamicPayloads();
    radio.setChannel(ch);

    radio.setPALevel(RF24_PA_MIN); // RF24_PA_MAX is default.
    radio.setDataRate(RF24_250KBPS);

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(1, (const uint8_t *)myaddr.c_str());

    while (true)
    {
        uint8_t pipe;
        if (radio.available(&pipe)) 
        {
            uint8_t bytes = radio.getPayloadSize();          // get the size of the payload
            cout << "I was notified of having received " << (unsigned int)bytes;
            cout << " bytes on pipe " << (unsigned int)pipe << flush;
            radio.read(buf, bytes);                     // fetch payload from FIFO
            //cout << ": " << payload;                 // print the payload's value
            //cout << " hex: " << hex << (unsigned int)b[0] << " " << (unsigned int)b[1] << " " 
            //     << (unsigned int)b[2] << " " << (unsigned int)b[3] << " " <<endl;
        }
    }
}


int main(int argc, char** argv) 
{
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
    // - 8-bit crc
    // - dynamic payloads (check in rf logs)
    // - what's the "primary mode"?
    // - do we need/want "custom ack payloads"?
    // - use isAckPayloadAvailable() once we've actually contacted an inverter successfully!

    radio.printPrettyDetails();

    string addr = serno2shockburstaddrbytes(114174608177);
    receiveForever(41, addr);

    return 0;
}

