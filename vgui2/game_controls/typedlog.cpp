//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "typedlog.h"
#include "dmxloader/dmxelement.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Helper function for unserializing value and time log arrays 
//-----------------------------------------------------------------------------
template<>
bool CTypedLog< Color >::UnSerializeValues( CDmxAttribute *pLogValues, CDmxAttribute *pLogTimes )
{
	Assert(0);
	return false;
}

template< typename T >
bool CTypedLog< T >::UnSerializeValues( CDmxAttribute *pLogValues, CDmxAttribute *pLogTimes )
{
	const CUtlVector< T > &logvalues = pLogValues->GetArray< T >();
	const CUtlVector< DmeTime_t > &logtimes = pLogTimes->GetArray< DmeTime_t >();
	int nCount = logvalues.Count();
	Assert( nCount == logtimes.Count() );

	for ( int i = 0; i < nCount; ++i )
	{
		m_values.AddToTail( logvalues[i] );
		m_times.AddToTail( logtimes[i] ); 
	}
	return true;
}


//-----------------------------------------------------------------------------
//	Read in the log values and timestamps
// Color is a special case because we are reading from a Color into color32
//-----------------------------------------------------------------------------
template<>
bool CTypedLog< color32 >::Unserialize( CDmxElement *pElement )
{	
	m_fAnimationRateMultiplier = pElement->GetValue<float>( "animationrate" );
	m_bLoop = pElement->GetValue<bool>( "loop" );
	m_UseDefaultValue = pElement->GetValue<bool>( "usedefault" );
	m_DefaultValue = pElement->GetValue<Color>( "defaultvalue" ).ToColor32();

	CDmxAttribute *pLogValues = pElement->GetAttribute( "logvalues" );
	if ( !pLogValues )
	{
		return true;
	}

	// There might be no entries.
	if ( pLogValues->GetType() != AT_COLOR_ARRAY )
	{
		return true;
	}

	CDmxAttribute *pLogTimes = pElement->GetAttribute( "logtimes" );
	if ( !pLogTimes )
	{
		// If there are no logtime entry, and there is a log value entry, there is an error.
		return false;
	}

	if ( pLogTimes->GetType() != AT_TIME_ARRAY )
	{
		// If there are no log times to go with the values.. there is an error.
		return false;
	}

	if ( m_UseDefaultValue )
	{
		Warning( "Warning: Possible unintended behavior: CTypedLog is set to use a default value when there are log entries.\n" );
	}

	const CUtlVector< Color > &logvalues = pLogValues->GetArray< Color >();
	const CUtlVector< DmeTime_t > &logtimes = pLogTimes->GetArray< DmeTime_t >();
	int nCount = logvalues.Count();
	Assert( nCount == logtimes.Count() );

	for ( int i = 0; i < nCount; ++i )
	{
		m_values.AddToTail( logvalues[i].ToColor32() );
		m_times.AddToTail( logtimes[i] ); 
	}
	return true;
}


template<>
bool CTypedLog< float >::Unserialize( CDmxElement *pElement )
{	
	m_fAnimationRateMultiplier = pElement->GetValue<float>( "animationrate" );
	m_bLoop = pElement->GetValue<bool>( "loop" );
	m_UseDefaultValue = pElement->GetValue<bool>( "usedefault" );
	m_DefaultValue = pElement->GetValue<float>( "defaultvalue" );

	CDmxAttribute *pLogValues = pElement->GetAttribute( "logvalues" );
	if ( !pLogValues )
	{
		return true;
	}

	// There might be no entries.
	if ( pLogValues->GetType() != AT_FLOAT_ARRAY )
	{
		return true;
	}

	CDmxAttribute *pLogTimes = pElement->GetAttribute( "logtimes" );
	if ( !pLogTimes )
	{
		return false;
	}

	if ( pLogTimes->GetType() != AT_TIME_ARRAY )
	{
		return false;
	}

	if ( m_UseDefaultValue )
	{
		Warning( "Warning: Possible unintended behavior: CTypedLog is set to use a default value when there are log entries.\n" );
	}

	return UnSerializeValues( pLogValues, pLogTimes );
}

