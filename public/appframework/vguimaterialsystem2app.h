//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Material editor
//=============================================================================

#ifndef VGUIMATERIALSYSTEM2APP_H
#define VGUIMATERIALSYSTEM2APP_H

#ifdef _WIN32
#pragma once
#endif


#include "appframework/materialsystem2app.h"

FORWARD_DECLARE_HANDLE( InputContextHandle_t );

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVGuiMaterialSystem2App : public CMaterialSystem2App
{
	typedef CMaterialSystem2App BaseClass;

public:
	CVGuiMaterialSystem2App();

	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual bool PostInit();
	virtual void PreShutdown();
	virtual void PostShutdown();
	virtual void Destroy();

	InputContextHandle_t GetAppInputContext();

private:
	InputContextHandle_t m_hAppInputContext;
};


#endif // VGUIMATERIALSYSTEM2APP_H
