//====== Copyright c 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ECON_ITEM_VIEW_HELPERS_H
#define ECON_ITEM_VIEW_HELPERS_H
#ifdef _WIN32
#pragma once
#endif

// These helpers are used to abstract away the distinction between inventory items with legitimate itemIDs 
// and items that are expressed as definition index + paint index.

// returns an itemID that encodes the definition index and paint id
inline uint64 CombinedItemIdMakeFromDefIndexAndPaint( uint16 iDefIndex, uint16 iPaint, uint8 ub1 = 0 )
{
	if ( iDefIndex == 0 && iPaint == 0 )
		return 0;
	else
		return 0xF000000000000000ull | ( uint64( ub1 ) << 32 ) | ( uint64( iPaint ) << 16 ) | ( uint64( iDefIndex ) );
}

// Returns whether the itemID in question is really an encoded base item
inline bool CombinedItemIdIsDefIndexAndPaint( uint64 ullItemId )
{
	return ullItemId >= 0xF000000000000000ull;
}

// Extracts the definition index from an encoded itemID
inline uint16 CombinedItemIdGetDefIndex( uint64 ullItemId )
{
	return uint16( ullItemId );
}

// Extracts the paint index from an encoded itemID
inline uint16 CombinedItemIdGetPaint( uint64 ullItemId )
{
	return uint16( ullItemId >> 16 );
}

// Extracts the extra byte from an encoded itemID
inline uint16 CombinedItemIdGetUB1( uint64 ullItemId )
{
	return uint8( ullItemId >> 32 );
}


///////////////////////////////////////////////////////////////////////////
//
// Tint ID manipulators
//
// Fomat of the TintID int32 value from low bits towards high bits:
// low byte (TintID & 0xFF) = bucket containing HSV subspace definition
// ((TintID >> 8)&0x7F):  7-bits indicating hue location from minhue(0) to maxhue(0x7F) in the HSV subspace
// ((TintID >> 15)&0x7F): 7-bits indicating saturation location in hue-interpolated minsat(0) to maxsat(0x7F) range in the HSV subspace
// ((TintID >> 22)&0x7F): 7-bits indicating value location in hue-interpolated minval(0) to maxval(0x7F) range in the HSV subspace
// ((TintID >> 29)&0x7):  3-bits reserved (must be zero)
//
#if ECON_SPRAY_TINT_IDS_FLOAT_COLORSPACE
inline uint32 CombinedTintIDMakeFromIDHSVf( uint8 unID, float fH, float fS, float fV )
{
	return
		uint32( unID & 0xFF )
		| ( ( uint32( fH * 0x7F ) & 0x7F ) << ( 8 + 7 * 0 ) )
		| ( ( uint32( fS * 0x7F ) & 0x7F ) << ( 8 + 7 * 1 ) )
		| ( ( uint32( fV * 0x7F ) & 0x7F ) << ( 8 + 7 * 2 ) )
		;
}
inline uint32 CombinedTintIDMakeFromIDHSVu( uint8 unID, uint32 fH, uint32 fS, uint32 fV )
{
	return
		uint32( unID & 0xFF )
		| ( ( uint32( fH ) & 0x7F ) << ( 8 + 7 * 0 ) )
		| ( ( uint32( fS ) & 0x7F ) << ( 8 + 7 * 1 ) )
		| ( ( uint32( fV ) & 0x7F ) << ( 8 + 7 * 2 ) )
		;
}
inline float CombinedTintIDGetHSVc( uint32 unTintID, int c )
{
	return ( ( unTintID >> ( 8 + 7 * c ) ) & 0x7F ) / float( 0x7F );
}
#endif
inline uint8 CombinedTintIDGetHSVID( uint32 unTintID )
{
	return unTintID & 0xFF;
}

#endif // ECON_ITEM_VIEW_HELPERS_H
