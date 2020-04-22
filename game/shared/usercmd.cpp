//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "usercmd.h"
#include "bitbuf.h"
#include "checksum_md5.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TF2 specific, need enough space for OBJ_LAST items from tf_shareddefs.h
#define WEAPON_SUBTYPE_BITS	6

//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
ConVar net_showusercmd( "net_showusercmd", "0", 0, "Show user command encoding" );
#define LogUserCmd( msg, ... ) if ( net_showusercmd.GetInt() ) { ConDMsg( msg, __VA_ARGS__ ); }
#else
#define LogUserCmd( msg, ... ) NULL;
#endif

//-----------------------------------------------------------------------------
static bool WriteUserCmdDeltaInt( bf_write *buf, char *what, int from, int to, int bits = 32 )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %d -> %d\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to, bits );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaShort( bf_write *buf, char *what, int from, int to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %d -> %d\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteShort( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaFloat( bf_write *buf, char *what, float from, float to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteFloat( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaCoord( bf_write *buf, char *what, float from, float to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteBitCoord( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaAngle( bf_write *buf, char *what, float from, float to, int bits )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from, to );

		buf->WriteOneBit( 1 );
		buf->WriteBitAngle( to, bits );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

static bool WriteUserCmdDeltaVec3Coord( bf_write *buf, char *what, const Vector &from, const Vector &to )
{
	if ( from != to )
	{
		LogUserCmd( "\t%s %2.2f -> %2.2f\n", what, from.x, to.x );

		buf->WriteOneBit( 1 );
		buf->WriteBitVec3Coord( to );
		return true;
	}

	buf->WriteOneBit( 0 );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Write a delta compressed user command.
// Input  : *buf - 
//			*to - 
//			*from - 
// Output : static
//-----------------------------------------------------------------------------
void WriteUsercmd( bf_write *buf, const CUserCmd *to, const CUserCmd *from )
{
	LogUserCmd("WriteUsercmd: from=%d to=%d\n", from->command_number, to->command_number );

	WriteUserCmdDeltaInt( buf, "command_number", from->command_number + 1, to->command_number, 32 );
	WriteUserCmdDeltaInt( buf, "tick_count", from->tick_count + 1, to->tick_count, 32 );

	WriteUserCmdDeltaFloat( buf, "viewangles[0]", from->viewangles[0], to->viewangles[0] );
	WriteUserCmdDeltaFloat( buf, "viewangles[1]", from->viewangles[1], to->viewangles[1] );
	WriteUserCmdDeltaFloat( buf, "viewangles[2]", from->viewangles[2], to->viewangles[2] );

	// aim direction for motion controllers
	WriteUserCmdDeltaFloat( buf, "aimdirection[0]", from->aimdirection[0], to->aimdirection[0] );
	WriteUserCmdDeltaFloat( buf, "aimdirection[1]", from->aimdirection[1], to->aimdirection[1] );
	WriteUserCmdDeltaFloat( buf, "aimdirection[2]", from->aimdirection[2], to->aimdirection[2] );

	WriteUserCmdDeltaFloat( buf, "forwardmove", from->forwardmove, to->forwardmove );
	WriteUserCmdDeltaFloat( buf, "sidemove", from->sidemove, to->sidemove );
	WriteUserCmdDeltaFloat( buf, "upmove", from->upmove, to->upmove );
	WriteUserCmdDeltaInt( buf, "buttons", from->buttons, to->buttons, 32 );
	WriteUserCmdDeltaInt( buf, "impulse", from->impulse, to->impulse, 8 );

#if defined( INFESTED_DLL ) || defined( DOTA_DLL )
	if ( to->crosshairtrace != from->crosshairtrace )
	{
		buf->WriteOneBit( 1 );
		buf->WriteBitVec3Coord( to->crosshairtrace );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->weaponselect != from->weaponselect )
	{
		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to->weaponselect, MAX_EDICT_BITS );		
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->weaponsubtype != from->weaponsubtype )
	{
		buf->WriteOneBit( 1 );
		buf->WriteUBitLong( to->weaponsubtype, WEAPON_SUBTYPE_BITS );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}
#endif


#ifdef INFESTED_DLL // asw - check weapon subtype seperately, since we use it to say which marine we're controlling

	if ( to->crosshair_entity != from->crosshair_entity )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->crosshair_entity );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->forced_action != from->forced_action )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->forced_action );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->sync_kill_ent != from->sync_kill_ent )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->sync_kill_ent );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	WriteUserCmdDeltaVec3Coord( buf, "skill_dest", from->skill_dest, to->skill_dest );

	if ( to->skill_dest_ent != from->skill_dest_ent )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->skill_dest_ent );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

