#include "unitlib/unitlib.h"
#include "tier1/commandbuffer.h"
#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"


#if defined( UNIT_TESTS_ENABLED ) && defined( _PS3 )
extern "C" bool _tier1_unit_tests(void)
{
	for( int i = 0; i < UnitTestCount(); i++ )
	{
		ITestCase* pThisTest = GetUnitTest( i );
		Assert( pThisTest );
		if( pThisTest )
			pThisTest->RunTest();
	}

	return true;
}

DEFINE_TESTSUITE( CommandBufferTestSuite )

DEFINE_TESTCASE( CommandBufferTestSimple, CommandBufferTestSuite )
{
	Msg( "Simple command buffer test...\n" );

	CCommandBuffer buffer;
	buffer.AddText( "test_command test_arg1 test_arg2" );
	buffer.AddText( "test_command2 test_arg3; test_command3 test_arg4" );
	buffer.AddText( "test_command4\ntest_command5" );
	buffer.AddText( "test_command6 // Comment; test_command7" );
	buffer.AddText( "test_command8 // Comment; test_command9\ntest_command10" );
	buffer.AddText( "test_command11 \"test_arg5 test_arg6\"" );
	buffer.AddText( "test_command12 \"\"" );
	buffer.AddText( "// Comment\ntest_command13\t\t\"test_arg7\"" );
	buffer.AddText( "test_command14\"test_arg8\"test_arg9" );
	buffer.AddText( "test_command15 test_arg10" );
	buffer.AddText( "test_command16 test_arg11:test_arg12" );

	CCommand command;
	buffer.BeginProcessingCommands( 1 );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 3 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg1" ) );
	Shipping_Assert( !Q_stricmp( command[2], "test_arg2" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "test_arg1 test_arg2" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 2 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command2" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg3" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "test_arg3" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 2 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command3" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg4" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "test_arg4" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 1 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command4" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 1 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command5" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 1 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command6" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 1 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command8" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 1 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command10" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 2 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command11" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg5 test_arg6" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "\"test_arg5 test_arg6\"" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 2 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command12" ) );
	Shipping_Assert( !Q_stricmp( command[1], "" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "\"\"" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 2 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command13" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg7" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "\"test_arg7\"" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 3 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command14" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg8" ) );
	Shipping_Assert( !Q_stricmp( command[2], "test_arg9" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "\"test_arg8\"test_arg9" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 2 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command15" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg10" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "test_arg10" ) );

	Verify( buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 4 );
	Shipping_Assert( !Q_stricmp( command[0], "test_command16" ) );
	Shipping_Assert( !Q_stricmp( command[1], "test_arg11" ) );
	Shipping_Assert( !Q_stricmp( command[2], ":" ) );
	Shipping_Assert( !Q_stricmp( command[3], "test_arg12" ) );
	Shipping_Assert( !Q_stricmp( command.ArgS(), "test_arg11:test_arg12" ) );

	Verify( !buffer.DequeueNextCommand( &command ) );
	Shipping_Assert( command.ArgC() == 0 );

	buffer.EndProcessingCommands( );
}


DEFINE_TESTCASE( CommandBufferTestTiming, CommandBufferTestSuite )
{
	Msg( "Delayed command buffer test...\n" );

	CCommandBuffer buffer;

	buffer.AddText( "test_command test_arg1 test_arg2" );
	buffer.AddText( "test_command2 test_arg1 test_arg2 test_arg3", kCommandSrcUserInput, 1 );
	buffer.AddText( "test_command3;wait;test_command4;wait 2;test_command5" );

	CCommand command;
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 3 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( command[2], "test_arg2" ) );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 1 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command3" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 4 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command2" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( command[2], "test_arg2" ) );
		Shipping_Assert( !Q_stricmp( command[3], "test_arg3" ) );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 1 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command4" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 1 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command5" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
}


