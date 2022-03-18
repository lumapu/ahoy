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

using namespace std;

// Generic:
RF24 radio(22, 0);
/****************** Linux (BBB,x86,etc) ***********************/
// See http://nRF24.github.io/RF24/pages.html for more information on usage
// See http://iotdk.intel.com/docs/master/mraa/ for more information on MRAA
// See https://www.kernel.org/doc/Documentation/spi/spidev for more information on SPIDEV

// For this example, we'll be using a payload containing
// a single float number that will be incremented
// on every successful transmission
static union {
float payload = 0.0;
uint8_t b[4];
};

void setRole(); // prototype to set the node's role
void master();  // prototype of the TX node's behavior
void slave();   // prototype of the RX node's behavior

// custom defined timer for evaluating transmission time in microseconds
struct timespec startTimer, endTimer;
uint32_t getMicros(); // prototype to get ellapsed time in microseconds


/** Convert given 5-byte address to human readable hex string */
string prettyPrintAddr(string &a)
{
    ostringstream o;
    o << hex << setw(2)
      << setfill('0') << setw(2) << int(a[0]) 
      << ":" << setw(2) << int(a[1])
      << ":" << setw(2) << int(a[2])
      << ":" << setw(2) << int(a[3])  
      << ":" << setw(2) << int(a[4]) << dec;
    return o.str(); 
}


/** Convert a hoymiles inverter/DTU serial number into its
 * corresponding NRF24 address byte sequence (5 bytes).
 *
 * The inverters use a BCD representation of the last 8
 * digits of the serial number, in reverse byte order, 
 * followed by a \x01.
 */
string serno2shockburstaddrbytes(uint64_t n)
{
    char b[5];
    b[3] =       (((n/10)%10) << 4) |       ((n/1)%10);
    b[2] =     (((n/1000)%10) << 4) |     ((n/100)%10);
    b[1] =   (((n/100000)%10) << 4) |   ((n/10000)%10);
    b[0] = (((n/10000000)%10) << 4) | ((n/1000000)%10);
    b[4] = 0x01;

    string s = string(b, sizeof(b));

    cout << dec << "ser# " << n << " --> addr "
         << prettyPrintAddr(s) << endl;
    return s;
}


/** Ping the given address.
 * @returns true if we received a reply, otherwise false.
 */
bool doPing(int ch, string src, string dst)
{
//    radio.setPayloadSize(sizeof(payload)); // float datatype occupies 4 bytes
    radio.setPayloadSize(4); // float datatype occupies 4 bytes
    radio.setChannel(ch);

    radio.setPALevel(RF24_PA_MIN); // RF24_PA_MAX is default.
    radio.setDataRate(RF24_250KBPS);

    // set the TX address of the RX node into the TX pipe
    radio.openWritingPipe((const uint8_t *)dst.c_str());

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(1, (const uint8_t *)src.c_str());

    radio.stopListening();                                          // put radio in TX mode

    clock_gettime(CLOCK_MONOTONIC_RAW, &startTimer);            // start the timer
 //   bool report = radio.write(&payload, sizeof(float));         // transmit & save the report
    bool report = radio.write(&payload, 4);         // transmit & save the report
    uint32_t timerEllapsed = getMicros();                       // end the timer

    if (report) {
        // payload was delivered
        payload += 0.01;                                        // increment float payload
        return true;
    }
    return false;  // no reply received
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

    // well-known valid DTU serial number
    // just in case the inverter only responds to addresses
    // that fulfil certain requirements.
    string masteraddr = serno2shockburstaddrbytes(99912345678);

    // serial numbers of all inverters that we are trying to find
    vector<string> dstaddrs;
    dstaddrs.push_back(string("1Node"));
    dstaddrs.push_back(string("2Node"));
    dstaddrs.push_back(serno2shockburstaddrbytes(114174608145));
    dstaddrs.push_back(serno2shockburstaddrbytes(114174608177));

    // channels that we will scan
    vector<int> channels{1, 3, 6, 9, 11, 23, 40, 61, 75, 76, 99};

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
            delay(20);
        }
        cout << endl;
    }

    radio.setChannel(76);

    // to use different addresses on a pair of radios, we need a variable to
    // uniquely identify which address this radio will use to transmit
    bool radioNumber = 1; // 0 uses address[0] to transmit, 1 uses address[1] to transmit

    // print example's name
    cout << argv[0] << endl;

    // Let these addresses be used for the pair
    uint8_t address[2][6] = {"1Node", "2Node"};
    // It is very helpful to think of an address as a path instead of as
    // an identifying device destination

    // Set the radioNumber via the terminal on startup
    cout << "Which radio is this? Enter '0' or '1'. Defaults to '0' ";
    string input;
    getline(cin, input);
    radioNumber = input.length() > 0 && (uint8_t)input[0] == 49;

    // save on transmission time by setting the radio to only transmit the
    // number of bytes we need to transmit a float
    radio.setPayloadSize(sizeof(payload)); // float datatype occupies 4 bytes

    // Set the PA Level low to try preventing power supply related problems
    // because these examples are likely run with nodes in close proximity to
    // each other.
    radio.setPALevel(RF24_PA_MIN); // RF24_PA_MAX is default.
    radio.setDataRate(RF24_250KBPS);

