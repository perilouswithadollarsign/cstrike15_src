//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox Launch Routines.
//
//=====================================================================================//

#ifndef _XBOX_LAUNCH_H_
#define _XBOX_LAUNCH_H_

#pragma once

#ifndef _CERT
#pragma comment( lib, "xbdm.lib" )
#endif

// id and version are used to tag the data blob, currently only need a singe hardcoded id
// when the version and id don't match, the data blob is not ours
#define VALVE_LAUNCH_ID			(('V'<<24)|('A'<<16)|('L'<<8)|('V'<<0))
#define VALVE_LAUNCH_VERSION	1

// launch flags
#define LF_ISDEBUGGING			0x80000000	// set if session was active prior to launch
#define LF_INTERNALLAUNCH		0x00000001	// set if launch was internal (as opposed to dashboard)
#define LF_EXITFROMINSTALLER	0x00000002	// set if exit was from an installer
#define LF_EXITFROMGAME			0x00000004	// set if exit was from a game
#define LF_EXITFROMCHOOSER		0x00000008	// set if exit was from the chooser
#define LF_WARMRESTART			0x00000010	// set if game wants to restart self (skips intro movies)
#define LF_INSTALLEDTOCACHE		0x00000040	// set if installer populated or validated cache partition
#define LF_UNKNOWNDATA			0x00000080

#pragma pack(1)
struct launchHeader_t
{
	unsigned int	id;
	unsigned int	version;
	unsigned int	flags;

	int				nUserID;
	int				nCtrlr2Storage[4];
	char			nSlot2Ctrlr[4];
	char			nSlot2Guest[4];
	int				numGameUsers;

	int				bForceEnglish;

	// increments at each engine re-launch
	DWORD			nAttractID;
	
	// for caller defined data, occurs after this header
	// limited to slightly less than MAX_LAUNCH_DATA_SIZE
	unsigned int	nDataSize;
};
#pragma pack()

// per docs, no larger than MAX_LAUNCH_DATA_SIZE
union xboxLaunchData_t
{
	launchHeader_t	header;		
	char			data[MAX_LAUNCH_DATA_SIZE];
};

//--------------------------------------------------------------------------------------
// Simple class to wrap the peristsent launch payload.
//
// Can be used by an application that does not use tier0 (i.e. the launcher). 
// Primarily designed to be anchored in tier0, so multiple systems can easily query and
// set the persistent payload.
//--------------------------------------------------------------------------------------
class CXboxLaunch
{
public:
	CXboxLaunch()
	{
		ResetLaunchData();
	}

	void ResetLaunchData()
	{
		// invalid until established
		// nonzero identifies a valid payload
		m_LaunchDataSize = 0;

		m_Launch.header.id = 0;
		m_Launch.header.version = 0;
		m_Launch.header.flags = 0;

		m_Launch.header.nUserID = XBX_INVALID_USER_ID;
		m_Launch.header.bForceEnglish = false;

		m_Launch.header.nCtrlr2Storage[0] = XBX_INVALID_STORAGE_ID;
		m_Launch.header.nCtrlr2Storage[1] = XBX_INVALID_STORAGE_ID;
		m_Launch.header.nCtrlr2Storage[2] = XBX_INVALID_STORAGE_ID;
		m_Launch.header.nCtrlr2Storage[3] = XBX_INVALID_STORAGE_ID;

		m_Launch.header.nSlot2Ctrlr[0] = 0;
		m_Launch.header.nSlot2Ctrlr[1] = 1;
		m_Launch.header.nSlot2Ctrlr[2] = 2;
		m_Launch.header.nSlot2Ctrlr[3] = 3;

		m_Launch.header.nSlot2Guest[0] = 0;
		m_Launch.header.nSlot2Guest[1] = 0;
		m_Launch.header.nSlot2Guest[2] = 0;
		m_Launch.header.nSlot2Guest[3] = 0;

		m_Launch.header.numGameUsers = 0;

		m_Launch.header.nAttractID = 0;

		m_Launch.header.nDataSize = 0;
	}

	// Returns how much space can be used by caller
	int MaxPayloadSize()
	{
		return sizeof( xboxLaunchData_t ) - sizeof( launchHeader_t );
	}

