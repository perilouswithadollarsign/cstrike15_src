//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GAMEUI_UTIL_H
#define GAMEUI_UTIL_H
#ifdef _WIN32
#pragma once
#endif

// Set by the player who "owns" the gameui/settings/etc.
void SetGameUIActiveSplitScreenPlayerSlot( int nSlot );
int GetGameUIActiveSplitScreenPlayerSlot();

#include "tier1/convar.h"

void GameUI_MakeSafeName( const char *oldName, char *newName, int newNameBufSize );

//-----------------------------------------------------------------------------
// Useful for game ui since game ui has a single active "splitscreen" owner and since
//  it can gracefully handle non-FCVAR_SS vars without code changes required.
//-----------------------------------------------------------------------------
class CGameUIConVarRef
{
public:
	explicit	CGameUIConVarRef( const char *pName );
				CGameUIConVarRef( const char *pName, bool bIgnoreMissing );
	explicit	CGameUIConVarRef( IConVar *pConVar );

	void Init( const char *pName, bool bIgnoreMissing );
	bool IsValid() const;
	bool IsFlagSet( int nFlags ) const;

	// Get/Set value
	float GetFloat( int iSlot = -1 ) const;
	int GetInt( int iSlot = -1 ) const;
	float GetMin( int iSlot = -1 ) const;
	float GetMax( int iSlot = -1 ) const;
	bool GetBool( int iSlot = -1 ) const { return !!GetInt(); }
	const char *GetString( int iSlot = -1 ) const;

	void SetValue( const char *pValue, int iSlot = -1 );
	void SetValue( float flValue, int iSlot = -1 );
	void SetValue( int nValue, int iSlot = -1 );
	void SetValue( bool bValue, int iSlot = -1 );

	const char *GetName( int iSlot = -1 ) const;

	const char *GetDefault() const;

	const char *GetBaseName() const;

protected:
	int		GetActiveSplitScreenPlayerSlot() const;
private:
	struct cv_t
	{
		IConVar *m_pConVar;
		ConVar *m_pConVarState;
	};

	cv_t	m_Info[ MAX_SPLITSCREEN_CLIENTS ];
};

// In GAMUI we should never use the regular ConVarRef
#define ConVarRef CGameUIConVarRef

FORCEINLINE int CGameUIConVarRef::GetActiveSplitScreenPlayerSlot() const
{
	return GetGameUIActiveSplitScreenPlayerSlot();
}

//-----------------------------------------------------------------------------
// Did we find an existing convar of that name?
//-----------------------------------------------------------------------------
FORCEINLINE bool CGameUIConVarRef::IsFlagSet( int nFlags ) const
{
	return ( m_Info[ 0 ].m_pConVar->IsFlagSet( nFlags ) != 0 );
}

FORCEINLINE const char *CGameUIConVarRef::GetName( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVar->GetName();
}

FORCEINLINE const char *CGameUIConVarRef::GetBaseName() const
{
	return m_Info[ 0 ].m_pConVar->GetBaseName();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a float
//-----------------------------------------------------------------------------
FORCEINLINE float CGameUIConVarRef::GetFloat( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as an int
//-----------------------------------------------------------------------------
FORCEINLINE int CGameUIConVarRef::GetInt( int iSlot ) const 
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: Return ConVar value as a string, return "" for bogus string pointer, etc.
//-----------------------------------------------------------------------------
FORCEINLINE const char *CGameUIConVarRef::GetString( int iSlot ) const 
{
	Assert( !IsFlagSet( FCVAR_NEVER_AS_STRING ) );
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetString();
}

FORCEINLINE_CVAR  float CGameUIConVarRef::GetMax( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetMaxValue();
}

// [jbright] - Convenience function for retrieving the min value of the convar
FORCEINLINE_CVAR  float CGameUIConVarRef::GetMin( int iSlot ) const
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	return m_Info[ nSlot ].m_pConVarState->GetMinValue();
}


FORCEINLINE void CGameUIConVarRef::SetValue( const char *pValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( pValue );
}

FORCEINLINE void CGameUIConVarRef::SetValue( float flValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( flValue );
}

FORCEINLINE void CGameUIConVarRef::SetValue( int nValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( nValue );
}

FORCEINLINE void CGameUIConVarRef::SetValue( bool bValue, int iSlot )
{
	int nSlot = iSlot == -1 ? GetActiveSplitScreenPlayerSlot() : iSlot;
	m_Info[ nSlot ].m_pConVar->SetValue( bValue ? 1 : 0 );
}

FORCEINLINE const char *CGameUIConVarRef::GetDefault() const
{
	return m_Info[ 0 ].m_pConVarState->GetDefault();
}

//-----------------------------------------------------------------------------

class CGameUiSetActiveSplitScreenPlayerGuard
{
public:
	explicit CGameUiSetActiveSplitScreenPlayerGuard( int slot );
	~CGameUiSetActiveSplitScreenPlayerGuard();
private:
	int	 m_nSaveSlot;
};

#define GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( slot )	CGameUiSetActiveSplitScreenPlayerGuard g_UISSGuard( slot );

#endif // GAMEUI_UTIL_H
