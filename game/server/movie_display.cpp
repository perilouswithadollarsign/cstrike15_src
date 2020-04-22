//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Allows movies to be played as a VGUI screen in the world
//
//=====================================================================================//

#include "cbase.h"
#include "EnvMessage.h"
#include "fmtstr.h"
#include "vguiscreen.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class CMovieDisplay : public CBaseEntity
{
public:

	DECLARE_CLASS( CMovieDisplay, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CMovieDisplay()
		: m_bForcePrecache( false )
	{
	}

	virtual ~CMovieDisplay();

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	virtual int  UpdateTransmitState();
	virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void OnRestore( void );

	void	ScreenVisible( bool bVisible );

	void	Disable( void );
	void	Enable( void );

	void	InputDisable( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );

	void	InputSetDisplayText( inputdata_t &inputdata );
	void	InputTakeOverAsMaster( inputdata_t &inputdata );

	void	InputSetMovie( inputdata_t &inputdata );

	void	InputSetUseCustomUVs( inputdata_t &inputdata );
	void	InputSetUMin( inputdata_t &inputdata );
	void	InputSetVMin( inputdata_t &inputdata );
	void	InputSetUMax( inputdata_t &inputdata );
	void	InputSetVMax( inputdata_t &inputdata );	

private:

	// Control panel
	void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
	void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
	void SpawnControlPanels( void );
	void RestoreControlPanels( void );

private:
	CNetworkVar( bool, m_bEnabled );
	CNetworkVar( bool, m_bLooping );
	CNetworkVar( bool, m_bStretchToFill );
	CNetworkVar( bool, m_bForcedSlave );
	bool m_bForcePrecache;

	CNetworkVar( bool, m_bUseCustomUVs );
	CNetworkVar( float, m_flUMin );
	CNetworkVar( float, m_flUMax );
	CNetworkVar( float, m_flVMin );
	CNetworkVar( float, m_flVMax );

	CNetworkString( m_szDisplayText, 128 );

	// Filename of the movie to play
	CNetworkString( m_szMovieFilename, 128 );
	string_t	m_strMovieFilename;

	// "Group" name.  Screens of the same group name will play the same movie at the same time
	// Effectively this lets multiple screens tune to the same "channel" in the world
	CNetworkString( m_szGroupName, 128 );
	string_t	m_strGroupName;

	int			m_iScreenWidth;
	int			m_iScreenHeight;

	bool		m_bDoFullTransmit;

	CHandle<CVGuiScreen>	m_hScreen;
};

LINK_ENTITY_TO_CLASS( vgui_movie_display, CMovieDisplay );

//-----------------------------------------------------------------------------
// Save/load 
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CMovieDisplay )

	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),

	DEFINE_AUTO_ARRAY_KEYFIELD( m_szDisplayText, FIELD_CHARACTER, "displaytext" ),

	DEFINE_AUTO_ARRAY( m_szMovieFilename, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_strMovieFilename, FIELD_STRING, "moviefilename" ),

	DEFINE_AUTO_ARRAY( m_szGroupName, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_strGroupName, FIELD_STRING, "groupname" ),

	DEFINE_KEYFIELD( m_iScreenWidth, FIELD_INTEGER, "width" ),
	DEFINE_KEYFIELD( m_iScreenHeight, FIELD_INTEGER, "height" ),
	DEFINE_KEYFIELD( m_bLooping, FIELD_BOOLEAN, "looping" ),
	DEFINE_KEYFIELD( m_bStretchToFill, FIELD_BOOLEAN, "stretch" ),
	DEFINE_KEYFIELD( m_bForcedSlave, FIELD_BOOLEAN, "forcedslave" ),
	DEFINE_KEYFIELD( m_bForcePrecache, FIELD_BOOLEAN, "forceprecache" ),

	DEFINE_FIELD( m_bUseCustomUVs, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flUMin, FIELD_FLOAT ),
	DEFINE_FIELD( m_flUMax, FIELD_FLOAT ),
	DEFINE_FIELD( m_flVMin, FIELD_FLOAT ),
	DEFINE_FIELD( m_flVMax, FIELD_FLOAT ),

	DEFINE_FIELD( m_bDoFullTransmit, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hScreen, FIELD_EHANDLE ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetDisplayText", InputSetDisplayText ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetMovie", InputSetMovie ),

	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetUseCustomUVs", InputSetUseCustomUVs ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetUMin", InputSetUMin ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetUMax", InputSetUMax ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetVMin", InputSetVMin ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetVMax", InputSetVMax ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TakeOverAsMaster", InputTakeOverAsMaster ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CMovieDisplay, DT_MovieDisplay )
	SendPropBool( SENDINFO( m_bEnabled ) ),
	SendPropBool( SENDINFO( m_bLooping ) ),
	SendPropString( SENDINFO( m_szMovieFilename ) ),
	SendPropString( SENDINFO( m_szGroupName ) ),
	SendPropBool( SENDINFO( m_bStretchToFill ) ),
	SendPropBool( SENDINFO( m_bForcedSlave ) ),
	SendPropBool( SENDINFO( m_bUseCustomUVs ) ),
	SendPropFloat( SENDINFO( m_flUMin ) ),
	SendPropFloat( SENDINFO( m_flUMax ) ),
	SendPropFloat( SENDINFO( m_flVMin ) ),
	SendPropFloat( SENDINFO( m_flVMax ) ),
