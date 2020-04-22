//============ Copyright (c) Valve Corporation, All rights reserved. ============

#ifndef DMEUSERSETTINGS_H
#define DMEUSERSETTINGS_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "idmeusersettingschangedlistener.h"

//-----------------------------------------------------------------------------

class CDmeUserSettings : public CDmElement
{
	DEFINE_ELEMENT( CDmeUserSettings, CDmElement );

public:
	static CDmeUserSettings *SharedUserSettings();

	template< class T> static void InitSettingsValue( const char *pRegistryPath, const T& value);
	template< class T> static void SetSettingsValue( const char *pRegistryPath, const T& value);
	template< class T > static const T& GetSettingsValue( const char *pRegistryPath, const T& defaultValue );
	virtual CDmeUserSettings *GetUserSettingsForRegistryPath( const char *pRegistryPath );

	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

	virtual void AddUserSettingsChangedListener( IDmeUserSettingsChangedListener *pListener );
	
protected:
	

private:
	CDmAttribute *FindAttributeForRegistryPath( const char *pRegistryPath );
	const char *FindRegistryPathForAttribute( CDmAttribute *pAttribute );

	bool RegistryPathHasValue( const char *pRegistryPath );
	void SetAttributeFromRegistry( CDmAttribute *pAttribute, const char *pRegistryPath );
	void SetRegistryFromAttribute( CDmAttribute *pAttribute, const char *pRegistryPath );
	
	void GetAttributeNameFromRegistryPath( const char *pRegistryPath, char *pAttributeName, int nAttributeNameLength );
	void SetAttributeForRegistryPathInDatabase( CDmAttribute *pAttribute, const char *pRegistryPath );

	void CreateRegistryEntryAndValueKey( const char *pRegistryPath, char *pEntryKey, int nEntryKeyLength, char *pValueKey, int nValueKeyLength );
	bool GetRegistryString(const char *pRegistryPath, char *pStringValue, int nStringValueLen);
	bool SetRegistryString(const char *pRegistryPath, const char *pStringValue);

	static CUtlVector< IDmeUserSettingsChangedListener * > s_UserSettingsChangedListeners;
};

//-----------------------------------------------------------------------------

template< class T >
void CDmeUserSettings::InitSettingsValue( const char *pRegistryPath, const T& value )
{
	CDmAttribute *pAttribute = SharedUserSettings()->FindAttributeForRegistryPath( pRegistryPath );
	if( pAttribute )
	{
		SharedUserSettings()->SetAttributeFromRegistry( pAttribute, pRegistryPath );
	}
	else
	{
		CDmElement *pUserSettingGroup = SharedUserSettings()->GetUserSettingsForRegistryPath( pRegistryPath );
		char pAttributeName[255];
		SharedUserSettings()->GetAttributeNameFromRegistryPath( pRegistryPath, pAttributeName, 255);
		if( pUserSettingGroup )
		{
			pAttribute = pUserSettingGroup->InitValue( pAttributeName, value );
			if( pAttribute )
			{
				pAttribute->AddFlag( FATTRIB_HAS_CALLBACK );
				SharedUserSettings()->SetAttributeForRegistryPathInDatabase( pAttribute, pRegistryPath );
				if( SharedUserSettings()->RegistryPathHasValue( pRegistryPath ) )
				{
					SharedUserSettings()->SetAttributeFromRegistry( pAttribute, pRegistryPath );
				}
				else
				{
					SharedUserSettings()->SetRegistryFromAttribute( pAttribute, pRegistryPath );
				}
			}
		}
	}
}

template< class T >
void CDmeUserSettings::SetSettingsValue( const char *pRegistryPath, const T& value )
{
	CDmAttribute *pAttribute = SharedUserSettings()->FindAttributeForRegistryPath( pRegistryPath );
	if( pAttribute )
	{
		pAttribute->SetValue( value );
	}
	else
	{
		InitSettingsValue( pRegistryPath, value );
	}
}

template< class T >
const T& CDmeUserSettings::GetSettingsValue( const char *pRegistryPath, const T& defaultValue )
{
	CDmAttribute *pAttribute = SharedUserSettings()->FindAttributeForRegistryPath( pRegistryPath );
	if( pAttribute )
	{
		return pAttribute->GetValue< T >( defaultValue );
	}
	else
	{
		return defaultValue;
	}
}

#endif // DMEUSERSETTINGS_H
