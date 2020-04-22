#ifndef PAINT_ENUM_H
#define PAINT_ENUM_H

//=============================================================================
// Paint Power Type Constants
// Note: This ordering is currently used for priority when determining the
//		 active paint power, so keep that in mind when modifying it.
//=============================================================================
enum PaintPowerType
{
	BOUNCE_POWER,
	REFLECT_POWER,
	SPEED_POWER,

	// Add new powers here
	PORTAL_POWER,


	NO_POWER,
	PAINT_POWER_TYPE_COUNT = NO_POWER,		// Note: Do not change this. We almost always
	PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER,	// just want the actual number of powers, but		
	// in PaintPowerUser the array of deactivating		
	// powers has an extra slot, so we don't have to		
	// branch to queue up the active power for
	// deactivation (the active power can be NO_POWER).
	INVALID_PAINT_POWER
};

#endif // PAINT_ENUM_H
