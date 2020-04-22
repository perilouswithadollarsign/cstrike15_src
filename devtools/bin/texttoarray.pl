#!/usr/bin/env perl
open( HANDLE,shift) || die;
undef $/;
binmode HANDLE;
$data=<HANDLE>;
$ctr=0;
$out.=sprintf("static unsigned char %s[] = {\n    ", shift);
for($i=0;$i<length($data);$i++)
  {
 $out.=sprintf("0x%02x,", unpack("C", substr($data,$i,1)) );
 $out.="\n    " if ( ( $ctr % 20) == 19);
 $ctr++;
  }
$out.="0x00\n};\n";
print $out;