END_SEND_TABLE()

CMovieDisplay::~CMovieDisplay()
{
	DestroyVGuiScreen( m_hScreen.Get() );
}

//-----------------------------------------------------------------------------
// Read in Hammer data
//-----------------------------------------------------------------------------
bool CMovieDisplay::KeyValue( const char *szKeyName, const char *szValue ) 
{
	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		Assert( GetMoveParent() == NULL );
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CMovieDisplay::UpdateTransmitState()
{
	if ( m_bDoFullTransmit )
	{
		m_bDoFullTransmit = false;
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return SetTransmitState( FL_EDICT_FULLCHECK );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways )
{
	// Are we already marked for transmission?
	if ( pInfo->m_pTransmitEdict->Get( entindex() ) )
		return;

	BaseClass::SetTransmit( pInfo, bAlways );

	// Force our screen to be sent too.
	m_hScreen->SetTransmit( pInfo, bAlways );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Spawn( void )
{
	// Move the strings into a networkable form
	Q_strcpy( m_szMovieFilename.GetForModify(), m_strMovieFilename.ToCStr() );
	Q_strcpy( m_szGroupName.GetForModify(), m_strGroupName.ToCStr() );

	Precache();

	BaseClass::Spawn();

	m_bEnabled = false;

	SpawnControlPanels();

	ScreenVisible( m_bEnabled );

	m_bDoFullTransmit = true;

	m_bUseCustomUVs = false;
	m_flUMin = 0;
	m_flUMax = 1;
	m_flVMin = 0;
	m_flVMax = 1;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Precache( void )
{
	BaseClass::Precache();

	PrecacheVGuiScreen( "movie_display_screen" );
	if ( m_bForcePrecache )
	{
		DevMsg( "Precaching vgui_movie_display %s with movie %s\n", m_iName->ToCStr(), m_szMovieFilename.Get() );
		PrecacheMovie( m_szMovieFilename );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::OnRestore( void )
{
	BaseClass::OnRestore();

	m_bDoFullTransmit = true;

	RestoreControlPanels();

	ScreenVisible( m_bEnabled );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::ScreenVisible( bool bVisible )
{
	// Set its active state
	m_hScreen->SetActive( bVisible );

	if ( bVisible )
	{
		m_hScreen->RemoveEffects( EF_NODRAW );
	}
	else
	{
		m_hScreen->AddEffects( EF_NODRAW );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Disable( void )
{
	if ( !m_bEnabled )
		return;

	m_bEnabled = false;

	ScreenVisible( false );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::Enable( void )
{
	if ( m_bEnabled )
		return;

	m_bEnabled = true;

	ScreenVisible( true );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputDisable( inputdata_t &inputdata )
{
	Disable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputEnable( inputdata_t &inputdata )
{
	Enable();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUseCustomUVs( inputdata_t &inputdata )
{
	m_bUseCustomUVs = inputdata.value.Bool();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUMin( inputdata_t &inputdata )
{
	m_flUMin = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetUMax( inputdata_t &inputdata )
{
	m_flUMax = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetVMin( inputdata_t &inputdata )
{
	m_flVMin = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetVMax( inputdata_t &inputdata )
{
	m_flVMax = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputTakeOverAsMaster( inputdata_t &inputdata )
{
	Enable();

	EntityMessageBegin( this );
		WRITE_BYTE( 0 );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetDisplayText( inputdata_t &inputdata )
{
	Q_strcpy( m_szDisplayText.GetForModify(), inputdata.value.String() );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::InputSetMovie( inputdata_t &inputdata )
{
	Q_strncpy( m_szMovieFilename.GetForModify(), inputdata.value.String(), 128 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::GetControlPanelInfo( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "movie_display_screen";
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::GetControlPanelClassName( int nPanelIndex, const char *&pPanelName )
{
	pPanelName = "vgui_screen";
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CMovieDisplay::SpawnControlPanels()
{
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		float flWidth = m_iScreenWidth;
		float flHeight = m_iScreenHeight;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, this, this, 0 );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( true );
		pScreen->MakeVisibleOnlyToTeammates( false );
		pScreen->SetTransparency( true );
		m_hScreen = pScreen;

		return;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMovieDisplay::RestoreControlPanels( void )
{
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		const char *pScreenName;
		GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		CVGuiScreen *pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( NULL, pScreenClassname );

		while ( ( pScreen && pScreen->GetOwnerEntity() != this ) || Q_strcmp( pScreen->GetPanelName(), pScreenName ) != 0 )
		{
			pScreen = (CVGuiScreen *)gEntList.FindEntityByClassname( pScreen, pScreenClassname );
		}

		if ( pScreen )
		{
			m_hScreen = pScreen;
			m_hScreen->SetActive( true );
		}

		return;
	}
}
