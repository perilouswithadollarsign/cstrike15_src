//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Player decals signature validation code
//
//=============================================================================//

#ifndef PLAYERDECALS_SIGNATURE_H
#define PLAYERDECALS_SIGNATURE_H
#ifdef _WIN32
#pragma once
#endif

//
// We will be using RSA 1024-bit private signing key
// PKCS1 signature length is guaranteed to be 128 bytes
//
#define PLAYERDECALS_SIGNATURE_VERSION 1
#define PLAYERDECALS_SIGNATURE_BYTELEN 128

#define PLAYERDECALS_NUMCHARGES 50

#define PLAYERDECALS_UNITS_SIZE 48
#define PLAYERDECALS_COOLDOWN_SECONDS 45

#define PLAYERDECALS_DURATION_SOLID 240
#define PLAYERDECALS_DURATION_FADE1  40
#define PLAYERDECALS_DURATION_FADE2 120
#define PLAYERDECALS_DURATION_APPLY 1.5f

#define PLAYERDECALS_LIMIT_COUNT 100



//
// SECURITY INFORMATION: this file is included in DLLs that are
// shipping to clients and to community gameservers with the intent
// for those processes to verify signatures.
// NEVER include/reference private key data in this header!
// Private key must be used only on GC.
//

inline bool BClientPlayerDecalSignatureComposeSignBuffer( PlayerDecalDigitalSignature const &data, CUtlBuffer &buf )
{
	if ( data.endpos_size() != 3 ) return false;
	for ( int k = 0; k < 3; ++ k ) buf.PutFloat( data.endpos( k ) );
	if ( data.startpos_size() != 3 ) return false;
	for ( int k = 0; k < 3; ++k ) buf.PutFloat( data.startpos( k ) );
	if ( data.right_size() != 3 ) return false;
	for ( int k = 0; k < 3; ++k ) buf.PutFloat( data.right( k ) );
	if ( data.normal_size() != 3 ) return false;
	for ( int k = 0; k < 3; ++k ) buf.PutFloat( data.normal( k ) );
	
	buf.PutInt( data.tx_defidx() );
	buf.PutInt( data.tint_id() );
	buf.PutInt( data.entindex() );
	buf.PutInt( data.hitbox() );
	
	buf.PutFloat( data.creationtime() );

	buf.PutUnsignedInt( data.accountid() );
	buf.PutUnsignedInt( data.rtime() );
	buf.PutUnsignedInt( data.trace_id() );

	return true;
}

inline bool BValidateClientPlayerDecalSignature( PlayerDecalDigitalSignature const &data )
{
	CUtlBuffer bufData;
	bufData.EnsureCapacity( PLAYERDECALS_SIGNATURE_BYTELEN );
	if ( !BClientPlayerDecalSignatureComposeSignBuffer( data, bufData ) )
		return false;

	// Removed for partner depot
	return true;
}

#endif // PLAYERDECALS_SIGNATURE_H
