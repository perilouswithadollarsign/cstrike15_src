//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSLOADOUT_H
#define CSLOADOUT_H
#ifdef _WIN32
#pragma once
#endif

const int cMaxEquipment = 6;  // if this # changes, bump version # cl_titledataversionblock3 and loadoutData in TitleData1 block for title data storageCCSSteamStats::SyncCSLoadoutsToTitleData
const int cMaxLoadouts = 4;	// if this # changes, bump version # cl_titledataversionblock3 and loadoutData in TitleData1 block  for title data storageCCSSteamStats::SyncCSLoadoutsToTitleData

class CCSEquipmentLoadout
{
public:
    CCSEquipmentLoadout()
    {
        ClearLoadout();
    }

    void ClearLoadout( void )
    {
    	SetEquipmentID( WEAPON_NONE );
    }

    CCSEquipmentLoadout& operator=( const CCSEquipmentLoadout& in_rhs )
    {
    	m_EquipmentID = in_rhs.m_EquipmentID;
		m_EquipmentPos = in_rhs.m_EquipmentPos;
    	m_Quantity = in_rhs.m_Quantity;

        return *this;
    }


    bool isEmpty() const 
    {
        return ( ( GetEquipmentID() == WEAPON_NONE ) || ( GetQuantity() == 0 ) );
    }

	CSWeaponID GetEquipmentID() const 
	{
		return m_EquipmentID;
	}

	void SetEquipmentID( CSWeaponID newid )
	{
		m_EquipmentID = newid;

		if ( newid == WEAPON_NONE )
		{
			m_Quantity = 0;
		}
	}

	int GetEquipmentPos() const 
	{
		return m_EquipmentPos;
	}

	void SetEquipmentPos( int pos )
	{
		m_EquipmentPos = pos;
	}

	int GetQuantity() const 
	{
		return m_Quantity;
	}

	void SetQuantity( int value )
	{
		m_Quantity = value;

		if ( !m_Quantity )
		{
			m_EquipmentID = WEAPON_NONE;
			m_EquipmentPos = 0;
		}
	}


	CSWeaponID  m_EquipmentID;
	int			m_EquipmentPos;
	int			m_Quantity;

};

inline const bool operator==( const CCSEquipmentLoadout& me, const CCSEquipmentLoadout& them )
{
	return ( ( me.GetEquipmentID() == them.GetEquipmentID() ) && ( me.GetQuantity() == them.GetQuantity() ) );
}

inline const bool operator!=( const CCSEquipmentLoadout& me, const CCSEquipmentLoadout& them )
{
	return !( me == them );
}

class CCSLoadout
{
protected:
	enum
	{
		HAS_TASER	= 0x1,
		HAS_BOMB	= 0x2,
		HAS_DEFUSE	= 0x4,
	};

	bool GetFlag( uint8 flag ) const
	{
		return ( m_flags & flag ) != 0;
	}

	void SetFlag( uint8 flag, bool setIt )
	{
		m_flags &= ~flag;
		if ( setIt )
			m_flags |= flag;
	}


public:
    CCSLoadout()
    {
        ClearLoadout();
    }

    void ClearLoadout( void )
    {
        m_primaryWeaponID = WEAPON_NONE;
        m_secondaryWeaponID = WEAPON_NONE;
		m_primaryWeaponItemPos = 0;
		m_secondaryWeaponItemPos = 0;
        m_flags = 0;

        for ( int i = 0; i < cMaxEquipment; i++ )
        {
            m_EquipmentArray[i].ClearLoadout();
        }
    }

    CCSLoadout& operator=( const CCSLoadout& in_rhs )
    {
        m_primaryWeaponID = in_rhs.m_primaryWeaponID;
        m_secondaryWeaponID = in_rhs.m_secondaryWeaponID;
		m_primaryWeaponItemPos = in_rhs.m_primaryWeaponItemPos;
		m_secondaryWeaponItemPos = in_rhs.m_secondaryWeaponItemPos;
		m_flags = in_rhs.m_flags;

        for ( int i = 0; i < cMaxEquipment; i++ )
        {
            m_EquipmentArray[i] = in_rhs.m_EquipmentArray[i];
        }

        return *this;
    }

    bool operator==( const CCSLoadout& in_rhs ) const
    {
        if ( m_primaryWeaponID != in_rhs.m_primaryWeaponID )
		{
            return false;
		}

        if ( m_secondaryWeaponID != in_rhs.m_secondaryWeaponID )
		{
            return false;
		}

		if ( m_flags != in_rhs.m_flags )
		{
            return false;
		}

        for ( int i = 0; i < cMaxEquipment; i++ )
        {
            if ( m_EquipmentArray[i].GetEquipmentID() != WEAPON_NONE )
            {
                bool bMatch = false;
                for ( int j = 0; j < cMaxEquipment; j++ )
                {
                    if ( in_rhs.m_EquipmentArray[j] == m_EquipmentArray[i] )
                        bMatch = true;
                }

                if ( bMatch == false )
				{
                    return false;
				}
            }
        }

        for ( int i = 0; i < cMaxEquipment; i++ )
        {
            if ( in_rhs.m_EquipmentArray[i].GetEquipmentID() != WEAPON_NONE )
            {
                bool bMatch = false;
                for ( int j = 0; j < cMaxEquipment; j++ )
                {
                    if ( m_EquipmentArray[j] == in_rhs.m_EquipmentArray[i] )
                        bMatch = true;
                }

                if ( bMatch == false )
				{
                    return false;
				}
            }
        }

        return true;
    }

