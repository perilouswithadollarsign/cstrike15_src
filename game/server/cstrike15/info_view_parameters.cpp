//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "info_view_parameters.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CInfoViewParameters )
	DEFINE_KEYFIELD( m_nViewMode, FIELD_INTEGER, "ViewMode" )
END_DATADESC()


LINK_ENTITY_TO_CLASS( info_view_parameters, CInfoViewParameters );


