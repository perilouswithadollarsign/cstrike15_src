//======== Copyright © 1996-2009, Valve, L.L.C., All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Purpose:
//
//=============================================================================


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/idatamodel.h"
#include "dmxeditlib/dmxedit.h"
#include "mdlobjects/dmeeyeball.h"
#include "mdlobjects/dmeeyelid.h"
#include "mdlobjects/dmemouth.h"
#include "movieobjects/dmobjserializer.h"
#include "movieobjects/dmemakefile.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmeflexrules.h"
#include "tier1/utlstack.h"
#include "dmeutils/dmmeshutils.h"
#include "meshutils/mesh.h"
#include <time.h>


#ifndef __func__
# ifdef __FUNCTION__
#  define __func__  __FUNCTION__
# else
#  define __func__  DmxEdit
# endif
#endif

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CDmxEdit : public CBaseAppSystem< IDmxEdit >
{
	typedef CBaseAppSystem< IDmxEdit > BaseClass;

public:

	CDmxEdit()
		: m_nDistanceType( CDmeMesh::DIST_ABSOLUTE )
		, m_pDmeMakefile( NULL )
	{
	}

	CDmeMakefile *GetMakefile()
	{
		if ( !m_pDmeMakefile )
		{
			m_pDmeMakefile = CreateElement< CDmeMakefile >( "python dmxedit", DMFILEID_INVALID );
		}

		if ( !m_pDmeMakefile )
			return NULL;

		return m_pDmeMakefile;
	}

	CDmeSource *AddSource( const char *pszSource, bool bDmx )
	{
		CDmeMakefile *pDmeMakefile = GetMakefile();
		if ( !pDmeMakefile )
			return NULL;

		CDmeSource *pDmeSource =  pDmeMakefile->AddSource< CDmeSource >( pszSource );
		if ( pDmeSource )
		{
			if ( bDmx )
			{
				pDmeSource->SetValue( "LoadDmx", true );
			}
			else
			{
				pDmeSource->SetValue( "LoadObj", true );
			}
		}

		return pDmeSource;
	}

	virtual DmxEditErrorState_t GetErrorState() const
	{
		return m_nErrorState;
	}

	virtual void ResetErrorState()
	{
		m_nErrorState = DMXEDIT_OK;
		m_sErrorString.Set( "" );
	}

	virtual const char *GetErrorString() const
	{
		return m_nErrorState == DMXEDIT_OK ? NULL : m_sErrorString.Get();
	}

	int SetErrorString( DmxEditErrorState_t nErrorState, const char *pszErrorString, ... )
	{
		va_list marker;
		va_start( marker, pszErrorString );
		const int nRetVal = IntSetErrorString( nErrorState, pszErrorString, marker );
		va_end( marker );
		return nRetVal;
	}

	//-----------------------------------------------------------------------------
	// This is a bit of a hack but CDmxEdit is already a friend of CDmeMesh...
	//-----------------------------------------------------------------------------
	static bool RemoveBaseState( CDmeMesh *pDmeMesh, CDmeVertexData *pDmeVertexData )
	{
		return pDmeMesh->RemoveBaseState( pDmeVertexData );
	}


	//-----------------------------------------------------------------------------
	// This is a bit of a hack but CDmxEdit is already a friend of CDmeMesh...
	//-----------------------------------------------------------------------------
	static CDmeVertexData *FindOrAddBaseState( CDmeMesh *pDmeMesh, CDmeVertexData *pDmeVertexData )
	{
		return pDmeMesh->FindOrAddBaseState( pDmeVertexData );
	}

	void SetDistanceType( CDmeMesh::Distance_t nDistanceType )
	{
		m_nDistanceType = nDistanceType;
	}

	CDmeMesh::Distance_t GetDistanceType() const
	{
		return m_nDistanceType;
	}

protected:
	DmxEditErrorState_t m_nErrorState;
	CUtlString m_sErrorString;

	int IntSetErrorString( DmxEditErrorState_t nErrorState, const char *pszErrorString, va_list vaList )
	{
		m_nErrorState = nErrorState;

		char tmpBuf[ BUFSIZ ] = "dmxedit.";
		enum {
			kOffset = 8
		};

#ifdef _WIN32
		int len = _vsnprintf( tmpBuf + kOffset, sizeof( tmpBuf ) - 1 - kOffset, pszErrorString, vaList );
#elif POSIX
		int len = vsnprintf( tmpBuf + kOffset, sizeof( tmpBuf ) - 1 - kOffset, pszErrorString, vaList );
#else
#error "define vsnprintf type."
#endif

		// Len < 0 represents an overflow
		if( len < 0 )
		{
			len = sizeof( tmpBuf ) - 1;
			tmpBuf[sizeof( tmpBuf ) - 1] = 0;
		}

		m_sErrorString.Set( tmpBuf );

		return len;
	}

	CDmeMesh::Distance_t m_nDistanceType;

	CDmeMakefile *m_pDmeMakefile;
};


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static CDmxEdit g_DmxEdit;
CDmxEdit *g_pDmxEditImpl = &g_DmxEdit;
IDmxEdit *g_pDmxEdit = &g_DmxEdit;


//-----------------------------------------------------------------------------
// Macros for error
//-----------------------------------------------------------------------------
#define DMXEDIT_ERROR( ... ) \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": " __VA_ARGS__ );


#define DMXEDIT_ERROR_RETURN_NULL( ... ) \
{ \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": " __VA_ARGS__ ); \
	return NULL; \
}


#define DMXEDIT_ERROR_RETURN_FALSE( ... ) \
{ \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": " __VA_ARGS__ ); \
	return NULL; \
}


#define DMXEDIT_ERROR_RETURN_EMPTY_STRING( ... ) \
{ \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": " __VA_ARGS__ ); \
	return ""; \
}


#define DMXEDIT_WARNING( ... ) \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": " __VA_ARGS__ );


#define DMXEDIT_WARNING_RETURN_NULL( ... ) \
{ \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": " __VA_ARGS__ ); \
	return NULL; \
}


#define DMXEDIT_WARNING_RETURN_FALSE( ... ) \
{ \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": " __VA_ARGS__ ); \
	return NULL; \
}


#define DMXEDIT_WARNING_RETURN_EMPTY_STRING( ... ) \
{ \
	g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": " __VA_ARGS__ ); \
	return ""; \
}


#define DMXEDIT_MESH_ERROR_RETURN_NULL( pDmeMesh ) \
{ \
	if ( !pDmeMesh ) \
	{ \
		g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": No mesh specified" __VA_ARGS__ ); \
		return NULL; \
	} \
}


#define DMXEDIT_MESH_ERROR_RETURN_FALSE( pDmeMesh ) \
{ \
	if ( !pDmeMesh ) \
	{ \
		g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": No mesh specified" __VA_ARGS__ ); \
		return false; \
	} \
}


#define DMXEDIT_MESH_ERROR_RETURN_EMPTY_STRING( pDmeMesh ) \
{ \
	if ( !pDmeMesh ) \
	{ \
		g_pDmxEditImpl->SetErrorString( DMXEDIT_ERROR, __func__ ": No mesh specified" __VA_ARGS__ ); \
		return ""; \
	} \
}


#define DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh ) \
{ \
	if ( !pDmeMesh ) \
	{ \
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": No mesh specified" __VA_ARGS__ ); \
		return NULL; \
	} \
}


#define DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh ) \
{ \
	if ( !pDmeMesh ) \
	{ \
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": No mesh specified" __VA_ARGS__ ); \
		return false; \
	} \
}


#define DMXEDIT_MESH_WARNING_RETURN_EMPTY_STRING( pDmeMesh ) \
{ \
	if ( !pDmeMesh ) \
	{ \
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, __func__ ": No mesh specified" __VA_ARGS__ ); \
		return false; \
	} \
}


//-----------------------------------------------------------------------------
// Checks whether the specified mesh & base state are valid and the base
// state actually belongs to the mesh
//-----------------------------------------------------------------------------
bool BaseStateSanityCheck( CDmeMesh *pDmeMesh, CDmeVertexData *pDmeBaseState, const char *pszFuncName )
{
	if ( !pDmeMesh )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: No mesh specified", pszFuncName );
		return false;
	}

	if ( !pDmeBaseState )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: No base state specified", pszFuncName );
		return false;
	}

	CDmeVertexData *pDmeBaseStateCheck = pDmeMesh->FindBaseState( pDmeBaseState->GetName() );
	if ( !pDmeBaseStateCheck || pDmeBaseState != pDmeBaseStateCheck )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: Base state \"%s\" doesn't belong to mesh \"%s\"", pszFuncName, pDmeBaseState->GetName(), pDmeMesh->GetName() );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static const char s_szEditBaseStateName[] = "__dmxedit_edit";
