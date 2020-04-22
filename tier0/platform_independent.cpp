//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: components of tier0 PLAT_ with (at least mostly) platform independent implementations.
//
// $NoKeywords: $
//===========================================================================//


#include "pch_tier0.h"
#include <time.h>

void GetCurrentDayOfTheWeek( int *pDay )
{
	struct tm *pNewTime;
	time_t long_time;

	time( &long_time );                /* Get time as long integer. */
	pNewTime = localtime( &long_time ); /* Convert to local time. */

	*pDay = pNewTime->tm_wday;
}

void GetCurrentDayOfTheYear( int *pDay )
{
	struct tm *pNewTime;
	time_t long_time;

	time( &long_time );                /* Get time as long integer. */
	pNewTime = localtime( &long_time ); /* Convert to local time. */

	*pDay = pNewTime->tm_yday;
}