#else
	if ( WriteUserCmdDeltaInt( buf, "weaponselect", from->weaponselect, to->weaponselect, MAX_EDICT_BITS ) )
	{
		WriteUserCmdDeltaInt( buf, "weaponsubtype", from->weaponsubtype, to->weaponsubtype, WEAPON_SUBTYPE_BITS );
	}
#endif

	// TODO: Can probably get away with fewer bits.
	WriteUserCmdDeltaShort( buf, "mousedx", from->mousedx, to->mousedx );
	WriteUserCmdDeltaShort( buf, "mousedy", from->mousedy, to->mousedy );

#if defined( HL2_CLIENT_DLL )
	if ( to->entitygroundcontact.Count() != 0 )
	{
		LogUserCmd( "\t%s %d\n", "entitygroundcontact", to->entitygroundcontact.Count() );

		buf->WriteOneBit( 1 );
		buf->WriteShort( to->entitygroundcontact.Count() );
		int i;
		for (i = 0; i < to->entitygroundcontact.Count(); i++)
		{
			LogUserCmd( "\t\t%s %d\n", "entitygroundcontact[%d].entindex", i, to->entitygroundcontact[i].entindex );
			buf->WriteUBitLong( to->entitygroundcontact[i].entindex, MAX_EDICT_BITS );

			LogUserCmd( "\t\t%s %2.2f\n", "entitygroundcontact[%d].minheight", i, to->entitygroundcontact[i].minheight );
			buf->WriteBitCoord( to->entitygroundcontact[i].minheight );

			LogUserCmd( "\t\t%s %2.2f\n", "entitygroundcontact[%d].maxheight", i, to->entitygroundcontact[i].maxheight );
			buf->WriteBitCoord( to->entitygroundcontact[i].maxheight );
		}
	}
	else
	{
		buf->WriteOneBit( 0 );
	}
#endif

#if defined ( PORTAL2 )
	if ( to->player_held_entity != from->player_held_entity )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->player_held_entity );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if ( to->held_entity_was_grabbed_through_portal != from->held_entity_was_grabbed_through_portal )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->held_entity_was_grabbed_through_portal );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if( to->command_acknowledgements_pending != from->command_acknowledgements_pending )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->command_acknowledgements_pending );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

	if( to->predictedPortalTeleportations != from->predictedPortalTeleportations )
	{
		buf->WriteOneBit( 1 );
		buf->WriteByte( to->predictedPortalTeleportations );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}
#endif 

#ifdef DOTA_DLL
	if ( to->dota_unitorders.m_nOrderSequenceNumber != from->dota_unitorders.m_nOrderSequenceNumber )
	{
		buf->WriteOneBit( 1 );
		buf->WriteShort( to->dota_unitorders.m_nOrderSequenceNumber );
		buf->WriteShort( to->dota_unitorders.m_nUnits.Count() );

		int i;
		for ( i = 0; i < to->dota_unitorders.m_nUnits.Count(); i++)
		{
			buf->WriteUBitLong( to->dota_unitorders.m_nUnits[i], MAX_EDICT_BITS );
		}

		buf->WriteShort( to->dota_unitorders.m_nOrderType );
		buf->WriteShort( to->dota_unitorders.m_nTargetIndex );
		buf->WriteBitVec3Coord( to->dota_unitorders.m_vPosition );
		buf->WriteUBitLong( to->dota_unitorders.m_nAbilityIndex, MAX_EDICT_BITS );
	}
	else
	{
		buf->WriteOneBit( 0 );
	}

