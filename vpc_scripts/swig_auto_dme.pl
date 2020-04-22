#!perl
#
# Runs swig to compute dependencies and update a .dep file
#
# Expects to be in $SRCDIR/vpc_scripts/swig_auto_dme.pl
#
# As paths are computed relative to the script location
#

use strict;

use File::Basename;
use File::Spec;
use File::Copy;
use File::Path;
use Cwd;
use Getopt::Std;
use File::Path;


our $sScriptPath = File::Spec->rel2abs( File::Spec->canonpath( $0 ) );
our ( $sScript, $sVpcDir ) = fileparse( ${sScriptPath} );
our $sPre = "[" . $sScript . "]";
our $srcDir = File::Spec->catdir( $sVpcDir, '..' );

our $opt_c = 0;
our $opt_h = 0;
our $opt_f = 0;
our $opt_q = 0;
our $opt_v = 0;
getopts( 'cfhqv' );

our $swigOutDir = shift( @ARGV );

main();

#
#
#
sub main
{
	if ( $opt_h )
	{
		print <<"EOF";
${sScript}(1)

NAME
    ${sScript} - Updates swig auto_<elementlib>.i files
	             auto_<elementlib>.i files are files generated from datamodel
				 elementlib.cpp files.  The macro USING_ELEMENT_FACTORY is
				 used to define various swig bindings for all of the elements
				 in the src/public/<elementlib>/<elementlib>.cpp file

SYNOPSYS
    ${srcDir}/devtools/runperl ${sScriptPath} [ -h ] | [ -c ] | < [ -f ] [ -q ] [ -v ] out_sub_dir swigfile >

OPTIONS
    -h  Print this message

    -c  Clean SWIG files from projects

    -f  Force update of projects

    -q  Produce no output (unless -v is also specified)

    -v  Produce more output (overrides -q)

    swigfile

EOF
	
	exit 0;
	}

	if ( $#ARGV < 0 )
	{
		die( "No swigfile specified\n" );
	}

	my $swigFile = shift( @ARGV );

	if ( ! -d $srcDir )
	{
		die( "Can't Find src directory: ${srcDir}\n" );
	}

	if ( $opt_v )
	{
		print "${sPre} * Script:      " . $sScriptPath . "\n";
		print "${sPre} * vpc_scripts: " . $sVpcDir . "\n";
		print "${sPre} * SRC:         " . $srcDir . "\n";
		print "${sPre} * out:         " . $swigOutDir . "\n";
		print "${sPre} * swigfile:    " . $swigFile . "\n";
	}

	if ( $opt_c )
	{
		Clean( $swigOutDir );
	}
	else
	{
		ComputeAutoDme( $swigOutDir, $swigFile );
	}
}


#
# Cleans up
#
sub Clean
{
	my $dir = shift( @_ );

	if ( -d $dir )
	{
		if ( !$opt_q || $opt_v )
		{
			print( "${sPre} rmtree " . $dir . "\n" );
		}

		rmtree( $dir );
	}
}


#
# Creates the output directory if necessary
#
sub CreateOutDir
{
	my $dir = shift( @_ );

	if ( ! -d $dir )
	{
		print( "${sPre} mkdir ${dir}\n" );
		mkpath( $dir );
	}

	if ( ! -d $dir )
	{
		die( "${sPre} ERROR - Couldn't Create ${dir}\n" );
	}

	if ( ! -w $dir )
	{
		die( "${sPre} ERROR - ${dir} Isn't Writable\n" );
	}

	return $dir;
}


#
# Compute auto DME list
#
sub ComputeAutoDme
{
	my $outSubDir = shift( @_ );
	my $swigFile = shift( @_ );

	#
	# Only create auto_*.i files for these element libraries
	#
	my %autos = ( "movieobjects", 1, "mdlobjects", 1, "materialobjects", 1, "sfmobjects", 1, "worldobjects", 1 );
	if ( !$autos{ $swigFile } )
	{
		return;
	}

	my $hFile = File::Spec->rel2abs( File::Spec->catdir( ${srcDir}, "public", ${swigFile}, ${swigFile} . ".h" ) );

	my @dependencies = (
		$sScriptPath,
		$hFile
	);

	my $bUpdate = $opt_f;
	my $maxTime = 0;

	my $dependency;
	foreach $dependency ( @dependencies )
	{
		if ( -r $dependency )
		{
			my ( $dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks ) = stat( $dependency );
			if ( $mtime >= $maxTime )
			{
				$maxTime = $mtime;
			}
		}
		else
		{
			$bUpdate = 1;
		}
	}

	my $autoFile = File::Spec->catdir( CreateOutDir( $outSubDir ), "auto_${swigFile}.i" );

	if ( -r $autoFile )
	{
		my ( $dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks ) = stat( $autoFile );
		if ( $mtime < $maxTime )
		{
			$bUpdate = 1;
		}
		else
		{
			# If this script is run but gets here, it means that the build system wanted to run this because some other dependency changed
			# but the actual auto file is ok because this script itself nor the header it parses has changed, so just update the last access
			# time of auto_*.i
			my $tCurrent = time;
			utime $tCurrent, $tCurrent, $autoFile;

			my ( $dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks ) = stat( $autoFile );
			if ( $opt_v )
			{
				print "${sPre}     - Touch " . localtime( $mtime ) . " : " . ${autoFile} . "\n";
			}
			else
			{
				print "${sPre} Touch " . localtime( $mtime ) . " : " . ${autoFile} . "\n";
			}
		}
	}
	else
	{
		$bUpdate = 1;
	}

	if ( $bUpdate )
	{
		open OUT, ">${autoFile}" || die( "${sPre} ERROR: Can't Open ${autoFile} For Writing" );
		print OUT <<"EOF";
//
//	This file is a processed version of:
//		${hFile}
//
//	It is created automatically by:
//		${sScriptPath}
//
//	During the Post-Build step of: ${swigFile}
//

%{
#include "datamodel/dmattribute.h"
#include "${swigFile}/${swigFile}.h"

PyObject *NewSwigDmElement( CDmElement *pDmElement );

%}

%import( package="vs", module="datamodel" ) "datamodel/idatamodel.h"
%import( package="vs", module="datamodel" ) "datamodel/dmelement.h"
%import( package="vs", module="datamodel" ) "datamodel/dmattribute.h"
EOF

		if ( ${swigFile} =~ /mdlobjects/i )
		{
			print OUT <<"EOF";
%import( package="vs", module="movieobjects" ) "movieobjects/dmeshape.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmedag.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmejoint.h"
EOF
		}

		print OUT "\n";

		print OUT <<"EOF";
%include "../swig_common/swig_dmelement_wrap.i"
%include "${swigFile}/${swigFile}.cpp"
EOF

		if ( ${swigFile} =~ /movieobjects/ )
		{
			print OUT <<"EOF";
%include "${swigFile}/${swigFile}_compiletools.cpp"
EOF
		}

		print OUT "\n";

		open IN, "${hFile}" || die( "${sPre} ERROR: Can't Open ${hFile} For Reading" );
		while ( <IN> )
		{
			chomp;
			if ( /^\s*#include\s("[^"]+")/ )
			{
				print OUT "%include $1\n"
			}
		}
		close IN;

		close OUT;

		if ( !$opt_q || $opt_v )
		{
			my ( $dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,$blksize,$blocks ) = stat( $autoFile );

			if ( $opt_v )
			{
				print "${sPre}     - Update " . localtime( $mtime ) . " : " . ${autoFile} . "\n";
			}
			else
			{
				print "${sPre} Update " . localtime( $mtime ) . " : " . ${autoFile} . "\n";
			}
		}
	}
}
