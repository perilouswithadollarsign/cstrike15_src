//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Declares the base class for all paint power users.
//
//=============================================================================//
#ifndef PAINT_POWER_USER_H
#define PAINT_POWER_USER_H

#include <utility>
#include <algorithm>
#include "gamestringpool.h"
#include "paint_power_user_interface.h"
#include "portal_player_shared.h"
#include "paint_power_info.h"

extern ConVar sv_enable_paint_power_user_debug;

//#define PAINT_POWER_USER_DEBUG

char const* const PAINT_POWER_USER_DATA_CLASS_NAME = "PaintPowerUser";
const int MAX_PAINT_SURFACE_CONTEXT_LENGTH = 32;

template< typename T >
inline T* GetBegin( CUtlVector<T>& v )
{
	return v.Base();
}


template< typename T >
inline T* GetEnd( CUtlVector<T>& v )
{
	return v.Base() + v.Count();
}


template< typename T >
inline const T* GetConstBegin( const CUtlVector<T>& v )
{
	return v.Base();
}


template< typename T >
inline const T* GetConstEnd( const CUtlVector<T>& v )
{
	return v.Base() + v.Count();
}


template< typename T >
inline const std::pair< T*, T* > GetRange( CUtlVector< T >& v )
{
	return std::pair< T*, T* >( v.Base(), v.Base() + v.Count() );
}


template< typename T >
inline const std::pair< const T*, const T* > GetConstRange( const CUtlVector< T >& v )
{
	return std::pair< const T*, const T* >( v.Base(), v.Base() + v.Count() );
}


template< typename IteratorType >
inline int GetCountFromRange( const std::pair< IteratorType, IteratorType >& range )
{
	return range.second - range.first;
}


template< typename IteratorType >
inline bool IsEmptyRange( const std::pair< IteratorType, IteratorType >& range )
{
	return range.first == range.second;
}


//=============================================================================
// class PaintPowerUser
// Purpose: Base class for entities which use paint powers.
//=============================================================================
template< typename BaseEntityType >
class PaintPowerUser : public BaseEntityType, public IPaintPowerUser
{
	DECLARE_CLASS( PaintPowerUser< BaseEntityType >, BaseEntityType );

	#if defined( CLIENT_DLL ) && !defined( NO_ENTITY_PREDICTION )
	DECLARE_PREDICTABLE();
	static const datamap_t PredMapInit();
	#endif

public:
	//-------------------------------------------------------------------------
	// Constructor/Virtual Destructor
	//-------------------------------------------------------------------------
	PaintPowerUser();
	virtual ~PaintPowerUser();

	//-------------------------------------------------------------------------
	// Public Accessors
	//-------------------------------------------------------------------------
	virtual const PaintPowerConstRange GetPaintPowers() const;
	virtual const PaintPowerInfo_t& GetPaintPower( unsigned powerType ) const;
	virtual const PaintPowerInfo_t* FindHighestPriorityActivePaintPower() const;

	//-------------------------------------------------------------------------
	// Paint Power Effects
	//-------------------------------------------------------------------------
	virtual void AddSurfacePaintPowerInfo( const PaintPowerInfo_t& contact, char const* context /*= 0*/ );
	virtual void UpdatePaintPowers();

	//-------------------------------------------------------------------------
	// Protected Types
	//-------------------------------------------------------------------------
	typedef CUtlVector< PaintPowerInfo_t > PaintPowerInfoVector;

	virtual void ChooseActivePaintPowers( PaintPowerInfoVector& activePowers ) = 0;	// Default provided
	void ClearSurfacePaintPowerInfo();

protected:

	//-------------------------------------------------------------------------
	// Paint Power Effects
	//-------------------------------------------------------------------------
	typedef int (*PaintPowerInfoCompare)( const PaintPowerInfo_t*, const PaintPowerInfo_t* );
	void PrioritySortSurfacePaintPowerInfo( PaintPowerInfoCompare comp );

	PaintPowerState ActivatePaintPower( PaintPowerInfo_t& power );
	PaintPowerState UsePaintPower( PaintPowerInfo_t& power );
	PaintPowerState DeactivatePaintPower( PaintPowerInfo_t& power );