radio.printPrettyDetails();

    // set the TX address of the RX node into the TX pipe
    radio.openWritingPipe(address[radioNumber]);     // always uses pipe 0

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(1, address[!radioNumber]); // using pipe 1

    // For debugging info
    // radio.printDetails();       // (smaller) function that prints raw register values
    // radio.printPrettyDetails(); // (larger) function that prints human readable data

    // ready to execute program now
    setRole(); // calls master() or slave() based on user input
    return 0;
}


/**
 * set this node's role from stdin stream.
 * this only considers the first char as input.
 */
void setRole() {
    string input = "";
    while (!input.length()) {
        cout << "*** PRESS 'T' to begin transmitting to the other node\n";
        cout << "*** PRESS 'R' to begin receiving from the other node\n";
        cout << "*** PRESS 'Q' to exit" << endl;
        getline(cin, input);
        if (input.length() >= 1) {
            if (input[0] == 'T' || input[0] == 't')
                master();
            else if (input[0] == 'R' || input[0] == 'r')
                slave();
            else if (input[0] == 'Q' || input[0] == 'q')
                break;
            else
                cout << input[0] << " is an invalid input. Please try again." << endl;
        }
        input = ""; // stay in the while loop
    } // while
} // setRole()


/**
 * make this node act as the transmitter
 */
void master() {
    radio.stopListening();                                          // put radio in TX mode

    unsigned int failure = 0;                                       // keep track of failures
    while (failure < 60) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &startTimer);            // start the timer
        bool report = radio.write(&payload, sizeof(float));         // transmit & save the report
        uint32_t timerEllapsed = getMicros();                       // end the timer

        if (report) {
            // payload was delivered
            cout << "Transmission successful! Time to transmit = ";
            cout << timerEllapsed;                                  // print the timer result
            cout << " us. Sent: " << payload;               // print payload sent
            cout << " hex: " << hex << (unsigned int)b[0] << " " << (unsigned int)b[1] << " " 
                 << (unsigned int)b[2] << " " << (unsigned int)b[3] << " " <<endl;
            payload += 0.01;                                        // increment float payload

        } else {
            // payload was not delivered
            cout << "Transmission failed or timed out" << endl;
            failure++;
        }

        // to make this example readable in the terminal
        delay(1000);  // slow transmissions down by 1 second
    }
    cout << failure << " failures detected. Leaving TX role." << endl;
}

/**
 * make this node act as the receiver
 */
void slave() {

    radio.startListening();                                  // put radio in RX mode

    time_t startTimer = time(nullptr);                       // start a timer
    while (time(nullptr) - startTimer < 60) {                 // use 6 second timeout
        uint8_t pipe;
        if (radio.available(&pipe)) {                        // is there a payload? get the pipe number that recieved it
            uint8_t bytes = radio.getPayloadSize();          // get the size of the payload
            radio.read(&payload, bytes);                     // fetch payload from FIFO
            cout << "Received " << (unsigned int)bytes;      // print the size of the payload
            cout << " bytes on pipe " << (unsigned int)pipe; // print the pipe number
            cout << ": " << payload;                 // print the payload's value
            cout << " hex: " << hex << (unsigned int)b[0] << " " << (unsigned int)b[1] << " " 
                 << (unsigned int)b[2] << " " << (unsigned int)b[3] << " " <<endl;
            startTimer = time(nullptr);                      // reset timer
        }
    }
    cout << "Nothing received in 6 seconds. Leaving RX role." << endl;
    radio.stopListening();
}


/**
 * Calculate the ellapsed time in microseconds
 */
uint32_t getMicros() {
    // this function assumes that the timer was started using
    // `clock_gettime(CLOCK_MONOTONIC_RAW, &startTimer);`

    clock_gettime(CLOCK_MONOTONIC_RAW, &endTimer);
    uint32_t seconds = endTimer.tv_sec - startTimer.tv_sec;
    uint32_t useconds = (endTimer.tv_nsec - startTimer.tv_nsec) / 1000;

    return ((seconds) * 1000 + useconds) + 0.5;
}
