//============ Copyright (c) Valve Corporation, All rights reserved. ============


#include "movieobjects/dmeusersettings.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattributevar.h"

#if !defined( _X360 )
#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE
#include <windows.h>
#endif

#include "tier0/memdbgon.h" // memdbgon must be the last include file in a .cpp file!!!

//-----------------------------------------------------------------------------

IMPLEMENT_ELEMENT_FACTORY( DmeUserSettings, CDmeUserSettings);

static CUtlMap< const char *, CDmAttribute * >s_RegistryPathToAttribute( DefLessFunc( const char * ) );

CUtlVector< IDmeUserSettingsChangedListener * > CDmeUserSettings::s_UserSettingsChangedListeners;

//-----------------------------------------------------------------------------

CDmeUserSettings *CDmeUserSettings::SharedUserSettings()
{
	static CDmeUserSettings *s_UserSettings;
	if( !s_UserSettings )
	{
		s_UserSettings = CreateElement< CDmeUserSettings >( "userSettings", DMFILEID_INVALID );
	}
	return s_UserSettings;
}

//-----------------------------------------------------------------------------

void CDmeUserSettings::OnConstruction()
{

}

void CDmeUserSettings::OnDestruction()
{
}

//-----------------------------------------------------------------------------

void CDmeUserSettings::AddUserSettingsChangedListener( IDmeUserSettingsChangedListener *pListener )
{
	s_UserSettingsChangedListeners.AddToTail( pListener );
}

//-----------------------------------------------------------------------------

void CDmeUserSettings::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );
	
	const char *pRegistryPath = FindRegistryPathForAttribute( pAttribute );
	if( pRegistryPath )
	{
		SetRegistryFromAttribute( pAttribute, pRegistryPath );

		KeyValues *pMessage = new KeyValues( "OnUserSettingsChanged", NameKey, pAttribute->GetName() );
		pMessage->SetPtr( AttributeKey, pAttribute );
		pMessage->SetString( RegistryPathKey, pRegistryPath );

		int nListenerCount = s_UserSettingsChangedListeners.Count();
		for( int i=nListenerCount - 1; i>=0 ; i-- )
		{
			s_UserSettingsChangedListeners[ i ]->OnUserSettingsChanged( pMessage );
		}
	}

}

//-----------------------------------------------------------------------------

void CDmeUserSettings::SetAttributeForRegistryPathInDatabase( CDmAttribute *pAttribute, const char *pRegistryPath )
{
	s_RegistryPathToAttribute.InsertOrReplace( pRegistryPath, pAttribute );
}

CDmAttribute *CDmeUserSettings::FindAttributeForRegistryPath( const char *pRegistryPath )
{
	int nIndex = s_RegistryPathToAttribute.Find( pRegistryPath );
	if( s_RegistryPathToAttribute.IsValidIndex( nIndex ) )
	{
		return s_RegistryPathToAttribute.Element( nIndex );
	}
	else
	{
		return NULL;
	}
}

const char *CDmeUserSettings::FindRegistryPathForAttribute( CDmAttribute *pAttribute )
{
	const char *pReturnValue = NULL;
	for ( int i = s_RegistryPathToAttribute.FirstInorder(); i != s_RegistryPathToAttribute.InvalidIndex(); i = s_RegistryPathToAttribute.NextInorder( i ) )
	{
		CDmAttribute *pCurrentAttribute = s_RegistryPathToAttribute[i];
		if( pCurrentAttribute == pAttribute )
		{
			pReturnValue = s_RegistryPathToAttribute.Key( i );
			break;
		}
	}
	return pReturnValue;
}

//-----------------------------------------------------------------------------

void CDmeUserSettings::GetAttributeNameFromRegistryPath( const char *pRegistryPath, char *pAttributeName, int nAttributeNameLength )
{
	CUtlVector<char*, CUtlMemory<char*, int> > attributeStrings;
	V_SplitString( pRegistryPath, "\\", attributeStrings );
	Q_snprintf( pAttributeName, nAttributeNameLength, "%s", attributeStrings.Tail());
	attributeStrings.PurgeAndDeleteElements();
}

CDmeUserSettings *CDmeUserSettings::GetUserSettingsForRegistryPath( const char *pRegistryPath )
{
	CDmeUserSettings *pReturnSettings = this;
	CDmeUserSettings *pCurrentSettings = this;
	CUtlVector<char*, CUtlMemory<char*, int> > attributeStrings;
	V_SplitString( pRegistryPath, "\\", attributeStrings );

	if( attributeStrings.Count() > 1 )
	{
		for( int i = 0; i < attributeStrings.Count() - 1; i++ )
		{
			const char *currentName = attributeStrings[i];
			pCurrentSettings = pReturnSettings->GetValueElement< CDmeUserSettings >(currentName );						
			if( !pCurrentSettings )
			{
				pCurrentSettings = CreateElement< CDmeUserSettings >(  currentName, pReturnSettings->GetFileId() );
				pReturnSettings->SetValue( currentName, pCurrentSettings, true );
			}
			pReturnSettings = pCurrentSettings;
		}
	}
	
	attributeStrings.PurgeAndDeleteElements();

	return pReturnSettings;
}


