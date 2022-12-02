#works for python 3.7.3

import serial
import os
import argparse
import time
import datetime
import binascii
import threading
import sys
from binascii import hexlify
# my own libs
import _libs2.com_handling as cph
import _libs2.file_handling as fh


# 2022-02-11: init mb


#some global variables
__version_info__ = ('2022', '11', '13')
__version__ = 'app version: ' + '-'.join(__version_info__)
DEBUG = False



############################################################################################################################
# here the program starts
def main():
    global DEBUG
    global com_stop
    ret = 0
    err = 0
    
    starttime = cph.millis_since(0)
    parser = argparse.ArgumentParser(description='simple Hoymiles Terminal')
    parser.add_argument('-p', type=str, default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('-b', type=int, default=57600, help='baudrate of the rf-module')
    parser.add_argument('-d', default=False, help='use parameter to print additional debug info', action='store_true')
    parser.add_argument('-v', '--version', action='version', version="%(prog)s (" + __version__ + ")")
    parser.add_argument('-l', type=str, default='', help='log-file path/name')
    parser.add_argument('-i', default=True, help='have a simple AT-command line', action='store_true')

    args = parser.parse_args()
    DEBUG = args.d
    #used later for Threads
    tp = None
    tf = None
    com = None
    logfile = None

    
    #shows the used lib versions
    fh.my_print('\n +++ starting AhoyUL Terminal ...')
    fh.my_print('\n +++ %40s' % (__version__))
    fh.my_print('\n +++ %40s' % (cph.__version_string__))
    fh.my_print('\n +++ %40s' % (fh.__version_string__))
    
    if DEBUG: fh.my_print('\n' + str(args))
    
    #output file logging
    com_port_name = (args.p).replace('/dev/','_').replace('/','_')    #for Linux, no matter on Windows
    if len(args.l) > 0:
        tmp1, tmp2 = fh.get_dir_file_name(args.l, DEBUG)
        logfile_name = cph.timestamp(3) + tmp2
        tf = fh.start_logging_thread_open_file(tmp1, logfile_name, 'w+', DEBUG)
    else:
        # default path of logfile, always logging
        tmp1, tmp2 = fh.get_dir_file_name('log/default_{0:s}.log'.format(com_port_name), DEBUG)
        logfile_name = cph.timestamp(3) + tmp2
        tf = fh.start_logging_thread_open_file(tmp1, logfile_name, 'w+', DEBUG)

    com = serial.Serial(args.p, args.b, timeout=0.2, rtscts=False, dsrdtr=False)      
    com.rts = True
    com.dtr = True     
    fh.my_print("\n +++ serial port is open: %s, baud: %d, rtscts: %s, rts: %s" % (com.portstr,com.baudrate,com.rtscts,com.rts))
    
    
    #start urc com logging thread
    tp = cph.openCom_startComReadThread(tp, com, DEBUG)

    try:     
        # do init AT here

        
        #start small AT-command terminal
        if args.i == True:
            result = cph.simple_terminal(com, DEBUG)
            if result == False: 
                starttime_sec = cph.millis_since(0) / 1000
                return -1
        

        # todo: do some automated scripting and data sending to the AhoyUL board

        
        #wait remeining data
        TO_sec = 5
        fh.my_print('\n +++ wait remaining data for %d sec' % (TO_sec,))
        time.sleep(TO_sec)
   
    except KeyboardInterrupt:
    	fh.my_print('\n +++ keyboard interrupt, end ...')

    finally:
        fh.my_print('\n +++ end, wait until finshed\n')
        
        #cleanup        
        #stop comport reader thread
        if tp: cph.stopComReadThread_closeCom(tp, com)

        # if logging to file was enabled
        #if len(args.l) > 0:
        if tf: fh.stop_logging_thread_close_file(tf, DEBUG)


if __name__ == "__main__":
    main()