	void MapSurfacesToPowers();
	bool SurfaceInfoContainsPower( const PaintPowerInfo_t& power, char const* context = 0 ) const;

	//-------------------------------------------------------------------------
	// Protected Accessors
	//-------------------------------------------------------------------------
	const PaintPowerConstRange GetSurfacePaintPowerInfo( char const* context = 0 ) const;
	bool HasAnySurfacePaintPowerInfo() const;

	//-------------------------------------------------------------------------
	// Protected Mutators
	//-------------------------------------------------------------------------
	void ForceSetPaintPower( const PaintPowerInfo_t& powerInfo );
	void ForcePaintPowerToState( PaintPowerType type, PaintPowerState newState );

private:
	//-------------------------------------------------------------------------
	// Private Types
	//-------------------------------------------------------------------------
	struct ContextSurfacePaintPowerInfo_t
	{
		PaintPowerInfoVector paintPowerInfo;
		string_t context;
	};

	typedef CUtlVector< ContextSurfacePaintPowerInfo_t > ContextPaintPowerInfoArray;

	//-------------------------------------------------------------------------
	// Private Data
	//-------------------------------------------------------------------------
	PaintPowerInfo_t m_PaintPowers[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER];	// Current powers and their states
	PaintPowerInfoVector m_SurfacePaintPowerInfo;							// Array of paint power info from surfaces touched
	ContextPaintPowerInfoArray m_ContextSurfacePaintPowerInfo;				// Array of paint power info from surfaces under some context

	//-------------------------------------------------------------------------
	// Private Accessors
	//-------------------------------------------------------------------------
	const PaintPowerRange GetNonConstSurfacePaintPowerInfo( char const* context = 0 );
	int FindContextSurfacePaintPowerInfo( char const* context ) const;

	//-------------------------------------------------------------------------
	// Paint Power Effects
	//-------------------------------------------------------------------------
	virtual PaintPowerState ActivateSpeedPower( PaintPowerInfo_t& powerInfo ) = 0;
	virtual PaintPowerState UseSpeedPower( PaintPowerInfo_t& powerInfo ) = 0;
	virtual PaintPowerState DeactivateSpeedPower( PaintPowerInfo_t& powerInfo ) = 0;

	virtual PaintPowerState ActivateBouncePower( PaintPowerInfo_t& powerInfo ) = 0;
	virtual PaintPowerState UseBouncePower( PaintPowerInfo_t& powerInfo ) = 0;
	virtual PaintPowerState DeactivateBouncePower( PaintPowerInfo_t& powerInfo ) = 0;

	virtual PaintPowerState ActivateNoPower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState UseNoPower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState DeactivateNoPower( PaintPowerInfo_t& powerInfo );
};


//=============================================================================
// PaintPowerUser Implementation
//=============================================================================

//#define DEFINE_PRED_TYPEDESCRIPTION( name, fieldtype )						\
//	{ FIELD_EMBEDDED, #name, offsetof(classNameTypedef, name), 1, FTYPEDESC_SAVE | FTYPEDESC_KEY, NULL, NULL, NULL, &fieldtype::m_PredMap }

