//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: schemainitutils: Helpful macros when initing stuff that you'll
//			want to record multiple errors from
//
//=============================================================================

#ifndef SCHEMAINITUTILS_H
#define SCHEMAINITUTILS_H
#ifdef _WIN32
#pragma once
#endif

// used for initialization functions. Adds an error message if we're recording
// them or returns false if we're not
#define SCHEMA_INIT_CHECK( expr, pchErrorMsg )							\
	if ( !( expr ) )											\
	{																	\
	if ( NULL == ( pVecErrors ) )									\
		{																\
		AssertMsg( expr, pchErrorMsg.Access() );								\
		return false;												\
		}																\
		else															\
		{																\
		pVecErrors->AddToTail( CUtlString( pchErrorMsg.Access() ) );			\
		}																\
	}																	

#define SCHEMA_INIT_SUCCESS( )											\
	( NULL == pVecErrors ) || ( 0 == pVecErrors->Count() )

#define SCHEMA_INIT_SUBSTEP( expr )										\
	if ( ( false == ( expr ) ) && ( NULL == ( pVecErrors ) ) )			\
	return false;


#endif // SCHEMAINITUTILS_H
