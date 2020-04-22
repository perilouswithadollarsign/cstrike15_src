//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			global system messages used by the GC. These are usually sent by
//			the GC Host so be very careful of versioning issues when you consider
//			changing them.  Note that we never use types with undefined length 
//			(like int).  Always use an explicit type (like int32).
//
//=============================================================================

#ifndef GCSYSTEMMSGS_H
#define GCSYSTEMMSGS_H
#ifdef _WIN32
#pragma once
#endif


namespace GCSDK
{


#pragma pack( push, 8 ) // this is a 8 instead of a 1 to maintain backward compatibility with Steam

enum EGCSystemMsg
{
	k_EGCMsgInvalid =							0,
	k_EGCMsgMulti =								1,

	k_EGCMsgGenericReply =						10,

	k_EGCMsgSystemBase =						50,
	k_EGCMsgAchievementAwarded =				51,
	k_EGCMsgConCommand =						52,		// A command from the GC's admin console
	k_EGCMsgStartPlaying =						53,
	k_EGCMsgStopPlaying =						54,
	k_EGCMsgStartGameserver =					55,
	k_EGCMsgStopGameserver =					56,
	k_EGCMsgWGRequest =							57,
	k_EGCMsgWGResponse =						58,
	k_EGCMsgGetUserGameStatsSchema =			59,		// Gets the user game stats schema for the app
	k_EGCMsgGetUserGameStatsSchemaResponse =	60,
	k_EGCMsgGetUserStatsDEPRECATED =			61,		// Gets user game stats for a user
	k_EGCMsgGetUserStatsResponse =				62,
	k_EGCMsgAppInfoUpdated =					63,		// Message sent to the GC when there has been an AppInfo update
	k_EGCMsgValidateSession =					64,		// Message sent by the GC when it wants to make sure a session exists
	k_EGCMsgValidateSessionResponse =			65,		// Message sent to the GC in response to ValidateSession 
	k_EGCMsgLookupAccountFromInput =			66,		// Sent by the GC to lookup user. Reply is k_EGCMsgGenericReply
	k_EGCMsgSendHTTPRequest =					67,		// Message sent by the GC to do a generic HTTP request
	k_EGCMsgSendHTTPRequestResponse =			68,		// Response back to the GC with the results of the HTTP request
	k_EGCMsgPreTestSetup =						69,		// Reset the GC database (usually for testing purposes)
	k_EGCMsgRecordSupportAction =				70,		// Logs a support action
	k_EGCMsgGetAccountDetails =					71,		// Requests the details for an account
	k_EGCMsgSendInterAppMessage =				72,		// Sends a message to another app's GC
	k_EGCMsgReceiveInterAppMessage =			73,		// Receives a message from another app's GC
	k_EGCMsgFindAccounts =						74,		// queries the AMs for accounts by name
	k_EGCMsgPostAlert =							75,		// posts an alert to Steam
	k_EGCMsgGetLicenses =						76,		// asks Steam for the user's licenses
	k_EGCMsgGetUserStats =						77,		// Gets user game stats for a user
	k_EGCMsgGetCommands =						78,		// request for a list of commands from a gc console
	k_EGCMsgGetCommandsResponse =				79,		// response with a list of commands for a gc console
	k_EGCMsgAddFreeLicense =					80,		// request for for Steam to add a license to the specified free package
	k_EGCMsgAddFreeLicenseResponse =			81,		// response with the result of the attempt to add a free license
	k_EGCMsgGetIPLocation = 					82,		// Get geolocation data for a specific IP
	k_EGCMsgGetIPLocationResponse = 			83,		// Geolocation response

	k_EGCMsgSystemStatsSchema =		 			84,		// Message sent by the GC specifying what its stats schema is
	k_EGCMsgGetSystemStats =		 			85,		// Message sent to the GC requesting its stats
	k_EGCMsgGetSystemStatsResponse =	 		86,		// Message sent by the GC with its stats

	k_EGCMsgSendEmail =		 					87,		// Sent by the GC to send an email to a user
	k_EGCMsgSendEmailResponse =	 				88,		// Response with the result of the send request
	k_EGCMsgGetEmailTemplate =					89,		// Sent to the GC to request an email template
	k_EGCMsgGetEmailTemplateResponse =			90,		// Get email template response

	// web API calls
	k_EGCMsgWebAPIBase =						100,
	k_EGCMsgWebAPIRegisterInterfaces =			k_EGCMsgWebAPIBase + 1, // sent once at startup to register APIs
	k_EGCMsgWebAPIJobRequest =					k_EGCMsgWebAPIBase + 2, // sent when an actual request is made
	k_EGCMsgWebAPIRegistrationRequested =		k_EGCMsgWebAPIBase + 3, // sent by the GC Host when it learns a web API server has started