	bool SetLaunchData( void *pData, int dataSize, int flags = 0 )
	{
#if defined( _DEMO )
		if ( pData && ( flags & LF_UNKNOWNDATA ) )
		{
			// not ours, put the demo structure back as-is
			XSetLaunchData( pData, dataSize );
			m_LaunchDataSize = dataSize;
			return true;
		}
#endif
		if ( pData && dataSize && dataSize > MaxPayloadSize() )
		{
			// not enough room
			return false;
		}

		if ( pData && dataSize && dataSize <= MaxPayloadSize() )
		{
			memcpy( m_Launch.data + sizeof( launchHeader_t ), pData, dataSize );
			m_Launch.header.nDataSize = dataSize;
		}
		else
		{
			m_Launch.header.nDataSize = 0;
		}

		flags |= LF_INTERNALLAUNCH;
#if !defined( _CERT )
		if ( DmIsDebuggerPresent() )
		{
			flags |= LF_ISDEBUGGING;
		}
#endif
		m_Launch.header.id = VALVE_LAUNCH_ID;
		m_Launch.header.version = VALVE_LAUNCH_VERSION;
		m_Launch.header.flags = flags;

		XSetLaunchData( &m_Launch, MAX_LAUNCH_DATA_SIZE );

		// assume successful, mark as valid
		m_LaunchDataSize = MAX_LAUNCH_DATA_SIZE;

		return true;
	}

	//--------------------------------------------------------------------------------------
	// Returns TRUE if the launch data blob is available. FALSE otherwise.
	// Caller is expected to validate and interpret contents based on ID.
	//--------------------------------------------------------------------------------------
	bool GetLaunchData( unsigned int *pID, void **pData, int *pDataSize )
	{
		if ( !m_LaunchDataSize )
		{
			// purposely not doing this in the constructor (unstable as used by tier0), but on first fetch
			bool bValid = false;
			DWORD dwLaunchDataSize;
			DWORD dwStatus = XGetLaunchDataSize( &dwLaunchDataSize );
			if ( dwStatus == ERROR_SUCCESS && dwLaunchDataSize <= MAX_LAUNCH_DATA_SIZE )
			{
				dwStatus = XGetLaunchData( (void*)&m_Launch, dwLaunchDataSize );
				if ( dwStatus == ERROR_SUCCESS )
				{
					bValid = true;
					m_LaunchDataSize = dwLaunchDataSize;
				}
			}

			if ( !bValid )
			{
				ResetLaunchData();
			}
		}

		// a valid launch payload could be ours (re-launch) or from an alternate booter (demo launcher)
		if ( m_LaunchDataSize == MAX_LAUNCH_DATA_SIZE && m_Launch.header.id == VALVE_LAUNCH_ID && m_Launch.header.version == VALVE_LAUNCH_VERSION )
		{
			// internal recognized format
			if ( pID )
			{
				*pID = m_Launch.header.id;
			}
			if ( pData )
			{
				*pData = m_Launch.data + sizeof( launchHeader_t );
			}
			if ( pDataSize )
			{
				*pDataSize = m_Launch.header.nDataSize;
			}
		}
		else if ( m_LaunchDataSize )
		{
			// not ours, unknown format, caller interprets
			if ( pID )
			{
				// assume payload was packaged with an initial ID
				*pID = *(unsigned int *)m_Launch.data;
			}
			if ( pData )
			{
				*pData = m_Launch.data;
			}
			if ( pDataSize )
			{
				*pDataSize = m_LaunchDataSize;
			}
		}
		else if ( !m_LaunchDataSize )
		{
			// mark for caller as all invalid
			if ( pID )
			{
				*pID = 0;
			}
			if ( pData )
			{
				*pData = NULL;
			}
			if ( pDataSize )
			{
				*pDataSize = 0;
			}
		}

		// valid when any data is available (not necessarily valve's tag)
		return ( m_LaunchDataSize != 0 );
	}

	//--------------------------------------------------------------------------------------
	// Returns TRUE if the launch data blob is available. FALSE otherwise.
	// Data blob could be ours or not.
	//--------------------------------------------------------------------------------------
	bool RestoreLaunchData()
	{
		return GetLaunchData( NULL, NULL, NULL );
	}

	//--------------------------------------------------------------------------------------
	// Restores the data blob. If the data blob is not ours, resets it.
	//--------------------------------------------------------------------------------------
	void RestoreOrResetLaunchData()
	{
		RestoreLaunchData();
#if !defined( _DEMO )
		if ( m_Launch.header.id != VALVE_LAUNCH_ID || m_Launch.header.version != VALVE_LAUNCH_VERSION )
		{
			// not interested in somebody else's data
			ResetLaunchData();
		}
#endif
	}

