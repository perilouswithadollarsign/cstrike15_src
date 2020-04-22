//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//	
//	Defines a logical entity which passes achievement related events to the gamerules system.

#include "cbase.h"
#include "gamerules.h"
#include "entityinput.h"
#include "entityoutput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Allows map logic to send achievement related events.
class CLogicAchievement : public CLogicalEntity
{
public:
	DECLARE_CLASS( CLogicAchievement, CLogicalEntity );

	CLogicAchievement();

protected:

	// Inputs
	void InputFireEvent( inputdata_t &inputdata );
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );
	void InputSetTargetPlayer( inputdata_t &inputdata );
	
	bool			m_bDisabled;
	string_t		m_iszAchievementName;				// Which achievement event this entity marks
	EHANDLE			m_hActivatingPlayer;

	COutputEvent	m_OnFired;

	DECLARE_DATADESC();
};


LINK_ENTITY_TO_CLASS( logic_achievement, CLogicAchievement );


BEGIN_DATADESC( CLogicAchievement )

	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_iszAchievementName, FIELD_STRING, "achievementname" ),

	DEFINE_FIELD( m_hActivatingPlayer, FIELD_EHANDLE ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "FireEvent", InputFireEvent ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetTargetPlayer", InputSetTargetPlayer ),

	// Outputs
	DEFINE_OUTPUT( m_OnFired, "OnFired" ),

END_DATADESC()



//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CLogicAchievement::CLogicAchievement(void)
{
	m_hActivatingPlayer = NULL;
	m_iszAchievementName = NULL_STRING;
}



//-----------------------------------------------------------------------------
// Purpose: Sends the achievement event to the achievement marking system.
//-----------------------------------------------------------------------------
void CLogicAchievement::InputFireEvent( inputdata_t &inputdata )
{
	// If we're active, and our string matched a valid achievement ID
	CBasePlayer *pPlayer = (CBasePlayer*)m_hActivatingPlayer.Get();
	if ( !m_bDisabled  && m_iszAchievementName != NULL_STRING )
	{
		UTIL_RecordAchievementEvent( m_iszAchievementName.ToCStr(), pPlayer );
		m_OnFired.FireOutput( inputdata.pActivator, this );
	}
}

//------------------------------------------------------------------------------
// Purpose: Turns on the relay, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CLogicAchievement::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns off the relay, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CLogicAchievement::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}

void CLogicAchievement::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}

void CLogicAchievement::InputSetTargetPlayer( inputdata_t &inputdata )
{ 
	m_hActivatingPlayer = inputdata.value.Entity();
}

