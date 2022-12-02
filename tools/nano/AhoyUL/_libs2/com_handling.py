
#handles at-commands send and response evaluation, a thread can catch the URCs of the module 

import os
import sys
import time
import datetime
import threading
import binascii
import serial.tools.list_ports
import _libs2.file_handling as fh


#some global variables
__version_info__ = ('2022', '11', '13')
__version_string__ = '%40s' % ('hm inverter handling version: ' + '-'.join(__version_info__))
lck2 = threading.Lock()                     # used for AT-commands to access serial port
com_stop = True
promt = '/> '
respo = '<< '
sendi = '>> '


def get_version():
    return __hm_handling_version__
    

# async reading thread of comport data (e.g. for URCs)
class AsyncComRead(threading.Thread):
    def __init__(self, _com, _addTimestamp, _debug):
        # calling superclass init
        threading.Thread.__init__(self)
        self.com = _com
        self.addTS = _addTimestamp
        self.debug = _debug
        self.data = ''

    def run(self):
        global com_stop
        if self.debug: print('\n +++ Thread: com reader started', end='', flush=True)
        urc_millis = millis_since(0)
        urc_ln = True
        while not com_stop:
            with lck2:
                urc = []
                while self.com.in_waiting:
                    line = self.com.readline().decode('ascii','ignore')
                    line = line.replace('\n','').replace('\r','')
                    if len(line) > 2:                                                           # filter out empty lines
                        if self.addTS:
                            urc.append('\n%s%s%s' % (timestamp(0), respo, line))
                        else:
                            urc.append('\n%s%s' % (respo, line))
                    
                    #todo: return values into callback function
                    
                    if not self.com.is_open or com_stop: break
            if urc:
                fh.my_print('\n%s' % ( (' '.join(urc)).replace('\n ', '\n') ))
                urc_millis = millis_since(0)
                urc_ln = False
            
            if (not urc_ln) and (millis_since(urc_millis) >= 3000):
                urc_ln = True
                print('\n%s' % (promt,), end = '')
            
            time.sleep(0.1)
        if self.debug: print('\n +++ Thread: com reader stopped', end='', flush=True)
    
    
#def parse_payload(_self, _data):
        

# opens the com-port and starts the Async com reading thread
def openCom_startComReadThread(_tp, _com, _debug):
    global com_stop
    #start urc com logging thread
    if not _com.is_open: _com.open()
    _tp = AsyncComRead(_com, True, _debug)
    _tp.setDaemon = True
    com_stop = False
    _tp.start()
    return _tp


# stops the com-reading thread and closes com-port
def stopComReadThread_closeCom(_tp, _com):
    global com_stop
    com_stop = True
    _tp.join(1.1)
    if _com.is_open: _com.close()
    return com_stop


# opens com-port if the given port-string exists and starts the Async com-port reading thread
def check_port_and_openCom(_tp, _com, _portstr, MATCH_sec, TIMEOUT_sec, _DEBUG):
    if (check_wait_port(_portstr, MATCH_sec, TIMEOUT_sec, _DEBUG) == 0):
        fh.my_print('\n +++  Port enumerated and detected: ' + _portstr.lower())
        # reopen port
        _tp = openCom_startComReadThread(_tp, _com, _DEBUG)
    else:
        fh.my_print('\n +++  No USB com-port detected again --> end')
        _tp = None

    return _tp



# simply list the available com-ports of the system 
def list_ports():
    portlist = serial.tools.list_ports.comports()
    fh.my_print('\n +++  detected ports:')
    for element in portlist:
        fh.my_print('   %s' % (str(element.device)))    

 


## simple Terminal to enter and display cmds, resonses are read by urc thread
def simple_terminal(_com, _debug):
    next = True
    while(next):
        time.sleep(1.0)
        _cmd_in = input('\n%s'%(promt,))
        fh.my_print('\n%s%s' % (promt, _cmd_in, ))
        
        if "start" in _cmd_in:
            #end terminal and return true
            return True
        elif "kill" in _cmd_in:
            #ends terminal and returns false
            return False
        elif '_time' in _cmd_in or '_tnow' in _cmd_in:
            _uartstr = 't{:d}'.format(int(time.time()))
            _com.write( _uartstr.encode('ascii') )
            fh.my_print('\n%s%s%s' % (timestamp(0), sendi, _uartstr, ))
            continue
        
        _uartstr = _cmd_in.encode('ascii') 
        _com.write( ('%s%s' % (sendi, _uartstr, )).encode('ascii') )
        fh.my_print('\n%s%s%s' % (timestamp(0), sendi, _uartstr, ))
    
    return True



# calculates the time in milli-seconds from a given start time 
def millis_since(thisstart):
    return int(round(time.time() * 1000) - thisstart)
    
        
# different timestamp formats
def timestamp(format):
    if(format==0):
        return ('%s: ' % (datetime.datetime.now().strftime("%Y-%m-%d_%H:%M:%S.%f")[:-3]))
    elif format==1:
        return str(time.time()) + ": "
    elif format==2:
        #for file name extention
        return datetime.datetime.now().strftime("%Y-%m-%d_%H%M_")
    elif format==3:
        #for file name extention with seconds
        return datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S_")
    elif format==4:
        return datetime.datetime.now().strftime("%H%M%S.%f")
    else:
        return datetime.datetime.now().strftime("%y/%m/%d,%H:%M:%S+01")

