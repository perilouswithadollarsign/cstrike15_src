//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

//
// Encoder suitable for encoding binary data as key values
//
// Every 3 characters = 4 encoded characters
// Syntax:
//  -   a  ...  {   A ... [  0 ... 9  
//  0   1  ...  27  28 .. 54 55 .. 64
//
//         SRC0 ... SRCN ...
//  FL      X0   .   XN ...
//  FL = 6 bits
//
//	if Xk == 64 then SRCk is pad
//  else
//		SRCk = Xk + 64 * ( FL >> ( 2 * k ) % 4 )
//

#ifndef UTLENCODE_H
#define UTLENCODE_H
#pragma once

namespace KvEncoder
{

inline char DecodeChar( char ch )
{
	if ( ch == '-' )
		return 0;
	if ( ch >= 'a' && ch <= '{' )
		return 1 + ch - 'a';
	if ( ch >= 'A' && ch <= '[' )
		return 28 + ch - 'A';
	if ( ch >= '0' && ch <= '9' )
		return 55 + ch - '0';
	return -1;
}

inline char EncodeChar( char ch )
{
	if ( ch == 0 )
		return '-';
	if ( ch >= 1 && ch <= 27 )
		return 'a' + ch - 1;
	if ( ch >= 28 && ch <= 54 )
		return 'A' + ch - 28;
	if ( ch >= 55 && ch <= 64 )
		return '0' + ch - 55;
	return -1;
}

inline unsigned long EncodeByte( unsigned char x )
{
	unsigned int iSegment = x / 64;
	unsigned int iOffset = x % 64;
	return (
		( unsigned long ) ( ( iOffset ) & 0xFFFF ) |
		( unsigned long ) ( ( ( iSegment ) & 0xFFFF ) << 16 )
		);
}

inline unsigned char DecodeByte( int iSegment, int iOffset )
{
	return iSegment * 64 + iOffset;
}

inline int GuessEncodedLength( int nLength )
{
	return 4 * ( ( nLength + 2 ) / 3 );
}

inline int GuessDecodedLength( int nLength )
{
	return 3 * ( ( nLength + 3 ) / 4 );
}

inline BOOL Encode( CUtlBuffer &src, CUtlBuffer &dst )
{
	int numBytes = dst.Size();
	int nReqLen = GuessEncodedLength( src.TellPut() );
	if ( numBytes < nReqLen )
		return FALSE;

	char *pBase = (char *) dst.Base();
	char *pSrc = (char *) src.Base();
	int srcBytes = src.TellPut();
	while ( srcBytes > 0 )
	{
		char *pSegs = pBase;
		char *pOffs = pBase + 1;
		int flags = 0;

		for ( int k = 0; k < 3; ++ k )
		{
			if ( srcBytes -- > 0 )
			{
				unsigned long enc = EncodeByte( *pSrc ++ );
				*( pOffs ++ ) = EncodeChar( enc & 0xFFFF );
				flags |= ( enc >> 16 ) << ( 2 * k );
			}
			else
			{
				*( pOffs ++ ) = EncodeChar( 64 );
			}
		}

		*pSegs = EncodeChar( flags );
		pBase = pOffs;
	}

	dst.SeekPut( CUtlBuffer::SEEK_HEAD, nReqLen );
	return TRUE;
}

inline BOOL Decode( CUtlBuffer &src, CUtlBuffer &dst )
{
	int numBytesLimit = dst.Size();

	char *pBase = (char *) src.Base();
	char *pData = (char *) dst.Base();
	char *pBaseEnd = pBase + src.TellPut();

	while ( pBase < pBaseEnd )
	{
		int flags = DecodeChar( *( pBase ++ ) );
		if ( -1 == flags )
			return FALSE;

		for ( int k = 0; k < 3 && pBase < pBaseEnd; ++ k )
		{
			int off = DecodeChar( *( pBase ++ ) );
			if ( off == -1 )
				return FALSE;
			if ( off == 64 )
				continue;
			
			int seg = flags >> ( 2 * k );
			seg %= 4;

			if ( numBytesLimit --> 0 )
				*( pData ++ ) = DecodeByte( seg, off );
			else
				return FALSE;
		}
	}

	int numBytes = dst.Size() - numBytesLimit;
	dst.SeekPut( CUtlBuffer::SEEK_HEAD, numBytes );

	return TRUE;
}

}; // namespace KvEncoder


#endif // UTLENCODE_H
