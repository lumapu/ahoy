#include "common.hpp"

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace std;

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


/** Convert a Hoymiles inverter/DTU serial number into its
 * corresponding NRF24 address byte sequence (5 bytes).
 *
 * The inverters use a BCD representation of the last 8
 * digits of their serial number, in reverse byte order, 
 * followed by \x01.
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

