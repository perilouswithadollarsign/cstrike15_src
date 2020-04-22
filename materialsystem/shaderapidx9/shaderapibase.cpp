//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#undef PROTECTED_THINGS_ENABLE   // prevent warnings when windows.h gets included

#include "shaderapibase.h"
#include "shaderapi/ishaderutil.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
// The Base implementation of the shader render class
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderAPIBase::CShaderAPIBase()
{
}

CShaderAPIBase::~CShaderAPIBase()
{
}


//-----------------------------------------------------------------------------
// Methods of IShaderDynamicAPI
//-----------------------------------------------------------------------------
void CShaderAPIBase::GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo )
{
	g_pShaderUtil->GetCurrentColorCorrection( pInfo );
}


