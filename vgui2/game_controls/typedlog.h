//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TYPEDLOG_H
#define TYPEDLOG_H

#ifdef _WIN32
#pragma once
#endif


#include "tier1/utlvector.h"
#include "tier1/timeutils.h"

class CDmxElement; 
class CDmxAttribute;

template< class T >
class CTypedLog
{
public:
	CTypedLog()
	{
		m_UseDefaultValue = false;
		m_bLoop = false;
		m_fAnimationRateMultiplier = 1.0f;
	}
	//~CTypedLog();

	bool Unserialize( CDmxElement *pElement );
	void GetValue( DmeTime_t time, T *pOutValue );

	bool HasValues()
	{ 
		return ( !IsEmpty() || UsesDefaultValue() ); 
	}
	bool IsEmpty(){ return ( m_values.Count() == 0 ); }
	bool UsesDefaultValue(){ return m_UseDefaultValue; }
	bool IsDone( DmeTime_t time )
	{
		int nNumEntries = m_values.Count();
		if ( nNumEntries == 0 ) 
		{
			return true;
		}
		else if ( time >= m_times[nNumEntries-1] )
		{	  
			return true;
		}

		return false;
	}

private:
	bool UnSerializeValues( CDmxAttribute *pLogValues, CDmxAttribute *pLogTimes );
	void AdjustTimeByLogAttributes( DmeTime_t &time );
	int GetValueForTime( DmeTime_t time );

	CUtlVector< T > m_values;
	CUtlVector< DmeTime_t > m_times;
	float m_fAnimationRateMultiplier;
	bool m_bLoop;

	bool m_UseDefaultValue; 
	T m_DefaultValue;
};




#endif // COLORLOG_H
