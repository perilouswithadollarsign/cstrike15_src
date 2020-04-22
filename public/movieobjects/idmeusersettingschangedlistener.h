//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef DMEUSERSETTINGSCHANGEDLISTENER_H
#define DMEUSERSETTINGSCHANGEDLISTENER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/KeyValues.h"

#define NameKey "name"
#define AttributeKey "attribute"
#define RegistryPathKey "registryPath"

//-----------------------------------------------------------------------------

class IDmeUserSettingsChangedListener
{
public:
	virtual void OnUserSettingsChanged( KeyValues *message ) = 0;
};


#endif // DMEUSERSETTINGSCHANGEDLISTENER_H
