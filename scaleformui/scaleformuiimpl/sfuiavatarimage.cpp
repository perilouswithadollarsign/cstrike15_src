//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "stdafx.h"
#include "sfuiavatarimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef NO_STEAM
extern CSteamAPIContext *steamapicontext;
#endif

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

using namespace SF::Render;

IScaleformAvatarImageProvider * ScaleformUIAvatarImage::sm_pProvider = NULL;	// static provider initialized upon first available avatar data arriving

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ScaleformUIAvatarImage::ScaleformUIAvatarImage( XUID xuid, const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager )
	: ScaleformUIImage( defaultRgba, defaultWidth, defaultHeight, defaultFormat, pTextureManager )
{
	m_xXUID = xuid;

	if ( ( m_xXUID & 0xFFFFFFFFull ) == m_xXUID )
	{
		static EUniverse eUniverse = steamapicontext->SteamUtils()->GetConnectedUniverse();
		m_xXUID = CSteamID( uint32( m_xXUID ), eUniverse, k_EAccountTypeIndividual ).ConvertToUint64();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ScaleformUIAvatarImage::LoadAvatarImage( IScaleformAvatarImageProvider *pProvider /* = NULL */ )
{
#ifndef NO_STEAM
	if ( steamapicontext->SteamFriends() && steamapicontext->SteamUtils() && steamapicontext->SteamUser() )
	{
		static XUID s_myXuid = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
		bool bLarge = ( s_myXuid == m_xXUID );
#if 0
		// For debugging we can set to always use entity avatar data to test our own avatar
		// and always keep own avatar the same size
		bLarge = false;
#endif

		static ConVarRef sv_reliableavatardata( "sv_reliableavatardata", true );
		if ( bLarge && sv_reliableavatardata.IsValid() && ( sv_reliableavatardata.GetInt() == 2 ) )
			bLarge = false;	// Server is overriding all avatars, force update own avatar from server data too
		
		if ( pProvider && !sm_pProvider )
			sm_pProvider = pProvider;
		if ( !pProvider && sm_pProvider )
			pProvider = sm_pProvider;

		IScaleformAvatarImageProvider::ImageInfo_t ii = {};
		if ( !bLarge && pProvider && pProvider->GetImageInfo( m_xXUID, &ii ) )
		{
			// Expect 64x64 RGB images here
			if ( ii.m_cbImageData == 64*64*3 )
			{
				uint32 wide = 64, tall = 64;
				int cubImage = wide * tall * 4;
				m_bufRgbaBuffer.EnsureCapacity( cubImage );
				memset( m_bufRgbaBuffer.Base(), 0xFF, cubImage );
				for ( int y = 0; y < 64; ++ y ) for ( int x = 0; x < 64; ++ x )
				{	// Explode RGB into RGBA
					V_memcpy( ( ( byte * ) m_bufRgbaBuffer.Base() ) + y*64*4 + x*4, ( ( byte const * ) ii.m_pvImageData ) + y*64*3 + x*3, 3 );
				}

				InitFromBuffer( ( byte* ) m_bufRgbaBuffer.Base(), wide, tall, IMAGE_FORMAT_RGBA8888 );
				return true;
			}
		}

#if 0
		// For debugging we never use Steam avatars, always try to use engine.dll provider
		return true;
#endif

		if ( bLarge )
		{
			// For the local user force to download the local medium avatar as well as large
			int iAvatar = steamapicontext->SteamFriends()->GetMediumFriendAvatar( m_xXUID );
			if ( !iAvatar || ( iAvatar == -1 ) )
			{
				( void ) steamapicontext->SteamFriends()->RequestUserInformation( m_xXUID, false );
			}
		}

		int iAvatar = bLarge
			? steamapicontext->SteamFriends()->GetLargeFriendAvatar( m_xXUID )
			: steamapicontext->SteamFriends()->GetMediumFriendAvatar( m_xXUID );
		if ( !iAvatar || ( iAvatar == -1 ) )
		{
			bool bRequestEnqueuedAsync = steamapicontext->SteamFriends()->RequestUserInformation( m_xXUID, false );
			if ( !bRequestEnqueuedAsync )	// try again one more time, Steam says everything is available
				iAvatar = bLarge
					? steamapicontext->SteamFriends()->GetLargeFriendAvatar( m_xXUID )
					: steamapicontext->SteamFriends()->GetMediumFriendAvatar( m_xXUID );
		}

		if ( iAvatar != 0 )
		{
			/*
			// See if it's in our list already
			*/

			uint32 wide, tall;
			if ( steamapicontext->SteamUtils()->GetImageSize( iAvatar, &wide, &tall ) )
			{
				bool bUseSteamImage = true;
				if ( wide == 0 || tall == 0 )
				{
					// attempt to handle rare data integrity issue, avatar got lost
					bUseSteamImage = false;
					// mock up solid white as 64x64
					wide = tall = 64;
				}			

				int cubImage = wide * tall * 4;
				m_bufRgbaBuffer.EnsureCapacity( cubImage );
				memset( m_bufRgbaBuffer.Base(), 0xFF, cubImage );
				if ( bUseSteamImage )
				{
					steamapicontext->SteamUtils()->GetImageRGBA( iAvatar, (byte*)m_bufRgbaBuffer.Base(), cubImage );
				}

				InitFromBuffer( (byte*)m_bufRgbaBuffer.Base(), wide, tall, IMAGE_FORMAT_RGBA8888 );
			}
		}
	}
#elif defined( _X360 )
	X360_ResetAsyncImageState();
	return true;
#endif

	return true;
}