	// Memcached
	k_EGCMsgMemCachedBase =						200,	// Get key(s) from memcached
	k_EGCMsgMemCachedGet =						k_EGCMsgMemCachedBase,	// Get key(s) from memcached
	k_EGCMsgMemCachedGetResponse =				k_EGCMsgMemCachedBase + 1,		// Retrieved keys
	k_EGCMsgMemCachedSet =						k_EGCMsgMemCachedBase + 2,		// Set key(s) into memcached
	k_EGCMsgMemCachedDelete =					k_EGCMsgMemCachedBase + 3,		// Delete key(s) from memcached
};


// generic zero-length message struct
struct MsgGCEmpty_t
{

};

// k_EGCMsgAchievementAwarded 
struct MsgGCAchievementAwarded_t
{
	uint16 m_usStatID;
	uint8 m_ubBit;
	// var data:
	//    string data: name of achievement earned
};

// k_EGCMsgConCommand
struct MsgGCConCommand_t
{
	// var data:
	//		string: the command as typed into the console
};


// k_EGCMsgStartPlaying
struct MsgGCStartPlaying_t
{
	CSteamID m_steamID;
	CSteamID m_steamIDGS;
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
};


// k_EGCMsgStartPlaying
// k_EGCMsgStopGameserver
struct MsgGCStopSession_t
{
	CSteamID m_steamID;
};


// k_EGCMsgStartGameserver
struct MsgGCStartGameserver_t
{
	CSteamID m_steamID;
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
};

// k_EGCMsgWGRequest
struct MsgGCWGRequest_t
{
	uint64 m_ulSteamID;		//SteamID of auth'd WG user
	uint32 m_unPrivilege;	// The EGCWebApiPrivilege value that the request was made with
	uint32  m_cubKeyValues;	// length of the key values data blob in message (starts after string request name data)
	// var data - 
	//		request name
	//		binary key values of web request
};

// k_EGCMsgWGResponse
struct MsgGCWGResponse_t
{
	bool m_bResult;			// True if the request was successful
	uint32  m_cubKeyValues;	// length of the key values data blob in message
	// var data - 
	//		binary key values of web response
};


// k_EGCMsgGetUserGameStatsSchemaResponse
struct MsgGetUserGameStatsSchemaResponse_t
{
	bool m_bSuccess;		// True is the request was successful
	// var data -
	//		binary key values containing the User Game Stats schema
};


// k_EGCMsgGetUserStats
struct MsgGetUserStats_t
{
	uint64	m_ulSteamID;	// SteamID the stats are requested for
	uint16  m_cStatIDs;		// A count of the number of statIDs requested
	// var data -
	//		Array of m_cStatIDs 16-bit StatIDs
};


// k_EGCMsgGetUserStatsResponse
struct MsgGetUserStatsResponse_t
{
	uint64	m_ulSteamID;	// SteamID the stats were requested for
	bool	m_bSuccess;		// True is the request was successful
	uint16	m_cStats;		// Number of stats returned in the message
	// var data -
	//		m_cStats instances of:
	//			uint16 usStatID - Stat ID
	//			uint32 unData   - Stat value
};

// k_EGCMsgValidateSession
struct MsgGCValidateSession_t
{
	uint64	m_ulSteamID;	// SteamID to validate
};

// k_EGCMsgValidateSessionResponse
struct MsgGCValidateSessionResponse_t
{
	uint64 m_ulSteamID;
	uint64 m_ulSteamIDGS;
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
	bool m_bOnline;
};

// response to k_EGCMsgLookupAccountFromInput
struct MsgGCLookupAccountResponse
{
	uint64	m_ulSteamID;
};

// k_EGCMsgSendHTTPRequest
struct MsgGCSendHTTPRequest_t
{
	// Variable data:
	//	- Serialized CHTTPRequest
};

// k_EGCMsgSendHTTPRequestResponse
struct MsgGCSendHTTPRequestResponse_t
{
	bool m_bCompleted;
	// Variable data:
	//	- if m_bCompleted is true, Serialized CHTTPResponse
};


// k_EGCMsgRecordSupportAction
struct MsgGCRecordSupportAction_t
{
	uint32 m_unAccountID;		// which  account is affected (object)
	uint32 m_unActorID;		// who made the change (subject)
	// Variable data:
	//	- string - Custom data for the event
	//  - string - A note with the reason for the change
};


// k_EGCMsgWebAPIRegisterInterfaces
struct MsgGCWebAPIRegisterInterfaces_t
{
	uint32 m_cInterfaces;
	// Variable data:
	// - KeyValues for interface - one per interface
};

// k_EGCMsgGetAccountDetails
struct MsgGCGetAccountDetails_t
{
	uint64	m_ulSteamID;	// SteamID to validate
};


// k_EGCMsgSendInterAppMessage
// k_EGCMsgReceiveInterAppMessage
struct MsgGCInterAppMessage_t
{
	AppId_t m_unAppID;

	// Variable data:
	//   The message to send. Both GCs need to agree on the protocol
};


// Used by k_EGCMsgFindAccounts
enum EAccountFindType
{
	k_EFindAccountTypeInvalid = 0, 
	k_EFindAccountTypeAccountName = 1,
	k_EFindAccountTypeEmail,
	k_EFindAccountTypePersonaName,
	k_EFindAccountTypeURL,
	k_EFindAccountTypeAllOnline,
	k_EFindAccountTypeAll,
	k_EFindClanTypeClanName,
	k_EFindClanTypeURL,
	k_EFindClanTypeOfficialURL,
	k_EFindClanTypeAppID,
};


extern void InitGCSystemMessageTypes();



} // namespace GCSDK

#pragma pack( pop )

#endif // GCSYSTEMMSGS_H