// OMFG HACK: A macro to individually add embedded types from arrays because the current macros don't handle arrays of embedded types properly
// OMFG TODO: Write a generic macro to work with templatized classes.
#define DEFINE_EMBEDDED_ARRAY_ELEMENT( elementType, arrayName, arrayIndex )						\
	{ FIELD_EMBEDDED, #arrayName"["#arrayIndex"]", offsetof(classNameTypedef, arrayName[arrayIndex]), 1, FTYPEDESC_SAVE | FTYPEDESC_KEY, NULL, NULL, NULL, &elementType::m_PredMap }


// OMFG HACK: Define the prediction table. The current macros don't work with templatized classes.
// OMFG TODO: Write a generic macro to work with templatized classes.
#if defined( CLIENT_DLL ) && !defined( NO_ENTITY_PREDICTION )

	template< typename BaseEntityType >
	datamap_t PaintPowerUser<BaseEntityType>::m_PredMap = PaintPowerUser<BaseEntityType>::PredMapInit();

	// Note: This type description table is unused but declared as part of DECLARE_PREDICTABLE. Initializing it here to get rid of warnings.
	template< typename BaseEntityType >
	typedescription_t PaintPowerUser<BaseEntityType>::m_PredDesc[1] = { { FIELD_VOID,0,0,0,0,0,0,0,0 } };

	template< typename BaseEntityType >
	datamap_t *PaintPowerUser<BaseEntityType>::GetPredDescMap( void )
	{
		return &m_PredMap;
	}

	template< typename BaseEntityType >
	const datamap_t PaintPowerUser<BaseEntityType>::PredMapInit()
	{
		typedef PaintPowerUser<BaseEntityType> classNameTypedef;
		static typedescription_t predDesc[] =
		{
			//{ FIELD_VOID,0,0,0,0,0,0,0,0 },
			//DEFINE_EMBEDDED_AUTO_ARRAY( m_PaintPowers ) // This only predicts the first element of the array right now
			DEFINE_EMBEDDED_ARRAY_ELEMENT( PaintPowerInfo_t, m_PaintPowers, BOUNCE_POWER ),
			DEFINE_EMBEDDED_ARRAY_ELEMENT( PaintPowerInfo_t, m_PaintPowers, SPEED_POWER ),
			DEFINE_EMBEDDED_ARRAY_ELEMENT( PaintPowerInfo_t, m_PaintPowers, NO_POWER )
		};

		datamap_t predMap = { predDesc, ARRAYSIZE( predDesc ), PAINT_POWER_USER_DATA_CLASS_NAME, &PaintPowerUser<BaseEntityType>::BaseClass::m_PredMap };

		return predMap;
	}
#endif // if defined( CLIENT_DLL ) && !defined( NO_ENTITY_PREDICTION )


template< typename BaseEntityType >
PaintPowerUser<BaseEntityType>::PaintPowerUser()
{
	for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
	{
		m_PaintPowers[i] = PaintPowerInfo_t();
		m_PaintPowers[i].m_State = INACTIVE_PAINT_POWER;
	}
}


template< typename BaseEntityType >
PaintPowerUser<BaseEntityType>::~PaintPowerUser()
{
}


template< typename BaseEntityType >
const PaintPowerConstRange PaintPowerUser<BaseEntityType>::GetPaintPowers() const
{
	return PaintPowerConstRange( m_PaintPowers, m_PaintPowers +	ARRAYSIZE(m_PaintPowers) );
}


template< typename BaseEntityType >
const PaintPowerInfo_t& PaintPowerUser<BaseEntityType>::GetPaintPower( unsigned powerType ) const
{
	AssertMsg( powerType < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER, "Out of bounds." );
	return m_PaintPowers[ powerType < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER ? powerType : NO_POWER ];
}


template< typename BaseEntityType >
const PaintPowerInfo_t* PaintPowerUser<BaseEntityType>::FindHighestPriorityActivePaintPower() const
{
	const PaintPowerInfo_t* pPower = 0;
	for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
	{
		if( m_PaintPowers[i].m_State == ACTIVE_PAINT_POWER )
		{
			pPower = m_PaintPowers + i;
			break;
		}
	}

	return pPower;
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::AddSurfacePaintPowerInfo( const PaintPowerInfo_t& contact, char const* context )
{
	if ( !engine->HasPaintmap() )
	{
		Warning( "MEMORY LEAK: adding surface paint powers in a level with no paintmaps.\n" );
		return;
	}

	// If there's no context, add it to the default list
	if( context == NULL)
	{
		m_SurfacePaintPowerInfo.AddToTail( contact );
	}
	// There is a context, so add it to the appropriate list
	else
	{
		// Search for context
		int index = FindContextSurfacePaintPowerInfo( context );

		// If not found, create it first
		if( index == m_ContextSurfacePaintPowerInfo.InvalidIndex() )
		{
			index = m_ContextSurfacePaintPowerInfo.Count();
			m_ContextSurfacePaintPowerInfo.EnsureCount( index + 1 );
			m_ContextSurfacePaintPowerInfo[index].context = AllocPooledString( context );
		}

		// Add the new surface info
		m_ContextSurfacePaintPowerInfo[index].paintPowerInfo.AddToTail( contact );
	}
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::ChooseActivePaintPowers( PaintPowerInfoVector& activePowers  )
{
	// Figure out colors/powers
	MapSurfacesToPowers();

	// If there is a contact with any surface
	if( m_SurfacePaintPowerInfo.Count() != 0 )
	{
		// Sort the surfaces by priority
		PrioritySortSurfacePaintPowerInfo( &DescendingPaintPriorityCompare );

		// The active power is the one with the highest priority
		activePowers.AddToTail( m_SurfacePaintPowerInfo.Head() );
	}
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::UpdatePaintPowers()
{
	// Only update if there's paint in the map
	if( engine->HasPaintmap() )
	{
		// Update which powers are active
		PaintPowerInfoVector activePowers;
		ChooseActivePaintPowers( activePowers );

		// Cache the powers
		PaintPowerInfo_t cachedPowers[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER];
		//V_memcpy( cachedPowers, m_PaintPowers, sizeof(PaintPowerInfo_t) * PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER );
		std::copy( m_PaintPowers, m_PaintPowers + PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER, cachedPowers );

		// Set all powers in a state other than inactive to deactivating
		for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
		{
			PaintPowerInfo_t& power = m_PaintPowers[i];
			power.m_State = IsInactivePower( power ) ? INACTIVE_PAINT_POWER : DEACTIVATING_PAINT_POWER;
		}

		// For each new active power
		PaintPowerConstRange activeRange = GetConstRange( activePowers );
		for( PaintPowerConstIter i = activeRange.first; i != activeRange.second; ++i )
		{
			// Set it as the current one
			const PaintPowerInfo_t& newPower = *i;
			const unsigned index = newPower.m_PaintPowerType;
			m_PaintPowers[index] = newPower;

			// If the old power was active and roughly the same, keep it active. Otherwise, activate it.
			const PaintPowerInfo_t& oldPower = cachedPowers[index];
			const bool stayActive = IsActivePower( oldPower ) && AreSamePower( oldPower, newPower );
			m_PaintPowers[index].m_State = stayActive ? ACTIVE_PAINT_POWER : ACTIVATING_PAINT_POWER;
		}

		// Clear the surface information
		// NOTE: Calling this after Activating/Using/Deactivating paint powers makes sticky boxes not very sticky
		//		 and that's why I moved it back over here. -Brett
		ClearSurfacePaintPowerInfo();

		// Update the state of each power
		for( unsigned i = 0; i < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER; ++i )
		{
			switch( m_PaintPowers[i].m_State )
			{	
				case ACTIVATING_PAINT_POWER:
					m_PaintPowers[i].m_State = ActivatePaintPower( m_PaintPowers[i] );
#if defined CLIENT_DLL
					RANDOM_CEG_TEST_SECRET_PERIOD( 127, 1023 );
#endif
					break;

				case ACTIVE_PAINT_POWER:
					m_PaintPowers[i].m_State = UsePaintPower( m_PaintPowers[i] );
					break;

				case DEACTIVATING_PAINT_POWER:
#if defined GAME_DLL
					RANDOM_CEG_TEST_SECRET_PERIOD( 937, 3821 );
#endif
					m_PaintPowers[i].m_State = DeactivatePaintPower( m_PaintPowers[i] );
					break;
			}
		}
	}
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::ClearSurfacePaintPowerInfo()
{
	m_SurfacePaintPowerInfo.RemoveAll();
	
	const int count = m_ContextSurfacePaintPowerInfo.Count();
	for( int i = 0; i < count; ++i )
	{
		m_ContextSurfacePaintPowerInfo[i].paintPowerInfo.RemoveAll();
	}
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::PrioritySortSurfacePaintPowerInfo( PaintPowerInfoCompare comp )
{
	// Sort the surfaces by priority
	m_SurfacePaintPowerInfo.Sort( comp );

	const int count = m_ContextSurfacePaintPowerInfo.Count();
	for( int i = 0; i < count; ++i )
	{
		m_ContextSurfacePaintPowerInfo[i].paintPowerInfo.Sort( comp );
	}
}


template< typename PaintPowerIterator >
void MapSurfacesToPowers( PaintPowerIterator begin, PaintPowerIterator end )
{
	for( PaintPowerIterator i = begin; i != end; ++i )
	{
		MapSurfaceToPower(*i);
	}
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::MapSurfacesToPowers()
{
	PaintPowerRange range = GetNonConstSurfacePaintPowerInfo();
	::MapSurfacesToPowers( range.first, range.second );

	const int count = m_ContextSurfacePaintPowerInfo.Count();
	for( int i = 0; i < count; ++i )
	{
		PaintPowerRange contextRange = GetRange( m_ContextSurfacePaintPowerInfo[i].paintPowerInfo );
		::MapSurfacesToPowers( contextRange.first, contextRange.second );
	}
}


template< typename BaseEntityType >
bool PaintPowerUser<BaseEntityType>::SurfaceInfoContainsPower( const PaintPowerInfo_t& power, char const* context ) const
{
	PaintPowerConstRange range = GetSurfacePaintPowerInfo( context );
	for( PaintPowerConstIter i = range.first; i != range.second; ++i )
	{
		if( AreSamePower(power, *i) )
			return true;
	}

	return false;
}


template< typename BaseEntityType >
const PaintPowerRange PaintPowerUser<BaseEntityType>::GetNonConstSurfacePaintPowerInfo( char const* context )
{
	if( !context )
		return GetRange( m_SurfacePaintPowerInfo );

	const int i = FindContextSurfacePaintPowerInfo( context );
#ifdef PAINT_POWER_USER_DEBUG
	if( i == m_ContextSurfacePaintPowerInfo.InvalidIndex() )
		Warning( "Trying to get a range that doesn't exist.\n" );
#endif
	return i != m_ContextSurfacePaintPowerInfo.InvalidIndex() ? GetRange( m_ContextSurfacePaintPowerInfo[i].paintPowerInfo ) : PaintPowerRange( (PaintPowerIter)0, (PaintPowerIter)0 );
}


template< typename BaseEntityType >
int PaintPowerUser<BaseEntityType>::FindContextSurfacePaintPowerInfo( char const* context ) const
{
	AssertMsg( context, "Null pointers are bad, and you should feel bad." );
	int index = m_ContextSurfacePaintPowerInfo.InvalidIndex();
	const int count = m_ContextSurfacePaintPowerInfo.Count();
	for( int i = 0; i < count; ++i )
	{
		if( !V_strncmp( STRING( m_ContextSurfacePaintPowerInfo[i].context ), context, MAX_PAINT_SURFACE_CONTEXT_LENGTH ) )
		{
			index = i;
			break;
		}
	}

	return index;
}


template< typename BaseEntityType >
PaintPowerState PaintPowerUser<BaseEntityType>::ActivatePaintPower( PaintPowerInfo_t& power )
{
	AssertMsg( power.m_State == ACTIVATING_PAINT_POWER, "Activating a paint power that's not trying to activate." );
	switch( power.m_PaintPowerType )
	{
		case BOUNCE_POWER:
			return ActivateBouncePower( power );

		case SPEED_POWER:
			return ActivateSpeedPower( power );

		case PORTAL_POWER:
			return ActivateNoPower( power );

		case REFLECT_POWER:
			return ActivateNoPower( power );

		case NO_POWER:
			return ActivateNoPower( power );

		default:
			AssertMsg( false, "Invalid power or power hasn't been added to PaintPowerUser properly." );
			return INACTIVE_PAINT_POWER;
	}
}


template< typename BaseEntityType >
PaintPowerState PaintPowerUser<BaseEntityType>::UsePaintPower( PaintPowerInfo_t& power )
{
	AssertMsg( power.m_State == ACTIVE_PAINT_POWER, "Using a power that hasn't been activated." );
	switch( power.m_PaintPowerType )
	{
		case BOUNCE_POWER:
			return UseBouncePower( power );

		case SPEED_POWER:
			return UseSpeedPower( power );

		case PORTAL_POWER:
			return UseNoPower( power );

		case REFLECT_POWER:
			return UseNoPower( power );

		case NO_POWER:
			return UseNoPower( power );

		default:
			AssertMsg( false, "Invalid power or power hasn't been added to PaintPowerUser properly." );
			return INACTIVE_PAINT_POWER;
	}
}


template< typename BaseEntityType >
PaintPowerState PaintPowerUser<BaseEntityType>::DeactivatePaintPower( PaintPowerInfo_t& power )
{
	AssertMsg( power.m_State == DEACTIVATING_PAINT_POWER, "Deactivating a power that's not trying to deactivate." );
	switch( power.m_PaintPowerType )
	{
		case BOUNCE_POWER:
			return DeactivateBouncePower( power );

		case SPEED_POWER:
			return DeactivateSpeedPower( power );

		case PORTAL_POWER:
			return DeactivateNoPower( power );

		case REFLECT_POWER:
			return DeactivateNoPower( power );

		case NO_POWER:
			return DeactivateNoPower( power );

		default:
			AssertMsg( false, "Invalid power or power hasn't been added to PaintPowerUser properly." );
			return INACTIVE_PAINT_POWER;
	}
}


template< typename BaseEntityType >
const PaintPowerConstRange PaintPowerUser<BaseEntityType>::GetSurfacePaintPowerInfo( char const* context ) const
{
	if( !context )
		return GetConstRange( m_SurfacePaintPowerInfo );

	const int i = FindContextSurfacePaintPowerInfo( context );
#ifdef PAINT_POWER_USER_DEBUG
	if( i == m_ContextSurfacePaintPowerInfo.InvalidIndex() )
		Warning( "Trying to get a range that doesn't exist.\n" );
#endif
	return i != m_ContextSurfacePaintPowerInfo.InvalidIndex() ? GetConstRange( m_ContextSurfacePaintPowerInfo[i].paintPowerInfo ) : PaintPowerConstRange( (PaintPowerConstIter)0, (PaintPowerConstIter)0 );
}


template< typename BaseEntityType >
bool PaintPowerUser<BaseEntityType>::HasAnySurfacePaintPowerInfo() const
{
	bool hasSurfaceInfo = m_SurfacePaintPowerInfo.Count() != 0;
	const int count = m_ContextSurfacePaintPowerInfo.Count();
	for( int i = 0; i < count && !hasSurfaceInfo; ++i )
	{
		hasSurfaceInfo = m_ContextSurfacePaintPowerInfo[i].paintPowerInfo.Count() != 0;
	}

	return hasSurfaceInfo;
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::ForceSetPaintPower( const PaintPowerInfo_t& powerInfo )
{
	m_PaintPowers[powerInfo.m_PaintPowerType] = powerInfo;
}


template< typename BaseEntityType >
void PaintPowerUser<BaseEntityType>::ForcePaintPowerToState( PaintPowerType type, PaintPowerState newState )
{
	// TODO: Implement some error checking for edge cases here.
	m_PaintPowers[type].m_State = newState;
}


template< typename BaseEntityType >
PaintPowerState PaintPowerUser<BaseEntityType>::ActivateNoPower( PaintPowerInfo_t& powerInfo )
{
	return ACTIVE_PAINT_POWER;
}


template< typename BaseEntityType >
PaintPowerState PaintPowerUser<BaseEntityType>::UseNoPower( PaintPowerInfo_t& powerInfo )
{
	return ACTIVE_PAINT_POWER;
}


template< typename BaseEntityType >
PaintPowerState PaintPowerUser<BaseEntityType>::DeactivateNoPower( PaintPowerInfo_t& powerInfo )
{
	return INACTIVE_PAINT_POWER;
}


#endif // ifndef PAINT_POWER_USER_H