#endif

	if ( IsHeadTrackingEnabled() )
	{
		// TrackIR head angles
		WriteUserCmdDeltaAngle( buf, "headangles[0]", from->headangles[ 0 ], to->headangles[ 0 ], 16 );
		WriteUserCmdDeltaAngle( buf, "headangles[1]", from->headangles[ 1 ], to->headangles[ 1 ], 16 );
		WriteUserCmdDeltaAngle( buf, "headangles[2]", from->headangles[ 2 ], to->headangles[ 2 ], 8 );

		// TrackIR head offset
		WriteUserCmdDeltaVec3Coord( buf, "headoffset", from->headoffset, to->headoffset );
		// TrackIR
	}

}

//-----------------------------------------------------------------------------
// Purpose: Read in a delta compressed usercommand.
// Input  : *buf - 
//			*move - 
//			*from - 
// Output : static void ReadUsercmd
//-----------------------------------------------------------------------------
void ReadUsercmd( bf_read *buf, CUserCmd *move, CUserCmd *from )
{
	// Assume no change
	*move = *from;

	if ( buf->ReadOneBit() )
	{
		move->command_number = buf->ReadUBitLong( 32 );
	}
	else
	{
		// Assume steady increment
		move->command_number = from->command_number + 1;
	}

	if ( buf->ReadOneBit() )
	{
		move->tick_count = buf->ReadUBitLong( 32 );
	}
	else
	{
		// Assume steady increment
		move->tick_count = from->tick_count + 1;
	}

	// Read direction
	if ( buf->ReadOneBit() )
	{
		move->viewangles[0] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->viewangles[1] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->viewangles[2] = buf->ReadFloat();
	}

	// Read aim direction
	if ( buf->ReadOneBit() )
	{
		move->aimdirection[0] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->aimdirection[1] = buf->ReadFloat();
	}

	if ( buf->ReadOneBit() )
	{
		move->aimdirection[2] = buf->ReadFloat();
	}

	// Read movement
	if ( buf->ReadOneBit() )
	{
		float flMove = buf->ReadFloat();
		move->forwardmove = IsFinite( flMove ) && IsEntityCoordinateReasonable( flMove ) ? flMove : 0;
	}

	if ( buf->ReadOneBit() )
	{
		float flMove = buf->ReadFloat();
		move->sidemove = IsFinite( flMove ) && IsEntityCoordinateReasonable( flMove ) ? flMove : 0;
	}

	if ( buf->ReadOneBit() )
	{
		float flMove = buf->ReadFloat();
		move->upmove = IsFinite( flMove ) && IsEntityCoordinateReasonable( flMove ) ? flMove : 0;
	}

	// read buttons
	if ( buf->ReadOneBit() )
	{
		move->buttons = buf->ReadUBitLong( 32 );
	}

	if ( buf->ReadOneBit() )
	{
		move->impulse = buf->ReadUBitLong( 8 );
	}

#if defined( INFESTED_DLL ) || defined( DOTA_DLL )
	if ( buf->ReadOneBit() )
	{
		buf->ReadBitVec3Coord( move->crosshairtrace );
	}
	if ( buf->ReadOneBit() )
	{
		move->weaponselect = buf->ReadUBitLong( MAX_EDICT_BITS );		
	}

	if ( buf->ReadOneBit() )
	{
		move->weaponsubtype = buf->ReadUBitLong( WEAPON_SUBTYPE_BITS );
	}
#endif

#ifdef INFESTED_DLL // asw - check weapon subtype seperately, since we use it to say which marine we're controlling


	if ( buf->ReadOneBit() )
	{
		move->crosshair_entity = buf->ReadShort();
	}
	if ( buf->ReadOneBit() )
	{
		move->forced_action = buf->ReadShort();
	}
	if ( buf->ReadOneBit() )
	{
		move->sync_kill_ent = buf->ReadShort();
	}
	if ( buf->ReadOneBit() )
	{
		buf->ReadBitVec3Coord(move->skill_dest);
	}
	if ( buf->ReadOneBit() )
	{
		move->skill_dest_ent = buf->ReadShort();
	}
#else
	if ( buf->ReadOneBit() )
	{
		move->weaponselect = buf->ReadUBitLong( MAX_EDICT_BITS );
		if ( buf->ReadOneBit() )
		{
			move->weaponsubtype = buf->ReadUBitLong( WEAPON_SUBTYPE_BITS );
		}
	}
#endif

	move->random_seed = MD5_PseudoRandom( move->command_number ) & 0x7fffffff;

	if ( buf->ReadOneBit() )
	{
		move->mousedx = buf->ReadShort();
	}

	if ( buf->ReadOneBit() )
	{
		move->mousedy = buf->ReadShort();
	}

#if defined( HL2_DLL )
	if ( buf->ReadOneBit() )
	{
		move->entitygroundcontact.SetCount( buf->ReadShort() );

		int i;
		for (i = 0; i < move->entitygroundcontact.Count(); i++)
		{
			move->entitygroundcontact[i].entindex = buf->ReadUBitLong( MAX_EDICT_BITS );
			move->entitygroundcontact[i].minheight = buf->ReadBitCoord( );
			move->entitygroundcontact[i].maxheight = buf->ReadBitCoord( );
		}
	}
#endif

#if defined ( PORTAL2 )
	if ( buf->ReadOneBit() )
	{
		move->player_held_entity = buf->ReadShort();
	}

	if ( buf->ReadOneBit() )
	{
		move->held_entity_was_grabbed_through_portal = buf->ReadShort();
	}

	if ( buf->ReadOneBit() )
	{
		move->command_acknowledgements_pending = buf->ReadShort();
	}

	if ( buf->ReadOneBit() )
	{
		move->predictedPortalTeleportations = buf->ReadByte();
	}
#endif 

#ifdef DOTA_DLL
	if ( buf->ReadOneBit() )
	{
		move->dota_unitorders.m_nOrderSequenceNumber = buf->ReadShort();
		move->dota_unitorders.m_nUnits.SetCount( buf->ReadShort() );

		int i;
		for (i = 0; i < move->dota_unitorders.m_nUnits.Count(); i++)
		{
			move->dota_unitorders.m_nUnits[i] = buf->ReadUBitLong( MAX_EDICT_BITS );
		}

		move->dota_unitorders.m_nOrderType = buf->ReadShort(); 
		move->dota_unitorders.m_nTargetIndex = buf->ReadShort();
		buf->ReadBitVec3Coord( move->dota_unitorders.m_vPosition );
		move->dota_unitorders.m_nAbilityIndex = buf->ReadUBitLong( MAX_EDICT_BITS );
	}
	
#endif

	if ( IsHeadTrackingEnabled() )
	{
		// TrackIR head angles
		if ( buf->ReadOneBit() )
		{
			move->headangles[0] = buf->ReadBitAngle( 16 );
		}

		if ( buf->ReadOneBit() )
		{
			move->headangles[1] = buf->ReadBitAngle( 16 );
		}

		if ( buf->ReadOneBit() )
		{
			move->headangles[2] = buf->ReadBitAngle( 8 );
		}

		// TrackIR head offset
		if ( buf->ReadOneBit() )
		{
			buf->ReadBitVec3Coord(move->headoffset);
		}
		// TrackIR
	}


}