    inline bool operator!=( const CCSLoadout& in_rhs ) const
	{
    	return !( *this == in_rhs );
	}

    bool isEmpty() const
    {
        if ( m_primaryWeaponID != WEAPON_NONE )
		{
            return false;
		}
        
        if ( m_secondaryWeaponID != WEAPON_NONE )
		{
            return false;
		}

        for ( int i = 0; i < cMaxEquipment; i++ )
        {
            if ( !m_EquipmentArray[i].isEmpty() )
                return false;
        }

        if ( m_flags )
		{
            return false;
		}

        return true;
    }

	int CountUniqueEquipment( void ) const
	{
		int result = 0;

		for ( result = 0; result < cMaxEquipment; result++ )
		{
			if ( m_EquipmentArray[result].isEmpty() )
				return result;
		}

		return 0;
	}

	bool GetHasTaser( void ) const
	{
		return GetFlag( HAS_TASER );
	}

	void SetHasTaser( bool value )
	{
		SetFlag( HAS_TASER, value );
	}

	bool GetHasBomb( void ) const
	{
		return GetFlag( HAS_BOMB );
	}

	void SetHasBomb( bool value )
	{
		SetFlag( HAS_BOMB, value );
	}

	bool GetHasDefuser( void ) const
	{
		return GetFlag( HAS_DEFUSE );
	}

	void SetHasDefuser( bool value )
	{
		SetFlag( HAS_DEFUSE, value );
	}

	CSWeaponID ReportFirstWeapon( void )
	{
		if ( m_primaryWeaponID )
			return m_primaryWeaponID;
		else if ( m_secondaryWeaponID )
			return m_secondaryWeaponID;
		else if ( GetHasTaser() )
			return WEAPON_TASER;
		else
			return m_EquipmentArray[0].GetEquipmentID();
	}

    // Data fields
    CSWeaponID          m_primaryWeaponID;
    CSWeaponID          m_secondaryWeaponID;
	int					m_primaryWeaponItemPos;
    int					m_secondaryWeaponItemPos;
    CCSEquipmentLoadout m_EquipmentArray[cMaxEquipment];
    uint8				m_flags;
};

class CCSBuyMenuLoadout
{

public:
	CCSBuyMenuLoadout()
	{
		ClearLoadout();
	}

	void ClearLoadout( void )
	{
		for ( int i = 0; i < ARRAYSIZE( m_WeaponID ); i++ )
		{
			m_WeaponID[ i ] = WEAPON_NONE;
		}
	}

	CCSBuyMenuLoadout& operator=( const CCSBuyMenuLoadout& in_rhs )
	{
		for ( int i = 0; i < ARRAYSIZE( m_WeaponID ); i++ )
		{
			m_WeaponID[ i ] = in_rhs.m_WeaponID[ i ];
		}

		return *this;
	}

	bool operator==( const CCSBuyMenuLoadout& in_rhs ) const
	{
		for ( int i = 0; i < ARRAYSIZE( m_WeaponID ); i++ )
		{
			if ( m_WeaponID[ i ] != in_rhs.m_WeaponID[ i ] )
			{
				return false;
			}
		}

		return true;
	}

	inline bool operator!=( const CCSBuyMenuLoadout& in_rhs ) const
	{
		return !( *this == in_rhs );
	}

	CEconItemView * GetWeaponView( loadout_positions_t pos, int nTeamNumber ) const;

	// Data fields
	itemid_t          m_WeaponID[ LOADOUT_POSITION_COUNT ];
};

inline CEconItemView * CCSBuyMenuLoadout::GetWeaponView( loadout_positions_t pos, int nTeamNumber ) const
{
	int nPos = pos;


	if ( ( nPos < 0 ) || ( nPos >= LOADOUT_POSITION_COUNT ) )
		return NULL;

	if ( !CSInventoryManager() || !CSInventoryManager()->GetLocalCSInventory() )
		return NULL;

	if ( m_WeaponID[ nPos ] == INVALID_ITEM_ID )
		return NULL;

	CEconItemView * pItemView = CSInventoryManager()->GetLocalCSInventory()->GetInventoryItemByItemID( m_WeaponID[ nPos ] );

	if ( pItemView && pItemView->IsValid() )
	{
		return pItemView;
	}
	else if ( CombinedItemIdIsDefIndexAndPaint( m_WeaponID[ nPos ] ) )
	{
		return CSInventoryManager()->FindOrCreateReferenceEconItem( m_WeaponID[ nPos ] );
	}
	else if ( CEconItemView * pItemView = CSInventoryManager()->GetItemInLoadoutForTeam( nTeamNumber, nPos ) )
	{
		return pItemView;
	}
	else
		return NULL;
}

extern CCSLoadout* GetBuyMenuLoadoutData( int in_teamID );

#endif //CSLOADOUT_H
