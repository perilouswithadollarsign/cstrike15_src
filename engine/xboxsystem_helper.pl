
sub PrintArgs
{
	local $m = shift;
	if ( $m > 0 ) { for $k ( 1 .. $m )
	{
		print ", " if ( $k > 1 );
		print "ArgType$k, ArgName$k";
	} }
}

sub PrintArgsC
{
	local $m = shift;
	if ( $m > 0 ) { for $k ( 1 .. $m )
	{
		print "ArgType$k ArgName$k, ";
	} }
}

sub PrintArgsA
{
	local $m = shift;
	if ( $m > 0 ) { for $k ( 1 .. $m )
	{
		print ", m_##ArgName$k( ArgName$k )";
	} }
}

sub PrintArgsP
{
	local $m = shift;
	if ( $m > 0 ) { for $k ( 1 .. $m )
	{
		print "m_##ArgName$k, ";
	} }
}

sub PrintArgsI
{
	local $m = shift;
	if ( $m > 0 ) { for $k ( 1 .. $m )
	{
		print "ArgName$k, ";
	} }
}

sub PrintArgsM
{
	local $m = shift;
	if ( $m > 0 ) { for $k ( 1 .. $m )
	{
		print "ArgType$k m_##ArgName$k;";
	} }
}



for $n ( 0 .. 9 )
{

print "#define HELPER_OVERLAPPED_SESSION_CALL_C_$n( ";
PrintArgs( $n );
print " ) ";
PrintArgsC( $n );
print "\n";

print "#define HELPER_OVERLAPPED_SESSION_CALL_A_$n( ";
PrintArgs( $n );
print " ) ";
PrintArgsA( $n );
print "\n";

print "#define HELPER_OVERLAPPED_SESSION_CALL_P_$n( ";
PrintArgs( $n );
print " ) ";
PrintArgsP( $n );
print "\n";

print "#define HELPER_OVERLAPPED_SESSION_CALL_I_$n( ";
PrintArgs( $n );
print " ) ";
PrintArgsI( $n );
print "\n";

print "#define HELPER_OVERLAPPED_SESSION_CALL_M_$n( ";
PrintArgs( $n );
print " ) ";
PrintArgsM( $n );
print "\n";

print "\n";

print "#define DECLARE_OVERLAPPED_SESSION_CALL_$n( XCallNameFN_T";
print ", " if ( $n > 0 );
PrintArgs( $n );
print " ) \\\n";
print "struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \\\n";
print "	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \\\n";
print "		HELPER_OVERLAPPED_SESSION_CALL_C_$n( ";
PrintArgs( $n );
print " ) \\\n";
print "		PXOVERLAPPED pxOverlapped ) : \\\n";
print "	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \\\n";
print "		HELPER_OVERLAPPED_SESSION_CALL_A_$n( ";
PrintArgs( $n );
print " ) {} \\\n";
print "	virtual char const * Name() { return #XCallNameFN_T; } \\\n";
print "	virtual DWORD Run() { \\\n";
print "		DWORD ret = ::XCallNameFN_T( m_hSession, \\\n";
print "		HELPER_OVERLAPPED_SESSION_CALL_P_$n( ";
PrintArgs( $n );
print " ) \\\n";
print "		m_pxOverlapped ? &m_xOverlapped : NULL ); \\\n";
print "		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( \"XCall \" #XCallNameFN_T \" failed ( ret = %d, overlapped = %p )!\\n\", ret, m_pxOverlapped ); Assert( 0 ); } \\\n";
print "		return ret; } \\\n";
print "	HELPER_OVERLAPPED_SESSION_CALL_M_$n( ";
PrintArgs( $n );
print " ) \\\n";
print "};\n\n";


print "#define IMPLEMENT_OVERLAPPED_SESSION_CALL_$n( XCallNameFN_T";
print ", " if ( $n > 0 );
PrintArgs( $n );
print " ) \\\n";
print "DECLARE_OVERLAPPED_SESSION_CALL_$n( XCallNameFN_T";
print ", " if ( $n > 0 );
PrintArgs( $n );
print " ) \\\n";
print "virtual DWORD XCallNameFN_T( HANDLE hSession, \\\n";
print "		HELPER_OVERLAPPED_SESSION_CALL_C_$n( ";
PrintArgs( $n );
print " ) \\\n";
print "		PXOVERLAPPED pxOverlapped ) { \\\n";
print "		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \\\n";
print "		HELPER_OVERLAPPED_SESSION_CALL_I_$n( ";
PrintArgs( $n );
print " ) \\\n";
print "		pxOverlapped ) ); \\\n";
print "};\n\n";


print "\n\n";


}
