#!perl

use File::Spec;
use File::Basename;
use Getopt::Std;

my $opt_q = 0;
my $opt_v = 0;
getopts( 'qv' );
if ( $opt_v )
{
	$opt_q = 0;
}

#
# Perl command to build and install swig generated things
#

our $sScriptPath = File::Spec->rel2abs( File::Spec->canonpath( $0 ) );
our ( $sScript, $sVpcDir ) = fileparse( ${sScriptPath} );
our $pre = "[" . $sScript . "]";

my $srcDir = $ARGV[ 0 ];
my $pyVer = $ARGV[ 1 ];
my $swigFile = $ARGV[ 2 ];
my $outBinDir = $ARGV[ 3 ];
my $swigOutDir = "swig_python${pyVer}";
my $swigC = "${swigFile}_wrap_python${pyVer}.cpp";

my $swig = $srcDir . "\\devtools\\swigwin-1.3.40\\swig.exe";
$swig =~ s:/:\\:g;

if ( ! -x $swig )
{
	$! = 1;
	die( "${pre} ERROR: Can't find swig executable ${swig}" );
}

if ( ! -d ${swigOutDir} )
{
	print( "${pre} mkdir ${swigOutDir}\n" );
	mkdir ${swigOutDir};
}

if ( ! -d ${swigOutDir} )
{
	$! = 1;
	die( "${pre} ERROR: Can't create directory ${swigOutDir}" );
}

if ( ! -d ${outBinDir} )
{
	print( "${pre} mkdir ${outBinDir}\n" );
	mkdir ${outBinDir};
}

if ( ! -d ${outBinDir} )
{
	$! = 1;
	die( "${pre} ERROR: Can't create directory ${swigOutDir}" );
}

if ( -f "${swigOutDir}/${swigC}" )
{
	if ( $opt_v )
	{
		print( "${pre} unlink ${swigOutDir}/${swigC}\n" );
	}
	unlink "${swigOutDir}/${swigC}" || die( "${pre} Can't unlink ${swigOutDir}/${swigC}" );
}

# Warning 383 is: Warning(383): operator++ ignored
# Warning 503 is: Warning(503): Can't wrap 'operator |' unless renamed to a valid identifier.
# We disable these to avoid spamming the console.
my $swigCmd = "${swig} -Fmicrosoft -ignoremissing -w383 -w503 -c++ -Iswig_python${pyVer} -I${srcDir}/public -outdir ${swigOutDir} -o ${swigOutDir}/${swigC} -python ${swigFile}.i";
$swigCmd =~ s:/:\\:g;
if ( !$opt_q )
{
	print( "${pre} $swigCmd\n" );
}
system( ${swigCmd} );

if ( $? )
{
	$! = 1;
	print( "${pre} ERROR: Swig failed\n" );
	exit( 255 );
	die( "${pre} ERROR: Swig failed" );
}

if ( ! -r "${swigOutDir}/${swigFile}.py" )
{
	$! = 1;
	die( "${pre} ERROR: No python code generated from swig" );
}

if ( $opt_v )
{
	print( "${pre} *** Swig Complete!\n" );
}

exit( 0 );
