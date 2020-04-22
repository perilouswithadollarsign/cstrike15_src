
//========= Copyright � 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "engine_helper_ps3.h"

#if defined( _PS3 )
#include <np.h>	
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#if defined( _PS3 )

#define ESRB_TEEN_RATING_AGE			13

#define AGE_RESTRICTION_DEFAULT			ESRB_TEEN_RATING_AGE
#define AGE_RESTRICTION_MAX				ESRB_TEEN_RATING_AGE

#define AGE_RESTRICTION_UAE				AGE_RESTRICTION_DEFAULT// age for UAE
#define AGE_RESTRICTION_ARGENTINIA		AGE_RESTRICTION_DEFAULT// age for Argentina
#define AGE_RESTRICTION_AUSTRIA			AGE_RESTRICTION_DEFAULT// age for Austria
#define AGE_RESTRICTION_AUSTRALIA		AGE_RESTRICTION_DEFAULT// age for Australia
#define AGE_RESTRICTION_BELGIUM			AGE_RESTRICTION_DEFAULT// age for Belgium
#define AGE_RESTRICTION_BULGARIA		AGE_RESTRICTION_DEFAULT// age for Bulgaria
#define AGE_RESTRICTION_BAHRAIN			AGE_RESTRICTION_DEFAULT// age for Bahrain
#define AGE_RESTRICTION_BRAZIL			AGE_RESTRICTION_DEFAULT// age for Brazil
#define AGE_RESTRICTION_CANADA			AGE_RESTRICTION_DEFAULT// age for Canada
#define AGE_RESTRICTION_SWISS			AGE_RESTRICTION_DEFAULT// age for Switzerland
#define AGE_RESTRICTION_CHILE			AGE_RESTRICTION_DEFAULT// age for Chile
#define AGE_RESTRICTION_COLOMBIA		AGE_RESTRICTION_DEFAULT// age for Colombia
#define AGE_RESTRICTION_CYPRUS			AGE_RESTRICTION_DEFAULT// age for Cyprus
#define AGE_RESTRICTION_CZECH_REP		AGE_RESTRICTION_DEFAULT// age for Czech Republic
#define AGE_RESTRICTION_GERMANY			AGE_RESTRICTION_DEFAULT// age for Germany
#define AGE_RESTRICTION_DENMARK			AGE_RESTRICTION_DEFAULT// age for Denmark
#define AGE_RESTRICTION_SPAIN			AGE_RESTRICTION_DEFAULT// age for Spain
#define AGE_RESTRICTION_FINLAND			AGE_RESTRICTION_DEFAULT// age for Finland
#define AGE_RESTRICTION_FRANCE			AGE_RESTRICTION_DEFAULT// age for France
#define AGE_RESTRICTION_UK				AGE_RESTRICTION_DEFAULT// age for UK
#define AGE_RESTRICTION_GREECE			AGE_RESTRICTION_DEFAULT// age for Greece
#define AGE_RESTRICTION_HONG_KONG		AGE_RESTRICTION_DEFAULT// age for Hong Kong
#define AGE_RESTRICTION_CROATIA			AGE_RESTRICTION_DEFAULT// age for Croatia
#define AGE_RESTRICTION_HUNGARY			AGE_RESTRICTION_DEFAULT// age for Hungary
#define AGE_RESTRICTION_INDONESIA		AGE_RESTRICTION_DEFAULT// age for Indonesia
#define AGE_RESTRICTION_IRELAND			AGE_RESTRICTION_DEFAULT// age for Ireland
#define AGE_RESTRICTION_ISREAL			AGE_RESTRICTION_DEFAULT// age for Isreal
#define AGE_RESTRICTION_INDIA			AGE_RESTRICTION_DEFAULT// age for India
#define AGE_RESTRICTION_ICELAND			AGE_RESTRICTION_DEFAULT// age for Iceland
#define AGE_RESTRICTION_ITALY			AGE_RESTRICTION_DEFAULT// age for Italy
#define AGE_RESTRICTION_JAPAN			AGE_RESTRICTION_DEFAULT// age for Japan
#define AGE_RESTRICTION_KOREA			AGE_RESTRICTION_DEFAULT// age for Korea
#define AGE_RESTRICTION_KUWAIT			AGE_RESTRICTION_DEFAULT// age for Kuwait
#define AGE_RESTRICTION_LEBANON			AGE_RESTRICTION_DEFAULT// age for Lebanon
#define AGE_RESTRICTION_LUXEMBOURG		AGE_RESTRICTION_DEFAULT// age for Luxembourg
#define AGE_RESTRICTION_MALTA			AGE_RESTRICTION_DEFAULT// age for Malta
#define AGE_RESTRICTION_MEXICO			AGE_RESTRICTION_DEFAULT// age for Mexico
#define AGE_RESTRICTION_MALAYSIA		AGE_RESTRICTION_DEFAULT// age for Malaysia
#define AGE_RESTRICTION_NETHERLANDS		AGE_RESTRICTION_DEFAULT// age for Netherlands
#define AGE_RESTRICTION_NORWAY			AGE_RESTRICTION_DEFAULT// age for Norway
#define AGE_RESTRICTION_NEW_ZEALAND		AGE_RESTRICTION_DEFAULT// age for New Zealand
#define AGE_RESTRICTION_OMAN			AGE_RESTRICTION_DEFAULT// age for Oman
#define AGE_RESTRICTION_PERU			AGE_RESTRICTION_DEFAULT// age for Peru
#define AGE_RESTRICTION_POLAND			AGE_RESTRICTION_DEFAULT// age for Poland
#define AGE_RESTRICTION_PORTUGAL		AGE_RESTRICTION_DEFAULT// age for Portugal
#define AGE_RESTRICTION_QATAR			AGE_RESTRICTION_DEFAULT// age for Qatar
#define AGE_RESTRICTION_ROMAINIA		AGE_RESTRICTION_DEFAULT// age for Romania
#define AGE_RESTRICTION_RUSSIA			AGE_RESTRICTION_DEFAULT// age for Russia
#define AGE_RESTRICTION_SAUDI_ARABIA	AGE_RESTRICTION_DEFAULT// age for Saudi Arabia
#define AGE_RESTRICTION_SWEDEN			AGE_RESTRICTION_DEFAULT// age for Sweden
#define AGE_RESTRICTION_SINGAPORE		AGE_RESTRICTION_DEFAULT// age for Singapore
#define AGE_RESTRICTION_SLOVENIA		AGE_RESTRICTION_DEFAULT// age for Slovenia
#define AGE_RESTRICTION_SLOVAKIA		AGE_RESTRICTION_DEFAULT// age for Slovakia
#define AGE_RESTRICTION_THAILAND		AGE_RESTRICTION_DEFAULT// age for Thailand
#define AGE_RESTRICTION_TURKEY			AGE_RESTRICTION_DEFAULT// age for Turkey
#define AGE_RESTRICTION_TAIWAN			AGE_RESTRICTION_DEFAULT// age for Taiwan
#define AGE_RESTRICTION_UKRAINE			AGE_RESTRICTION_DEFAULT// age for Ukraine
#define AGE_RESTRICTION_UNITED_STATES	AGE_RESTRICTION_DEFAULT// age for United States
#define AGE_RESTRICTION_SOUTH_AFRICA	AGE_RESTRICTION_DEFAULT// age for South Africa



