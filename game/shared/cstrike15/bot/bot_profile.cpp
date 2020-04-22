//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"

#pragma warning( disable : 4530 )					// STL uses exceptions, but we are not compiling with them - ignore warning

#define DEFINE_DIFFICULTY_NAMES
#include "bot_profile.h"
#include "shared_util.h"

#include "bot.h"
#include "bot_util.h"
#include "cs_bot.h"		// BOTPORT: Remove this CS dependency

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BotProfileManager *TheBotProfiles = NULL;


//--------------------------------------------------------------------------------------------------------
/**
 * Generates a filename-decorated skin name
 */
static const char * GetDecoratedSkinName( const char *name, const char *filename )
{
	const int BufLen = _MAX_PATH + 64;
	static char buf[BufLen];
	Q_snprintf( buf, sizeof( buf ), "%s/%s", filename, name );
	return buf;
}

//--------------------------------------------------------------------------------------------------------------
const char* BotProfile::GetWeaponPreferenceAsString( int i ) const
{
	if ( i < 0 || i >= m_weaponPreferenceCount )
		return NULL;

	return WeaponIDToAlias( m_weaponPreference[ i ] );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this profile has a primary weapon preference
 */
bool BotProfile::HasPrimaryPreference() const
{
	for ( int i=0; i < m_weaponPreferenceCount; ++i )
	{
		if ( IsPrimaryWeapon( m_weaponPreference[i] ) )
			return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this profile has a pistol weapon preference
 */
bool BotProfile::HasPistolPreference() const
{
	for ( int i=0; i < m_weaponPreferenceCount; ++i )
		if ( IsSecondaryWeapon( m_weaponPreference[i] ) )
			return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Return true if this profile is valid for the specified team
 */
bool BotProfile::IsValidForTeam( int team ) const
{
	return ( team == TEAM_UNASSIGNED || m_teams == TEAM_UNASSIGNED || team == m_teams );
}


//--------------------------------------------------------------------------------------------------------------
/**
* Return true if this profile inherits from the specified template
*/
bool BotProfile::InheritsFrom( const char *name ) const
{
	if ( WildcardMatch( name, GetName() ) )
		return true;

	for ( int i=0; i<m_templates.Count(); ++i )
	{
		const BotProfile *queryTemplate = m_templates[i];
		if ( queryTemplate && queryTemplate->InheritsFrom( name ) )
		{
			return true;
		}
	}
	return false;
}

void BotProfile::SetName( const char* name )
{
	if ( name )
	{
		if ( m_name )
		{
			// Delete the current name if it exists
			delete [] m_name;
		}

		m_name = CloneString( name );
	}
}

const BotProfile* BotProfile::GetTemplate( int index ) const
{
	if ( index > 0 && index < m_templates.Count() )
	{
		return m_templates[index];
	}

	return NULL;
}


void BotProfile::Clone( const BotProfile* source_profile )
{
	if ( source_profile )
	{
		SetName( source_profile->GetName() );

		for ( int device = 0; device < BotProfileInputDevice::COUNT; ++device )
		{
			m_aggression = source_profile->GetAggression();

			m_skill = source_profile->GetSkill();

			m_teamwork = source_profile->GetTeamwork();

			m_aimFocusInitial = source_profile->GetAimFocusInitial();
			m_aimFocusDecay = source_profile->GetAimFocusDecay();
			m_aimFocusOffsetScale = source_profile->GetAimFocusOffsetScale();
			m_aimFocusInterval = source_profile->GetAimFocusInterval();

			m_weaponPreferenceCount = source_profile->GetWeaponPreferenceCount();
			for( int i = 0; i < source_profile->GetWeaponPreferenceCount(); ++i )
			{
				m_weaponPreference[i] = source_profile->GetWeaponPreference(i);
			}

			m_cost = source_profile->GetCost();

			m_skin = source_profile->GetSkin();

			m_difficultyFlags = source_profile->GetDifficultyFlags();

			m_voicePitch = source_profile->GetVoicePitch();

			m_reactionTime = source_profile->GetReactionTime();

			m_attackDelay = source_profile->GetAttackDelay();

			m_lookAngleMaxAccelNormal = source_profile->GetLookAngleMaxAccelerationNormal();
			m_lookAngleStiffnessNormal = source_profile->GetLookAngleStiffnessNormal();
			m_lookAngleDampingNormal = source_profile->GetLookAngleDampingNormal();
		
			m_lookAngleMaxAccelAttacking = source_profile->GetLookAngleMaxAccelerationAttacking();
			m_lookAngleStiffnessAttacking = source_profile->GetLookAngleStiffnessAttacking();
			m_lookAngleDampingAttacking = source_profile->GetLookAngleDampingAttacking();
		}

		m_teams = source_profile->GetTeams();

		m_voiceBank = source_profile->GetVoiceBank();

		m_prefersSilencer = source_profile->PrefersSilencer();

		for ( int i = 0; i < source_profile->GetTemplatesCount(); ++i )
		{
			m_templates.AddToTail( source_profile->GetTemplate( i ) );
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Constructor
 */
BotProfileManager::BotProfileManager( void )
{
	m_nextSkin = 0;
	for (int i=0; i<NumCustomSkins; ++i)
	{
		m_skins[i] = NULL;
		m_skinFilenames[i] = NULL;
		m_skinModelnames[i] = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Load the bot profile database
 */
void BotProfileManager::Init( const char *filename, unsigned int *checksum )
{
	FileHandle_t file = filesystem->Open( filename, "r" );

	if (!file)
	{
		if ( true ) // UTIL_IsGame( "czero" ) )
		{
			CONSOLE_ECHO( "WARNING: Cannot access bot profile database '%s'\n", filename );
		}
		return;
	}

	int dataLength = filesystem->Size( filename );
	char *dataPointer = new char[ dataLength ];
	int dataReadLength = filesystem->Read( dataPointer, dataLength, file );
	filesystem->Close( file );
	if ( dataReadLength > 0 )
	{
		// NULL-terminate based on the length read in, since Read() can transform \r\n to \n and
		// return fewer bytes than we were expecting.
		dataPointer[ dataReadLength - 1 ] = 0;
	}

	const char *dataFile = dataPointer;

	// compute simple checksum
	if (checksum)
	{
		*checksum = 0; // ComputeSimpleChecksum( (const unsigned char *)dataPointer, dataLength );
	}

	BotProfile defaultProfile;

	//
	// Parse the BotProfile.db into BotProfile instances
	//
	while( true )
	{
		dataFile = SharedParse( dataFile );
		if (!dataFile)
			break;

		char *token = SharedGetToken();

		bool isDefault = ( !stricmp( token, "Default" ));
		bool isTemplate = ( !stricmp( token, "Template" ));
		bool isCustomSkin = ( !stricmp( token, "Skin" ));

		if ( isCustomSkin )
		{
			const int BufLen = 64;
			char skinName[BufLen];

			// get skin name
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected skin name\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			Q_snprintf( skinName, sizeof( skinName ), "%s", token );

			// get attribute name
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'Model'\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			if (stricmp( "Model", token ))
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'Model'\n", filename );
				delete [] dataPointer;
				return;
			}

			// eat '='
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			if (strcmp( "=", token ))
			{
				CONSOLE_ECHO( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}

			// get attribute value
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected attribute value\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();

			const char *decoratedName = GetDecoratedSkinName( skinName, filename );
			bool skinExists = GetCustomSkinIndex( decoratedName ) > 0;
			if ( m_nextSkin < NumCustomSkins && !skinExists )
			{
				// decorate the name
				m_skins[ m_nextSkin ] = CloneString( decoratedName );

				// construct the model filename
				m_skinModelnames[ m_nextSkin ] = CloneString( token );
				m_skinFilenames[ m_nextSkin ] = new char[ strlen( token )*2 + strlen("models/player//.mdl") + 1 ];
				Q_snprintf( m_skinFilenames[ m_nextSkin ], sizeof( m_skinFilenames[ m_nextSkin ] ), "models/player/%s/%s.mdl", token, token );
				++m_nextSkin;
			}

			// eat 'End'
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'End'\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();
			if (strcmp( "End", token ))
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'End'\n", filename );
				delete [] dataPointer;
				return;
			}

			continue; // it's just a custom skin - no need to do inheritance on a bot profile, etc.
		}

		// encountered a new profile
		BotProfile *profile;

		if (isDefault)
		{
			profile = &defaultProfile;
		}
		else
		{
			profile = new BotProfile;

			// always inherit from Default
			*profile = defaultProfile;
		}

		// do inheritance in order of appearance
		if (!isTemplate && !isDefault)
		{
			const BotProfile *inherit = NULL;

			// template names are separated by "+"
			while(true)
			{
				char *c = strchr( token, '+' );
				if (c)
					*c = '\000';

				// find the given template name
				FOR_EACH_LL( m_templateList, it )
				{
					BotProfile *profile = m_templateList[ it ];
					if ( !stricmp( profile->GetName(), token ))
					{
						inherit = profile;
						break;
					}
				}

				if (inherit == NULL)
				{
					CONSOLE_ECHO( "Error parsing '%s' - invalid template reference '%s'\n", filename, token );
					delete [] dataPointer;
					return;
				}

				// inherit the data
				profile->Inherit( inherit, &defaultProfile );

				if (c == NULL)
					break;
				
				token = c+1;
			}
		}


		// get name of this profile
		if (!isDefault)
		{
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing '%s' - expected name\n", filename );
				delete [] dataPointer;
				return;
			}
			profile->m_name = CloneString( SharedGetToken() );

			/**
			 * HACK HACK
			 * Until we have a generalized means of storing bot preferences, we're going to hardcode the bot's
			 * preference towards silencers based on his name.
			 */
			if ( profile->m_name[0] % 2 )
			{
				profile->m_prefersSilencer = true;
			}
		}

		// read attributes for this profile
		bool isFirstWeaponPref = true;
		while( true )
		{
			// get next token
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'End'\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();

			// check for End delimiter
			if ( !stricmp( token, "End" ))
				break;

			// found attribute name - keep it
			char attributeName[64];
			strcpy( attributeName, token );

			// eat '='
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}

			token = SharedGetToken();
			if (strcmp( "=", token ))
			{
				CONSOLE_ECHO( "Error parsing %s - expected '='\n", filename );
				delete [] dataPointer;
				return;
			}

			// get attribute value
			dataFile = SharedParse( dataFile );
			if (!dataFile)
			{
				CONSOLE_ECHO( "Error parsing %s - expected attribute value\n", filename );
				delete [] dataPointer;
				return;
			}
			token = SharedGetToken();

			// store value in appropriate attribute
			if ( !stricmp( "Aggression", attributeName ) )
			{
				profile->m_aggression = (float)atof( token ) / 100.0f;
			}
			else if ( !stricmp( "Skill", attributeName ) )
			{
				profile->m_skill = (float)atof( token ) / 100.0f;
			}
			else if ( !stricmp( "Skin", attributeName ) )
			{
				profile->m_skin = atoi( token );
				if ( profile->m_skin == 0 )
				{
					// atoi() failed - try to look up a custom skin by name
					profile->m_skin = GetCustomSkinIndex( token, filename );
				}
			}
			else if ( !stricmp( "Teamwork", attributeName ))
			{
				profile->m_teamwork = (float)atof( token ) / 100.0f;
			}
			else if ( !V_stricmp( "AimFocusInitial", attributeName ) )
			{
				profile->m_aimFocusInitial = (float)atof( token );
			}
			else if ( !V_stricmp( "AimFocusDecay", attributeName ) )
			{
				profile->m_aimFocusDecay = (float)atof( token );
			}
			else if ( !V_stricmp( "AimFocusOffsetScale", attributeName ) )
			{
				profile->m_aimFocusOffsetScale = (float)atof( token );
			}
			else if ( !V_stricmp( "AimFocusInterval", attributeName ) )
			{
				profile->m_aimFocusInterval = (float)atof( token );
			}
			else if ( !stricmp( "Cost", attributeName ) )
			{
				profile->m_cost = atoi( token );
			}
			else if ( !stricmp( "VoicePitch", attributeName ) )
			{
				profile->m_voicePitch = atoi( token );
			}
			else if ( !stricmp( "VoiceBank", attributeName ) )
			{
				profile->m_voiceBank = FindVoiceBankIndex( token );
			}
			else if ( !stricmp( "WeaponPreference", attributeName ) )
			{
				ParseWeaponPreference( isFirstWeaponPref, profile->m_weaponPreferenceCount, profile->m_weaponPreference, token );
			}
			else if ( !stricmp( "ReactionTime", attributeName ) )
			{
				profile->m_reactionTime = (float)atof( token );

			}
			else if ( !stricmp( "AttackDelay", attributeName ))
			{
				profile->m_attackDelay = (float)atof( token );
			}
			else if ( !stricmp( "Difficulty", attributeName ))
			{
				// override inheritance
				ParseDifficultySetting( profile->m_difficultyFlags, token );
			}
			else if ( !stricmp( "Team", attributeName ))
			{
				if ( !stricmp( token, "T" ) )
				{
					profile->m_teams = TEAM_TERRORIST;
				}
				else if ( !stricmp( token, "CT" ) )
				{
					profile->m_teams = TEAM_CT;
				}
				else
				{
					profile->m_teams = TEAM_UNASSIGNED;
				}
			}
			else if ( !stricmp( "LookAngleMaxAccelNormal", attributeName ) )
			{
				profile->m_lookAngleMaxAccelNormal = atof( token );
			}
			else if ( !stricmp( "LookAngleStiffnessNormal", attributeName ) )
			{
				profile->m_lookAngleStiffnessNormal = atof( token );
			}
			else if ( !stricmp( "LookAngleDampingNormal", attributeName ) )
			{
				profile->m_lookAngleDampingNormal = atof( token );
			}
			else if ( !stricmp( "LookAngleMaxAccelAttacking", attributeName ) )
			{
				profile->m_lookAngleMaxAccelAttacking = atof( token );
			}
			else if ( !stricmp( "LookAngleStiffnessAttacking", attributeName ) )
			{
				profile->m_lookAngleStiffnessAttacking = atof( token );
			}
			else if ( !stricmp( "LookAngleDampingAttacking", attributeName ) )
			{
				profile->m_lookAngleDampingAttacking = atof( token );
			}
			else
			{
				CONSOLE_ECHO( "Error parsing %s - unknown attribute '%s'\n", filename, attributeName );
			}
		}

		if (!isDefault)
		{
			if (isTemplate)
			{
				// add to template list
				m_templateList.AddToTail( profile );
			}
			else
			{
				// add profile to the master list
				m_profileList.AddToTail( profile );
			}
		}
	}

	delete [] dataPointer;
}

void BotProfileManager::ParseDifficultySetting( unsigned char &difficultyFlags, char* token)
{
	// override inheritance
	difficultyFlags = 0;

	// parse bit flags
	while(true)
	{
		char *c = strchr( token, '+' );
		if (c)
		{
			*c = '\000';
		}

		for( int i=0; i<NUM_DIFFICULTY_LEVELS; ++i )
		{
			if ( !stricmp( BotDifficultyName[i], token ))
			{
				difficultyFlags |= (1 << i);
			}
		}

		if (c == NULL)
		{
			break;
		}
					
		token = c+1;
	}
}

void BotProfileManager::ParseWeaponPreference( bool &isFirstWeaponPref, int &weaponPreferenceCount, CSWeaponID* weaponPreference, char* token )
{
	// weapon preferences override parent prefs
	if (isFirstWeaponPref)
	{
		isFirstWeaponPref = false;
		weaponPreferenceCount = 0;
	}

	if ( !stricmp( token, "none" ))
	{
		weaponPreferenceCount = 0;
	}
	else
	{
		if (weaponPreferenceCount < BotProfile::MAX_WEAPON_PREFS)
		{
			weaponPreference[ weaponPreferenceCount++ ] = AliasToWeaponID( token );
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
BotProfileManager::~BotProfileManager( void )
{
	Reset();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Free all bot profiles
 */
void BotProfileManager::Reset( void )
{
	m_profileList.PurgeAndDeleteElements();
	m_templateList.PurgeAndDeleteElements();

	int i;

	for (i=0; i<NumCustomSkins; ++i)
	{
		if ( m_skins[i] )
		{
			delete[] m_skins[i];
			m_skins[i] = NULL;
		}
		if ( m_skinFilenames[i] )
		{
			delete[] m_skinFilenames[i];
			m_skinFilenames[i] = NULL;
		}
		if ( m_skinModelnames[i] )
		{
			delete[] m_skinModelnames[i];
			m_skinModelnames[i] = NULL;
		}
	}

	for ( i=0; i<m_voiceBanks.Count(); ++i )
	{
		delete[] m_voiceBanks[i];
	}
	m_voiceBanks.RemoveAll();
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns custom skin name at a particular index
 */
const char * BotProfileManager::GetCustomSkin( int index )
{
	if ( index < FirstCustomSkin || index > LastCustomSkin )
	{
		return NULL;
	}

	return m_skins[ index - FirstCustomSkin ];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns custom skin filename at a particular index
 */
const char * BotProfileManager::GetCustomSkinFname( int index )
{
	if ( index < FirstCustomSkin || index > LastCustomSkin )
	{
		return NULL;
	}

	return m_skinFilenames[ index - FirstCustomSkin ];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns custom skin modelname at a particular index
 */
const char * BotProfileManager::GetCustomSkinModelname( int index )
{
	if ( index < FirstCustomSkin || index > LastCustomSkin )
	{
		return NULL;
	}

	return m_skinModelnames[ index - FirstCustomSkin ];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Looks up a custom skin index by filename-decorated name (will decorate the name if filename is given)
 */
int BotProfileManager::GetCustomSkinIndex( const char *name, const char *filename )
{
	const char * skinName = name;
	if ( filename )
	{
		skinName = GetDecoratedSkinName( name, filename );
	}

	for (int i=0; i<NumCustomSkins; ++i)
	{
		if ( m_skins[i] )
		{
			if ( !stricmp( skinName, m_skins[i] ) )
			{
				return FirstCustomSkin + i;
			}
		}
	}
	return 0;
}


//--------------------------------------------------------------------------------------------------------
/**
 * return index of the (custom) bot phrase db, inserting it if needed
 */
int BotProfileManager::FindVoiceBankIndex( const char *filename )
{
	int index = 0;

	for ( int i=0; i<m_voiceBanks.Count(); ++i )
	{
		if ( !stricmp( filename, m_voiceBanks[i] ) )
		{
			return index;
		}
	}

	m_voiceBanks.AddToTail( CloneString( filename ) );
	return index;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Return random unused profile that matches the given difficulty level
 */
const BotProfile *BotProfileManager::GetRandomProfile( BotDifficultyType difficulty, int team, CSWeaponType weaponType, bool forceMatchHighestDifficulty ) const
{
	// count up valid profiles
	CUtlVector< const BotProfile * > profiles;
	FOR_EACH_LL( m_profileList, it )
	{
		const BotProfile *profile = m_profileList[ it ];

		// Match difficulty
		if ( forceMatchHighestDifficulty )
		{
			if ( !profile->IsMaxDifficulty( difficulty ) )
			{
				continue;
			}
		}
		else
		{
			if ( !profile->IsDifficulty( difficulty ) )
				continue;
		}

		// Prevent duplicate names
		if ( UTIL_IsNameTaken( profile->GetName() ) )
			continue;

		// Match team choice
		if ( !profile->IsValidForTeam( team ) )
			continue;

		// Match desired weapon
		if ( weaponType != WEAPONTYPE_UNKNOWN )
		{
			if ( !profile->GetWeaponPreferenceCount() )
				continue;

			if ( weaponType != WeaponClassFromWeaponID( (CSWeaponID)profile->GetWeaponPreference( 0 ) ) )
				continue;
		}

		profiles.AddToTail( profile );
	}

	if ( !profiles.Count() )
		return NULL;

	// select one at random
	int which = RandomInt( 0, profiles.Count()-1 );
	return profiles[which];
}

