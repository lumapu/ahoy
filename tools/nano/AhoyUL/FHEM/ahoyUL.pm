
package main;

use strict;
use warnings;
use DevIo; # load DevIo.pm if not already loaded

# called upon loading the module MY_MODULE
sub AHOYUL_Initialize($)
{
  my ($hash) = @_;

  $hash->{DefFn}    = "AHOYUL_Define";
  $hash->{UndefFn}  = "AHOYUL_Undef";
  $hash->{SetFn}    = "AHOYUL_Set";
  $hash->{ReadFn}   = "AHOYUL_Read";
  $hash->{ReadyFn}  = "AHOYUL_Ready";
  
  $hash->{ParseFn}  = "AHOYUL_Parse";
  
}

# called when a new definition is created (by hand or from configuration read on FHEM startup)
sub AHOYUL_Define($$)
{
  my ($hash, $def) = @_;
  my @a = split("[ \t]+", $def);

  my $name = $a[0];
  
  # $a[1] is always equals the module name "MY_MODULE"
  
  # first argument is a serial device (e.g. "/dev/ttyUSB0@57600,8,N,1")
  my $dev = $a[2]; 

  return "no device given" unless($dev);
  
  # close connection if maybe open (on definition modify)
  DevIo_CloseDev($hash) if(DevIo_IsOpen($hash));  

  # add a default baud rate (9600), if not given by user
  $dev .= '@57600,8,N,1' if(not $dev =~ m/\@\d+$/);
  
  # set the device to open
  $hash->{DeviceName} = $dev;
  
  # open connection with custom init function
  my $ret = DevIo_OpenDev($hash, 0, "AHOYUL_Init"); 
 
  return undef;
}

# called when definition is undefined 
# (config reload, shutdown or delete of definition)
sub AHOYUL_Undef($$)
{
  my ($hash, $name) = @_;
 
  # close the connection 
  DevIo_CloseDev($hash);
  
  return undef;
}

# called repeatedly if device disappeared
sub AHOYUL_Ready($)
{
  my ($hash) = @_;
  
  # try to reopen the connection in case the connection is lost
  return DevIo_OpenDev($hash, 1, "AHOYUL_Init"); 
}

# called when data was received
sub AHOYUL_Read($)
{
  my ($hash) = @_;
  my $name = $hash->{NAME};
  
  # read the available data
  my $buf = DevIo_SimpleRead($hash);
  
  # stop processing if no data is available (device disconnected)
  return if(!defined($buf));
  
  Log3 $name, 5, "AHOYUL ($name) - received: $buf"; 
  
  #
  # do something with $buf, e.g. generate readings, send answers via DevIo_SimpleWrite(), ...
  #
   
}

# called if set command is executed
sub AHOYUL_Set($$@)
{
    my ($hash, $name, $params) = @_;
	my @a = split("[ \t]+", $params);
	$cmd = $params[0]

    my $usage = "unknown argument $cmd, choose one of statusRequest:noArg on:noArg off:noArg";

	# get command overview from ahoy-nano device
    if($cmd eq "?")
    {	
		#todo
        DevIo_SimpleWrite($hash, "?\r\n", 2);
    }
    elsif($cmd eq "a")
    {
        #todo handle automode and send command to ahoy-nano via cmd a[[:<period_sec>]:<12 digit inverter id>:]
		DevIo_SimpleWrite($hash, "a:{$params[1]}:{$params[2]}:\r\n", 2);
    }
    elsif($cmd eq "c")
    {
        #todo
		#DevIo_SimpleWrite($hash, "off\r\n", 2);
    }
	elsif($cmd eq "d")
    {
        #todo
		#DevIo_SimpleWrite($hash, "off\r\n", 2);
    }
	elsif($cmd eq "i")
    {
        #todo
		#DevIo_SimpleWrite($hash, "off\r\n", 2);
    }
	elsif($cmd eq "s")
    {
        #todo
		#DevIo_SimpleWrite($hash, "off\r\n", 2);
    }
    else
    {
        return $usage;
    }
}
    
# will be executed upon successful connection establishment (see DevIo_OpenDev())
sub AHOYUL_Init($)
{
    my ($hash) = @_;

    # send init to device, here e.g. enable automode to send DevInfoReq (0x15 ... 0x0B ....) every 120sec and enable simple decoding in ahoy-nano   
	DevIo_SimpleWrite($hash, "a120:::::d1:\r\n", 2);
    
    return undef; 
}

1;