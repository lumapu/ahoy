
# has a thread for periodic log file writing and some generic file handling
# my_print function write into global variable to be stored in log file

import os
import sys
import time
import datetime
import threading
import binascii


#some global variables
__version_info__ = ('2022', '12', '19')
__version_string__ = 'file handling version: ' + '-'.join(__version_info__)
lck = threading.Lock()                      # used for log_sum writing
log_sum = 'logstart... '
log_stop = True


def get_version():
    return __version_string__

def set_log_stop(_state):
    global log_stop
    log_stop = _state
    
def get_log_stop():
    return log_stop
       

def my_print(out, _local_print=True):
    global lck
    global log_sum
    if (_local_print):
        print(out, end='', flush=True)
    with lck: 
        log_sum += out


# async writing thread of log file data
class AsyncWrite(threading.Thread):
    def __init__(self, _logfile, _debug):
        # calling superclass init
        threading.Thread.__init__(self)
        self.file = _logfile
        self.debug = _debug

    def run(self):
        global lck
        global log_sum
        global log_stop
        
        log_stop = False
        if self.debug: print('\n +++ Thread: file writer started', end='', flush=True)
        while not log_stop:
            with lck:
                if len(log_sum) > 2000:
                    if self.debug: print('\n +++ writing to logfile, ' + str(len(log_sum)) + ' bytes', end='', flush=True)
                    self.file.write(log_sum.replace('\r','').replace('\n\n','\n'))
                    log_sum = ''
                    
            time.sleep(2.0)

        #write remaining data to log
        with lck:
            if self.debug: print('\n +++ writing to logfile remaining, ' + str(len(log_sum)) + ' bytes', end='', flush=True)
            self.file.write(log_sum.replace('\r','').replace('\n\n','\n'))
            log_sum = ''
            self.file.flush()                                   #needed to finally write all data into file after stop
            self.file.close()

        if self.debug: print('\n +++ Thread: file writer stopped and file closed', end='', flush=True)
        

def start_logging_thread_open_file(_dirname, _filename, _mode, _DEBUG):
    global log_stop
    
    #open file
    _logfile = open_file_makedir(_dirname, _filename, _mode, _DEBUG)
    if _DEBUG: my_print('\n +++ generate log file: ' + str(_dirname + '/' + _filename))
    
    #start logging thread
    _tf = AsyncWrite(_logfile, _DEBUG)
    _tf.setDaemon = True
    log_stop = False
    _tf.start()
    return _tf

def stop_logging_thread_close_file(_tf, _DEBUG):
    global log_stop
    log_stop = True
    if _DEBUG: my_print('\n +++ log_stop is: ' + str(log_stop))
    if _tf: _tf.join(2.1)



## extracts the pathname and filename from fiven input string and returns two strings
def get_dir_file_name(_pathfile, _DEBUG):
    _dir_name = './'
    _file_name = ''
    
    if len(_pathfile) > 0:
        #if '/' not in _pathfile or '\\' not in _pathfile:              
            #_pathfile = './' + _pathfile
        _dir_name, _file_name = os.path.split(_pathfile)
        if len(_dir_name) <= 0:
            _dir_name = './'
    else:
        my_print("\n +++ no file name, exit ")
        sys.exit()
    
    if _DEBUG: 
        my_print("\n +++ get_dir_file_name: " + str(_dir_name + ' / ' + _file_name))
    
    return _dir_name,_file_name

    
## handling of file opening for read and write in a given directory, directory will be created if not existing 
## it returns the file connection pointer  
def open_file_makedir(_dir_name, _file_name, _mode, _DEBUG):
    if _DEBUG: 
        my_print("\n +++ open_file_makedir: " + str(_dir_name + ' / ' + _file_name))
    if len(_dir_name) > 0: 
        os.makedirs(_dir_name, exist_ok=True)
    if _DEBUG: 
        my_print("\n +++ open file: " + str(_dir_name + ' / ' + _file_name) + "  mode: " + _mode)
    _file = open(_dir_name + '/' + _file_name, _mode)
    return _file    
 
