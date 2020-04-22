/**
 * EntityUtil.h
 * Various utility functions
 */

#ifndef _ENTITY_UTIL_H_
#define _ENTITY_UTIL_H_

//------------------------------------------------------------------------------------------
enum WhoType
{
	ANYONE,
	ONLY_FRIENDS,
	ONLY_ENEMIES
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all entities in the game, invoking functor on each.
 * If functor returns false, stop iteration and return false.
 */
template < typename Functor >
bool ForEachEntity( Functor &func )
{
	CBaseEntity *entity = gEntList.FirstEnt();
	while ( entity )
	{
		if ( func( entity ) == false )
			return false;

		entity = gEntList.NextEnt( entity );
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------
#ifdef GAME_DLL
class FindInViewConeFunctor
{
public:
	FindInViewConeFunctor( CBasePlayer *me, WhoType who, float tolerance, CBaseEntity *ignore = NULL )
	{
		m_me = me;
		m_who = who;
		m_tolerance = tolerance;
		m_target = NULL;
		m_range = 9999999.9f;
		m_ignore = ignore;

		me->EyeVectors(&m_dir, NULL, NULL);
		m_origin = me->GetAbsOrigin() + me->GetViewOffset();
	}

	bool operator() ( CBasePlayer *player )
	{
		if (player != m_me && player != m_ignore && player->IsAlive())
		{
			if (m_who == ONLY_FRIENDS && m_me->GetTeamNumber() != player->GetTeamNumber() )
				return true;

			if (m_who == ONLY_ENEMIES && m_me->GetTeamNumber() == player->GetTeamNumber() )
				return true;

			Vector to = player->WorldSpaceCenter() - m_origin;
			float range = to.NormalizeInPlace();

			if (DotProduct( to, m_dir ) > m_tolerance && range < m_range)
			{
				if ( m_me->IsLineOfSightClear( player ) )
				{
					m_target = player;
					m_range = range;			
				}
			}
		}

		return true;
	}

	CBasePlayer *m_me;
	WhoType m_who;
	float m_tolerance;
	CBaseEntity *m_ignore;

	Vector m_origin;
	Vector m_dir;

	CBasePlayer *m_target;
	float m_range;
};


/**
 * Find the closest player within the given view cone
 */
inline CBasePlayer *GetClosestPlayerInViewCone( CBasePlayer *me, WhoType who, float tolerance = 0.95f, float *range = NULL, CBaseEntity *ignore = NULL )
{
	// choke the victim we are pointing at
	FindInViewConeFunctor checkCone( me, who, tolerance, ignore );

	ForEachPlayer( checkCone );

	if (range)
		*range = checkCone.m_range;

	return checkCone.m_target;
}
#endif


//--------------------------------------------------------------------------------------------------------------
/**
 * Find player closest to ray.
 * Return perpendicular distance to ray in 'offset' if it is non-NULL.
 */
extern CBasePlayer *GetPlayerClosestToRay( CBasePlayer *me, WhoType who, const Vector &start, const Vector &end, float *offset = NULL );


//--------------------------------------------------------------------------------------------------------------
/**
 * Compute the closest point on the ray to 'pos' and return it in 'pointOnRay'.
 * If point projects beyond the ends of the ray, return false - but clamp 'pointOnRay' to the correct endpoint.
 */
inline bool ClosestPointOnRay( const Vector &pos, const Vector &rayStart, const Vector &rayEnd, Vector *pointOnRay )
{
	Vector to = pos - rayStart;
	Vector dir = rayEnd - rayStart;
	float length = dir.NormalizeInPlace();

	float rangeAlong = DotProduct( dir, to );

	if (rangeAlong < 0.0f)
	{
		// off start point
		*pointOnRay = rayStart;
		return false;
	}
	else if (rangeAlong > length)
	{
		// off end point
		*pointOnRay = rayEnd;
		return false;
	}
	else // within ray bounds
	{
		Vector onRay = rayStart + rangeAlong * dir;
		*pointOnRay = onRay;
		return true;
	}
}


//------------------------------------------------------------------------------------------
/**
 * Functor to find the player closest to a ray.
 * For use with ForEachPlayerNearRay().
 */
class ClosestToRayFunctor
{
public:
	ClosestToRayFunctor( CBasePlayer *ignore, WhoType who = ANYONE )
	{
		m_ignore = ignore;
		m_who = who;
		m_closestPlayer = NULL;
		m_closestPlayerRange = 999999999.9f;
	}

	bool operator() ( CBasePlayer *player, float rangeToRay )
	{
		if (player == m_ignore)
			return true;

		if (!player->IsAlive())
			return true;

		if (m_who == ONLY_FRIENDS && m_ignore->GetTeamNumber() != player->GetTeamNumber() )
			return true;

		if (m_who == ONLY_ENEMIES && m_ignore->GetTeamNumber() == player->GetTeamNumber() )
			return true;

		// keep the player closest to the ray
		if (rangeToRay < m_closestPlayerRange)
		{
			m_closestPlayerRange = rangeToRay;
			m_closestPlayer = player;
		}	

		return true;
	}

	CBasePlayer *m_ignore;
	WhoType m_who;
	CBasePlayer *m_closestPlayer;
	float m_closestPlayerRange;
};


//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all players that are within range of the ray and invoke functor.
 * If functor returns false, stop iteration and return false.
 * @todo Check LOS to ray.
 */
template < typename Functor >
bool ForEachPlayerNearRay( Functor &func, const Vector &start, const Vector &end, float maxRadius )
{
    for( int i=1; i<=gpGlobals->maxClients; ++i )
    {
        CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

        if (player == NULL)
			continue;

        if (FNullEnt( player->edict() ))
			continue;

        if (!player->IsPlayer())
			continue;

        if (!player->IsAlive())
			continue;

		float range = DistanceToRay( player->WorldSpaceCenter(), start, end );

		if (range < 0.0f || range > maxRadius)
			continue;

        if (func( player, range ) == false)
			return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Iterate over all players that are within range of the sphere and invoke functor.
 * If functor returns false, stop iteration and return false.
 * @todo Check LOS to ray.
 */
template < typename Functor >
bool ForEachPlayerNearSphere( Functor &func, const Vector &origin, float radius )
{
    for( int i=1; i<=gpGlobals->maxClients; ++i )
    {
        CBasePlayer *player = static_cast<CBasePlayer *>( UTIL_PlayerByIndex( i ) );

        if (player == NULL)
			continue;

        if (FNullEnt( player->edict() ))
			continue;

        if (!player->IsPlayer())
			continue;

        if (!player->IsAlive())
			continue;

		Vector to = player->WorldSpaceCenter() - origin;
		float range = to.Length();
		if (range > radius)
			continue;

        if (func( player, range ) == false)
			return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Reflect a vector.
 * Assumes 'unitNormal' is normalized.
 */
inline Vector VectorReflect( const Vector &in, const Vector &unitNormal )
{
	// compute the unit vector out of the plane of 'in' and 'unitNormal'
	Vector lat = CrossProduct( in, unitNormal );
	Vector forward = CrossProduct( unitNormal, lat );
	forward.NormalizeInPlace();

	Vector forwardComponent = forward * DotProduct( in, forward );	
	Vector normalComponent = unitNormal * DotProduct( in, unitNormal );

	return forwardComponent - normalComponent;
}


#endif // _ENTITY_UTIL_H_
