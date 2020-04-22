#!/usr/bin/perl -w

use Data::Dumper;
use FindBin;
use Cwd qw(getcwd abs_path);
use File::Basename;
use lib "$FindBin::Bin/../devtools/lib";
use P4Run;
use strict;

die "Usage: $0 <file>\n" unless @ARGV;
my $path = shift @ARGV;
my $dir = abs_path(dirname($path));
my $file = "$dir/".basename($path);

if ($ENV{VALVE_NO_AUTO_P4})
{
	print "VALVE_NO_AUTO_P4 Set. Making $file writable\n";
	if ($^O eq 'MSWin32')
	{
		system('attrib', '-r', $file);
	}
	else
	{
		open(my $fh, "<", $file);
		my $perm = (stat($fh))[2] | 0220;
		chmod $perm, $fh;
	}
}
else
{
	my $desc = $^O eq 'MSWin32' ? 'Visual Studio Auto Checkout' : 'GCC Auto Checkout';
	$desc = '360 Visual Studio Auto Checkout' if $file =~ /_360|launcher_main|default.xex/;
	my $change = P4Run::FindChange($desc) || P4Run::NewChange($desc);
	die "Failed to create Change List: $desc\n" unless $change;
	my ($stat) = P4Run('fstat', $file);
	if ($stat)
	{
		print "Opening $file for edit in CL#$change ($desc)\n";
		P4Run('edit', '-c', $change, $file);
	}
	else
	{
		print "Adding new file $file to CL#$change ($desc)\n";
		open(my $fh, ">", $file); close($fh);
		P4Run('add', '-c', $change, '-t', 'xbinary', $file);
		unlink($file);
	}
}

warn "$file is not writable\n" if (-e $file && ! -w $file);
exit 0;