template<>
bool CTypedLog< Vector2D >::Unserialize( CDmxElement *pElement )
{	
	m_fAnimationRateMultiplier = pElement->GetValue<float>( "animationrate" );
	m_bLoop = pElement->GetValue<bool>( "loop" );
	m_UseDefaultValue = pElement->GetValue<bool>( "usedefault" );
	m_DefaultValue = pElement->GetValue<Vector2D>( "defaultvalue" );

	CDmxAttribute *pLogValues = pElement->GetAttribute( "logvalues" );
	if ( !pLogValues )
	{
		return true;
	}

	// There might be no entries.
	if ( pLogValues->GetType() != AT_VECTOR2_ARRAY )
	{
		return true;
	}

	CDmxAttribute *pLogTimes = pElement->GetAttribute( "logtimes" );
	if ( !pLogTimes )
	{
		return false;
	}

	// There might be no entries.
	if ( pLogTimes->GetType() != AT_TIME_ARRAY )
	{
		return false;
	}

	if ( m_UseDefaultValue )
	{
		Warning( "Warning: Possible unintended behavior: CTypedLog is set to use a default value when there are log entries.\n" );
	}

	return UnSerializeValues( pLogValues, pLogTimes );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
template<>
bool CTypedLog<CUtlString>::Unserialize( CDmxElement *pElement )
{	
	m_fAnimationRateMultiplier = pElement->GetValue<float>( "animationrate" );
	m_bLoop = pElement->GetValue<bool>( "loop" );
	m_UseDefaultValue = pElement->GetValue<bool>( "usedefault" );
	m_DefaultValue = pElement->GetValue<CUtlString>( "defaultvalue" );

	CDmxAttribute *pLogValues = pElement->GetAttribute( "logvalues" );
	if ( !pLogValues )
	{
		return true;
	}

	// There might be no entries.
	if ( pLogValues->GetType() != AT_STRING_ARRAY )
	{
		return true;
	}

	CDmxAttribute *pLogTimes = pElement->GetAttribute( "logtimes" );
	if ( !pLogTimes )
	{
		return false;
	}

	// There might be no entries.
	if ( pLogTimes->GetType() != AT_TIME_ARRAY )
	{
		return false;
	}

	if ( m_UseDefaultValue )
	{
		Warning( "Warning: Possible unintended behavior: CTypedLog is set to use a default value when there are log entries.\n" );
	}

	return UnSerializeValues( pLogValues, pLogTimes );
}

//-----------------------------------------------------------------------------
// All unserialization is specialized in order to get the values array type correctly.
//-----------------------------------------------------------------------------
template< class T >
bool CTypedLog<T>::Unserialize( CDmxElement *pElement )
{	
	Assert( 0 );
	return false;
}



//-----------------------------------------------------------------------------
// Given a time, return the log index to use
//-----------------------------------------------------------------------------
template< class T >
int CTypedLog<T>::GetValueForTime( DmeTime_t time )
{
	int nNumEntries = m_values.Count();
	if ( ( nNumEntries == 1 ) || ( time == DMETIME_ZERO ) )
	{
		return 0;
	}
	else
	{
		for ( int i = 0; i < nNumEntries; ++i )
		{
			if ( time > m_times[i] )
			{	  
				continue;
			}
			else
			{
				return i;
			}
		}

		return nNumEntries - 1; 
	}
}


//-----------------------------------------------------------------------------
// Given a time, adjust it for animation rate and looping.
// Return true if we are past the end of the last log time.
// On the game side we do not have channels to take care of doing this so do it now.
//-----------------------------------------------------------------------------
template< class T >
void CTypedLog<T>::AdjustTimeByLogAttributes( DmeTime_t &time )
{
	time *= m_fAnimationRateMultiplier;
	DmeTime_t totalTime = m_times[m_values.Count()-1];
	if ( m_bLoop )
	{	
		// Put time inside our duration.
		time = time % totalTime;		
	}

	// If we've run past the end of the log, clamp it to the end for 
	// ease of value retrieval.
	if ( time > totalTime )
	{
		time = totalTime;
	}

}

//-----------------------------------------------------------------------------
// Given a time, return the log value. 
//-----------------------------------------------------------------------------
template<>
void CTypedLog<color32>::GetValue( DmeTime_t time, color32 *pOutValue )
{
	if ( m_UseDefaultValue )
	{
		*pOutValue = m_DefaultValue;
		return;
	}

	int nNumEntries = m_values.Count();
	if ( nNumEntries == 0 )
	{
		return;
	}

	AdjustTimeByLogAttributes( time );
	int i = GetValueForTime( time );
	if ( i == 0 )	
	{
		*pOutValue = m_values[0];
		return;
	}

	// Linear interp for now.
	Assert( i != 0 );
	DmeTime_t startTimeStamp = m_times[i-1];
	DmeTime_t endTimeStamp = m_times[i];
	DmeTime_t timeIntoInterval = time - startTimeStamp;
	DmeTime_t timeOfInterval = endTimeStamp - startTimeStamp;
	float dt = timeIntoInterval /  timeOfInterval;

	pOutValue->r =  m_values[ i-1 ].r * ( 1.0f - dt ) + m_values[ i ].r * dt;
	pOutValue->g =  m_values[ i-1 ].g * ( 1.0f - dt ) + m_values[ i ].g * dt;
	pOutValue->b =  m_values[ i-1 ].b * ( 1.0f - dt ) + m_values[ i ].b * dt;
	pOutValue->a =  m_values[ i-1 ].a * ( 1.0f - dt ) + m_values[ i ].a * dt;	
}


template<>
void CTypedLog< float >::GetValue( DmeTime_t time, float *pOutValue )
{
	if ( m_UseDefaultValue )
	{
		*pOutValue = m_DefaultValue;
		return;
	}

	int nNumEntries = m_values.Count();
	if ( nNumEntries == 0 )
	{
		return;
	}

	AdjustTimeByLogAttributes( time );
	int i = GetValueForTime( time );
	if ( i == 0 )	
	{
		*pOutValue = m_values[0];
		return;
	}

	// Now get distance between the two log entries
	// Linear interp for now.
	float startVal = m_values[i-1];
	float endVal = m_values[i];
	float distance = endVal - startVal;

	Assert( i != 0 );
	DmeTime_t startTimeStamp = m_times[i-1];
	DmeTime_t endTimeStamp = m_times[i];
	DmeTime_t timeIntoInterval = time - startTimeStamp;
	DmeTime_t timeOfInterval = endTimeStamp - startTimeStamp;
	float dt = timeIntoInterval / timeOfInterval;
	*pOutValue = startVal + distance * dt;		
}

template<>
void CTypedLog< Vector2D >::GetValue( DmeTime_t time, Vector2D *pOutValue )
{
	if ( m_UseDefaultValue )
	{
		*pOutValue = m_DefaultValue;
		return;
	}

	int nNumEntries = m_values.Count();
	if ( nNumEntries == 0 )
	{
		return;
	}

	AdjustTimeByLogAttributes( time );
	int i = GetValueForTime( time );
	if ( i == 0 )	
	{
		*pOutValue = m_values[0];
		return;
	}

	// Now get distance between the two log entries
	// Linear interp for now.

	Vector2D distance;
	Vector2DSubtract( m_values[i], m_values[i-1], distance );

	Assert( i != 0 );
	DmeTime_t startTimeStamp = m_times[i-1];
	DmeTime_t endTimeStamp = m_times[i];
	DmeTime_t timeIntoInterval = time - startTimeStamp;
	DmeTime_t timeOfInterval = endTimeStamp - startTimeStamp;
	float dt = timeIntoInterval / timeOfInterval;
	pOutValue->x = m_values[i-1].x + distance.x * dt;
	pOutValue->y = m_values[i-1].y + distance.y * dt;	
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
template<>
void CTypedLog<CUtlString>::GetValue( DmeTime_t time, CUtlString *pOutValue )
{
	if ( m_UseDefaultValue )
	{
		*pOutValue = m_DefaultValue;
		return;
	}

	int nNumEntries = m_values.Count();
	if ( nNumEntries == 0 )
	{
		return;
	}

	time *= m_fAnimationRateMultiplier;

	AdjustTimeByLogAttributes( time );
	int i = GetValueForTime( time );
	*pOutValue = m_values[i];
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
template< class T >
void CTypedLog<T>::GetValue( DmeTime_t time, T *pOutValue )
{
	Assert(0);
}






