int EngineHelperPS3::GetAgeRestrictionByRegion( void )
{
	SceNpCountryCode countryCode;
	int langCode;

	int ret = sceNpManagerGetAccountRegion( &countryCode, &langCode );
	if ( ret < 0 )   
	{
		return AGE_RESTRICTION_MAX;
	}

	if ( strcmp( countryCode.data, "ae" )  == 0 )
		return AGE_RESTRICTION_UAE;// age for UAE

	if ( strcmp( countryCode.data, "ar" )  == 0 )
		return AGE_RESTRICTION_ARGENTINIA;// age for Argentina

	if ( strcmp( countryCode.data, "at" )  == 0 )
		return AGE_RESTRICTION_AUSTRIA;// age for Austria

	if ( strcmp( countryCode.data, "au" )  == 0 )
		return AGE_RESTRICTION_AUSTRALIA;// age for Australia

	if ( strcmp( countryCode.data, "be" )  == 0 )
		return AGE_RESTRICTION_BELGIUM;// age for Belgium

	if ( strcmp( countryCode.data, "bg" )  == 0 )
		return AGE_RESTRICTION_BULGARIA;// age for Bulgaria

	if ( strcmp( countryCode.data, "bh" )  == 0 )
		return AGE_RESTRICTION_BAHRAIN;// age for Bahrain

	if ( strcmp( countryCode.data, "br" )  == 0 )
		return AGE_RESTRICTION_BRAZIL;// age for Brazil

	if ( strcmp( countryCode.data, "ca" )  == 0 )
		return AGE_RESTRICTION_CANADA;// age for Canada

	if ( strcmp( countryCode.data, "ch" )  == 0 )
		return AGE_RESTRICTION_SWISS;// age for Switzerland

	if ( strcmp( countryCode.data, "cl" )  == 0 )
		return AGE_RESTRICTION_CHILE;// age for Chile

	if ( strcmp( countryCode.data, "co" )  == 0 )
		return AGE_RESTRICTION_COLOMBIA;// age for Colombia

	if ( strcmp( countryCode.data, "cy" )  == 0 )
		return AGE_RESTRICTION_CYPRUS;// age for Cyprus

	if ( strcmp( countryCode.data, "cz" )  == 0 )
		return AGE_RESTRICTION_CZECH_REP;// age for Czech Republic

	if ( strcmp( countryCode.data, "de" )  == 0 )
		return AGE_RESTRICTION_GERMANY;// age for Germany

	if ( strcmp( countryCode.data, "dk" )  == 0 )
		return AGE_RESTRICTION_DENMARK;// age for Denmark
	if ( strcmp( countryCode.data, "es" )  == 0 )
		return AGE_RESTRICTION_SPAIN;// age for Spain

	if ( strcmp( countryCode.data, "fi" )  == 0 )
		return AGE_RESTRICTION_FINLAND;// age for Finland

	if ( strcmp( countryCode.data, "fr" )  == 0 )
		return AGE_RESTRICTION_FRANCE;// age for France

	if ( strcmp( countryCode.data, "gb" )  == 0 )
		return AGE_RESTRICTION_UK;// age for UK

	if ( strcmp( countryCode.data, "gr" )  == 0 )
		return AGE_RESTRICTION_GREECE;// age for Greece

	if ( strcmp( countryCode.data, "hk" )  == 0 )
		return AGE_RESTRICTION_HONG_KONG;// age for Hong Kong

	if ( strcmp( countryCode.data, "hr" )  == 0 )
		return AGE_RESTRICTION_CROATIA;// age for Croatia

	if ( strcmp( countryCode.data, "hu" )  == 0 )
		return AGE_RESTRICTION_HUNGARY;// age for Hungary

	if ( strcmp( countryCode.data, "id" )  == 0 )
		return AGE_RESTRICTION_INDONESIA;// age for Indonesia

	if ( strcmp( countryCode.data, "ie" )  == 0 )
		return AGE_RESTRICTION_IRELAND;// age for Ireland

	if ( strcmp( countryCode.data, "il" )  == 0 )
		return AGE_RESTRICTION_ISREAL;// age for Isreal

	if ( strcmp( countryCode.data, "in" )  == 0 )
		return AGE_RESTRICTION_INDIA;// age for India

	if ( strcmp( countryCode.data, "is" )  == 0 )
		return AGE_RESTRICTION_ICELAND;// age for Iceland

	if ( strcmp( countryCode.data, "it" )  == 0 )
		return AGE_RESTRICTION_ITALY;// age for Italy

	if ( strcmp( countryCode.data, "jp" )  == 0 )
		return AGE_RESTRICTION_JAPAN;// age for Japan

	if ( strcmp( countryCode.data, "kr" )  == 0 )
		return AGE_RESTRICTION_KOREA;// age for Korea

	if ( strcmp( countryCode.data, "kw" )  == 0 )
		return AGE_RESTRICTION_KUWAIT;// age for Kuwait

	if ( strcmp( countryCode.data, "lb" )  == 0 )
		return AGE_RESTRICTION_LEBANON;// age for Lebanon

	if ( strcmp( countryCode.data, "lu" )  == 0 )
		return AGE_RESTRICTION_LUXEMBOURG;// age for Luxembourg

	if ( strcmp( countryCode.data, "mt" )  == 0 )
		return AGE_RESTRICTION_MALTA;// age for Malta

	if ( strcmp( countryCode.data, "mx" )  == 0 )
		return AGE_RESTRICTION_MEXICO;// age for Mexico

	if ( strcmp( countryCode.data, "my" )  == 0 )
		return AGE_RESTRICTION_MALAYSIA;// age for Malaysia

	if ( strcmp( countryCode.data, "nl" )  == 0 )
		return AGE_RESTRICTION_NETHERLANDS;// age for Netherlands

	if ( strcmp( countryCode.data, "no" )  == 0 )
		return AGE_RESTRICTION_NORWAY;// age for Norway

	if ( strcmp( countryCode.data, "nz" )  == 0 )
		return AGE_RESTRICTION_NEW_ZEALAND;// age for New Zealand

	if ( strcmp( countryCode.data, "om" )  == 0 )
		return AGE_RESTRICTION_OMAN;// age for Oman

	if ( strcmp( countryCode.data, "pe" )  == 0 )
		return AGE_RESTRICTION_PERU;// age for Peru

	if ( strcmp( countryCode.data, "pl" )  == 0 )
		return AGE_RESTRICTION_POLAND;// age for Poland

	if ( strcmp( countryCode.data, "pt" )  == 0 )
		return AGE_RESTRICTION_PORTUGAL;// age for Portugal

	if ( strcmp( countryCode.data, "qa" )  == 0 )
		return AGE_RESTRICTION_QATAR;// age for Qatar

	if ( strcmp( countryCode.data, "ro" )  == 0 )
		return AGE_RESTRICTION_ROMAINIA;// age for Romania

	if ( strcmp( countryCode.data, "ru" )  == 0 )
		return AGE_RESTRICTION_RUSSIA;// age for Russia

	if ( strcmp( countryCode.data, "sa" )  == 0 )
		return AGE_RESTRICTION_SAUDI_ARABIA;// age for Saudi Arabia

	if ( strcmp( countryCode.data, "se" )  == 0 )
		return AGE_RESTRICTION_SWEDEN;// age for Sweden

	if ( strcmp( countryCode.data, "sg" )  == 0 )
		return AGE_RESTRICTION_SINGAPORE;// age for Singapore

	if ( strcmp( countryCode.data, "si" )  == 0 )
		return AGE_RESTRICTION_SLOVENIA;// age for Slovenia

	if ( strcmp( countryCode.data, "sk" )  == 0 )
		return AGE_RESTRICTION_SLOVAKIA;// age for Slovakia

	if ( strcmp( countryCode.data, "th" )  == 0 )
		return AGE_RESTRICTION_THAILAND;// age for Thailand

	if ( strcmp( countryCode.data, "tr" )  == 0 )
		return AGE_RESTRICTION_TURKEY;// age for Turkey

	if ( strcmp( countryCode.data, "tw" )  == 0 )
		return AGE_RESTRICTION_TAIWAN;// age for Taiwan

	if ( strcmp( countryCode.data, "ua" )  == 0 )
		return AGE_RESTRICTION_UKRAINE;// age for Ukraine

	if ( strcmp( countryCode.data, "us" )  == 0 )
		return AGE_RESTRICTION_UNITED_STATES;// age for United States

	if ( strcmp( countryCode.data, "za" )  == 0 )
		return AGE_RESTRICTION_SOUTH_AFRICA;// age for South Africa

	return AGE_RESTRICTION_DEFAULT;

}