static const char s_szEditOldCurrentState[] = "__dmxedit_oldCurrentState";


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CleanupMeshEditBaseState( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexData *pDmeBindBaseState = pDmeMesh->GetBindBaseState();
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeBindBaseState, __func__ ) )
		return false;

	if ( !Q_strcmp( pDmeBindBaseState->GetName(), s_szEditOldCurrentState ) )
		return false;

	// Remove edit base state and restore current base state
	if ( pDmeMesh->HasAttribute( s_szEditOldCurrentState ) )
	{
		CDmeVertexData *pDmeOldCurrentBaseState = pDmeMesh->GetValueElement< CDmeVertexData >( s_szEditOldCurrentState );
		if ( pDmeOldCurrentBaseState )
		{
			pDmeMesh->SetCurrentBaseState( pDmeOldCurrentBaseState->GetName() );
		}
		pDmeMesh->RemoveAttribute( s_szEditOldCurrentState );
	}
	else
	{
		pDmeMesh->SetCurrentBaseState( pDmeBindBaseState->GetName() );
	}

	pDmeMesh->DeleteBaseState( s_szEditBaseStateName );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexData *FindMeshEditBaseState( CDmeMesh *pDmeMesh, const char *pszFuncName )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeVertexData *pDmeEditBaseState = pDmeMesh->FindBaseState( s_szEditBaseStateName );

	if ( pDmeEditBaseState && BaseStateSanityCheck( pDmeMesh, pDmeEditBaseState, pszFuncName ) )
		return pDmeEditBaseState;

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexData *FindOrCreateMeshEditBaseState( CDmeMesh *pDmeMesh, const char *pszFuncName )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeVertexData *pDmeEditBaseState = pDmeMesh->FindBaseState( s_szEditBaseStateName );
	if ( pDmeEditBaseState )
	{
		if ( BaseStateSanityCheck( pDmeMesh, pDmeEditBaseState, pszFuncName ) )
			return pDmeEditBaseState;

		return NULL;
	}

	CDmeVertexData *pDmeBindBaseState = pDmeMesh->GetBindBaseState();
	if ( !pDmeBindBaseState )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: No bind base state found on mesh \"%s\"", pszFuncName, pDmeMesh->GetName() );
		return NULL;
	}

	pDmeEditBaseState = pDmeMesh->FindOrCreateBaseState( s_szEditBaseStateName );
	if ( !pDmeEditBaseState )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: Couldn't create edit base state on mesh \"%s\"", pszFuncName, pDmeMesh->GetName() );
		return NULL;
	}

	pDmeBindBaseState->CopyTo( pDmeEditBaseState );
	pDmeEditBaseState->SetFileId( DMFILEID_INVALID, TD_ALL );

	// Save the current base state so we can restore it on save
	CDmAttribute *pDmeOldCurrentStateAttr = NULL;

	if ( pDmeMesh->HasAttribute( s_szEditOldCurrentState ) )
	{
		pDmeOldCurrentStateAttr = pDmeMesh->GetAttribute( s_szEditOldCurrentState );
		if ( !pDmeOldCurrentStateAttr )
		{
			Msg( "WARNING %s: Attribute %s.%s is of type %s, not AT_ELEMENT, removing", pszFuncName, pDmeMesh->GetName(), pDmeOldCurrentStateAttr->GetName(), pDmeOldCurrentStateAttr->GetTypeString() );
			pDmeMesh->RemoveAttribute( s_szEditOldCurrentState );
			pDmeOldCurrentStateAttr = NULL;
		}
	}

	if ( pDmeOldCurrentStateAttr == NULL )
	{
		pDmeOldCurrentStateAttr = pDmeMesh->AddAttributeElement< CDmeVertexData >( s_szEditOldCurrentState );
		if ( !pDmeOldCurrentStateAttr )
		{
			g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: Couldn't create %s.%s attribute", pszFuncName, pDmeMesh->GetName(), s_szEditOldCurrentState );
			return NULL;
		}

		pDmeOldCurrentStateAttr->AddFlag( FATTRIB_DONTSAVE );
	}

	pDmeOldCurrentStateAttr->SetValue< CDmeVertexData >( pDmeMesh->GetCurrentBaseState() );

	pDmeMesh->SetCurrentBaseState( pDmeEditBaseState->GetName() );

	CDmeVertexData *pDmeBaseState = pDmeMesh->GetCurrentBaseState();

	if ( BaseStateSanityCheck( pDmeMesh, pDmeBaseState, pszFuncName ) )
		return pDmeBaseState;

	return NULL;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDmElement *LoadDmx( const char *pszFilename )
{
	CDmElement *pRoot = NULL;

	if ( !Q_stricmp( "dmx", Q_GetFileExtension( pszFilename ) ) )
	{
		g_pDmxEditImpl->AddSource( pszFilename, true );
		g_pDataModel->RestoreFromFile( pszFilename, NULL, NULL, &pRoot, CR_COPY_NEW );
		if ( !pRoot )
			DMXEDIT_ERROR_RETURN_NULL( "DMX Load Fail: \"%s\"", pszFilename );
	}
	else
		DMXEDIT_ERROR_RETURN_NULL( "File without .dmx extension passed to LoadDmx: \"%s\"", pszFilename );

	return pRoot;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDmElement *LoadObj( const char *pszFilename, const char *pszLoadType /* = "ABSOLUTE" */ )
{
	CDmElement *pRoot = NULL;

	if ( !Q_stricmp( "obj", Q_GetFileExtension( pszFilename ) ) )
	{
		g_pDmxEditImpl->AddSource( pszFilename, false );
		// Load OBJs
		bool bAbsoluteObjs = true;
		if ( pszLoadType )
		{
			if ( !Q_stricmp( "absolute", pszLoadType ) )
			{
				bAbsoluteObjs = true;
			}
			else if ( !Q_stricmp( "relative", pszLoadType ) )
			{
				bAbsoluteObjs = false;
			}
			else
				DMXEDIT_ERROR_RETURN_NULL( "Invalid OBJ loadType specified (%s), must be \"ABSOLUTE\" or \"RELATIVE\"", pszLoadType );
		}

		pRoot = CDmObjSerializer().ReadOBJ( pszFilename, NULL, true, bAbsoluteObjs );
		if ( !pRoot )
			DMXEDIT_ERROR_RETURN_NULL( "OBJ Load Fail: \"%s\"", pszFilename );
	}
	else
		DMXEDIT_ERROR_RETURN_NULL( "File without .obj extension passed to LoadObj: \"%s\"", pszFilename );

	return pRoot;
}


//-----------------------------------------------------------------------------
// Not really pushing and popping as it's not implemented as a stack
// only one level or push allowed.  Could be a DM_ELEMENT_ARRAY if needed
// to support arbitrary nesting if required but currently only called
// by Save
//-----------------------------------------------------------------------------
void PushPopEditState( CDmeMesh *pDmeMesh, bool bPush )
{
	if ( !pDmeMesh )
		return;

	const char szPushEdit[] = "__dmxedit_pushEditBase";
	const char szPushCurr[] = "__dmxedit_pushCurrBase";

	if ( bPush )
	{
		CDmeVertexData *pDmeEditBaseState = FindMeshEditBaseState( pDmeMesh, __func__ );
		if ( !pDmeEditBaseState || pDmeMesh->BaseStateCount() <= 1 )
			return;

		pDmeMesh->SetValue( szPushEdit, pDmeEditBaseState );
		pDmeMesh->GetAttribute( szPushEdit )->AddFlag( FATTRIB_DONTSAVE );

		CDmeVertexData *pDmeCurrentBaseState = pDmeMesh->GetCurrentBaseState();
		if ( pDmeCurrentBaseState == pDmeEditBaseState )
		{
			pDmeMesh->SetValue( szPushCurr, pDmeCurrentBaseState );
			pDmeMesh->GetAttribute( szPushCurr )->AddFlag( FATTRIB_DONTSAVE );
		}

		CDmxEdit::RemoveBaseState( pDmeMesh, pDmeEditBaseState );

		if ( pDmeMesh->HasAttribute( s_szEditOldCurrentState ) )
		{
			CDmeVertexData *pDmeOldCurrentVertexData = pDmeMesh->GetValueElement< CDmeVertexData >( s_szEditOldCurrentState );
			if ( pDmeOldCurrentVertexData )
			{
				pDmeMesh->SetCurrentBaseState( pDmeOldCurrentVertexData->GetName() );
			}
			else 
			{
				pDmeOldCurrentVertexData = pDmeMesh->GetBindBaseState();
				if ( pDmeOldCurrentVertexData )
				{
					pDmeMesh->SetCurrentBaseState( pDmeOldCurrentVertexData->GetName() );
				}
				else
				{
					pDmeMesh->SetCurrentBaseState( pDmeMesh->GetBaseState( 0 )->GetName() );
				}
			}
		}
	}
	else
	{
		if ( pDmeMesh->HasAttribute( szPushEdit ) )
		{
			CDmxEdit::FindOrAddBaseState( pDmeMesh, pDmeMesh->GetValueElement< CDmeVertexData >( szPushEdit ) );
			pDmeMesh->RemoveAttribute( szPushEdit );
		}

		if ( pDmeMesh->HasAttribute( szPushCurr ) )
		{
			pDmeMesh->SetCurrentBaseState( pDmeMesh->GetValueElement< CDmeVertexData >( szPushCurr )->GetName() );
			pDmeMesh->RemoveAttribute( szPushCurr );
		}
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void PushPopEditStates( CDmElement *pDmRoot, bool bPush )
{
	if ( !pDmRoot )
		return;

	CDmeDag *pDmeDag = CastElement< CDmeDag >( pDmRoot );
	if ( !pDmeDag )
	{
		pDmeDag = pDmRoot->GetValueElement< CDmeDag >( "model" );
	}

	if ( !pDmeDag )
		return;

	CUtlStack< CDmeDag * > traverseStack;
	traverseStack.Push( pDmeDag );

	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			traverseStack.Push( pDmeDag->GetChild( i ) );
		}

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
		if ( !pDmeMesh )
			continue;

		PushPopEditState( pDmeMesh, bPush );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CleanupDmxEdit( CDmeMesh *pDmeMesh )
{
	if ( !pDmeMesh )
		return;

	const char szPushEdit[] = "__dmxedit_pushEditBase";
	const char szPushCurr[] = "__dmxedit_pushCurrBase";

	CDmeVertexData *pDmeEditBaseState = FindMeshEditBaseState( pDmeMesh, __func__ );
	if ( !pDmeEditBaseState || pDmeMesh->BaseStateCount() <= 1 )
		return;

	pDmeMesh->SetValue( szPushEdit, pDmeEditBaseState );
	pDmeMesh->GetAttribute( szPushEdit )->AddFlag( FATTRIB_DONTSAVE );

	CDmeVertexData *pDmeCurrentBaseState = pDmeMesh->GetCurrentBaseState();
	if ( pDmeCurrentBaseState == pDmeEditBaseState )
	{
		pDmeMesh->SetValue( szPushCurr, pDmeCurrentBaseState );
		pDmeMesh->GetAttribute( szPushCurr )->AddFlag( FATTRIB_DONTSAVE );
	}

	CDmxEdit::RemoveBaseState( pDmeMesh, pDmeEditBaseState );

	if ( pDmeMesh->HasAttribute( s_szEditOldCurrentState ) )
	{
		CDmeVertexData *pDmeOldCurrentVertexData = pDmeMesh->GetValueElement< CDmeVertexData >( s_szEditOldCurrentState );
		if ( pDmeOldCurrentVertexData )
		{
			pDmeMesh->SetCurrentBaseState( pDmeOldCurrentVertexData->GetName() );
		}
		else 
		{
			pDmeOldCurrentVertexData = pDmeMesh->GetBindBaseState();
			if ( pDmeOldCurrentVertexData )
			{
				pDmeMesh->SetCurrentBaseState( pDmeOldCurrentVertexData->GetName() );
			}
			else
			{
				pDmeMesh->SetCurrentBaseState( pDmeMesh->GetBaseState( 0 )->GetName() );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void UpdateMakefile( CDmElement *pDmeRoot )
{
	if ( !pDmeRoot )
		return;

	CDmeMakefile *pDmeMakefile = g_pDmxEditImpl->GetMakefile();
	if ( !pDmeMakefile )
		return;

	pDmeRoot->SetValue( "makefile", pDmeMakefile );
	pDmeMakefile->SetFileId( pDmeRoot->GetFileId(), TD_ALL );
}


//-----------------------------------------------------------------------------
// In winstuff.cpp
//-----------------------------------------------------------------------------
void MyGetUserName( char *pszBuf, unsigned long *pBufSiz );
void MyGetComputerName( char *pszBuf, unsigned long *pBufSiz );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void AddExportTags( CDmElement *pDmeRoot, const char *pszFilename )
{
	if ( !pDmeRoot )
		return;

	CDmElement *pExportTags = CreateElement< CDmElement >( "python_dmxedit_exportTags", pDmeRoot->GetFileId() );

	char szTmpBuf[ BUFSIZ ];

	_strdate( szTmpBuf );
	pExportTags->SetValue( "date", szTmpBuf );

	_strtime( szTmpBuf );
	pExportTags->SetValue( "time", szTmpBuf );

	unsigned long dwSize( sizeof( szTmpBuf ) );

	*szTmpBuf ='\0';
	MyGetUserName( szTmpBuf, &dwSize);
	pExportTags->SetValue( "user", szTmpBuf );

	*szTmpBuf ='\0';
	dwSize = sizeof( szTmpBuf );
	MyGetComputerName( szTmpBuf, &dwSize);
	pExportTags->SetValue( "machine", szTmpBuf );

	pExportTags->SetValue( "app", "python" );
	pExportTags->SetValue( "cmdLine", "python <wuzza>" );

	CDmAttribute *pDmeLoadDmxAttr = pExportTags->AddAttribute( "LoadDmx", AT_STRING_ARRAY );
	CDmAttribute *pDmeLoadObjAttr = pExportTags->AddAttribute( "LoadObj", AT_STRING_ARRAY );
	CDmeMakefile *pDmeMakefile = g_pDmxEditImpl->GetMakefile();
	if ( pDmeMakefile )
	{
		const int nSourceCount = pDmeMakefile->GetSourceCount();
		for ( int i = 0; i < nSourceCount; ++i )
		{
			CDmeSource *pDmeSource = pDmeMakefile->GetSource( i );
			if ( pDmeSource->HasAttribute( "LoadDmx" ) )
			{
				CDmrStringArray( pDmeLoadDmxAttr ).AddToTail( pDmeSource->GetName() );
			}
			else
			{
				CDmrStringArray( pDmeLoadObjAttr ).AddToTail( pDmeSource->GetName() );
			}
		}
	}
	pExportTags->SetValue( "Save", pszFilename );

	pDmeRoot->SetValue( "python_dmxedit_exportTags", pExportTags );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void RemoveExportTags( CDmElement *pRoot, const char *pExportTagsName )
{
	if ( !pRoot )
		return;

	pRoot->RemoveAttribute( pExportTagsName );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool SaveDmx( CDmElement *pDmeRoot, const char *pszFilename )
{
	if ( !pszFilename )
		DMXEDIT_WARNING_RETURN_FALSE( "No filename specified" );

	if ( !pDmeRoot )
		DMXEDIT_WARNING_RETURN_FALSE( "No root DmElement specified" );

	RemoveExportTags( pDmeRoot, "vsDmxIO_exportTags" );
	AddExportTags( pDmeRoot, pszFilename );
	UpdateMakefile( pDmeRoot );

	PushPopEditStates( pDmeRoot, true );	// push - hide them on save

	bool bRetVal = false;

	if ( !Q_stricmp( "dmx", Q_GetFileExtension( pszFilename ) ) )
	{
		bRetVal = g_pDataModel->SaveToFile( pszFilename, NULL, "keyvalues2", "model", pDmeRoot );
		if ( !bRetVal )
		{
			DMXEDIT_WARNING( "Couldn't write dmx file \"%s\"", pszFilename );
		}
	}
	else
	{
		DMXEDIT_WARNING( "Filename without .dmx extension passed to SaveDmx( \"%s\" )", pszFilename );
	}

	PushPopEditStates( pDmeRoot, false );	// pop

	return bRetVal;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool SaveObj( CDmElement *pDmeRoot, const char *pszFilename, const char *pszObjSaveType /* = "ABSOLUTE" */, const char *pszDeltaName /* = NULL */ )
{
	if ( !pszFilename )
		DMXEDIT_WARNING_RETURN_FALSE( "No filename specified" );

	if ( !pDmeRoot )
		DMXEDIT_WARNING_RETURN_FALSE( "No root DmElement specified" );

	PushPopEditStates( pDmeRoot, true );	// push - hide them on save

	bool bRetVal = false;

	if ( !Q_stricmp( "obj", Q_GetFileExtension( pszFilename ) ) )
	{
		bool bAbsoluteObjs = true;
		if ( pszObjSaveType )
		{
			if ( !Q_stricmp( "absolute", pszObjSaveType ) )
			{
				bAbsoluteObjs = true;
			}
			else if ( !Q_stricmp( "relative", pszObjSaveType ) )
			{
				bAbsoluteObjs = false;
			}
			else
			{
				DMXEDIT_ERROR( "Invalid OBJ Save specified (%s), must be \"ABSOLUTE\" or \"RELATIVE\"", pszObjSaveType );
			}
		}

		if ( pszDeltaName )
		{
			if ( !Q_stricmp( "base", pszDeltaName ) || !Q_stricmp( "bind", pszDeltaName ) )
			{
				bRetVal = CDmObjSerializer().WriteOBJ( pszFilename, pDmeRoot, false, NULL, bAbsoluteObjs );
			}
			else
			{
				bRetVal = CDmObjSerializer().WriteOBJ( pszFilename, pDmeRoot, true, pszDeltaName, bAbsoluteObjs );
			}
		}
		else
		{
			bRetVal = CDmObjSerializer().WriteOBJ( pszFilename, pDmeRoot, true, NULL, bAbsoluteObjs );
		}
	}
	else
	{
		DMXEDIT_WARNING( "Filename without .obj extension passed to SaveDmx( \"%s\" )", pszFilename );
	}

	PushPopEditStates( pDmeRoot, false );	// pop

	return bRetVal;
}


//-----------------------------------------------------------------------------
// The internal version of FindMesh
//-----------------------------------------------------------------------------
CDmeMesh *FindMesh( CDmElement *pRoot, const char *pszMeshSearchName, bool bComboOnly )
{
	if ( !pRoot )
		return NULL;

	CDmeDag *pDmeDag = CastElement< CDmeDag >( pRoot );
	if ( !pDmeDag )
	{
		pDmeDag = pRoot->GetValueElement< CDmeDag >( "model" );
	}

	if ( !pDmeDag )
		DMXEDIT_WARNING_RETURN_NULL( "Invalid DmElement passed, DmeDag or element with \"model\" attribute required" );

	CUtlStack< CDmeDag * > traverseStack;
	traverseStack.Push( pDmeDag );

	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			traverseStack.Push( pDmeDag->GetChild( i ) );
		}

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
		if ( !pDmeMesh )
			continue;

		// Looking for a named mesh?  Return if found
		if ( pszMeshSearchName && ( !Q_strcmp( pszMeshSearchName, pDmeDag->GetName() ) || !Q_strcmp( pszMeshSearchName, pDmeMesh->GetName() ) ) )
			return pDmeMesh;

		// Looking for a combo mesh?  Return if found
		if ( bComboOnly && pDmeMesh->DeltaStateCount() )
			return pDmeMesh;

		// Looking for a named or combo mesh, this wasn't it so keep looking
		if ( bComboOnly || pszMeshSearchName )
			continue;

		// Looking for the first mesh?  Ok!
		return pDmeMesh;
	}

	// No mesh found
	return NULL;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDmeMesh *GetFirstComboMesh( CDmElement *pRoot )
{
	CDmeMesh *pDmeMesh = FindMesh( pRoot, NULL, true );
	if ( !pDmeMesh )
		DMXEDIT_WARNING_RETURN_NULL( "No mesh with combinations found" );

	return pDmeMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDmeMesh *GetNamedMesh( CDmElement *pRoot, const char *pszMeshSearchName )
{
	CDmeMesh *pDmeMesh = FindMesh( pRoot, pszMeshSearchName, false );
	if ( !pDmeMesh )
		DMXEDIT_WARNING_RETURN_NULL( "No mesh named \"%s\" found", pszMeshSearchName );

	return pDmeMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CDmeMesh *GetFirstMesh( CDmElement *pDmeRoot )
{
	CDmeMesh *pDmeMesh = FindMesh( pDmeRoot, NULL, false );
	if ( !pDmeMesh )
		DMXEDIT_WARNING_RETURN_NULL( "No mesh found" );

	return pDmeMesh;
}


//-----------------------------------------------------------------------------
// Do a depth first walk of all siblings of the dmeDag owning this mesh
//-----------------------------------------------------------------------------
CDmeMesh *GetNextMesh( CDmeMesh *pCurrentDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pCurrentDmeMesh );

	CDmeDag *pDmeDag = FindReferringElement< CDmeDag >( pCurrentDmeMesh, "shape" );
	if ( !pDmeDag )
		DMXEDIT_WARNING_RETURN_NULL( "No dmeDag owning mesh \"%s\"", pCurrentDmeMesh->GetName() );

	// Walk up to the root
	CDmeDag *pDmeDagParent = NULL;
	for ( ;; )
	{
		pDmeDagParent = FindReferringElement< CDmeDag >( pDmeDag, "children" );
		if ( !pDmeDagParent )
			break;
		pDmeDag = pDmeDagParent;
	}

	CUtlStack< CDmeDag * > traverseStack;
	for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
	{
		traverseStack.Push( pDmeDag->GetChild( i ) );
	}

	bool bNext = false;
	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			traverseStack.Push( pDmeDag->GetChild( i ) );
		}

		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
		if ( !pDmeMesh )
			continue;

		if ( pDmeMesh == pCurrentDmeMesh )
		{
			bNext = true;
			continue;
		}

		if ( bNext )
			return pDmeMesh;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Prints a list of all of the deltas present in the specified mesh
//-----------------------------------------------------------------------------
bool ListDeltas( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	const int nDeltas = pDmeMesh->DeltaStateCount();
	if ( nDeltas <= 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "Mesh \"%s\" has no deltas", pDmeMesh->GetName() );

	for ( int i( 0 ); i < nDeltas; ++i )
	{
		Msg( "# Delta %d: %s\n", i, pDmeMesh->GetDeltaState( i )->GetName() );
	}

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int DeltaCount( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	return pDmeMesh->DeltaStateCount();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *DeltaName( CDmeMesh *pDmeMesh, int nDeltaIndex )
{
	DMXEDIT_MESH_WARNING_RETURN_EMPTY_STRING( pDmeMesh );

	const int nDeltaStateCount = pDmeMesh->DeltaStateCount();

	if ( nDeltaStateCount <= 0 )
		DMXEDIT_WARNING_RETURN_EMPTY_STRING( "Mesh \"%s\" has no deltas", pDmeMesh->GetName() );

	if ( nDeltaIndex < 0 && nDeltaIndex >= nDeltaStateCount )
		DMXEDIT_WARNING_RETURN_EMPTY_STRING( "Delta %n out of range, Mesh \"%s\" has %d deltas", nDeltaIndex, pDmeMesh->GetName(), nDeltaStateCount );

	return pDmeMesh->GetDeltaState( nDeltaIndex )->GetName();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *GetDeltaState( CDmeMesh *pDmeMesh, int nDeltaIndex )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	const int nDeltaStateCount = pDmeMesh->DeltaStateCount();

	if ( nDeltaStateCount <= 0 )
		DMXEDIT_WARNING_RETURN_NULL( "Mesh \"%s\" has no deltas", pDmeMesh->GetName() );

	if ( nDeltaIndex < 0 && nDeltaIndex >= nDeltaStateCount )
		DMXEDIT_WARNING_RETURN_NULL( "Delta %n out of range, Mesh \"%s\" has %d deltas", nDeltaIndex, pDmeMesh->GetName(), nDeltaStateCount );

	return pDmeMesh->GetDeltaState( nDeltaIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeVertexDeltaData *GetDeltaState( CDmeMesh *pDmeMesh, const char *pszDeltaName )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( pDmeDelta )
		DMXEDIT_WARNING_RETURN_NULL( "Mesh \"%s\" has no deltas", pDmeMesh->GetName() );

	return pDmeDelta;
}


//-----------------------------------------------------------------------------
// Checks whether the specified mesh & base state are valid and the base
// state actually belongs to the mesh
// TODO: verify base state is the same as bind state?
// TODO: Size checks?
//-----------------------------------------------------------------------------
bool DeltaStateSanityCheck( CDmeMesh *pDmeMesh, CDmeVertexDeltaData *pDmeDeltaState, const char *pszFuncName )
{
	if ( !pDmeMesh )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: No mesh specified", pszFuncName );
		return false;
	}

	if ( !pDmeDeltaState )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: Non-existent delta state specified", pszFuncName );
		return false;
	}

	CDmeVertexDeltaData *pDmeDeltaStateCheck = pDmeMesh->FindDeltaState( pDmeDeltaState->GetName() );
	if ( !pDmeDeltaStateCheck || pDmeDeltaState != pDmeDeltaStateCheck )
	{
		g_pDmxEditImpl->SetErrorString( DMXEDIT_WARNING, "%s: Delta state \"%s\" doesn't belong to mesh \"%s\"", pszFuncName, pDmeDeltaState->GetName(), pDmeMesh->GetName() );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ResetState( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexData *pDmeEditBaseState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditBaseState, __func__ ) )
		return false;

	return pDmeMesh->SetBaseStateToDelta( NULL, pDmeEditBaseState );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetState( CDmeMesh *pDmeMesh, const char *pszDeltaName )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexData *pDmeEditBaseState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !pDmeEditBaseState )
		return false;

	if ( !Q_stricmp( "base", pszDeltaName ) || !Q_stricmp( "bind", pszDeltaName ) )
		return ResetState( pDmeMesh );

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !DeltaStateSanityCheck( pDmeMesh, pDmeDelta, __func__ ) )
		return false;
	pDmeDelta->Resolve();

	return pDmeMesh->SetBaseStateToDelta( pDmeDelta, pDmeEditBaseState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool RemoveFacesWithMaterial( CDmeMesh *pDmeMesh, const char *pszMaterialName )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	return CDmMeshUtils::RemoveFacesWithMaterial( pDmeMesh, pszMaterialName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool RemoveFacesWithMoreThanNVerts( CDmeMesh *pDmeMesh, int nVertexCount )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	return CDmMeshUtils::RemoveFacesWithMoreThanNVerts( pDmeMesh, nVertexCount );
}


//-----------------------------------------------------------------------------
// After an operation to the bind base state, copy the bind data around to all
// other base states...
//-----------------------------------------------------------------------------
void FixupBaseStates( CDmeMesh * pDmeMesh )
{
	CDmeVertexData *pBindState = pDmeMesh->GetBindBaseState();
	if ( pBindState )
	{
		const int nBaseStateCount = pDmeMesh->BaseStateCount();
		for ( int i = 0; i < nBaseStateCount; ++i )
		{
			CDmeVertexData *pBaseState = pDmeMesh->GetBaseState( i );
			if ( pBindState != pBaseState )
			{
				pBindState->CopyTo( pBaseState );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Mirror( CDmeMesh *pDmeMesh, const char *pszAxis /* = "x" */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	if ( !pszAxis || !*pszAxis )
		DMXEDIT_WARNING_RETURN_FALSE( "No axis specified" );

	int nAxis = -1;
	switch ( *pszAxis )
	{
	case 'x':
	case 'X':
		nAxis = 0;
		break;
	case 'y':
	case 'Y':
		nAxis = 1;
		break;
	case 'z':
	case 'Z':
		nAxis = 2;
		break;
	}

	if ( nAxis < 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "Invalid axis \"%s\" specified, must be one of \"x\", \"y\" or \"z\"", pszAxis );

	// Mirror operates on "bind" state
	const bool bRetVal = CDmMeshUtils::Mirror( pDmeMesh, nAxis );

	FixupBaseStates( pDmeMesh );

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ComputeNormals( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	pDmeMesh->ComputeDeltaStateNormals();

	return true;
}


//-----------------------------------------------------------------------------
// Returns the CDmeCombinationOperator ultimately controlling the specified
// DmeMesh by searched backwards on elements referring to "targets"
// Returns NULL if not found
//-----------------------------------------------------------------------------
CDmeCombinationOperator *GetComboOpFromMesh( CDmeMesh *pDmeMesh )
{
	if ( !pDmeMesh )
		return NULL;

	CUtlRBTree< CDmElement * > visited( CDefOps< CDmElement * >::LessFunc );
	visited.Insert( pDmeMesh );

	const CUtlSymbolLarge sTargets = g_pDataModel->GetSymbol( "targets" );
	const CUtlSymbolLarge sTarget = g_pDataModel->GetSymbol( "target" );

	CDmElement *pDmThisElement = pDmeMesh;
	CDmElement *pDmNextElement = NULL;

	while ( pDmThisElement )
	{
		pDmNextElement = FindReferringElement< CDmElement >( pDmThisElement, sTargets );
		if ( !pDmNextElement )
		{
			pDmNextElement = FindReferringElement< CDmElement >( pDmThisElement, sTarget );
		}

		if ( !pDmNextElement )
			break;

		pDmThisElement = pDmNextElement;

		if ( CastElement< CDmeCombinationOperator >( pDmThisElement ) )
			return CastElement< CDmeCombinationOperator >( pDmThisElement );

		if ( visited.IsValidIndex( visited.Find( pDmThisElement ) ) )
			break;

		visited.Insert( pDmThisElement );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ComputeWrinkles( CDmeMesh *pDmeMesh, bool bOverwrite /* = false */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	pDmeCombo->GenerateWrinkleDeltas( bOverwrite );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ComputeNormalWrinkles( CDmeMesh *pDmeMesh, float flScale, bool bOverwrite /* = false */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );


	pDmeCombo->GenerateWrinkleDeltas( bOverwrite, true, flScale );


	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ComputeWrinkle( CDmeMesh *pDmeMesh, const char *pszDeltaName, float flScale, const char *pszOperation /* = "replace" */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !pDmeDelta )
		DMXEDIT_WARNING_RETURN_FALSE( "Cannot find Delta state \"%s\" on mesh \"%s\"", pszDeltaName, pDmeMesh->GetName() );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	CDmMeshUtils::WrinkleOp wrinkleOp = StringHasPrefix( pszOperation, "r" ) ? CDmMeshUtils::kReplace : CDmMeshUtils::kAdd;

	const int nControlCount = pDmeCombo->GetControlCount();
	for ( int nControlIndex = 0; nControlIndex < nControlCount; ++nControlIndex )
	{
		const int nRawControlCount = pDmeCombo->GetRawControlCount( nControlIndex );
		for ( int nRawControlIndex = 0; nRawControlIndex < nRawControlCount; ++nRawControlIndex )
		{
			if ( Q_strcmp( pszDeltaName, pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex ) ) )
				continue;

			pDmeCombo->SetWrinkleScale( nControlIndex, pszDeltaName, 1.0f );

			break;
		}
	}

	CDmeVertexData *pDmeBindState = pDmeMesh->GetBindBaseState();
	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	return CDmMeshUtils::CreateWrinkleDeltaFromBaseState( pDmeDelta, flScale, wrinkleOp, pDmeMesh, pDmeBindState, pDmeEditState );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ComputeNormalWrinkle( CDmeMesh *pDmeMesh, const char *pszDeltaName, float flScale, const char *pszOperation /* = "replace" */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !pDmeDelta )
		DMXEDIT_WARNING_RETURN_FALSE( "Cannot find Delta state \"%s\" on mesh \"%s\"", pszDeltaName, pDmeMesh->GetName() );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	CDmMeshUtils::WrinkleOp wrinkleOp = StringHasPrefix( pszOperation, "r" ) ? CDmMeshUtils::kReplace : CDmMeshUtils::kAdd;

	const int nControlCount = pDmeCombo->GetControlCount();
	for ( int nControlIndex = 0; nControlIndex < nControlCount; ++nControlIndex )
	{
		const int nRawControlCount = pDmeCombo->GetRawControlCount( nControlIndex );
		for ( int nRawControlIndex = 0; nRawControlIndex < nRawControlCount; ++nRawControlIndex )
		{
			if ( Q_strcmp( pszDeltaName, pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex ) ) )
				continue;

			pDmeCombo->SetWrinkleScale( nControlIndex, pszDeltaName, 1.0f );

			break;
		}
	}

	CDmeVertexData *pDmeBindState = pDmeMesh->GetBindBaseState();
	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	return CDmMeshUtils::CreateWrinkleDeltaFromBaseState( pDmeDelta, flScale, wrinkleOp, pDmeMesh, pDmeBindState, pDmeEditState, true );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SaveDelta( CDmeMesh *pDmeMesh, const char *pszDeltaName )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	// See if it's the "base" state we're updating and not a new delta state at all
	if ( !Q_stricmp( "base", pszDeltaName ) || !Q_stricmp( "bind", pszDeltaName ) )
	{
		CDmeVertexData *pDmeBind = pDmeMesh->GetBindBaseState();
		if ( !pDmeBind )
			DMXEDIT_WARNING_RETURN_FALSE( "Couldn't get bind base state from mesh \"%s\"", pDmeMesh->GetName() );

		if ( pDmeEditState == pDmeBind )
			DMXEDIT_WARNING_RETURN_FALSE( "Current state on mesh is the bind state on mesh \"%s\"", pDmeMesh->GetName() );

		pDmeEditState->CopyTo( pDmeBind );

		return true;
	}

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->ModifyOrCreateDeltaStateFromBaseState( pszDeltaName, pDmeEditState );
	if ( pDmeDelta )
	{
		pDmeDelta->Resolve();
		return true;
	}

	DMXEDIT_WARNING_RETURN_FALSE( "Couldn't create new delta state from base state on mesh \"%s\"", pDmeMesh->GetName() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool DeleteDelta( CDmeMesh *pDmeMesh, const char *pszDeltaName )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !pDmeDelta )
		DMXEDIT_WARNING_RETURN_FALSE( "Cannot find Delta state \"%s\" on mesh \"%s\"", pszDeltaName, pDmeMesh->GetName() );

	return pDmeMesh->DeleteDeltaState( pszDeltaName );
}


//-----------------------------------------------------------------------------
// Helper function for Scale
//-----------------------------------------------------------------------------
static void ScaleDeltaPositions(
	const CDmrArrayConst< Vector > &bindPosData,
	CDmeVertexDeltaData *pDmeDelta,
	float flScaleX,
	float flScaleY,
	float flScaleZ )
{
	const int nPosIndex = pDmeDelta->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return;

	CDmrArray< Vector > posData = pDmeDelta->GetVertexData( nPosIndex );
	const int nPosDataCount = posData.Count();
	if ( nPosDataCount <= 0 )
		return;

	Vector *pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );

	for ( int j = 0; j < nPosDataCount; ++j )
	{
		const Vector &s = posData.Get( j );
		Vector &d = pPosArray[ j ];
		d.x = s.x * flScaleX;
		d.y = s.y * flScaleY;
		d.z = s.z * flScaleZ;
	}

	posData.SetMultiple( 0, nPosDataCount, pPosArray );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Scale( CDmeMesh *pDmeMesh, float flScaleX, float flScaleY, float flScaleZ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	int nArraySize = 0;
	Vector *pPosArray = NULL;

	const int nBaseStateCount = pDmeMesh->BaseStateCount();
	for ( int i = 0; i < nBaseStateCount; ++i )
	{
		CDmeVertexData *pBase = pDmeMesh->GetBaseState( i );
		const int nPosIndex = pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
		if ( nPosIndex < 0 )
			continue;

		CDmrArray< Vector > posData = pBase->GetVertexData( nPosIndex );
		const int nPosDataCount = posData.Count();
		if ( nPosDataCount <= 0 )
			continue;

		if ( nArraySize < nPosDataCount || pPosArray == NULL )
		{
			pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );
			if ( pPosArray )
			{
				nArraySize = nPosDataCount;
			}
		}

		if ( nArraySize < nPosDataCount )
			continue;

		for ( int j = 0; j < nPosDataCount; ++j )
		{
			const Vector &s = posData.Get( j );
			Vector &d = pPosArray[ j ];
			d.x = s.x * flScaleX;
			d.y = s.y * flScaleY;
			d.z = s.z * flScaleZ;
		}

		posData.SetMultiple( 0, nPosDataCount, pPosArray );
	}

	{
		CDmeVertexData *pBind = pDmeMesh->GetBindBaseState();
		const int nPosIndex = pBind ? pBind->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) : -1;

		if ( !pBind || nPosIndex < 0 )
			DMXEDIT_WARNING_RETURN_FALSE( "Can't scale delta states on mesh \"%s\"", pDmeMesh->GetName() );

		const CDmrArrayConst< Vector > posData = pBind->GetVertexData( nPosIndex );

		const int nDeltaStateCount = pDmeMesh->DeltaStateCount();
		for ( int i = 0; i < nDeltaStateCount; ++i )
		{
			ScaleDeltaPositions( posData, pDmeMesh->GetDeltaState( i ), flScaleX, flScaleY, flScaleZ );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Scale( CDmeMesh *pDmeMesh, float flScale )
{
	return Scale( pDmeMesh, flScale, flScale, flScale );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetStereoControl( CDmeMesh *pDmeMesh, const char *pszControlName, bool bStereo /* = true */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pszControlName );
	if ( nControlIndex < 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "No control named \"%s\" found on combo op \"%s\" on mesh \"%s\"", pszControlName, pDmeCombo->GetName(), pDmeMesh->GetName() );

	pDmeCombo->SetStereoControl( nControlIndex, bStereo );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetEyelidControl( CDmeMesh *pDmeMesh, const char *pszControlName, bool bEyelid /* = true */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pszControlName );
	if ( nControlIndex < 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "No control named \"%s\" found on combo op \"%s\" on mesh \"%s\"", pszControlName, pDmeCombo->GetName(), pDmeMesh->GetName() );

	pDmeCombo->SetEyelidControl( nControlIndex, bEyelid );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float MaxDeltaDistance( CDmeMesh *pDmeMesh, const char *pszDeltaName )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !pDmeDelta )
		DMXEDIT_WARNING_RETURN_FALSE( "Cannot find Delta state \"%s\" on mesh \"%s\"", pszDeltaName, pDmeMesh->GetName() );

	if ( !pDmeDelta )
		return 0.0f;

	float fSqMaxDelta = 0.0f;
	float fTmpSqLength;

	const CUtlVector< Vector > &positions = pDmeDelta->GetPositionData();

	const int nPositionCount = positions.Count();
	for ( int i = 0; i < nPositionCount; ++i )
	{
		fTmpSqLength = positions[ i ].LengthSqr();
		if ( fTmpSqLength < fSqMaxDelta )
			continue;

		fSqMaxDelta = fTmpSqLength;
	}

	return sqrt( fSqMaxDelta );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetWrinkleScale( CDmeMesh *pDmeMesh, const char *pszControlName, const char *pszRawControlName, float flScale )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pszControlName );
	if ( nControlIndex < 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "No control named \"%s\" found on combo op \"%s\" on mesh \"%s\"", pszControlName, pDmeCombo->GetName(), pDmeMesh->GetName() );

	// Check to see if the raw control exists
	bool bFoundRawControl = false;
	for ( int nRawControlIndex = 0; nRawControlIndex < pDmeCombo->GetRawControlCount( nControlIndex ); ++nRawControlIndex )
	{
		if ( !Q_strcmp( pszRawControlName, pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex ) ) )
		{
			bFoundRawControl = true;
			break;
		}
	}

	if ( !bFoundRawControl )
	{
		CUtlString rawControls;
		for ( int nRawControlIndex = 0; nRawControlIndex < pDmeCombo->GetRawControlCount( nControlIndex ); ++nRawControlIndex )
		{
			if ( rawControls.Length() > 0 )
			{
				rawControls += ", ";
			}
			rawControls += pDmeCombo->GetRawControlName( nControlIndex, nRawControlIndex );
		}

		DMXEDIT_WARNING_RETURN_FALSE( "Control \"%s\" does not have Raw Control \"%s\" on combo op \"%s\" on mesh \"%s\"", pszControlName, pszRawControlName, pDmeCombo->GetName(), pDmeMesh->GetName() );
	}

	pDmeCombo->SetWrinkleScale( nControlIndex, pszRawControlName, flScale );

	return true;
}


//-----------------------------------------------------------------------------
// If it finds a duplicate control name, reports an error message and
// returns it found one
// Helper function for GroupControls
//-----------------------------------------------------------------------------
bool HasDuplicateControlName(
	CDmeCombinationOperator *pDmeCombo,
	const char *pControlName,
	CUtlVector< const char * > &retiredControlNames )
{
	int i;
	int nRetiredControlNameCount = retiredControlNames.Count();
	for ( i = 0; i < nRetiredControlNameCount; ++i )
	{
		if ( !Q_stricmp( retiredControlNames[i], pControlName ) )
			break;
	}

	if ( i == nRetiredControlNameCount )
	{
		if ( pDmeCombo->FindControlIndex( pControlName ) >= 0 )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool GroupControls( CDmeMesh *pDmeMesh, const char *pszGroupName, CUtlVector< const char * > &rawControlNames )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	// Loop through controls to see if any are already group controls, warn and remove

	CUtlVector< const char * > validControlNames;
	bool bStereo = false;
	bool bEyelid = false;

	for ( int i = 0; i < rawControlNames.Count(); ++i )
	{
		ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( rawControlNames[ i ] );
		if ( nControlIndex < 0 )
		{
			DMXEDIT_WARNING( "Control \"%s\" Doesn't Exist, Ignoring", pszGroupName );
			continue;
		}

		if ( pDmeCombo->GetRawControlCount( nControlIndex ) > 1 )
		{
			DMXEDIT_WARNING( "Control \"%s\" Isn't A Raw Control, Ignoring", pszGroupName );
			continue;
		}

		validControlNames.AddToTail( rawControlNames[ i ] );

		if ( pDmeCombo->IsStereoControl( nControlIndex ) )
		{
			bStereo = true;
		}

		if ( pDmeCombo->IsEyelidControl( nControlIndex ) )
		{
			bEyelid = true;
		}
	}

	if ( HasDuplicateControlName( pDmeCombo, pszGroupName, validControlNames ) )
		DMXEDIT_WARNING_RETURN_FALSE( "Duplicate Control \"%s\" Found", pszGroupName );

	if ( validControlNames.Count() <= 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "No Valid Controls Specified" );

	// Remove the old controls
	for ( int i = 0; i < validControlNames.Count(); ++i )
	{
		pDmeCombo->RemoveControl( validControlNames[i] );
	}

	// Create new control
	ControlIndex_t nNewControl = pDmeCombo->FindOrCreateControl( pszGroupName, bStereo );
	pDmeCombo->SetEyelidControl( nNewControl, bEyelid );
	for ( int i = 0; i < validControlNames.Count(); ++i )
	{
		pDmeCombo->AddRawControl( nNewControl, validControlNames[i] );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void GetDeltaNames( CDmeMesh *pDmeMesh, CUtlVector< const char * > *pOutStringList )
{
	if ( !pOutStringList )
	{
		DMXEDIT_WARNING( "No storage passed for result" );
		return;
	}

	pOutStringList->RemoveAll();

	if ( !pDmeMesh )
	{
		DMXEDIT_WARNING( "No mesh specified" );
		return;
	}

	const int nDeltaStateCount = pDmeMesh->DeltaStateCount();

	for ( int i = 0; i < nDeltaStateCount; ++i )
	{
		const CDmeVertexDeltaData *pDmeDelta = pDmeMesh->GetDeltaState( i );
		if ( !pDmeDelta )
		{
			pOutStringList->AddToTail( "<unknown>" );
		}
		else
		{
			pOutStringList->AddToTail( pDmeDelta->GetName() );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void GetRawControlNames( CDmeMesh *pDmeMesh, CUtlVector< const char * > *pOutStringList, const char *pszControlName /* = NULL */ )
{
	if ( !pOutStringList )
	{
		DMXEDIT_WARNING( "No storage passed for result" );
		return;
	}

	pOutStringList->RemoveAll();

	if ( !pDmeMesh )
	{
		DMXEDIT_WARNING( "No mesh specified" );
		return;
	}

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
	{
		DMXEDIT_WARNING( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );
		return;
	}

	if ( pszControlName )
	{
		const ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( pszControlName );
		if ( nControlIndex < 0 )
		{
			DMXEDIT_WARNING( "No control named \"%s\" on DmeCombinationOperator \"%s\" controlling mesh \"%s\"", pszControlName, pDmeCombo->GetName(), pDmeMesh->GetName() );
		}
		else
		{
			const int nRawControlCount = pDmeCombo->GetRawControlCount( nControlIndex );
			for ( int i = 0; i < nRawControlCount; ++i )
			{
				pOutStringList->AddToTail( pDmeCombo->GetRawControlName( nControlIndex, i ) );
			}
		}
	}
	else
	{
		const int nRawControlCount = pDmeCombo->GetRawControlCount();
		for ( int i = 0; i < nRawControlCount; ++i )
		{
			pOutStringList->AddToTail( pDmeCombo->GetRawControlName( i ) );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void GetControlNames( CDmeMesh *pDmeMesh, CUtlVector< const char * > *pOutStringList )
{
	if ( !pOutStringList )
	{
		DMXEDIT_WARNING( "No storage passed for result" );
		return;
	}

	pOutStringList->RemoveAll();

	if ( !pDmeMesh )
	{
		DMXEDIT_WARNING( "No mesh specified" );
		return;
	}

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
	{
		DMXEDIT_WARNING( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );
		return;
	}

	const int nControlCount = pDmeCombo->GetControlCount();
	for ( int i = 0; i < nControlCount; ++i )
	{
		pOutStringList->AddToTail( pDmeCombo->GetControlName( i ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ReorderControls( CDmeMesh *pDmeMesh, CUtlVector< const char * > &controlNames )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	// Loop through controls to see if any are already group controls, warn and remove

	CUtlVector< const char * > validControlNames;

	for ( int i = 0; i < controlNames.Count(); ++i )
	{
		ControlIndex_t nControlIndex = pDmeCombo->FindControlIndex( controlNames[ i ] );
		if ( nControlIndex < 0 )
		{
			DMXEDIT_WARNING( "Control \"%s\" doesn't exist, ignoring", controlNames[ i ] );
			continue;
		}

		validControlNames.AddToTail( controlNames[ i ] );
	}

	if ( validControlNames.Count() <= 0 )
		DMXEDIT_WARNING_RETURN_FALSE( "No Valid Controls Specified" );

	for ( int i = 0; i < validControlNames.Count(); ++i )
	{
		pDmeCombo->MoveControlBefore( validControlNames[ i ], pDmeCombo->GetControlName( i ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool AddDominationRule( CDmeMesh *pDmeMesh, CUtlVector< const char * > &dominators, CUtlVector< const char * > &supressed )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeCombo = GetComboOpFromMesh( pDmeMesh );
	if ( !pDmeCombo )
		DMXEDIT_WARNING_RETURN_FALSE( "No DmeCombinationOperator found controlling mesh \"%s\"", pDmeMesh->GetName() );

	pDmeCombo->AddDominationRule(
		dominators.Count(), ( const char ** )dominators.Base(),
		supressed.Count(), ( const char ** )supressed.Base() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static const char s_szSelAttrName[] = "__dmxedit_selection";


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeSingleIndexedComponent *FindOrCreateMeshSelection( CDmeMesh *pDmeMesh, CDmeSingleIndexedComponent *pDmePassedSelection )
{
	if ( pDmePassedSelection )
		return pDmePassedSelection;

	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmAttribute *pDmeSelAttr = NULL;

	if ( pDmeMesh->HasAttribute( s_szSelAttrName ) )
	{
		pDmeSelAttr = pDmeMesh->GetAttribute( s_szSelAttrName, AT_ELEMENT );
		if ( !pDmeSelAttr )
		{
			DMXEDIT_WARNING( "Attribute %s.%s is of type %s, not AT_ELEMENT, removing", pDmeMesh->GetName(), pDmeSelAttr->GetName(), pDmeSelAttr->GetTypeString() );
			pDmeMesh->RemoveAttribute( s_szSelAttrName );
			pDmeSelAttr = NULL;
		}
	}

	if ( pDmeSelAttr == NULL )
	{
		CDmeSingleIndexedComponent *pTempSelection = CreateElement< CDmeSingleIndexedComponent >( s_szSelAttrName, DMFILEID_INVALID );
		if ( !pTempSelection )
			DMXEDIT_WARNING_RETURN_NULL( "Couldn't create CDmeSingleIndexedComponent %s element", s_szSelAttrName );

		pDmeSelAttr = pDmeMesh->AddAttributeElement< CDmeSingleIndexedComponent >( s_szSelAttrName );
		if ( !pDmeSelAttr )
			DMXEDIT_WARNING_RETURN_NULL( "Couldn't create %s.%s attribute", pDmeMesh->GetName(), s_szSelAttrName );

		pDmeSelAttr->AddFlag( FATTRIB_DONTSAVE );
		pDmeSelAttr->SetValue< CDmeSingleIndexedComponent >( pTempSelection );
	}

	return pDmeSelAttr->GetValueElement< CDmeSingleIndexedComponent >();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CleanupMeshSelection( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	pDmeMesh->RemoveAttribute( s_szSelAttrName );

	return true;
}


//-----------------------------------------------------------------------------
// Combines the two selections via the selectOp and puts result into pOriginal
//-----------------------------------------------------------------------------
void DoSelectOp( const SelectOp_t &nSelectOp, CDmeSingleIndexedComponent *pDmeOriginalSel, const CDmeSingleIndexedComponent *pDmeNewSel )
{
	if ( !pDmeOriginalSel || !pDmeNewSel )
		return;

	switch ( nSelectOp )
	{
	case ADD_SELECT_OP:
		pDmeOriginalSel->Add( pDmeNewSel );
		break;
	case SUBTRACT_SELECT_OP:
		pDmeOriginalSel->Subtract( pDmeNewSel );
		break;
	case TOGGLE_SELECT_OP:
		{
			CDmeSingleIndexedComponent *pIntersection = CreateElement< CDmeSingleIndexedComponent >( "__dmxedit_intersection", DMFILEID_INVALID );
			if ( !pIntersection )
				return;

			CDmeSingleIndexedComponent *pNewCopy = CreateElement< CDmeSingleIndexedComponent >( "__dmxedit_newCopy", DMFILEID_INVALID );
			if ( !pNewCopy )
			{
				g_pDataModel->DestroyElement( pIntersection->GetHandle() );
				return;
			}

			pDmeOriginalSel->CopyAttributesTo( pIntersection );
			pIntersection->Intersection( pDmeNewSel );
			pDmeOriginalSel->Subtract( pIntersection );

			pDmeNewSel->CopyAttributesTo( pNewCopy );
			pNewCopy->Subtract( pIntersection );
			pDmeOriginalSel->Add( pNewCopy );

			g_pDataModel->DestroyElement( pIntersection->GetHandle() );
			g_pDataModel->DestroyElement( pNewCopy->GetHandle() );
		}
		break;
	case INTERSECT_SELECT_OP:
		pDmeOriginalSel->Intersection( pDmeNewSel );
		break;
	case REPLACE_SELECT_OP:
		{
			CUtlString originalName = pDmeOriginalSel->GetName();
			pDmeNewSel->CopyAttributesTo( pDmeOriginalSel );
			pDmeOriginalSel->SetName( originalName );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeSingleIndexedComponent *Select(
	CDmeMesh *pDmeMesh,
	SelectOp_t nSelectOp,
	const char *pszSelectString,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	if ( nSelectOp < 0 || nSelectOp >= INVALID_SELECT_OP )
		return NULL;

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_NULL( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );

	// Figure out if pszSelectString is one of the keywords, all, none or a delta state
	// NOTE: This means that delta states with a name of all or none are not selectable

	if ( !Q_stricmp( "ALL", pszSelectString ) )
	{
		pDmeMesh->SelectAllVertices( pDmeSelection );
	}
	else if ( !Q_stricmp( "NONE", pszSelectString ) )
	{
		pDmeSelection->Clear();
	}
	else
	{
		CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszSelectString );
		if ( !pDmeDelta )
			DMXEDIT_WARNING_RETURN_NULL( "Mesh \"%s\" does not have a delta state named \"%s\" to select", pDmeMesh->GetName(), pszSelectString );

		if ( nSelectOp == REPLACE_SELECT_OP )
		{
			pDmeMesh->SelectVerticesFromDelta( pDmeDelta, pDmeSelection );
		}
		else
		{
			CDmeSingleIndexedComponent *pDmeTmpSelection = CreateElement< CDmeSingleIndexedComponent >( "__dmxedit_tmpSelection", DMFILEID_INVALID );
			if ( !pDmeTmpSelection )
				DMXEDIT_WARNING_RETURN_NULL( "Couldn't create a tmp selection element while selecting delta \"%s\" on mesh \"%s\"", pszSelectString, pDmeMesh->GetName() );

			pDmeMesh->SelectVerticesFromDelta( pDmeDelta, pDmeTmpSelection );
			DoSelectOp( nSelectOp, pDmeSelection, pDmeTmpSelection );

			g_pDataModel->DestroyElement( pDmeTmpSelection->GetHandle() );
		}
	}

	return pDmeSelection;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
SelectOp_t StringToSelectOp_t( const char *pszSelectOpString )
{
	if ( StringHasPrefix( pszSelectOpString, "A" ) )
		return ADD_SELECT_OP;

	if ( StringHasPrefix( pszSelectOpString, "S" ) )
		return SUBTRACT_SELECT_OP;

	if ( StringHasPrefix( pszSelectOpString, "T" ) )
		return TOGGLE_SELECT_OP;

	if ( StringHasPrefix( pszSelectOpString, "I" ) )
		return INTERSECT_SELECT_OP;

	if ( StringHasPrefix( pszSelectOpString, "R" ) )
		return REPLACE_SELECT_OP;

	DMXEDIT_WARNING( "Invalid Selection Operation string specified: \"%s\"", pszSelectOpString );

	return INVALID_SELECT_OP;
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeSingleIndexedComponent *Select(
	CDmeMesh *pDmeMesh,
	const char *pszSelectString,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	return Select( pDmeMesh, REPLACE_SELECT_OP, pszSelectString, pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeSingleIndexedComponent *Select(
	CDmeMesh *pDmeMesh,
	const char *pszSelectOpString,
	const char *pszSelectString,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	return Select( pDmeMesh, StringToSelectOp_t( pszSelectOpString ), pszSelectString, pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
// An Efficient Bounding Sphere
// by Jack Ritter
// from "Graphics Gems", Academic Press, 1990
//
// Routine to calculate tight bounding sphere over
// a set of points in 3D
// This contains the routine find_bounding_sphere(),
// the struct definition, and the globals used for parameters.
// The abs() of all coordinates must be < BIGNUMBER
// Code written by Jack Ritter and Lyle Rains.
//-----------------------------------------------------------------------------
void FindBoundingSphere( CUtlVector< Vector > &points, Vector &cen, float &fRad )
{
	if ( points.Count() <= 0 )
	{
		cen.Zero();
		fRad = 0.0f;

		return;
	}

	double dx,dy,dz;
	double rad_sq,xspan,yspan,zspan,maxspan;
	double old_to_p,old_to_p_sq,old_to_new;
	Vector xmin,xmax,ymin,ymax,zmin,zmax,dia1,dia2;

//	DVec cen;
	double rad;

	cen = points[ 0 ];
	fRad = 0.0f;

	xmin.x=ymin.y=zmin.z= FLT_MAX; /* initialize for min/max compare */
	xmax.x=ymax.y=zmax.z= -FLT_MAX;

	for ( int i = 0; i < points.Count(); ++i )
	{
		const Vector &caller_p = points[ i ];

		if (caller_p.x<xmin.x)
			xmin = caller_p; /* New xminimum point */
		if (caller_p.x>xmax.x)
			xmax = caller_p;
		if (caller_p.y<ymin.y)
			ymin = caller_p;
		if (caller_p.y>ymax.y)
			ymax = caller_p;
		if (caller_p.z<zmin.z)
			zmin = caller_p;
		if (caller_p.z>zmax.z)
			zmax = caller_p;
	}

	/* Set xspan = distance between the 2 points xmin & xmax (squared) */
	dx = xmax.x - xmin.x;
	dy = xmax.y - xmin.y;
	dz = xmax.z - xmin.z;
	xspan = dx*dx + dy*dy + dz*dz;

	/* Same for y & z spans */
	dx = ymax.x - ymin.x;
	dy = ymax.y - ymin.y;
	dz = ymax.z - ymin.z;
	yspan = dx*dx + dy*dy + dz*dz;

	dx = zmax.x - zmin.x;
	dy = zmax.y - zmin.y;
	dz = zmax.z - zmin.z;
	zspan = dx*dx + dy*dy + dz*dz;

	/* Set points dia1 & dia2 to the maximally separated pair */
	dia1 = xmin; dia2 = xmax; /* assume xspan biggest */
	maxspan = xspan;
	if (yspan>maxspan)
	{
		maxspan = yspan;
		dia1 = ymin; dia2 = ymax;
	}
	if (zspan>maxspan)
	{
		dia1 = zmin; dia2 = zmax;
	}

	/* dia1,dia2 is a diameter of initial sphere */
	/* calc initial center */
	cen.x = (dia1.x+dia2.x)/2.0;
	cen.y = (dia1.y+dia2.y)/2.0;
	cen.z = (dia1.z+dia2.z)/2.0;
	/* calculate initial radius**2 and radius */
	dx = dia2.x-cen.x; /* x component of radius vector */
	dy = dia2.y-cen.y; /* y component of radius vector */
	dz = dia2.z-cen.z; /* z component of radius vector */
	rad_sq = dx*dx + dy*dy + dz*dz;
	rad = sqrt(rad_sq);

	/* SECOND PASS: increment current sphere */

	for ( int i = 0; i < points.Count(); ++i )
	{
		const Vector &caller_p = points[ i ];

		dx = caller_p.x-cen.x;
		dy = caller_p.y-cen.y;
		dz = caller_p.z-cen.z;
		old_to_p_sq = dx*dx + dy*dy + dz*dz;
		if (old_to_p_sq > rad_sq) 	/* do r**2 test first */
		{ 	/* this point is outside of current sphere */
			old_to_p = sqrt(old_to_p_sq);
			/* calc radius of new sphere */
			rad = (rad + old_to_p) / 2.0;
			rad_sq = rad*rad; 	/* for next r**2 compare */
			old_to_new = old_to_p - rad;
			/* calc center of new sphere */
			cen.x = (rad*cen.x + old_to_new*caller_p.x) / old_to_p;
			cen.y = (rad*cen.y + old_to_new*caller_p.y) / old_to_p;
			cen.z = (rad*cen.z + old_to_new*caller_p.z) / old_to_p;
		}
	}

	/*
	fCen.x = cen.x;
	fCen.y = cen.y;
	fCen.z = cen.z;
	*/
	fRad = rad;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float DeltaRadius( CDmeMesh *pDmeMesh, const char *pszDeltaName )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeVertexData *pBind = pDmeMesh->GetBindBaseState();
	if ( !pBind || !BaseStateSanityCheck( pDmeMesh, pBind, __func__ ) )
		return 0.0f;

	CDmeVertexDeltaData *pDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !pDelta || !DeltaStateSanityCheck( pDmeMesh, pDelta, __func__ ) )
		return 0.0f;

	const CUtlVector< Vector > &bindPos = pBind->GetPositionData();
	const CUtlVector< int > &deltaPosIndices = pDelta->GetVertexIndexData( CDmeVertexData::FIELD_POSITION );
	const CUtlVector< Vector > &deltaPos = pDelta->GetPositionData(); 

	Assert( deltaPosIndices.Count() == deltaPos.Count() );

	CUtlVector< Vector > newPos;
	newPos.SetSize( deltaPos.Count() );

	for ( int i = 0; i < newPos.Count(); ++i )
	{
		newPos[ i ] = bindPos[ deltaPosIndices[ i ] ] + deltaPos[ i ];
	}

	Vector vCenter;
	float flRadius = 0.0;

	FindBoundingSphere( newPos, vCenter, flRadius );

	return flRadius;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float SelectionRadius( CDmeMesh *pDmeMesh, CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
	{
		DMXEDIT_WARNING( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );
		return 0.0f;
	}

	if ( pDmeSelection->Count() == 0 )
		return 0.0f;

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return 0.0f;

	CUtlVector< int > selection;
	pDmeSelection->GetComponents( selection );
	const CUtlVector< Vector > &pos = pDmeEditState->GetPositionData();

	CUtlVector< Vector > newPos;
	newPos.SetSize( selection.Count() );

	for ( int i = 0; i < newPos.Count(); ++i )
	{
		newPos[ i ] = pos[ selection[ i ] ];
	}

	Vector vCenter;
	float flRadius = 0.0;

	FindBoundingSphere( newPos, vCenter, flRadius );

	return flRadius;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool GrowSelection( CDmeMesh *pDmeMesh, int nSize /* = 1 */, CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
	{
		DMXEDIT_WARNING( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );
		return 0.0f;
	}

	// TODO: Cache the CDmMeshComp object, make it into winged or half edge data structure
	pDmeMesh->GrowSelection( nSize, pDmeSelection, NULL );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ShrinkSelection( CDmeMesh *pDmeMesh, int nSize /* = 1 */, CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
	{
		DMXEDIT_WARNING( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );
		return 0.0f;
	}

	// TODO: Cache the CDmMeshComp object, make it into winged or half edge data structure
	pDmeMesh->ShrinkSelection( nSize, pDmeSelection, NULL );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int StringToDistanceType( const char *pszDistanceTypeString )
{
	if ( pszDistanceTypeString && ( *pszDistanceTypeString == 'a' || *pszDistanceTypeString == 'A' ) )
		return CDmeMesh::DIST_ABSOLUTE;

	if ( pszDistanceTypeString && ( *pszDistanceTypeString == 'r' || *pszDistanceTypeString == 'R' ) )
		return CDmeMesh::DIST_RELATIVE;

	if ( pszDistanceTypeString && ( *pszDistanceTypeString == 'd' || *pszDistanceTypeString == 'D' ) )
		return CDmeMesh::DIST_DEFAULT;

	DMXEDIT_WARNING( "Invalid Distance Type string specified: \"%s\"", pszDistanceTypeString );
	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetDistanceType( CDmeMesh::Distance_t nDistanceType )
{
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown distance type: %d", nDistanceType );

	g_pDmxEditImpl->SetDistanceType( nDistanceType );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetDistanceType( const char *pszDistanceType )
{
	const int nDistanceType = StringToDistanceType( pszDistanceType );
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		return false;

	return SetDistanceType( static_cast< CDmeMesh::Distance_t >( nDistanceType ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int StringToFalloffType( const char *pszFalloffTypeString )
{
	if ( !Q_strnicmp( pszFalloffTypeString, "L", 1 ) )
		return CDmeMesh::LINEAR;

	else if ( !Q_strnicmp( pszFalloffTypeString, "ST", 2 ) )
		return CDmeMesh::STRAIGHT;

	else if ( !Q_strnicmp( pszFalloffTypeString, "B", 1 ) )
		return CDmeMesh::BELL;

	else if ( !Q_strnicmp( pszFalloffTypeString, "SM", 2 ) )
		return CDmeMesh::SMOOTH;

	else if ( !Q_strnicmp( pszFalloffTypeString, "SP", 2 ) )
		return CDmeMesh::SPIKE;

	else if ( !Q_strnicmp( pszFalloffTypeString, "D", 1 ) )
		return CDmeMesh::DOME;

	DMXEDIT_WARNING( "Invalid Falloff Type string specified: \"%s\"", pszFalloffTypeString );
	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Interp(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight /* = 1.0f */,
	float flFeatherDistance /* = 0.0f */,
	CDmeMesh::Falloff_t nFalloffType /* CDmeMesh::STRAIGHT */,
	CDmeMesh::Distance_t nDistanceType /* = CDmeMesh::DIST_DEFAULT */,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown falloff type: %d", nFalloffType );

	nDistanceType = nDistanceType == CDmeMesh::DIST_DEFAULT ? g_pDmxEditImpl->GetDistanceType() : nDistanceType;

	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown distance type: %d", nDistanceType );

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_NULL( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );

	CDmeSingleIndexedComponent *pNewSelection = flFeatherDistance > 0.0f ? pDmeMesh->FeatherSelection( flFeatherDistance, nFalloffType, nDistanceType, pDmeSelection, NULL ) : NULL;

	bool bRetVal = false;

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	if ( !Q_stricmp( "base", pszDeltaName ) || !Q_stricmp( "bind", pszDeltaName ) )
	{
		bRetVal = pDmeMesh->InterpMaskedDelta( NULL, pDmeEditState, flWeight, pNewSelection ? pNewSelection : pDmeSelection );
	}
	else
	{
		CDmeVertexDeltaData *pDelta = pDmeMesh->FindDeltaState( pszDeltaName );
		if ( !DeltaStateSanityCheck( pDmeMesh, pDelta, __func__ ) )
			return false;

		bRetVal = pDmeMesh->InterpMaskedDelta( pDelta, pDmeEditState, flWeight, pNewSelection ? pNewSelection : pDmeSelection );
	}

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Interp(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight /* = 1.0f */,
	float flFeatherDistance /* = 0.0f */,
	const char *pszFalloffType /* = "STRAIGHT" */,
	const char *pszDistanceType /* = "DEFAULT" */,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	const int nFalloffType = StringToFalloffType( pszFalloffType );
	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		return false;

	const int nDistanceType = StringToDistanceType( pszDistanceType );
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		return false;

	return Interp( pDmeMesh, pszDeltaName, flWeight, flFeatherDistance, static_cast< CDmeMesh::Falloff_t >( nFalloffType ), CDmeMesh::Distance_t( nDistanceType ), pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Add(
	CDmeMesh *pDmeMesh,
	const char *pDeltaName,
	float flWeight /* = 1.0f */,
	float flFeatherDistance /* = 0.0f */,
	CDmeMesh::Falloff_t nFalloffType /* = CDmeMesh::STRAIGHT */,
	CDmeMesh::Distance_t nDistanceType /* = CDmeMesh::DIST_DEFAULT */,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown falloff type: %d", nFalloffType );

	nDistanceType = nDistanceType == CDmeMesh::DIST_DEFAULT ? g_pDmxEditImpl->GetDistanceType() : nDistanceType;

	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown distance type: %d", nDistanceType );

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_NULL( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );

	CDmeSingleIndexedComponent *pNewSelection = flFeatherDistance > 0.0f ? pDmeMesh->FeatherSelection( flFeatherDistance, nFalloffType, nDistanceType, pDmeSelection, NULL ) : NULL;

	bool bRetVal = false;

	CDmeVertexDeltaData *pDelta = pDmeMesh->FindDeltaState( pDeltaName );
	if ( !DeltaStateSanityCheck( pDmeMesh, pDelta, __func__ ) )
		return false;

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	bRetVal = pDmeMesh->AddMaskedDelta( pDelta, pDmeEditState, flWeight, pNewSelection ? pNewSelection : pDmeSelection );

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Add(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight /* = 1.0f */,
	float flFeatherDistance /* = 0.0f */,
	const char *pszFalloffType /* = "STRAIGHT" */,
	const char *pszDistanceType /* = "DEFAULT" */,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	const int nFalloffType = StringToFalloffType( pszFalloffType );
	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		return false;

	const int nDistanceType = StringToDistanceType( pszDistanceType );
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		return false;

	return Add( pDmeMesh, pszDeltaName, flWeight, flFeatherDistance, static_cast< CDmeMesh::Falloff_t >( nFalloffType ), CDmeMesh::Distance_t( nDistanceType ), pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool AddCorrected(
	CDmeMesh *pDmeMesh,
	const char *pDeltaName,
	float flWeight /* = 1.0f */,
	float flFeatherDistance /* = 0.0f */,
	CDmeMesh::Falloff_t nFalloffType /* = CDmeMesh::STRAIGHT */,
	CDmeMesh::Distance_t nDistanceType /* = CDmeMesh::DIST_DEFAULT */,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown falloff type: %d", nFalloffType );

	nDistanceType = nDistanceType == CDmeMesh::DIST_DEFAULT ? g_pDmxEditImpl->GetDistanceType() : nDistanceType;

	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown distance type: %d", nDistanceType );

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_NULL( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );

	CDmeSingleIndexedComponent *pNewSelection = flFeatherDistance > 0.0f ? pDmeMesh->FeatherSelection( flFeatherDistance, nFalloffType, nDistanceType, pDmeSelection, NULL ) : NULL;

	bool bRetVal = false;

	CDmeVertexDeltaData *pDelta = pDmeMesh->FindDeltaState( pDeltaName );
	if ( !DeltaStateSanityCheck( pDmeMesh, pDelta, __func__ ) )
		return false;

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	bRetVal = pDmeMesh->AddCorrectedMaskedDelta( pDelta, pDmeEditState, flWeight, pNewSelection ? pNewSelection : pDmeSelection );

	if ( pNewSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool AddCorrected(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight /* = 1.0f */,
	float flFeatherDistance /* = 0.0f */,
	const char *pszFalloffType /* = "STRAIGHT" */,
	const char *pszDistanceType /* = "DEFAULT" */,
	CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	const int nFalloffType = StringToFalloffType( pszFalloffType );
	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		return false;

	const int nDistanceType = StringToDistanceType( pszDistanceType );
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		return false;

	return AddCorrected( pDmeMesh, pszDeltaName, flWeight, flFeatherDistance, static_cast< CDmeMesh::Falloff_t >( nFalloffType ), CDmeMesh::Distance_t( nDistanceType ), pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Translate(
	CDmeMesh *pDmeMesh,
	Vector vT,
	float flFeatherDistance,
	CDmeMesh::Falloff_t nFalloffType,
	CDmeMesh::Distance_t nDistanceType,
	CDmeSingleIndexedComponent *pDmePassedSelection )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	nDistanceType = nDistanceType == CDmeMesh::DIST_DEFAULT ? g_pDmxEditImpl->GetDistanceType() : nDistanceType;

	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown distance type: %d", nDistanceType );

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_NULL( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );

	CDmeSingleIndexedComponent *pTmpSelection = NULL;

	if ( !pDmeSelection || pDmeSelection->Count() == 0 )
	{
		pTmpSelection = CreateElement< CDmeSingleIndexedComponent >( "__selectAll", DMFILEID_INVALID );
		pDmeSelection = pTmpSelection;
		pDmeMesh->SelectAllVertices( pDmeSelection );
	}

	CDmeSingleIndexedComponent *pNewSelection = flFeatherDistance > 0.0f ? pDmeMesh->FeatherSelection( flFeatherDistance, nFalloffType, nDistanceType, pDmeSelection, NULL ) : pDmeSelection;

	int nArraySize = 0;
	Vector *pPosArray = NULL;

	const int nPosIndex = pDmeEditState->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return false;

	CDmrArray< Vector > posData = pDmeEditState->GetVertexData( nPosIndex );
	const int nPosDataCount = posData.Count();
	if ( nPosDataCount <= 0 )
		return false;

	if ( nArraySize < nPosDataCount || pPosArray == NULL )
	{
		pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );
		if ( pPosArray )
		{
			nArraySize = nPosDataCount;
		}
	}

	if ( nArraySize < nPosDataCount )
		return false;

	if ( nDistanceType == CDmeMesh::DIST_RELATIVE )
	{
		Vector vCenter;
		float fRadius;

		pDmeMesh->GetBoundingSphere( vCenter, fRadius, pDmeEditState, pDmeSelection );

		vT *= fRadius;
	}

	if ( pNewSelection )
	{
		memcpy( pPosArray, posData.Base(), nPosDataCount * sizeof( Vector ) );

		const int nSelectionCount = pNewSelection->Count();
		int nIndex;
		float fWeight;
		for ( int j = 0; j < nSelectionCount; ++j )
		{
			pNewSelection->GetComponent( j, nIndex, fWeight );
			const Vector &s = posData.Get( nIndex );
			Vector &d = pPosArray[ nIndex ];
			d = s + vT * fWeight;
		}
	}
	else
	{
		for ( int j = 0; j < nPosDataCount; ++j )
		{
			const Vector &s = posData.Get( j );
			Vector &d = pPosArray[ j ];
			d = s + vT;
		}
	}

	posData.SetMultiple( 0, nPosDataCount, pPosArray );

	if ( pTmpSelection )
	{
		g_pDataModel->DestroyElement( pTmpSelection->GetHandle() );
	}

	if ( pNewSelection != pDmeSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Translate(
    CDmeMesh *pDmeMesh,
    float flTx,
    float flTy,
    float flTz,
    float flFeatherDistance /* = 0.0f */,
	const char *pszFalloffType /* = "STRAIGHT" */,
	const char *pszDistanceType /* = "DEFAULT" */,
    CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	const int nFalloffType = StringToFalloffType( pszFalloffType );
	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		return false;

	const int nDistanceType = StringToDistanceType( pszDistanceType );
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		return false;

	return Translate( pDmeMesh, Vector( flTx, flTy, flTz ), flFeatherDistance, static_cast< CDmeMesh::Falloff_t >( nFalloffType ), static_cast< CDmeMesh::Distance_t >( nDistanceType ), pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Rotate(
	CDmeMesh *pDmeMesh,
	Vector vR,
	Vector vO,
	float flFeatherDistance,
	CDmeMesh::Falloff_t nFalloffType,
	CDmeMesh::Distance_t nDistanceType,
	CDmeSingleIndexedComponent *pDmePassedSelection )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	nDistanceType = nDistanceType == CDmeMesh::DIST_DEFAULT ? g_pDmxEditImpl->GetDistanceType() : nDistanceType;

	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		DMXEDIT_WARNING_RETURN_FALSE( "Unknown distance type: %d", nDistanceType );

	CDmeVertexData *pDmeEditState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditState, __func__ ) )
		return false;

	CDmeSingleIndexedComponent *pDmeSelection = FindOrCreateMeshSelection( pDmeMesh, pDmePassedSelection );
	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_NULL( "Couldn't find or create selection for mesh %s\n", pDmeMesh->GetName() );

	CDmeSingleIndexedComponent *pTmpSelection = NULL;

	if ( !pDmeSelection || pDmeSelection->Count() == 0 )
	{
		pTmpSelection = CreateElement< CDmeSingleIndexedComponent >( "__selectAll", DMFILEID_INVALID );
		pDmeSelection = pTmpSelection;
		pDmeMesh->SelectAllVertices( pDmeSelection );
	}

	CDmeSingleIndexedComponent *pNewSelection = flFeatherDistance > 0.0f ? pDmeMesh->FeatherSelection( flFeatherDistance, nFalloffType, nDistanceType, pDmeSelection, NULL ) : pDmeSelection;
	int nArraySize = 0;
	Vector *pPosArray = NULL;

	const int nPosIndex = pDmeEditState->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return false;

	CDmrArray< Vector > posData = pDmeEditState->GetVertexData( nPosIndex );
	const int nPosDataCount = posData.Count();
	if ( nPosDataCount <= 0 )
		return false;

	if ( nArraySize < nPosDataCount || pPosArray == NULL )
	{
		pPosArray = reinterpret_cast< Vector * >( alloca( nPosDataCount * sizeof( Vector ) ) );
		if ( pPosArray )
		{
			nArraySize = nPosDataCount;
		}
	}

	if ( nArraySize < nPosDataCount )
		return false;

	Vector vCenter;
	float fRadius;

	pDmeMesh->GetBoundingSphere( vCenter, fRadius, pDmeEditState, pDmeSelection );

	if ( nDistanceType == CDmeMesh::DIST_RELATIVE )
	{
		vR *= fRadius;
	}

	VectorAdd( vCenter, vO, vCenter );

	matrix3x4_t rpMat;
	SetIdentityMatrix( rpMat );
	PositionMatrix( vCenter, rpMat );

	matrix3x4_t rpiMat;
	SetIdentityMatrix( rpiMat );
	PositionMatrix( -vCenter, rpiMat );

	matrix3x4_t rMat;
	SetIdentityMatrix( rMat );

	if ( pNewSelection )
	{
		memcpy( pPosArray, posData.Base(), nPosDataCount * sizeof( Vector ) );

		const int nSelectionCount = pNewSelection->Count();
		int nIndex;
		float fWeight;
		for ( int j = 0; j < nSelectionCount; ++j )
		{
			pNewSelection->GetComponent( j, nIndex, fWeight );
			const Vector &s = posData.Get( nIndex );
			Vector &d = pPosArray[ nIndex ];
			AngleMatrix( RadianEuler( DEG2RAD( vR.x * fWeight ), DEG2RAD( vR.y * fWeight ), DEG2RAD( vR.z * fWeight ) ), rMat );

			ConcatTransforms( rpMat, rMat, rMat );
			ConcatTransforms( rMat, rpiMat, rMat );

			VectorTransform( s, rMat, d );
		}
	}
	else
	{
		AngleMatrix( RadianEuler( DEG2RAD( vR.x ), DEG2RAD( vR.y ), DEG2RAD( vR.z ) ), rMat );

		ConcatTransforms( rpMat, rMat, rMat );
		ConcatTransforms( rMat, rpiMat, rMat );

		for ( int j = 0; j < nPosDataCount; ++j )
		{
			const Vector &s = posData.Get( j );
			Vector &d = pPosArray[ j ];
			VectorTransform( s, rMat, d );
		}
	}

	posData.SetMultiple( 0, nPosDataCount, pPosArray );

	if ( pTmpSelection )
	{
		g_pDataModel->DestroyElement( pTmpSelection->GetHandle() );
	}

	if ( pNewSelection != pDmeSelection )
	{
		g_pDataModel->DestroyElement( pNewSelection->GetHandle() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Rotate(
    CDmeMesh *pDmeMesh,
    float flRx,
    float flRy,
    float flRz,
    float flOx /* = 0.0f */,
    float flOy /* = 0.0f */,
    float flOz /* = 0.0f */,
	float flFeatherDistance /* = 0.0f */,
	const char *pszFalloffType /* = "STRAIGHT" */,
	const char *pszDistanceType /* = "DEFAULT" */,
    CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	const int nFalloffType = StringToFalloffType( pszFalloffType );
	if ( nFalloffType < 0 || nFalloffType > CDmeMesh::DOME )
		return false;

	const int nDistanceType = StringToDistanceType( pszDistanceType );
	if ( nDistanceType < 0 || nDistanceType > CDmeMesh::DIST_DEFAULT )
		return false;

	return Rotate( pDmeMesh, Vector( flRx, flRy, flRz ), Vector( flOx, flOy, flOz ), flFeatherDistance, static_cast< CDmeMesh::Falloff_t >( nFalloffType ), static_cast< CDmeMesh::Distance_t >( nDistanceType ), pDmePassedSelection );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool RemapMaterial( CDmeMesh *pDmeMesh, int nMaterialIndex, const char *pszNewMaterialName )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	return CDmMeshUtils::RemapMaterial( pDmeMesh, nMaterialIndex, pszNewMaterialName );
}


//-----------------------------------------------------------------------------
// Adds the src mesh into the dst mesh
//-----------------------------------------------------------------------------
bool Combine( CDmeMesh *pDmeMesh, CDmeMesh *pDmeSrcMesh, const char *pszDstSkinningBoneName /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeSrcMesh );

	int nSkinningJointIndex = -1;

	if ( pszDstSkinningBoneName )
	{
		// Find the DmeModel of the destination mesh scene
		CDmeDag *pDmeDag = FindReferringElement< CDmeDag >( pDmeMesh, "shape" );
		while ( pDmeDag && !pDmeDag->IsA( CDmeModel::GetStaticTypeSymbol() ) )
		{
			pDmeDag = pDmeDag->GetParent();
		}

		CDmeModel *pDmeModel = CastElement< CDmeModel >( pDmeDag );
		if ( pDmeModel )
		{
			CDmeDag *pDmeJoint = pDmeModel->GetJoint( pszDstSkinningBoneName );
			if ( pDmeJoint )
			{
				nSkinningJointIndex = pDmeModel->GetJointIndex( pDmeJoint );
			}
		}
		else
		{
			DMXEDIT_WARNING( "Couldn't find the DmeModel associated with mesh \"%s\"", pDmeMesh->GetName() );
		}

		if ( nSkinningJointIndex < 0 )
		{
			DMXEDIT_WARNING( "Couldn't find bone named \"%s\", for skinning", pszDstSkinningBoneName );
		}
	}

	const bool bRetVal = CDmMeshUtils::Merge( pDmeSrcMesh, pDmeMesh, nSkinningJointIndex );

	FixupBaseStates( pDmeMesh );

	return bRetVal;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Merge( CDmElement *pDmDstRoot, CDmeMesh *pSrcMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pSrcMesh );

	if ( !pDmDstRoot )
		DMXEDIT_WARNING_RETURN_FALSE( "No element passed" );

	CDmMeshComp srcComp( pSrcMesh );

	CUtlVector< CUtlVector< CDmMeshComp::CEdge * > > srcBorderEdgesList;
	if ( srcComp.GetBorderEdges( srcBorderEdgesList ) == 0 )
		return false;

	CDmeMesh *pDmeSocketMesh = CastElement< CDmeMesh >( pDmDstRoot );
	if ( pDmeSocketMesh )
	{
		const int nEdgeListIndex = CDmMeshUtils::FindMergeSocket( srcBorderEdgesList, pDmeSocketMesh );
		if ( nEdgeListIndex < 0 )
			DMXEDIT_WARNING_RETURN_FALSE( "No merge socket found on specified mesh: \"%s\"\n", pDmeSocketMesh->GetName() );

		CleanupMeshEditBaseState( pSrcMesh );
		CleanupMeshSelection( pSrcMesh );

		bool bRetVal = CDmMeshUtils::Merge( srcComp, srcBorderEdgesList[ nEdgeListIndex ], pDmeSocketMesh );

		FixupBaseStates( pDmeSocketMesh );

		pDmeSocketMesh->Resolve();

		return bRetVal;
	}

	CDmeDag *pDmeDag = CastElement< CDmeDag >( pDmDstRoot );
	if ( !pDmeDag )
	{
		pDmeDag = pDmDstRoot->GetValueElement< CDmeDag >( "model" );
	}

	if ( !pDmeDag )
		DMXEDIT_WARNING_RETURN_FALSE( "Invalid DmElement passed, DmeMesh, DmeDag or DmElement with \"model\" attribute required" );

	Vector vSrcCenter;
	float flSrcRadius;

	pSrcMesh->GetBoundingSphere( vSrcCenter, flSrcRadius );

	Vector vDstCenter;
	float flDstRadius;

	float flSqDist = FLT_MAX;

	int nEdgeListIndex = -1;

	CUtlStack< CDmeDag * > traverseStack;
	traverseStack.Push( pDmeDag );

	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			traverseStack.Push( pDmeDag->GetChild( i ) );
		}

		CDmeMesh *pDmeTmpMesh = CastElement< CDmeMesh >( pDmeDag->GetShape() );
		if ( !pDmeTmpMesh )
			continue;

		const int nTmpEdgeListIndex = CDmMeshUtils::FindMergeSocket( srcBorderEdgesList, pDmeTmpMesh );
		if ( nTmpEdgeListIndex < 0 )
			continue;

		pDmeTmpMesh->GetBoundingSphere( vDstCenter, flDstRadius );
		flDstRadius = vDstCenter.DistToSqr( vSrcCenter );

		if ( flDstRadius < flSqDist )
		{
			flSqDist = flDstRadius;
			pDmeSocketMesh = pDmeTmpMesh;
			nEdgeListIndex = nTmpEdgeListIndex;
		}
	}

	if ( pDmeSocketMesh && nEdgeListIndex >= 0 )
	{
		CleanupMeshEditBaseState( pSrcMesh );
		CleanupMeshSelection( pSrcMesh );

		bool bRetVal = CDmMeshUtils::Merge( srcComp, srcBorderEdgesList[ nEdgeListIndex ], pDmeSocketMesh );

		FixupBaseStates( pDmeSocketMesh );

		pDmeSocketMesh->Resolve();

		return bRetVal;
	}

	DMXEDIT_WARNING_RETURN_FALSE( "No merge socket found in specified scene, i.e. a Set of border edges on the source model that are found on the merge model" );
}


bool ApplyMaskToDelta( CDmeVertexDeltaData *pTheDelta, CDmeSingleIndexedComponent *pDmePassedSelection )
{
	CDmrArray< int > positionIndices = pTheDelta->GetAttribute( "positionsIndices" );
	CDmrArray< Vector > positions = pTheDelta->GetAttribute( "positions" );
	CDmrArray< float > weightData = pDmePassedSelection->GetAttribute( "weights" );
	int numVerts = positionIndices.Count();

	for ( int n=0; n < numVerts; n++ )
	{
		int idx = positionIndices[ n ];
		float fWeight = weightData[ idx ];
		const Vector vNewPos = positions[ n ] * fWeight;

		positions.Set( n, vNewPos );
	}

	//do we have normals to deal with?
	CDmAttribute *pNormalsIndicesAttr = pTheDelta->GetAttribute( "normalsIndices" );
	if ( pNormalsIndicesAttr )
	{
		CDmrArray< int > normalsIndices = pNormalsIndicesAttr;
		CDmrArray< Vector > normals = pTheDelta->GetAttribute( "normals" );

		for ( int n=0; n < numVerts; n++ )
		{
			int idx = normalsIndices[ n ];
			float fWeight = weightData[ idx ];

			Vector vNewNormal = normals[ n ] * fWeight;
			vNewNormal.NormalizeInPlace();

			normals.Set( n, Vector( vNewNormal ) );
		}
	}

	return pTheDelta == NULL;
}


bool CreateDeltaFromMesh( CDmeMesh *pBaseMesh, CDmeMesh *pMeshToUseAsDelta, const char *pDeltaName, CDmeSingleIndexedComponent *pDmePassedSelection /* = NULL */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pBaseMesh );

	DMXEDIT_MESH_WARNING_RETURN_FALSE( pMeshToUseAsDelta );

	CDmeVertexDeltaData *pRet = pBaseMesh->ModifyOrCreateDeltaStateFromBaseState( pDeltaName, pMeshToUseAsDelta->GetCurrentBaseState(), true );

	if ( pDmePassedSelection )
	{
		ApplyMaskToDelta( pRet, pDmePassedSelection );
	}

	return pRet == NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeMesh *ComputeConvexHull3D( CDmeMesh *pDmeMesh, float flCoplanarEpsilon /* = ONE_32ND_UNIT */ )
{
	CMesh mesh;

	if ( ConvertMeshFromDMX( &mesh, pDmeMesh ) )
	{
		CMesh convexHullMesh;
		ConvexHull3D( &convexHullMesh, mesh, flCoplanarEpsilon );

		CUtlString meshName( pDmeMesh->GetName() );
		meshName += "_convexHull";

		CDmeMesh *pDmeConvexHullMesh = CreateElement< CDmeMesh >( meshName.Get(), pDmeMesh->GetFileId() );
		if ( pDmeConvexHullMesh )
		{
			ConvertMeshToDMX( pDmeConvexHullMesh, &convexHullMesh, true );

			return pDmeConvexHullMesh;
		}
	}

	return NULL;
}
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeCombinationOperator *FindOrCreateComboOp( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeCombinationOperator *pDmeComboOp = GetComboOpFromMesh( pDmeMesh );
	if ( pDmeComboOp )
		return pDmeComboOp;

	// Find the DmeModel of the destination mesh scene
	CDmeDag *pDmeDag = FindReferringElement< CDmeDag >( pDmeMesh, "shape" );
	while ( pDmeDag && !pDmeDag->IsA( CDmeModel::GetStaticTypeSymbol() ) )
	{
		pDmeDag = pDmeDag->GetParent();
	}

	CDmeModel *pDmeModel = CastElement< CDmeModel >( pDmeDag );
	if ( !pDmeModel )
		DMXEDIT_ERROR_RETURN_NULL( "Couldn't Find The DmeModel for the mesh: \"%s\"", pDmeMesh->GetName() );

	// Find the root node for the DmeModel (by "model" or "skeleton" attribute):
	CDmElement *pDmRoot = FindReferringElement< CDmElement >( pDmeModel, "model" );
	if ( !pDmRoot )
	{
		pDmRoot = FindReferringElement< CDmElement >( pDmeModel, "skeleton" );
	}

	if ( !pDmRoot )
		DMXEDIT_ERROR_RETURN_NULL( "Couldn't Find The root Dme node referring to the DmeModel \"%s\" for the mesh: \"%s\"", pDmeModel->GetName(), pDmeMesh->GetName() );

	pDmeComboOp = pDmRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	if ( !pDmeComboOp )
	{

		CUtlString comboOpName = pDmeMesh->GetName();
		comboOpName += "_comboOp";

		pDmeComboOp = CreateElement< CDmeCombinationOperator >( comboOpName.Get(), pDmeMesh->GetFileId() );

		if ( !pDmeComboOp )
			DMXEDIT_WARNING_RETURN_NULL( "Couldn't create DmeCombinationOperator for \"%s\"", pDmeMesh->GetName() );

		pDmRoot->SetValue( "combinationOperator", pDmeComboOp );
	}

	pDmeComboOp->AddTarget( pDmeMesh );
	return pDmeComboOp;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetMeshFromSkeleton( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexData *pDmeBindBaseState = pDmeMesh->GetBindBaseState();
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeBindBaseState, __func__ ) )
		return false;

	CDmeVertexData *pDmeEditBaseState = FindOrCreateMeshEditBaseState( pDmeMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeEditBaseState, __func__ ) )
		return false;

	if ( !pDmeBindBaseState->HasSkinningData() )
		DMXEDIT_ERROR_RETURN_FALSE( "No skinning data on specified mesh: \"%s\"", pDmeMesh->GetName() );

	const FieldIndex_t nBindPosField = pDmeBindBaseState->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nBindPosField < 0 )
		DMXEDIT_ERROR_RETURN_FALSE( "Cannot find position data on mesh \"%s\" base state", pDmeMesh->GetName() );

	const FieldIndex_t nEditPosField = pDmeEditBaseState->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	if ( nEditPosField < 0 )
		DMXEDIT_ERROR_RETURN_FALSE( "Cannot find position data on mesh \"%s\" edit state", pDmeMesh->GetName() );

	CDmeDag *pDmeDag = pDmeMesh->GetParent();
	while ( pDmeDag && !pDmeDag->IsA( CDmeModel::GetStaticTypeSymbol() ) )
	{
		pDmeDag = pDmeDag->GetParent();
	}
	CDmeModel *pDmeModel = CastElement< CDmeModel >( pDmeDag );
	if ( !pDmeModel )
		DMXEDIT_ERROR_RETURN_NULL( "Couldn't Find The DmeModel for the mesh: \"%s\"", pDmeMesh->GetName() );

	const CUtlVector< Vector > &posData = CDmrArrayConst< Vector >( pDmeBindBaseState->GetVertexData( nBindPosField ) ).Get();
	const int nVertexCount = posData.Count();

	const CUtlVector< Vector > &editPosData = CDmrArrayConst< Vector >( pDmeEditBaseState->GetVertexData( nEditPosField ) ).Get();
	const int nEditVertexCount = editPosData.Count();

	Assert( nVertexCount == nEditVertexCount );
	if ( nVertexCount != nEditVertexCount )
		DMXEDIT_ERROR_RETURN_FALSE( "Base Pos Count: %d Versus Edit Pos Count: %d\n", nVertexCount, nEditVertexCount );

	Vector *pVertices = reinterpret_cast< Vector * >( alloca( nVertexCount * sizeof( Vector ) ) );

	matrix3x4_t shapeToWorld;
	SetIdentityMatrix( shapeToWorld );

	CDmeModel::s_ModelStack.Push( pDmeModel );
	matrix3x4_t *pPoseToWorld = CDmeModel::SetupModelRenderState( shapeToWorld, pDmeBindBaseState->HasSkinningData(), true );
	CDmeModel::s_ModelStack.Pop();

	CDmeMeshRenderInfo renderInfo( pDmeBindBaseState );
	Assert( renderInfo.HasPositionData() );

	for ( int i = 0; i < nVertexCount; ++i )
	{
		renderInfo.ComputePosition( i, pPoseToWorld, static_cast< Vector * >( NULL ), pVertices );
	}

	pDmeEditBaseState->SetVertexData( nEditPosField, 0, nVertexCount, AT_VECTOR3, pVertices );

	CDmeModel::CleanupModelRenderState();

	return false;
}


//-----------------------------------------------------------------------------
// Sets wrinkle deltas from the specified selection
//-----------------------------------------------------------------------------
void SetWrinkleWeight( CDmeVertexData *pDmeBindBaseState, CDmeVertexDeltaData *pDmeDelta, CDmeSingleIndexedComponent *pDmeSelection, float flScale )
{
	FieldIndex_t nPosIndex = pDmeDelta->FindFieldIndex( CDmeVertexDeltaData::FIELD_POSITION );
	if ( nPosIndex < 0 )
		return;

	FieldIndex_t nBaseTexCoordIndex = pDmeBindBaseState->FindFieldIndex( CDmeVertexData::FIELD_TEXCOORD );
	if ( nBaseTexCoordIndex < 0 )
		return;

	FieldIndex_t nWrinkleIndex = pDmeDelta->FindFieldIndex( CDmeVertexDeltaData::FIELD_WRINKLE );
	if ( nWrinkleIndex < 0 )
	{
		nWrinkleIndex = pDmeDelta->CreateField( CDmeVertexDeltaData::FIELD_WRINKLE );
	}

	const CUtlVector<int> &baseTexCoordIndices = pDmeBindBaseState->GetVertexIndexData( nBaseTexCoordIndex );

	CDmrArrayConst<Vector2D> texData( pDmeBindBaseState->GetVertexData( nBaseTexCoordIndex ) );
	const int nBaseTexCoordCount = texData.Count();
	const int nBufSize = ( ( nBaseTexCoordCount + 7 ) >> 3 );
	unsigned char *pUsedBits = (unsigned char*)_alloca( nBufSize );
	memset( pUsedBits, 0, nBufSize );

	// Construct a temporary array of wrinkle values for the entire mesh
	// Same size as the number of texture coordinate values
	// 0 means no wrinkle data, so initialize to 0
	CUtlVector< float > wrinkleData;
	wrinkleData.SetCount( nBaseTexCoordCount );
	memset( wrinkleData.Base(), 0, wrinkleData.Count() * sizeof( float ) );

	// Copy the old wrinkle data if any exists
	CDmAttribute *pWrinkleDeltaAttr = pDmeDelta->GetVertexData( nWrinkleIndex );
	if ( pWrinkleDeltaAttr )
	{
		CDmrArrayConst< float > wrinkleDeltaArray( pWrinkleDeltaAttr );
		if ( wrinkleDeltaArray.Count() )
		{
			const CUtlVector< int > &wrinkleDeltaIndices = pDmeDelta->GetVertexIndexData( nWrinkleIndex );
			Assert( wrinkleDeltaIndices.Count() == wrinkleDeltaArray.Count() );

			int nWrinkleIndex;
			for ( int i = 0; i < wrinkleDeltaIndices.Count(); ++i )
			{
				nWrinkleIndex = wrinkleDeltaIndices[ i ];
				wrinkleData[ nWrinkleIndex ] = wrinkleDeltaArray[ i ];
			}
		}
	}

	// Write new wrinkle values overtop of existing wrinkle values
	int nComponentIndex;
	float flWeight;

	for ( int i = 0; i < pDmeSelection->Count(); ++i )
	{
		if ( !pDmeSelection->GetComponent( i, nComponentIndex, flWeight ) )
			continue;

		flWeight *= flScale;

		// NOTE: This will produce bad behavior in cases where two positions share the
		// same texcoord, which shouldn't theoretically happen.
		const CUtlVector< int > &baseVerts = pDmeBindBaseState->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, nComponentIndex );
		for ( int j = 0; j < baseVerts.Count(); ++j )
		{
			// See if we have a delta for this texcoord...
			const int nTexCoordIndex = baseTexCoordIndices[ baseVerts[j] ];
			if ( pUsedBits[ nTexCoordIndex >> 3 ] & ( 1 << ( nTexCoordIndex & 0x7 ) ) )
				continue;

			pUsedBits[ nTexCoordIndex >> 3 ] |= 1 << ( nTexCoordIndex & 0x7 );

			wrinkleData[ nTexCoordIndex ] = flWeight;
		}
	}

	// Remove previous wrinkle data (if any exists)
	pDmeDelta->RemoveAllVertexData( nWrinkleIndex );

	// Write new non-zero wrinkles
	for ( int i = 0; i < wrinkleData.Count(); ++i )
	{
		if ( fabs( wrinkleData[ i ] ) < 0.00001 )
			continue;

		int nDeltaIndex = pDmeDelta->AddVertexData( nWrinkleIndex, 1 );
		pDmeDelta->SetVertexIndices( nWrinkleIndex, nDeltaIndex, 1, &i );
		pDmeDelta->SetVertexData( nWrinkleIndex, nDeltaIndex, 1, AT_FLOAT, &wrinkleData[ i ] );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetWrinkleWeight( CDmeMesh *pDmeMesh, const char *pszDeltaName, CDmeSingleIndexedComponent *pDmeSelection, float flScale /* = 1.0 */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeVertexData *pDmeBindBaseState = pDmeMesh->GetBindBaseState();
	if ( !BaseStateSanityCheck( pDmeMesh, pDmeBindBaseState, __func__ ) )
		return false;

	CDmeVertexDeltaData *pDmeDelta = pDmeMesh->FindDeltaState( pszDeltaName );
	if ( !pDmeDelta )
		DMXEDIT_WARNING_RETURN_FALSE( "Mesh \"%s\" has no delta named \"%s\"", pDmeMesh->GetName(), pszDeltaName );

	if ( !pDmeSelection )
		DMXEDIT_WARNING_RETURN_FALSE( "No selection specified\n" );

	SetWrinkleWeight( pDmeBindBaseState, pDmeDelta, pDmeSelection, flScale );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float WrapDeform_ComputeOptimalDistSqr( const CUtlVector< Vector > &iPos, const CUtlVector< Vector > &tPos )
{
	float flRetDistSqr = 0.0f;
	float flMaxDistSqr;
	float flTmpDistSqr;

	for ( int i = 0; i < tPos.Count(); ++i )
	{
		flMaxDistSqr = 0.0f;
		const Vector &vt = tPos[i];

		for ( int j = 0; j < iPos.Count(); ++j )
		{
			flTmpDistSqr = vt.DistToSqr( iPos[j] );
			if ( flTmpDistSqr > flMaxDistSqr )
			{
				flMaxDistSqr = flTmpDistSqr;
			}
		}

		if ( flMaxDistSqr > flRetDistSqr )
		{
			flRetDistSqr = flMaxDistSqr;
		}
	}

	return flRetDistSqr;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CWrapVert
{
public:
	CWrapVert()
	: m_flTotalDist( 0.0f )
	{}

	void AddInfluenceVert( int nInfluenceVert, float flDistSqr )
	{
		const float flDist = sqrtf( flDistSqr );

		if ( Count() < s_nMaxInfluence )
		{
			WrapVert_s &wrapVert = m_wrapVerts[ m_wrapVerts.AddToTail() ];
			wrapVert.m_nInfluenceIndex = nInfluenceVert;
			wrapVert.m_flDist = sqrtf( flDistSqr );
			m_flTotalDist += wrapVert.m_flDist;
		}
		else
		{
			WrapVert_s &wrapVert = m_wrapVerts.Tail();
			if ( flDist >= wrapVert.m_flDist )
				return;

			wrapVert.m_nInfluenceIndex = nInfluenceVert;
			wrapVert.m_flDist = sqrtf( flDistSqr );
			m_flTotalDist = 0.0f;
			for ( int i = 0; i < m_wrapVerts.Count(); ++i )
			{
				m_flTotalDist += m_wrapVerts[i].m_flDist;
			}
		}

		qsort( m_wrapVerts.Base(), m_wrapVerts.Count(), sizeof( WrapVert_s ), SortWrapVertFunc );
	}

	int Count() const
	{
		return m_wrapVerts.Count();
	}

	bool GetInfluenceIndexAndWeight( int &nInfluenceIndex, float &flWeight, int nLocalIndex ) const
	{
		const int nCount = Count();

		if ( nCount <= 0 || nLocalIndex >= nCount || nLocalIndex < 0 || m_flTotalDist <= 0.0f )
			return false;

		const WrapVert_s &wrapVert = m_wrapVerts[nLocalIndex];
		nInfluenceIndex = wrapVert.m_nInfluenceIndex;
		if ( nCount == 1 )
		{
			flWeight = 1.0f;
		}
		else
		{
			float flTotal = 0.0f;
			for ( int i = 0; i < m_wrapVerts.Count(); ++i )
			{
				flTotal += ( m_flTotalDist - m_wrapVerts[i].m_flDist );
			}
			flWeight = ( m_flTotalDist - wrapVert.m_flDist ) / flTotal;
		}

		return true;
	}

protected:

	static int SortWrapVertFunc( const void *pLhs, const void *pRhs )
	{
		const WrapVert_s &lWrapVert = *reinterpret_cast< const WrapVert_s * >( pLhs );
		const WrapVert_s &rWrapVert = *reinterpret_cast< const WrapVert_s * >( pRhs );

		if ( lWrapVert.m_flDist < rWrapVert.m_flDist )
			return -1;

		if ( lWrapVert.m_flDist > rWrapVert.m_flDist )
			return 1;

		return 0;
	}

	struct WrapVert_s
	{
		int m_nInfluenceIndex;
		float m_flDist;
	};

	CUtlVector< WrapVert_s > m_wrapVerts;
	float m_flTotalDist;

	static int s_nMaxInfluence;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CWrapVert::s_nMaxInfluence = 10;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void WrapDeform_ComputeInfluence( CUtlVector< CWrapVert > &influenceIndices, const CUtlVector< Vector > &iPos, const CUtlVector< Vector > &tPos, float flMaxDistSqr )
{
	influenceIndices.SetCount( tPos.Count() );

	float flTmpDistSqr;

	for ( int i = 0; i < tPos.Count(); ++i )
	{
		const Vector &vt = tPos[i];
		CWrapVert &wrapVert = influenceIndices[i];

		for ( int j = 0; j < iPos.Count(); ++j )
		{
			flTmpDistSqr = vt.DistToSqr( iPos[j] );
			if ( flTmpDistSqr <= flMaxDistSqr )
			{
				wrapVert.AddInfluenceVert( j, flTmpDistSqr );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void WrapDeform_ComputeWrap(
	CUtlVector< Vector > &tPosCurr,
	const CUtlVector< CWrapVert > &influenceIndices,
	const CUtlVector< Vector > &iPosBind,
	const CUtlVector< Vector > &iPosCurr,
	const CUtlVector< Vector > &tPosBind )
{
	Assert( tPosBind.Count() == influenceIndices.Count() );

	tPosCurr = tPosBind;	// Copy as a default and to set size

	int nInfluenceIndex;
	float flInfluenceWeight;
	Vector vTemp0;
	Vector vTemp1;

	for ( int i = 0; i < tPosCurr.Count(); ++i )
	{
		Vector &vt = tPosCurr[i];

		const CWrapVert &wrapVert = influenceIndices[i];
		for ( int j = 0; j < wrapVert.Count(); ++j )
		{
			wrapVert.GetInfluenceIndexAndWeight( nInfluenceIndex, flInfluenceWeight, j );
			VectorSubtract( iPosCurr[nInfluenceIndex], iPosBind[nInfluenceIndex], vTemp0 );
			VectorScale( vTemp0, flInfluenceWeight, vTemp1 );
			VectorAdd( vTemp1, vt, vt );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool WrapDeform( CDmeMesh *pDmeInfluenceMesh, CDmeMesh *pDmeTargetMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeInfluenceMesh );
	CDmeVertexData *pIBind = pDmeInfluenceMesh->GetBindBaseState();
	if ( !BaseStateSanityCheck( pDmeInfluenceMesh, pIBind, __func__ ) )
		return false;
	pIBind->Resolve();

	CDmeVertexData *pICurrent = pDmeInfluenceMesh->GetCurrentBaseState();
	if ( !BaseStateSanityCheck( pDmeInfluenceMesh, pICurrent, __func__ ) )
		return false;
	pICurrent->Resolve();

	if ( pIBind == pICurrent )
		DMXEDIT_ERROR_RETURN_FALSE( "Bind base state must be different from current base state on influence mesh in order for Wrap to do anything" );

	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeTargetMesh );
	CDmeVertexData *pTBind = pDmeTargetMesh->GetBindBaseState();
	if ( !BaseStateSanityCheck( pDmeTargetMesh, pTBind, __func__ ) )
		return false;
	pTBind->Resolve();

	CDmeVertexData *pTEdit = FindOrCreateMeshEditBaseState( pDmeTargetMesh, __func__ );
	if ( !BaseStateSanityCheck( pDmeTargetMesh, pTEdit, __func__ ) )
		return false;

	const CUtlVector< Vector > &iPosBind = pIBind->GetPositionData();
	const CUtlVector< Vector > &tPosBind = pTBind->GetPositionData();

	const float flMinDistSqr = WrapDeform_ComputeOptimalDistSqr( iPosBind, tPosBind ) + 4.0 + FLT_EPSILON;

	CUtlVector< CWrapVert > influenceIndices;
	WrapDeform_ComputeInfluence( influenceIndices, iPosBind, tPosBind, flMinDistSqr );

	const CUtlVector< Vector > &iPosCurr = pICurrent->GetPositionData();
	CUtlVector< Vector > tPosCurr;

	WrapDeform_ComputeWrap( tPosCurr, influenceIndices, iPosBind, iPosCurr, tPosBind );

	const int nEditPosField = pTEdit->FindFieldIndex( CDmeVertexData::FIELD_POSITION );
	pTEdit->RemoveAllVertexData( nEditPosField );
	pTEdit->AddVertexData( nEditPosField, tPosCurr.Count() );
	pTEdit->SetVertexData( nEditPosField, 0, tPosCurr.Count(), AT_VECTOR3, tPosCurr.Base() );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
Vector ComputeMean( const CUtlVector< Vector > &pointList )
{
	Vector vMean;
	vMean.Zero();

	const int nPointCount = pointList.Count();
	for ( int i = 0; i < nPointCount; ++i )
	{
		VectorAdd( vMean, pointList[i], vMean );
	}

	VectorDivide( vMean, static_cast< float >( nPointCount ), vMean );

	return vMean;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
matrix3x4_t ComputeCovariantMatrix( const CUtlVector< Vector > &pointList, const Vector &vMean )
{
	matrix3x4_t mCovariant;
	SetIdentityMatrix( mCovariant );

	const int nPointCount = pointList.Count();

	if ( nPointCount <= 0 )
		return mCovariant;

	CUtlVector< Vector > skewedPointList;
	skewedPointList = pointList;

	Assert( nPointCount == skewedPointList.Count() );

	for ( int i = 0; i < nPointCount; ++i )
	{
		VectorSubtract( skewedPointList[i], vMean, skewedPointList[i] );
	}

	float flCovariance;

	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			flCovariance = 0.0f;

			for ( int k = 0; k < nPointCount; ++k )
			{
				flCovariance += skewedPointList[k][i] * skewedPointList[k][j];
			}

			mCovariant[i][j] = flCovariance / static_cast< float >( nPointCount );
		}
	}

	return mCovariant;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
matrix3x4_t ComputeCovariantMatrix( const CUtlVector< Vector > &pointList )
{
	const Vector vMean = ComputeMean( pointList );

	return ComputeCovariantMatrix( pointList, vMean );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool MergeMeshAndSkeleton( CDmeMesh *pDstMesh, CDmeMesh *pSrcMesh )
{
	return CDmMeshUtils::MergeMeshAndSkeleton( pDstMesh, pSrcMesh );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool FlexLocalVar( CDmeMesh *pDmeMesh, const char *pszFlexRuleLocalVar )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeFlexRules *pDmeFlexRules = FindOrAddDmeFlexRules( pDmeMesh );
	if ( !pDmeFlexRules )
		DMXEDIT_WARNING_RETURN_FALSE( "DmeFlexRules could not be created for mesh \"%s\"", pDmeMesh->GetName() );

	CDmeFlexRuleLocalVar *pDmeFlexRuleLocalVar = CreateElement< CDmeFlexRuleLocalVar >( pszFlexRuleLocalVar, pDmeMesh->GetFileId() );
	pDmeFlexRules->AddFlexRule( pDmeFlexRuleLocalVar );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool FlexRuleExpression( CDmeMesh *pDmeMesh, const char *pszExpression )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeFlexRules *pDmeFlexRules = FindOrAddDmeFlexRules( pDmeMesh );
	if ( !pDmeFlexRules )
		DMXEDIT_WARNING_RETURN_FALSE( "DmeFlexRules could not be created for mesh \"%s\"", pDmeMesh->GetName() );

	char szBuf[ 512 ];
	if ( sscanf( pszExpression, "%512s =", szBuf ) != 1 )
		DMXEDIT_WARNING_RETURN_FALSE( "Improperly formatted expression: \"%s\"", pszExpression );

	CDmeFlexRuleExpression *pDmeFlexRuleExpression = CreateElement< CDmeFlexRuleExpression >( szBuf, pDmeMesh->GetFileId() );
	pDmeFlexRuleExpression->SetExpression( pszExpression );
	pDmeFlexRules->AddFlexRule( pDmeFlexRuleExpression );

	return true;
}


//-----------------------------------------------------------------------------
// Returns the CDmeFlexRules ultimately controlling the specified
// DmeMesh by searched backwards on elements referring to "targets" or "target"
// Returns NULL if not found
//-----------------------------------------------------------------------------
static CDmeFlexRules *GetFlexRulesForMesh( CDmeMesh *pDmeMesh )
{
	if ( !pDmeMesh )
		return NULL;

	CUtlRBTree< CDmElement * > visited( CDefOps< CDmElement * >::LessFunc );
	visited.Insert( pDmeMesh );

	const CUtlSymbolLarge sTargets = g_pDataModel->GetSymbol( "targets" );
	const CUtlSymbolLarge sTarget = g_pDataModel->GetSymbol( "target" );

	CDmElement *pDmThisElement = pDmeMesh;
	CDmElement *pDmNextElement = NULL;

	while ( pDmThisElement )
	{
		pDmNextElement = FindReferringElement< CDmElement >( pDmThisElement, sTargets );
		if ( !pDmNextElement )
		{
			pDmNextElement = FindReferringElement< CDmElement >( pDmThisElement, sTarget );
		}

		if ( !pDmNextElement )
			break;

		pDmThisElement = pDmNextElement;

		if ( CastElement< CDmeFlexRules >( pDmThisElement ) )
			return CastElement< CDmeFlexRules >( pDmThisElement );

		if ( visited.IsValidIndex( visited.Find( pDmThisElement ) ) )
			break;

		visited.Insert( pDmThisElement );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeFlexRules *FindOrAddDmeFlexRules( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeFlexRules *pDmeFlexRules = GetFlexRulesForMesh( pDmeMesh );
	if ( pDmeFlexRules )
		return pDmeFlexRules;

	CDmeCombinationOperator *pDmeComboOp = FindOrCreateComboOp( pDmeMesh );
	if ( !pDmeComboOp )
		DMXEDIT_WARNING_RETURN_NULL( "DmeCombinationOperator could not be created for mesh \"%s\"", pDmeMesh->GetName() );

	CUtlString flexRulesName = pDmeMesh->GetName();
	flexRulesName += "_flexRules";

	pDmeFlexRules = CreateElement< CDmeFlexRules >( flexRulesName.Get(), pDmeMesh->GetFileId() );
	if ( !pDmeFlexRules )
		DMXEDIT_WARNING_RETURN_NULL( "DmeFlexRules could not be created for mesh \"%s\"", pDmeMesh->GetName() );

	pDmeComboOp->RemoveAllTargets();

	pDmeComboOp->AddTarget( pDmeFlexRules );

	pDmeFlexRules->SetTarget( pDmeMesh );

	return pDmeFlexRules;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
ControlIndex_t FlexControl( CDmeMesh *pDmeMesh, const char *pszName, float flMin /* = 0.0f */, float flMax /* = 1.0f */ )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeComboOp = FindOrCreateComboOp( pDmeMesh );
	if ( !pDmeComboOp )
		DMXEDIT_WARNING_RETURN_FALSE( "DmeCombinationOperator could not be created for mesh \"%s\"", pDmeMesh->GetName() );

	const ControlIndex_t nIndex = pDmeComboOp->FindOrCreateControl( pszName, false, true );

	if ( nIndex >= 0 && ( flMin != 0.0f || flMax != 1.0f ) )
	{
		SetControlMinMax( pDmeMesh, nIndex, flMin, flMax );
	}

	return nIndex;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetControlMinMax( CDmeMesh *pDmeMesh, ControlIndex_t nControlIndex, float flMin, float flMax )
{
	DMXEDIT_MESH_WARNING_RETURN_FALSE( pDmeMesh );

	CDmeCombinationOperator *pDmeComboOp = FindOrCreateComboOp( pDmeMesh );
	if ( !pDmeComboOp )
		DMXEDIT_WARNING_RETURN_FALSE( "DmeCombinationOperator could not be created for mesh \"%s\"", pDmeMesh->GetName() );

	CDmAttribute *pControlsAttr = pDmeComboOp->GetAttribute( "controls", AT_ELEMENT_ARRAY );
	if ( !pControlsAttr )
		DMXEDIT_WARNING_RETURN_FALSE( "No 'controls' attribute on ComboOp \"%s\"", pDmeComboOp->GetName() );

	CDmrElementArray< CDmElement > controlsAttr( pControlsAttr );
	if ( nControlIndex < controlsAttr.Count() )
	{
		CDmElement *pControlElement = controlsAttr[ nControlIndex ];
		if ( pControlElement )
		{
			pControlElement->SetValue( "flexMin", flMin );
			pControlElement->SetValue( "flexMax", flMax );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeModel *GetDmeModel( CDmeMesh *pDmeMesh )
{
	if ( !pDmeMesh )
		return NULL;

	// Find the DmeModel of the destination mesh scene
	CDmeDag *pDmeDag = pDmeMesh->GetParent();
	while ( pDmeDag && !pDmeDag->IsA( CDmeModel::GetStaticTypeSymbol() ) )
	{
		pDmeDag = pDmeDag->GetParent();
	}

	return CastElement< CDmeModel >( pDmeDag );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmAttribute *GetQcModelElementsAttr( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeModel *pDmeModel = GetDmeModel( pDmeMesh );
	if ( !pDmeModel )
		DMXEDIT_WARNING_RETURN_NULL( "Can't Find DmeModel for mesh \"%s\"", pDmeMesh->GetName() );

	CDmAttribute *pDmAttr = pDmeModel->GetAttribute( "qcModelElements" );
	if ( !pDmAttr )
	{
		pDmAttr = pDmeModel->AddAttribute( "qcModelElements", AT_ELEMENT_ARRAY );
	}

	if ( !pDmAttr || pDmAttr->GetType() != AT_ELEMENT_ARRAY )
		DMXEDIT_WARNING_RETURN_NULL( "Can't find or create %s.qcModelElements element array attribute", pDmeModel->GetName() );

	return pDmAttr;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Eyeball(
	CDmeMesh *pDmeMesh,
	const char *pszName,
	const char *pszBoneName,
	const Vector &vPosition,
	const char *pszMaterialName,
	const float flDiameter,
	const float flAngle,
	const float flPupilScale )
{
	CDmAttribute *pDmAttr = GetQcModelElementsAttr( pDmeMesh );
	if ( !pDmAttr )
		return false;

	CDmrElementArray<> qcModelElements( pDmAttr );

	CDmeEyeball *pDmeEyeball = CreateElement< CDmeEyeball >( pszName, pDmAttr->GetOwner()->GetFileId() );
	if ( !pDmeEyeball )
		DMXEDIT_WARNING_RETURN_FALSE( "Can't create DmeEyeball element" );

	pDmeEyeball->m_sParentBoneName.Set( pszBoneName );
	pDmeEyeball->m_vPosition.Set( vPosition );
	pDmeEyeball->m_sMaterialName.Set( pszMaterialName );
	pDmeEyeball->m_flRadius.Set( flDiameter / 2.0f );
	pDmeEyeball->m_flYawAngle.Set( flAngle );
	pDmeEyeball->m_flIrisScale.Set( flPupilScale );

	qcModelElements.AddToTail( pDmeEyeball );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Eyelid(
	CDmeMesh *pDmeMesh,
	bool bUpper,
	const char *pszLowererFlex,
	float flLowererHeight,
	const char *pszNeutralFlex,
	float flNeutralHeight,
	const char *pszRaiserFlex,
	float flRaiserHeight,
	const char *pszRightMaterialName,
	const char *pszLeftMaterialName )
{
	CDmAttribute *pDmAttr = GetQcModelElementsAttr( pDmeMesh );
	if ( !pDmAttr )
		return false;

	CDmrElementArray<> qcModelElements( pDmAttr );

	CDmeEyelid *pDmeEyelid = CreateElement< CDmeEyelid >( bUpper ? "upper" : "lower", pDmAttr->GetOwner()->GetFileId() );
	if ( !pDmeEyelid )
		DMXEDIT_WARNING_RETURN_FALSE( "Can't create DmeEyelid element" );

	pDmeEyelid->m_bUpper.Set( bUpper );
	pDmeEyelid->m_sLowererFlex.Set( pszLowererFlex );
	pDmeEyelid->m_flLowererHeight.Set( flLowererHeight );
	pDmeEyelid->m_sNeutralFlex.Set( pszNeutralFlex );
	pDmeEyelid->m_flNeutralHeight.Set( flNeutralHeight );
	pDmeEyelid->m_sRaiserFlex.Set( pszRaiserFlex );
	pDmeEyelid->m_flRaiserHeight.Set( flRaiserHeight );
	pDmeEyelid->m_sRightEyeballName.Set( pszRightMaterialName );
	pDmeEyelid->m_sLeftEyeballName.Set( pszLeftMaterialName );

	qcModelElements.AddToTail( pDmeEyelid );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool Mouth(
	CDmeMesh *pDmeMesh,
	int nMouthNumber,
	const char *pszFlexControllerName,
	const char *pszBoneName,
	const Vector &vForward )
{
	CDmAttribute *pDmAttr = GetQcModelElementsAttr( pDmeMesh );
	if ( !pDmAttr )
		return false;

	CDmrElementArray<> qcModelElements( pDmAttr );

	CUtlString sMouthName( "mouth" );
	sMouthName += nMouthNumber;

	CDmeMouth *pDmeMouth = CreateElement< CDmeMouth >( sMouthName.Get(), pDmAttr->GetOwner()->GetFileId() );
	if ( !pDmeMouth )
		DMXEDIT_WARNING_RETURN_FALSE( "Can't create DmeMouth element" );

	pDmeMouth->m_nMouthNumber.Set( nMouthNumber );
	pDmeMouth->m_sFlexControllerName.Set( pszFlexControllerName );
	pDmeMouth->m_sBoneName.Set( pszBoneName );
	pDmeMouth->m_vForward.Set( vForward );

	qcModelElements.AddToTail( pDmeMouth );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ClearFlexRules( CDmeMesh *pDmeMesh )
{
	DMXEDIT_MESH_WARNING_RETURN_NULL( pDmeMesh );

	CDmeFlexRules *pDmeFlexRules = GetFlexRulesForMesh( pDmeMesh );
	if ( pDmeFlexRules )
	{
		pDmeFlexRules->RemoveAllRules();
	}

	CDmeCombinationOperator *pDmeComboOp = GetComboOpFromMesh( pDmeMesh );
	if ( pDmeComboOp )
	{
		pDmeComboOp->RemoveAllControls();
	}

	return true;
}