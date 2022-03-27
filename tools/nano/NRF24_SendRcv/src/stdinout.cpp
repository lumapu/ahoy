#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include <stdio.h>
#include "stdinout.h"

// Function that printf and related will use to print
static int serial_putchar(char c, FILE *f)
{
        if(c == '\n') {
                serial_putchar('\r', f);
        }

        return Serial.write(c) == 1 ? 0 : 1;
}
// Function that scanf and related will use to read
static int serial_getchar(FILE *)
{
        // Wait until character is avilable
        while(Serial.available() <= 0) { ; }
        int ch = Serial.read();
        Serial.write(ch);
        return ch;
}

static FILE serial_stdinout;

static void setup_stdin_stdout()
{
        // Set up stdout and stdin
        fdev_setup_stream(&serial_stdinout, serial_putchar, serial_getchar, _FDEV_SETUP_RW);
        stdout = &serial_stdinout;
        stdin  = &serial_stdinout;
        stderr = &serial_stdinout;
}

// Initialize the static variable to 0
size_t initializeSTDINOUT::initnum = 0;

// Constructor that calls the function to set up stdin and stdout
initializeSTDINOUT::initializeSTDINOUT()
{
        if(initnum++ == 0) {
                setup_stdin_stdout();
        }
}