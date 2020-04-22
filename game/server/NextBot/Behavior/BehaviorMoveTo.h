// BehaviorMoveTo.h
// Move to a potentially far away position
// Author: Michael Booth, June 2007
// Copyright (c) 2007 Turtle Rock Studios, Inc. - All Rights Reserved

#ifndef _BEHAVIOR_MOVE_TO_H_
#define _BEHAVIOR_MOVE_TO_H_


//----------------------------------------------------------------------------------------------
/**
 * Move to a potentially far away position, using path planning.
 */
template < typename Actor, typename PathCost >
class BehaviorMoveTo : public Action< Actor >
{
public:
	BehaviorMoveTo( const Vector &goal );

	virtual ActionResult< Actor > OnStart( Actor *me, Action< Actor > *priorAction );
	virtual ActionResult< Actor > Update( Actor *me, float interval );

	// derive to supply specific cost functor - default uses simple shortest path
	virtual bool ComputePath( Actor *me, const Vector &goal, PathFollower *path );

	virtual const char *GetName( void ) const	{ return "BehaviorMoveTo"; }

private:
	Vector m_goal;
	PathFollower m_path;
};


//----------------------------------------------------------------------------------------------
template < typename Actor, typename PathCost >
inline BehaviorMoveTo< Actor, PathCost >::BehaviorMoveTo( const Vector &goal )
{
	m_goal = goal;
	m_path.Invalidate();
}


//----------------------------------------------------------------------------------------------
template < typename Actor, typename PathCost >
inline bool BehaviorMoveTo< Actor, PathCost >::ComputePath( Actor *me, const Vector &goal, PathFollower *path )
{
	PathCost cost( me );
	return path->Compute( me, goal, cost );
}


//----------------------------------------------------------------------------------------------
template < typename Actor, typename PathCost >
inline ActionResult< Actor > BehaviorMoveTo< Actor, PathCost >::OnStart( Actor *me, Action< Actor > *priorAction )
{
	if ( !ComputePath( me, m_goal, &m_path ) )
	{
		return Done( "No path to goal" );
	}

	return Continue();
}


//----------------------------------------------------------------------------------------------
template < typename Actor, typename PathCost >
inline ActionResult< Actor > BehaviorMoveTo< Actor, PathCost >::Update( Actor *me, float interval )
{
	// move along path
	m_path.Update( me );

	if ( m_path.IsValid() )
	{
		return Continue();
	}

	return Done();
}


#endif // _BEHAVIOR_MOVE_TO_H_

