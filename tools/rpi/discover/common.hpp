#include <string>
#include <stdint.h>

using namespace std;

/** Convert given 5-byte address to human readable hex string */
string prettyPrintAddr(string &a);


/** Convert a Hoymiles inverter/DTU serial number into its
 * corresponding NRF24 address byte sequence (5 bytes).
 *
 * The inverters use a BCD representation of the last 8
 * digits of their serial number, in reverse byte order, 
 * followed by \x01.
 */
string serno2shockburstaddrbytes(uint64_t n);