//-----------------------------------------------------------------------------

bool CDmeUserSettings::RegistryPathHasValue( const char *pRegistryPath )
{
	char pValueString[1024] = "";
	bool bKeyIsAvailable = GetRegistryString( pRegistryPath, pValueString, sizeof( pValueString ) );
	return bKeyIsAvailable;
}

void CDmeUserSettings::SetAttributeFromRegistry( CDmAttribute *pAttribute, const char *pRegistryPath )
{
	char pValueString[1024];
	GetRegistryString( pRegistryPath, pValueString, sizeof( pValueString ) );
	pAttribute->SetValueFromString( pValueString );
}

void CDmeUserSettings::SetRegistryFromAttribute( CDmAttribute *pAttribute, const char *pRegistryPath )
{
	char pValueString[1024];
	pAttribute->GetValueAsString( pValueString, sizeof( pValueString ) );
	SetRegistryString( pRegistryPath, pValueString );
}

//-----------------------------------------------------------------------------
// [10/23/2009 Stefan] Replace the following methods by something in tierX when it is available

void CDmeUserSettings::CreateRegistryEntryAndValueKey( const char *pRegistryPath, char *pEntryKey, int nEntryKeyLength, char *pValueKey, int nValueKeyLength )
{
	char pFullPath[1024];

	V_snprintf( 
		pFullPath, 
		sizeof(pFullPath), 
		"Software\\Valve\\SourceFilmMaker\\%s", 
		pRegistryPath );

	int nFullPathLength = V_strlen( pFullPath );
	int nSplitPosition = -1;
	for( int i = nFullPathLength - 1; i >= 0; i-- )
	{
		if( pFullPath[i]=='\\')
		{
			nSplitPosition = i + 1;
			break;
		}
	}
	if( nSplitPosition >= 0 && nSplitPosition < nFullPathLength )
	{
		V_StrRight( pFullPath, nFullPathLength - nSplitPosition, pValueKey, nValueKeyLength );
		V_StrLeft( pFullPath, nSplitPosition, pEntryKey, nEntryKeyLength );
	}
	else
	{
		V_snprintf( pEntryKey, nEntryKeyLength, "%s", pFullPath );
		V_snprintf( pValueKey, nValueKeyLength, "" );
	}

}

bool CDmeUserSettings::SetRegistryString(const char *pRegistryPath, const char *pStringValue)
{
#ifndef _X360
	HKEY hKey;
	HKEY hSlot = HKEY_CURRENT_USER;

	char pEntryKey[1024];
	char pValueKey[1024];
	CreateRegistryEntryAndValueKey( pRegistryPath, pEntryKey, sizeof( pEntryKey), pValueKey, sizeof( pValueKey ) );

	if ( RegCreateKeyEx( hSlot, pEntryKey ,NULL, NULL, REG_OPTION_NON_VOLATILE, pStringValue ? KEY_WRITE : KEY_ALL_ACCESS, NULL, &hKey , NULL) != ERROR_SUCCESS )
	{
		return false;
	}

	if ( RegSetValueEx( hKey, pValueKey, NULL, REG_SZ, (unsigned char *)pStringValue, strlen(pStringValue) + 1 ) == ERROR_SUCCESS )
	{
		RegCloseKey(hKey);
		return true;
	}

	RegCloseKey(hKey);
#endif

	return false;
}

bool CDmeUserSettings::GetRegistryString(const char *pRegistryPath, char *pStringValue, int nStringValueLen)
{
#ifndef _X360
	pStringValue[0] = 0;

	HKEY hKey;
	HKEY hSlot = HKEY_CURRENT_USER;
	
	char pEntryKey[1024];
	char pValueKey[1024];
	CreateRegistryEntryAndValueKey( pRegistryPath, pEntryKey, sizeof( pEntryKey), pValueKey, sizeof( pValueKey ) );

	if ( RegOpenKeyEx( hSlot, pEntryKey, NULL, KEY_READ, &hKey ) != ERROR_SUCCESS )
	{
		return false;
	}

	unsigned long len = nStringValueLen;
	if ( RegQueryValueEx( hKey, pValueKey, NULL, NULL, (unsigned char *)pStringValue, &len ) == ERROR_SUCCESS )
	{		
		RegCloseKey(hKey);
		return true;
	}

	RegCloseKey(hKey);
#endif

	return false;
}
