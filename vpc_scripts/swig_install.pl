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
our $srcDir = File::Spec->catdir( $sVpcDir, '..' );
our $diff = File::Spec->catdir( ${srcDir}, "devtools/bin/diff.exe" );
our $p4Edit = File::Spec->catdir( ${srcDir}, "vpc_scripts/valve_p4_edit.cmd" );

if ( ! -x $diff )
{
	$! = 1;
	die( "${pre} ERROR: Can't find diff executable ${diff}" );
}

if ( ! -x $p4Edit )
{
	$! = 1;
	die( "${pre} ERROR: Can't find versioning executable ${p4Edit}" );
}

our $src = File::Spec->canonpath( shift( @ARGV ) );
our $dst = File::Spec->canonpath( shift( @ARGV ) );

my $bAdd = 0;
my $bCopy = 0;

if ( -r ${dst} )
{
	$diffCmd = "${diff} -q \"${src}\" \"${dst}\"";
	if ( $opt_v )
	{
		print( "${pre} ${diffCmd}\n" );
	}
	system( $diffCmd );
	if ( $? )
	{
		$bCopy = 1;
	}
}
else
{
	$bAdd = 1;
	$bCopy = 1;
}

if ( $bCopy )
{
	if ( ! $bAdd )
	{
		$editCmd = "${p4Edit} \"${dst}\" ${srcDir}";
		$editCmd =~ s:/:\\:g;
		if ( !$opt_q )
		{
			print( "${pre}: ${editCmd}\n" );
		}
		system( ${editCmd} );
	}

	$copyCmd = "\"${src}\" \"${dst}\"";
	$copyCmd =~ s:/:\\:g;
	$copyCmd = "copy /Y ${copyCmd}";
	if ( !$opt_q )
	{
		print( "${pre}: ${copyCmd}\n" );
	}
	system( ${copyCmd} );

	if ( $bAdd )
	{
		$editCmd = "${p4Edit} \"${dst}\" ${srcDir}";
		$editCmd =~ s:/:\\:g;
		if ( !$opt_q )
		{
			print( "${pre}: ${editCmd}\n" );
		}
		system( ${editCmd} );
	}
}