// note:  We assume if we aren't connected or initialized, that the chat is NOT restricted
bool EngineHelperPS3::PS3_IsUserRestrictedFromChat( void )
{
	int isRestricted;
	int ret = sceNpManagerGetChatRestrictionFlag( &isRestricted );
	if (ret < 0) 
	{
		// handle the error
		if ( ret == SCE_NP_ERROR_NOT_INITIALIZED )
		{
			// not yet initialized...  check back in a bit
			return false;
		}
		if ( ret == SCE_NP_ERROR_INVALID_STATE ) 
		{
			// invalid state...  sign in procedure hasn't been completed yet
			return false;
		}
		if ( ret == SCE_NP_ERROR_OFFLINE )
		{
			// we're offline, no account info available
			return false;
		}

		return false;
	}

	if ( isRestricted ) 
	{
		return true;
	} 
	else 
	{
		return false;
	}
}

// NOTE:  If we're not signed in yet, or not initialized, we consider this as not restricted from online
bool EngineHelperPS3::PS3_IsUserRestrictedFromOnline( void )
{
	int age;
	int isRestricted;
	int ret;

	ret = sceNpManagerGetAccountAge( &age );
	if ( ret < 0 ) 
	{
		if ( ret == SCE_NP_ERROR_NOT_INITIALIZED )
		{
			// not yet initialized...  check back in a bit
			return false;
		}
		if ( ret == SCE_NP_ERROR_INVALID_STATE ) 
		{
			// invalid state...  sign in procedure hasn't been completed yet
			return false;
		}
		if ( ret == SCE_NP_ERROR_OFFLINE )
		{
			// we're offline, no account info available
			return false;
		}

		return false;
	}

	ret = sceNpManagerGetContentRatingFlag( &isRestricted, &age );

	if ( isRestricted && age < GetAgeRestrictionByRegion() ) 
	{
		return true;
	} 
	else 
	{
		return false;
	}
}

bool EngineHelperPS3::PS3_PendingInvitesFound( void )
{
	uint32_t count = 0;

	if ( sceNpBasicGetMessageEntryCount( SCE_NP_BASIC_MESSAGE_INFO_TYPE_BOOTABLE_INVITATION, &count ) == 0 )
	{
		if ( count > 0 )
		{
			return true;
		}
	}

	return false;
}

void EngineHelperPS3::PS3_ShowInviteOverlay( void )
{
	sceNpBasicRecvMessageCustom( SCE_NP_BASIC_MESSAGE_MAIN_TYPE_INVITE,
								 SCE_NP_BASIC_RECV_MESSAGE_OPTIONS_INCLUDE_BOOTABLE,
								 SYS_MEMORY_CONTAINER_ID_INVALID );
}

#endif // _PS3

