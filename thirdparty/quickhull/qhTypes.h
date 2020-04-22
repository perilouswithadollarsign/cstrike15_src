//--------------------------------------------------------------------------------------------------
/**
	@file		qhTypes.h

	@author		Dirk Gregorius
	@version	0.1
	@date		30/11/2011

	Copyright(C) 2011 by D. Gregorius. All rights reserved.
*/
//--------------------------------------------------------------------------------------------------
#pragma once

#include "tier0/dbg.h"


//--------------------------------------------------------------------------------------------------
// qhTypes
//--------------------------------------------------------------------------------------------------
typedef float				qhReal;
typedef unsigned int		qhIndex;

#define QH_REAL_MIN			std::numeric_limits< qhReal >::min()
#define QH_REAL_MAX			std::numeric_limits< qhReal >::max()
#define QH_REAL_EPSILON		std::numeric_limits< qhReal >::epsilon()

#ifndef _MSC_VER
#	define QH_GLOBAL_CONSTANT	static
#else
#	define QH_GLOBAL_CONSTANT	extern __declspec( selectany )
#endif 



//--------------------------------------------------------------------------------------------------
// qhAssert
//--------------------------------------------------------------------------------------------------
#define QH_ASSERT( Cond )	Assert( Cond )
