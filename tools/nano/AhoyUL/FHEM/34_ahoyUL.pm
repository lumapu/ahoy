
package main;
use strict;
use warnings;
use DevIo;                                              # load DevIo.pm if not already loaded
use Time::HiRes qw(gettimeofday);


my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
my $last_hour=0;

my %sets = (
  "a"	   => ":[0-9]+:", 
  "a"	   => ":[0-9]+:[0-9]{1}:[0-9]{12}:",                   #automode enable and configure
  "c"	     => "[0-9]+", 
	"smac"  => ":ch[0-9]{2}:[a-fA-F0-9]:rx[0-9]{2}:",    				#smac command (automode off)
	"s"      => "",                                             #automode of
	"d"      => "0,1",									                        #query decoding now
  "?"      => "",
);


#my %Inv = { "1141xxxxxxxx" };
my $yield_day = 0;                       #todo make per inverter
my $yield_total = 0;



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
  #my $ret = DevIo_OpenDev($hash, 0, "ahoyUL_Init");
  my $ret = DevIo_OpenDev($hash, 0, undef);
  InternalTimer(gettimeofday()+5, "ahoyUL_Init", $hash); 

  # start periodic reading
  InternalTimer(gettimeofday()+15, "ahoyUL_GetUpdate", $hash); 
  return undef;
}


# will be executed upon successful connection establishment (see DevIo_OpenDev())
sub ahoyUL_Init($)
{
    my ($hash) = @_;
    my $name = $hash->{NAME};
    Log3 $name, 2, "ahoy device Init() called ...";
    # send init to device, here e.g. enable automode to send DevInfoReq (0x15 ... 0x0B ....) every 120sec and enable simple decoding in ahoy-nano   
	  DevIo_SimpleWrite($hash, "a120:\nd1\r\n", 2);
    
    return undef; 
}



# called upon loading the module MY_MODULE
sub ahoyUL_Initialize($)
{
  my ($hash) = @_;
  my $name = $hash->{NAME};

  $hash->{DefFn}    = "ahoyUL_Define";
  $hash->{UndefFn}  = "ahoyUL_Undef";
  $hash->{SetFn}    = "ahoyUL_Set";
  $hash->{ReadFn}   = "ahoyUL_Read";
  $hash->{ReadyFn}  = "ahoyUL_Ready";
  $hash->{ParseFn}  = "ahoyUL_Parse";
  $hash->{ParseFn}  = "ahoyUL_GetUpdate";                             #to be initialized in X_Define with a period
  #$hash->{AttrFn}   = "ahoyUL_Attr";
  $hash->{AttrList}  = "disable:0,1 " .
					   "header " .
                       "inv_polling_sec " .
                       "$readingFnAttributes ";

  Log3 $name, 2, "ahoy_Initialize called";

  return undef;
}


sub ahoyUL_GetUpdate($)
{
	my ($hash) = @_;
	my $name = $hash->{NAME};
	Log3 $name, 4, "ahoy_GetUpdate called ... todo later";

	# neuen Timer starten in einem konfigurierten Interval.
	#InternalTimer(gettimeofday()+$hash->{cmdInterval}, "ahoyUL_GetUpdate", $hash);
  InternalTimer(gettimeofday() + 1800, "ahoyUL_GetUpdate", $hash);

  #todo: call cmd sender method or do it right here
}


# called repeatedly if device disappeared
sub ahoyUL_Ready($)
{
  my ($hash) = @_;
  my $name = $hash->{NAME};
  Log3 $name, 3, "ahoyUL_Ready() called ...";
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

    $last_hour = $hour;
    ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime();
    if($hour < $last_hour) {
        $yield_day = 0;
        $yield_total = 0;
        readingsSingleUpdate($hash, "yield_dc", "day $yield_day Wh  total $yield_total kWh" , 1);
    }#end if

    if($rmsg =~ m/[rR]{1}MAC/) {
        # handle rmac responses
        $hash->{RMAC} = $rmsg;
        $hash->{RMAC_started} = 1;
        $hash->{RMAC_complete} = 0;
  
    } elsif ($hash->{RMAC_started}) {
        $hash->{RMAC} .= $rmsg;
        $hash->{RMAC_started} += 1;
        if($rmsg =~ m/OK|ERR/) {
            #end of rmac detected
            $hash->{RMAC_complete} = 1;
            $hash->{RMAC} =~ s/\r\n//g;
            $hash->{RMAC} .= "\n";

        } elsif ($hash->{RMAC_started} > 10) {
            #stop rmac collection insufficiently if OK missed
            $hash->{RMAC_started} = 0;
            $hash->{RMAC_complete} = 0;
        }
    
    } elsif($rmsg =~ m/payload/) {
        # user payload
        readingsSingleUpdate($hash, "Payload", $rmsg , 1);

    } elsif($rmsg =~ m/(ch0[0-4])/) {                                           # regex match results to $1
        # decoded message from arduino
        readingsSingleUpdate($hash, "dec_$1", $rmsg , 1);
        if ($1 eq "ch00") {
            #end
            readingsSingleUpdate($hash, "yield_dc", "day $yield_day Wh  total $yield_total kWh" , 1);
            $yield_day = 0;
            $yield_total = 0;
        } else {
            $rmsg =~ m/YieldDay: ([0-9\.]+).*YieldTotal: ([0-9\.]+)/;           # regex match results to $1 and $2
            $yield_day += $1;
            $yield_total += $2;
        }
        
    } 

    # do further rmac parsing hereafter
    if ($hash->{RMAC_complete}) {
        readingsSingleUpdate($hash, "MACresp", $hash->{RMAC} , 1);
        $hash->{RMAC_started} = 0;
        $hash->{RMAC_complete} = 0;
        #todo data valid check for OK or ERR
    }

}# end X_Parse


# called if set command is executed
sub ahoyUL_Set($$@)
{
    my ($hash, $name, @params) = @_;
	  #my @a = split("[ \t]+", @params);
    
	  my $cmd = $params[0];

    return "unknown argument $cmd choose one of " . join(" ", sort keys %sets) 
        if(@params < 2);
    
    my $usage = "should not come";

	  # get command overview from ahoy-nano device
    if($cmd eq "?")
    {	
        DevIo_SimpleWrite($hash, "$cmd\n", 2);
    }
    elsif($cmd eq "a")
    {
		    DevIo_SimpleWrite($hash, "a:$params[1]:$params[2]:$params[3]:\n", 2);
    }
    elsif($cmd eq "c")
    {
        DevIo_SimpleWrite($hash, "c$params[1]:\n", 2);
    }
	  elsif($cmd eq "d")
    {
        DevIo_SimpleWrite($hash, "d$params[1]:\n", 2);
    }
	  elsif($cmd eq "s")
    {
        DevIo_SimpleWrite($hash, "s\n", 2);
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


# default sub to set and del attributes, maybe used later
sub ahoyUL_Attr($$$$)
{
	my ( $cmd, $name, $aName, $aValue ) = @_;
    
  	# $cmd  - Vorgangsart - kann die Werte "del" (löschen) oder "set" (setzen) annehmen
	# $name - Gerätename
	# $aName/$aValue sind Attribut-Name und Attribut-Wert
    
	if ($cmd eq "set") {
		if ($aName eq "Regex") {
			eval { qr/$aValue/ };
			if ($@) {
				Log3 $name, 3, "X ($name) - Invalid regex in attr $name $aName $aValue: $@";
				return "Invalid Regex $aValue: $@";
			}
		}
	}
	return undef;
}

1;
