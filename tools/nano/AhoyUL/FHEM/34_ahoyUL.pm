
package main;

use strict;
use warnings;
use DevIo;                                              # load DevIo.pm if not already loaded

# called upon loading the module MY_MODULE
sub AHOYUL_Initialize($)
{
  my ($hash) = @_;

  $hash->{DefFn}    = "ahoyUL_Define";
  $hash->{UndefFn}  = "ahoyUL_Undef";
  $hash->{SetFn}    = "ahoyUL_Set";
  $hash->{ReadFn}   = "ahoyUL_Read";
  $hash->{ReadyFn}  = "ahoyUL_Ready";
  
  $hash->{ParseFn}  = "ahoyUL_Parse";
  
}

# called when a new definition is created (by hand or from configuration read on FHEM startup)
sub ahoyUL_Define($$)
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
  my $ret = DevIo_OpenDev($hash, 0, "ahoyUL_Init"); 
 
  return undef;
}

# called when definition is undefined 
# (config reload, shutdown or delete of definition)
sub ahoyUL_Undef($$)
{
  my ($hash, $name) = @_;
 
  # close the connection 
  DevIo_CloseDev($hash);
  
  return undef;
}

# called repeatedly if device disappeared
sub ahoyUL_Ready($)
{
  my ($hash) = @_;
  
  # try to reopen the connection in case the connection is lost
  return DevIo_OpenDev($hash, 1, "ahoyUL_Init"); 
}

# called when data was received
sub ahoyUL_Read($)
{
  my ($hash) = @_;
  my $name = $hash->{NAME};
  
  # read the available data
  my $buf = DevIo_SimpleRead($hash);
  # stop processing if no data is available (device disconnected)
  return if(!defined($buf));
  #Log3 $name, 5, "ahoyUL ($name) - received: $buf"; 

	my $pandata = $hash->{PARTIAL};
	Log3 $name, 5, "ahoyUL/RAW: $pandata + $buf";
	$pandata .= $buf;

	while ( $pandata =~ m/\n/ ) {											        # while-loop as long as "\n" in $pandata
		my $rmsg;
		( $rmsg, $pandata ) = split( "\n", $pandata, 2 );
		$rmsg =~ s/\r//;													              # substitution, replace "\r" by nothing
		ahoyUL_Parse( $hash, $hash, $name, $rmsg ) if ($rmsg);
	}
	$hash->{PARTIAL} = $pandata; 
}


# called when one line of data was received
sub ahoyUL_Parse($$$$)
{
  my ( $hash, $iohash, $name, $rmsg) = @_;
  Log3 $name, 3, "ahoyUL: $rmsg";

  if($rmsg =~ m/rMAC/) {
    # handle rmac responses


  } elsif($rmsg =~ m/payload/) {
    # payload


  } elsif($rmsg =~ m/ch00/) {
    # AC channel
    readingsBeginUpdate($hash);
		readingsBulkUpdateIfChanged($hash, "myPV1", $rmsg , 1);
		readingsEndUpdate($hash, 1);
  
  } elsif($rmsg =~ m/ch0[1-4]/) {
    # one DC channel
    readingsBeginUpdate($hash);
		readingsBulkUpdateIfChanged($hash, "myPV1", $rmsg , 1);
		readingsEndUpdate($hash, 1);
  }

}



# called if set command is executed
sub ahoyUL_Set($$@)
{
    my ($hash, $name, @params) = @_;
	  #my @a = split("[ \t]+", @params);
    
    return "ahoyul_set needs at least a command" if(@params < 1);
	  my $cmd = $params[0];
    
    my $usage = "unknown argument $cmd, choose one of a:c:d:iadd:idel:ilst:sMAC:?";

	  # get command overview from ahoy-nano device
    if($cmd eq "?")
    {	
        DevIo_SimpleWrite($hash, "$cmd\r\n", 2);
    }
    elsif($cmd eq "a")
    {
		    DevIo_SimpleWrite($hash, "a:{$params[1]}:{$params[2]}:{$params[3]}:\r\n", 2);
    }
    elsif($cmd eq "c")
    {
        DevIo_SimpleWrite($hash, "c:{$params[1]}:\r\n", 2);
    }
	  elsif($cmd eq "d")
    {
        DevIo_SimpleWrite($hash, "d:{$params[1]}:\r\n", 2);
    }
	  elsif($cmd eq "i")
    {
        #todo
		    #DevIo_SimpleWrite($hash, "off\r\n", 2);
    }
	  elsif($cmd eq "sMAC")
    {
        #todo
		    DevIo_SimpleWrite($hash, "$cmd\r\n", 2);
    }
    else
    {
        return $usage;
    }
}
    
# will be executed upon successful connection establishment (see DevIo_OpenDev())
sub ahoyUL_Init($)
{
    my ($hash) = @_;

    # send init to device, here e.g. enable automode to send DevInfoReq (0x15 ... 0x0B ....) every 120sec and enable simple decoding in ahoy-nano   
	  DevIo_SimpleWrite($hash, "a120:::::d1:\r\n", 2);
    
    return undef; 
}

1;