	//--------------------------------------------------------------------------------------
	// Returns OUR internal launch flags.
	//--------------------------------------------------------------------------------------
	int GetLaunchFlags()
	{
		// establish the data
		RestoreOrResetLaunchData();
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return 0;
		}
#endif
		return m_Launch.header.flags;
	}

	void SetLaunchFlags( unsigned int ufNewFlags )
	{
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return;
		}
#endif
		m_Launch.header.flags = ufNewFlags;
	}

	void GetStorageID( int storageID[4] )
	{
		RestoreOrResetLaunchData();
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			storageID[0] = XBX_INVALID_STORAGE_ID;
			storageID[1] = XBX_INVALID_STORAGE_ID;
			storageID[2] = XBX_INVALID_STORAGE_ID;
			storageID[3] = XBX_INVALID_STORAGE_ID;
			return;
		}
#endif
		memcpy( storageID, m_Launch.header.nCtrlr2Storage, sizeof( m_Launch.header.nCtrlr2Storage ) );
	}
	void SetStorageID( int const storageID[4] )
	{
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return;
		}
#endif
		memcpy( m_Launch.header.nCtrlr2Storage, storageID, sizeof( m_Launch.header.nCtrlr2Storage ) );
	}

	void GetSlotUsers( int &numGameUsers, char nSlot2Ctrlr[4], char nSlot2Guest[4] )
	{
		RestoreOrResetLaunchData();
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			numGameUsers = 0;

			nSlot2Ctrlr[0] = 0;
			nSlot2Ctrlr[1] = 1;
			nSlot2Ctrlr[2] = 2;
			nSlot2Ctrlr[3] = 3;

			nSlot2Guest[0] = 0;
			nSlot2Guest[1] = 0;
			nSlot2Guest[2] = 0;
			nSlot2Guest[3] = 0;
			return;
		}
#endif
		numGameUsers = m_Launch.header.numGameUsers;
		memcpy( nSlot2Ctrlr, m_Launch.header.nSlot2Ctrlr, sizeof( m_Launch.header.nSlot2Ctrlr ) );
		memcpy( nSlot2Guest, m_Launch.header.nSlot2Guest, sizeof( m_Launch.header.nSlot2Guest ) );
	}
	void SetSlotUsers( int numGameUsers, char const nSlot2Ctrlr[4], char const nSlot2Guest[4] )
	{
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return;
		}
#endif
		m_Launch.header.numGameUsers = numGameUsers;
		memcpy( m_Launch.header.nSlot2Ctrlr, nSlot2Ctrlr, sizeof( m_Launch.header.nSlot2Ctrlr ) );
		memcpy( m_Launch.header.nSlot2Guest, nSlot2Guest, sizeof( m_Launch.header.nSlot2Guest ) );
	}

	int GetUserID( void )
	{
		RestoreOrResetLaunchData();
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return XBX_INVALID_USER_ID;
		}
#endif
		return m_Launch.header.nUserID;
	}
	void SetUserID( int userID )
	{
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return;
		}
#endif
		m_Launch.header.nUserID = userID;
	}

	bool GetForceEnglish( void )
	{
		RestoreOrResetLaunchData();
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return false;
		}
#endif
		return m_Launch.header.bForceEnglish ? true : false;
	}
	void SetForceEnglish( bool bForceEnglish )
	{
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return;
		}
#endif
		m_Launch.header.bForceEnglish = bForceEnglish;
	}

	DWORD GetAttractID( void )
	{
		RestoreOrResetLaunchData();
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return 0;
		}
#endif
		return m_Launch.header.nAttractID;
	}
	void SetAttractID( DWORD nAttractID )
	{
#if defined( _DEMO )
		if ( m_Launch.header.id && m_Launch.header.id != VALVE_LAUNCH_ID )
		{
			return;
		}
#endif
		m_Launch.header.nAttractID = nAttractID;
	}

	void Launch( const char *pNewImageName = NULL )
	{
		if ( !pNewImageName )
		{
#if defined( _DEMO )
			pNewImageName = XLAUNCH_KEYWORD_DEFAULT_APP;
#else
			pNewImageName = "default.xex";
#endif
		}

		XLaunchNewImage( pNewImageName, 0 );
	}

private:
	xboxLaunchData_t	m_Launch;
	DWORD				m_LaunchDataSize;
};

#if defined( PLATFORM_H )
// For applications that use tier0.dll
PLATFORM_INTERFACE CXboxLaunch *XboxLaunch();
#endif

#endif
