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
#include <unistd.h>    // usleep()

using namespace std;

#include "common.hpp"

// Generic:
RF24 radio(22, 0, 1000000);
// See http://nRF24.github.io/RF24/pages.html for more information on usage


/** Receive forever
 */
void receiveForever(int ch, string myaddr)
{
    uint8_t buf[30];

    radio.setChannel(ch);
    radio.enableDynamicPayloads();
    radio.setAutoAck(true);

    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_250KBPS);
    radio.openWritingPipe((const uint8_t *)"dummy");
    radio.flush_rx();
    radio.flush_tx();
    radio.openReadingPipe(1, (const uint8_t *)myaddr.c_str());
    radio.startListening();
    radio.printPrettyDetails();
 
    cout << endl << "I'm listening..." << endl;

    while (true)
    {
        uint8_t pipe;
	usleep(500000);
	if (radio.failureDetected) {
		cout << "!f! " << flush;
	}
	if (radio.rxFifoFull()) {
		cout << "!F! " << flush;
	}
        if (radio.available(&pipe)) 
        {
            uint8_t bytes = radio.getDynamicPayloadSize();          // get the size of the payload
            cout << "I was notified of having received " << dec << (unsigned int)bytes;
            cout << " bytes on pipe " << (unsigned int)pipe << ": " << flush;
            radio.read(buf, bytes);                     // fetch payload from FIFO
            for(int i=0; i<bytes; i++)
	    {
		cout << " " << hex << setfill('0') << setw(2) << (int)buf[i];
	    }
	    cout << " '";
            for(int i=0; i<bytes; i++)
	    {
		cout << buf[i];
	    }
	    cout << "'" << endl;
            //radio.printPrettyDetails();
       }
    }
}


int main(int argc, char** argv) 
{
    delay(200);
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
    // - do we need/want "custom ack payloads"?
    // - use isAckPayloadAvailable() once we've actually contacted an inverter successfully!

    string addr = serno2shockburstaddrbytes(114174608177);
    receiveForever(9, "2Node");

    return 0;
}