DEFINE_TESTCASE( CommandBufferTestNested, CommandBufferTestSuite )
{
	Msg( "Nested command buffer test...\n" );

	CCommandBuffer buffer;
	buffer.AddText( "test_command test_arg1 test_arg2" );
	buffer.AddText( "test_command2 test_arg3 test_arg4 test_arg5", 2 );

	CCommand command;
	{
		buffer.BeginProcessingCommands( 2 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 3 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( command[2], "test_arg2" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.AddText( "test_command3;test_command4", 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 1 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command3" ) );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 1 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command4" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 4 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command2" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg3" ) );
		Shipping_Assert( !Q_stricmp( command[2], "test_arg4" ) );
		Shipping_Assert( !Q_stricmp( command[3], "test_arg5" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
}


DEFINE_TESTCASE( CommandBufferTestOverflow, CommandBufferTestSuite )
{
	Msg( "Command buffer overflow test...\n" );

	CCommandBuffer buffer;

	buffer.LimitArgumentBufferSize( 40 );
	bool bOk = buffer.AddText( "test_command test_arg1 test_arg2" );	// 32 chars
	Shipping_Assert( bOk );
	bOk = buffer.AddText( "test_command2 test_arg3 test_arg4 test_arg5", 2 );	// 43 chars
	Shipping_Assert( !bOk );

	CCommand command;
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 3 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( command[2], "test_arg2" ) );

		bOk = buffer.AddText( "test_command3 test_arg6;wait;test_command4" );
		Shipping_Assert( bOk );

		// This makes sure that AddText doesn't cause argv to become bogus after
		// compacting memory
		Shipping_Assert( !Q_stricmp( command[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( command[2], "test_arg2" ) );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 2 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command3" ) );
		Shipping_Assert( !Q_stricmp( command[1], "test_arg6" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		Verify( buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 1 );
		Shipping_Assert( !Q_stricmp( command[0], "test_command4" ) );

		Verify( !buffer.DequeueNextCommand( &command ) );
		Shipping_Assert( command.ArgC() == 0 );

		buffer.EndProcessingCommands( );
	}
}

template < class A >
bool TestValueAndGutters( A atest[3], const A &a, const char *mtest )
{
	bool bSuccess0 = V_memcmp( &atest[ 0 ], mtest, sizeof( atest[ 0 ] ) ) == 0;
	bool bSuccess1 = atest[ 1 ] == a;
	bool bSuccess2 = V_memcmp( &atest[ 2 ], mtest, sizeof( atest[ 2 ] ) ) == 0;
	Shipping_Assert( bSuccess0 && bSuccess1 && bSuccess2 );
	return bSuccess0 && bSuccess1 && bSuccess2;
}

bool TestString( CUtlBuffer &buf, const char *pString )
{
	char strtest[ 1023 ];
	int nLen = buf.GetUpTo( strtest, sizeof( strtest ) );
	strtest[ nLen ] = '\0';

	bool bSuccessStr = V_strcmp( pString, strtest ) == 0;
	Shipping_Assert( bSuccessStr );
	return bSuccessStr;
}

template < class A >
void TestScanfAndPrintf( const char *pString, const A &a )
{
	char mtest[] = "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"; // 0x555555...
	A atest[3]; V_memset( atest, mtest[0], sizeof( atest ) );


	CUtlBuffer strbuf( pString, V_strlen( pString ) + 1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	const char *pScanFmt = GetFmtStr<A>();
	strbuf.Scanf( pScanFmt, &atest[1] );

	bool bSuccessA = TestValueAndGutters( atest, a, mtest );
	if ( !bSuccessA )
	{
		Msg( "CUtlBuffer::Scanf '%s' FAILED!\n", pString );
		return;
	}


	CUtlBuffer valbuf( 0, 1024, CUtlBuffer::TEXT_BUFFER );
	const char *pPrintFmt = GetFmtStr<A>();
	valbuf.Printf( pPrintFmt, atest[1] ); // we know that a == atest[1] from before

	if ( !TestString( valbuf, pString ) )
	{
		Msg( "CUtlBuffer::Printf '%s' FAILED!\n", pString );
	}
}

template < class A, class B >
void TestScanfAndPrintf( const char *pString, const A &a, const B &b )
{
	char mtest[] = "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"; // 0x555555...
	A atest[3]; V_memset( atest, mtest[0], sizeof( atest ) );
	B btest[3]; V_memset( btest, mtest[0], sizeof( btest ) );


	CUtlBuffer strbuf( pString, V_strlen( pString ) + 1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	char scanfmt[ 256 ];
	V_snprintf( scanfmt, sizeof( scanfmt ), "%s %s", GetFmtStr<A>( 10, false ), GetFmtStr<B>( 10, false ) );
	strbuf.Scanf( scanfmt, &atest[1], &btest[1] );

	bool bSuccessA = TestValueAndGutters( atest, a, mtest );
	bool bSuccessB = TestValueAndGutters( btest, b, mtest );
	if ( !bSuccessA || !bSuccessB )
	{
		Msg( "CUtlBuffer::Scanf '%s' FAILED!\n", pString );
		return;
	}


	CUtlBuffer valbuf( 0, 1024, CUtlBuffer::TEXT_BUFFER );
	char printfmt[ 256 ];
	V_snprintf( printfmt, sizeof( printfmt ), "%s %s", GetFmtStr<A>(), GetFmtStr<B>() );
	valbuf.Printf( printfmt, atest[1], btest[1] ); // we know that a == atest[1] and b == btest[1] from before

	if ( !TestString( valbuf, pString ) )
	{
		Msg( "CUtlBuffer::Printf '%s' FAILED!\n", pString );
	}
}

template < class A, class B, class C >
void TestScanfAndPrintf( const char *pString, const A &a, const B &b, const C &c )
{
	const char mtest[] = "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"; // 0x555555...
	A atest[3]; V_memset( atest, mtest[0], sizeof( atest ) );
	B btest[3]; V_memset( btest, mtest[0], sizeof( btest ) );
	C ctest[3]; V_memset( ctest, mtest[0], sizeof( ctest ) );


	CUtlBuffer strbuf( pString, V_strlen( pString ) + 1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	char scanfmt[ 256 ];
	V_snprintf( scanfmt, sizeof( scanfmt ), "%s %s %s", GetFmtStr<A>( 10, false ), GetFmtStr<B>( 10, false ), GetFmtStr<C>( 10, false ) );
	strbuf.Scanf( scanfmt, &atest[1], &btest[1], &ctest[1] );

	bool bSuccessA = TestValueAndGutters( atest, a, mtest );
	bool bSuccessB = TestValueAndGutters( btest, b, mtest );
	bool bSuccessC = TestValueAndGutters( ctest, c, mtest );
	if ( !bSuccessA || !bSuccessB || !bSuccessC )
	{
		Msg( "CUtlBuffer::Scanf '%s' FAILED!\n", pString );
		return;
	}


	CUtlBuffer valbuf( 0, 1024, CUtlBuffer::TEXT_BUFFER );
	char printfmt[ 256 ];
	V_snprintf( printfmt, sizeof( printfmt ), "%s %s %s", GetFmtStr<A>(), GetFmtStr<B>(), GetFmtStr<C>() );
	valbuf.Printf( printfmt, atest[1], btest[1], ctest[1] ); // we know that a == atest[1] and b == btest[1] and c == ctest[1] from before

	if ( !TestString( valbuf, pString ) )
	{
		Msg( "CUtlBuffer::Printf '%s' FAILED!\n", pString );
	}
}

template < class T >
void TestGetPut( T ( CUtlBuffer::*getfunc )(), void ( CUtlBuffer::*putfunc )( T ), const char *pString, const T &value )
{
	{
		CUtlBuffer valbuf( 0, 1024, CUtlBuffer::TEXT_BUFFER );

		char mtest[] = "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"; // 0x555555...
		T valtest[3];
		V_memset( valtest, mtest[0], sizeof( valtest ) );
		valtest[1] = value;

		( valbuf.*putfunc )( valtest[1] );

		if ( !TestValueAndGutters( valtest, value, mtest ) )
		{
			Msg( "CUtlBuffer::PutXXX FAILED!\n" );
		}

		if ( !TestString( valbuf, pString ) )
		{
			Msg( "CUtlBuffer::PutXXX FAILED!\n" );
		}
	}

	{
		CUtlBuffer strbuf( pString, V_strlen( pString ) + 1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

		char mtest[] = "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"; // 0x555555...
		T valtest[3];
		V_memset( valtest, mtest[0], sizeof( valtest ) );

		valtest[1] = (strbuf.*getfunc)();

		if ( !TestValueAndGutters( valtest, value, mtest ) )
		{
			Msg( "CUtlBuffer::GetXXX FAILED!\n" );
		}
	}

	// 	{
	// 		CUtlBuffer strbuf( pString, V_strlen( pString ) + 1, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );
	// 
	// 		char mtest[] = "UUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUU"; // 0x555555...
	// 		T valtest[3];
	// 		V_memset( valtest, mtest[0], sizeof( valtest ) );
	// 
	// 		char fmt[ 256 ];
	// 		V_snprintf( fmt, sizeof( fmt ), "%s", GetFmtStr<T>() );
	// 
	// 		strbuf.GetType( valtest[1], fmt );
	// 
	// 		if ( !TestValueAndGutters( valtest, value, mtest ) )
	// 		{
	// 			Msg( "CUtlBuffer::GetType FAILED!\n" );
	// 		}
	// 	}
}


DEFINE_TESTSUITE( UtlBufferTestSuite )


DEFINE_TESTCASE( UtlBufferTestScanf, UtlBufferTestSuite )
{
	Msg( "CUtlBuffer::Scanf test...\n" );

	TestScanfAndPrintf( "-1234567890 12345 0.123456789012345", -1234567890, 12345u, 0.123456789012345 );
	TestScanfAndPrintf( "-12345 0.123456", -12345, 0.123456f );
	TestScanfAndPrintf( "1234567890", 1234567890u );
	TestScanfAndPrintf( "1234567890123456789", 1234567890123456789ll );
}

typedef unsigned char uchar;

DEFINE_TESTCASE( UtlBufferTestGetPut, UtlBufferTestSuite )
{
	Msg( "CUtlBuffer Get/Put test...\n" );

	// 	TestGetPut< char   >( &CUtlBuffer::GetChar,				&CUtlBuffer::PutChar,			"-123", -123i8 ); // GetChar()/PutChar() always read/write a single byte, even in text mode
	TestGetPut< uchar  >( &CUtlBuffer::GetUnsignedChar,		&CUtlBuffer::PutUnsignedChar,	"123", 123 );
	TestGetPut< short  >( &CUtlBuffer::GetShort,			&CUtlBuffer::PutShort,			"-12345", -12345 );
	TestGetPut< ushort >( &CUtlBuffer::GetUnsignedShort,	&CUtlBuffer::PutUnsignedShort,	"12345", 12345u );
	TestGetPut< int    >( &CUtlBuffer::GetInt,				&CUtlBuffer::PutInt,			"-1234567890", -1234567890 );
	TestGetPut< uint   >( &CUtlBuffer::GetUnsignedInt,		&CUtlBuffer::PutUnsignedInt,	"1234567890", 1234567890u );
	//	TestGetPut< int    >( &CUtlBuffer::GetIntHex,			&CUtlBuffer::PutIntHex,			"123", 123i8 ); // there is no PutIntHex()
	TestGetPut< int64  >( &CUtlBuffer::GetInt64,			&CUtlBuffer::PutInt64,			"1234567890123456789", 1234567890123456789ll );
	TestGetPut< float  >( &CUtlBuffer::GetFloat,			&CUtlBuffer::PutFloat,			"0.123456", 0.123456f );
	TestGetPut< double >( &CUtlBuffer::GetDouble,			&CUtlBuffer::PutDouble,			"0.123456789012345", 0.123456789012345 );
}

#endif // _DEBUG

