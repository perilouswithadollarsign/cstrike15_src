//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: rate limits OOB server queries
//
//=============================================================================//

struct ns_address;

// returns false if this IP exceeds rate limits
bool CheckConnectionLessRateLimits( const ns_address &adr );

