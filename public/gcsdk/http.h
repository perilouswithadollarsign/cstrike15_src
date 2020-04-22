//====== Copyright © 1996-2010, Valve Corporation, All rights reserved. =======
//
// Purpose: HTTP related enums and objects, stuff that both clients and server use should go here
//
//=============================================================================

#ifndef HTTP_H
#define HTTP_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steamhttpenums.h"
#include "tier1/keyvalues.h"
#include "tier1/netadr.h"

class CMsgHttpRequest;
class CMsgHttpResponse;
#include "tier0/memdbgon.h"

namespace GCSDK
{

// Container class for useful parsing methods and other utility code used by client or server http code
class CHTTPUtil
{
public:
	// Check if a status code allows a body to exist
	static bool BStatusCodeAllowsBody( EHTTPStatusCode eHTTPStatus );

};


class CHTTPRequest;
class CHTTPResponse;
class CHTTPServerClientConnection;

// A request parameter, either get or post, or parsed out of the URL
class CHTTPRequestParam
{
public:

	// Default constructor
	CHTTPRequestParam()
	{
		m_pData = NULL;
		m_cubDataLength = 0;
	}

	// Constructor with data
	CHTTPRequestParam( const char *pchName, uint8 *pData, uint32 cubDataLen )
	{
		m_strName = pchName;

		// We always allocate an extra byte to null terminate, so we can treat as a string safely
		// even though treating as a string may truncate early if the data is binary.
		m_pData = new uint8[cubDataLen+1];
		Q_memcpy( m_pData, pData, cubDataLen );
		m_pData[cubDataLen] = 0;

		m_cubDataLength = cubDataLen;
	}

	// Copy constructor (does a deep copy)
	CHTTPRequestParam(const CHTTPRequestParam& rhs) 
	{
		m_strName = rhs.m_strName;

		m_cubDataLength = rhs.m_cubDataLength;

		m_pData = new uint8[rhs.m_cubDataLength+1];
		Q_memcpy( m_pData, rhs.m_pData, rhs.m_cubDataLength );
		m_pData[m_cubDataLength] = 0;

	}

	// Operator = 
	const CHTTPRequestParam& operator=(const CHTTPRequestParam& rhs)
	{
		if ( m_pData )
			delete[] m_pData;

		m_strName = rhs.m_strName;

		m_cubDataLength = rhs.m_cubDataLength;

		m_pData = new uint8[rhs.m_cubDataLength+1];
		Q_memcpy( m_pData, rhs.m_pData, rhs.m_cubDataLength );
		m_pData[m_cubDataLength] = 0;

		return *this;
	}

	// Destructor
	~CHTTPRequestParam()
	{
		if ( m_pData )
			delete[] m_pData;

		m_pData = NULL;
	}

	// Set the request parameters name
	void SetName( const char *pchName )
	{
		m_strName = pchName;
	}

	// Set the data for the parameter, this is expected to either be binary
	// data, or to be in string form, the conversion Getters will all Q_atoi
	// or such when converting to non-string forms.
	void SetRawData( uint8 *pData, uint32 cubDataLen )
	{
		if ( m_pData )
			delete[] m_pData;

		m_cubDataLength = cubDataLen;

		m_pData = new uint8[cubDataLen+1];
		Q_memcpy( m_pData, pData, cubDataLen );
		m_pData[m_cubDataLength] = 0;
	}


	// Get the name
	const char *GetName() const
	{
		return m_strName.Get();
	}


	// Get pointer to the data
	const uint8 *GetPubData() const
	{
		return m_pData;
	}


	// Get the length of the data
	uint32 GetCubData() const
	{
		return m_cubDataLength;
	}


	// Get the data as a string
	const char *ToString() const
	{
		return (char *)m_pData;
	}


	// Get the data converted to an int32
	int32 ToInt32() const
	{
		return Q_atoi( (char *)m_pData );
	}


	// Get the data converted to an int64
	int64 ToInt64() const
	{
		return Q_atoi64( (char*)m_pData );
	}


	// Get the data converted to an uint32
	uint32 ToUInt32() const
	{
		return (uint32)V_atoui64( (char *)m_pData );
	}


	// Get the data converted to an uint64
	uint64 ToUInt64() const 
	{
		return V_atoui64( (char *)m_pData );
	}

	// Get the data converted to a float
	float ToFloat() const
	{
		return Q_atof( (char *)m_pData );
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_strName );
		validator.ClaimMemory( m_pData );
	}
#endif // DBGFLAG_VALIDATE

private:
	CUtlString m_strName;
	uint32 m_cubDataLength;
	uint8 *m_pData;
};

class CHTTPRequest
{
public:
	CHTTPRequest();
	CHTTPRequest( EHTTPMethod eMethod, const char *pchHost, const char *pchRelativeURL );
	CHTTPRequest( EHTTPMethod eMethod, const char *pchAbsoluteURL );

	virtual ~CHTTPRequest();

	// Get the method type for the request (ie, GET, POST, etc)
	EHTTPMethod GetEHTTPMethod() { return m_eRequestMethod; }

	// Get the relative URL for the request
	const char *GetURL() { return m_strURL.Get(); }

	// Get the value of a GET parameter, using the default value if not set.  This is case-insensitive by default.
	const CHTTPRequestParam *GetGETParam( const char *pchGetParamName, bool bMatchCase = false ) const;
	const char *GetGETParamString( const char *pchGetParamName, const char *pchDefault, bool bMatchCase = false ) const;
	bool	GetGETParamBool( const char *pchGetParamName, bool bDefault, bool bMatchCase = false ) const;
	int32	GetGETParamInt32( const char *pchGetParamName, int32 nDefault, bool bMatchCase = false ) const;
	uint32	GetGETParamUInt32( const char *pchGetParamName, uint32 unDefault, bool bMatchCase = false ) const;
	int64	GetGETParamInt64( const char *pchGetParamName, int64 nDefault, bool bMatchCase = false ) const;
	uint64	GetGETParamUInt64( const char *pchGetParamName, uint64 unDefault, bool bMatchCase = false ) const;
	float	GetGETParamFloat( const char *pchGetParamName, float fDefault, bool bMatchCase = false ) const;


	// Get the value of a POST parameter, using the default value if not set.  This is case-insensitive by default.
	const CHTTPRequestParam *GetPOSTParam( const char *pchPostParamName, bool bMatchCase = false ) const;
	const char *GetPOSTParamString( const char *pchGetParamName, const char *pchDefault, bool bMatchCase = false ) const;
	bool	GetPOSTParamBool( const char *pchGetParamName, bool bDefault, bool bMatchCase = false ) const;
	int32	GetPOSTParamInt32( const char *pchGetParamName, int32 nDefault, bool bMatchCase = false ) const;
	uint32	GetPOSTParamUInt32( const char *pchGetParamName, uint32 unDefault, bool bMatchCase = false ) const;
	int64	GetPOSTParamInt64( const char *pchGetParamName, int64 nDefault, bool bMatchCase = false ) const;
	uint64	GetPOSTParamUInt64( const char *pchGetParamName, uint64 unDefault, bool bMatchCase = false ) const;
	float	GetPOSTParamFloat( const char *pchGetParamName, float fDefault, bool bMatchCase = false ) const;

	// Add a GET param to the request
	void SetGETParamString( const char *pchGetParamName, const char *pString ) { SetGETParamRaw( pchGetParamName, (uint8*)pString, Q_strlen(pString) ); }
	void SetGETParamBool( const char *pchPostParamName, bool bValue ) { SetGETParamRaw( pchPostParamName, (uint8*)(bValue ? "1" : "0"), 1 ); }
	void SetGETParamInt32( const char *pchPostParamName, int32 nValue ) { CFmtStrN<32> str( "%d", nValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetGETParamUInt32( const char *pchPostParamName, uint32 unValue ) { CFmtStrN<32> str( "%u", unValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetGETParamInt64( const char *pchPostParamName, int64 nValue ) { CFmtStrN<32> str( "%lld", nValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetGETParamUInt64( const char *pchPostParamName, uint64 unValue ) { CFmtStrN<32> str( "%llu", unValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetGETParamFloat( const char *pchPostParamName, float fValue ) { CFmtStrN<32> str( "%f", fValue ); SetGETParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }

	// Adds a GET param containing raw data to the request. If you are using the Web API, you probably do not want this function
	void SetGETParamRaw( const char *pchGetParamName, uint8 *pData, uint32 cubDataLen );

	// Add a POST param to the request given a string for the name and value
	void SetPOSTParamString( const char *pchPostParamName, const char *pString ) { SetPOSTParamRaw( pchPostParamName, (uint8*)pString, Q_strlen(pString) ); }
	void SetPOSTParamBool( const char *pchPostParamName, bool bValue ) { SetPOSTParamRaw( pchPostParamName, (uint8*)(bValue ? "1" : "0"), 1 ); }
	void SetPOSTParamInt32( const char *pchPostParamName, int32 nValue ) { CFmtStrN<32> str( "%d", nValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetPOSTParamUInt32( const char *pchPostParamName, uint32 unValue ) { CFmtStrN<32> str( "%u", unValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetPOSTParamInt64( const char *pchPostParamName, int64 nValue ) { CFmtStrN<32> str( "%lld", nValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetPOSTParamUInt64( const char *pchPostParamName, uint64 unValue ) { CFmtStrN<32> str( "%llu", unValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }
	void SetPOSTParamFloat( const char *pchPostParamName, float fValue ) { CFmtStrN<32> str( "%f", fValue ); SetPOSTParamRaw( pchPostParamName, (uint8*)str.Access(), str.Length() ); }

	// Adds a POST param containing raw data to the request. If you are using the Web API, you probably do not want this function
	void SetPOSTParamRaw( const char *pchPostParamName, uint8 *pData, uint32 cubDataLen );

	// Get count of POST params in the request
	uint32 GetPOSTParamCount() { return m_vecPostParams.Count(); }

	// Get count of GET params in the request
	uint32 GetGETParamCount() { return m_vecGetParams.Count(); }

	// Fetch a request header by header name and convert it to a time value
	RTime32 GetRequestHeaderTimeValue( const char *pchRequestHeaderName, RTime32 rtDefault = 0 );

	// Fetch a request headers value by header name
	const char *GetRequestHeaderValue( const char *pchRequestHeaderName, const char *pchDefault = NULL ) { return m_pkvRequestHeaders->GetString( pchRequestHeaderName, pchDefault ); }

	// Check if the request has any value for the header with the given name
	bool BHasRequestHeader( const char *pchRequestHeaderName ) { return (m_pkvRequestHeaders->GetString(pchRequestHeaderName, NULL ) ? true : false); }

	// Set the method for the request object
	void SetEHTTPMethod( EHTTPMethod eMethod ) { m_eRequestMethod = eMethod; }

	// Set the relative URL for the request
	void SetURL( const char *pchURL ) 
	{ 
		AssertMsg1( pchURL && pchURL[0] == '/', "URLs must start with the slash (/) character. Param: %s", pchURL );
		m_strURL = pchURL; 
	}

	// Set a header field for the request
	void SetRequestHeaderValue( const char *pchHeaderName, const char *pchHeaderString ) { m_pkvRequestHeaders->SetString( pchHeaderName, pchHeaderString ); }

	// Set body data
	bool BSetBodyData( uint8 *pubData, int cubData );

	// Get direct access to body data buffer
	CUtlBuffer *GetBodyBuffer() { return &m_bufBody; }

	// Set hostname for request to target (or host it was received on)
	void SetHostname( const char *pchHost ) { m_strHostname.Set( pchHost ); }
	void SetHostnameDirect( const char *pchHost, uint32 unLength ) { m_strHostname.SetDirect( pchHost, unLength ); }
	const char *GetHostname() { return m_strHostname.Get(); }

	// Get direct access to GET param vector
	CUtlVector<CHTTPRequestParam> &GetGETParamVector() { return m_vecGetParams; }
	CUtlVector<CHTTPRequestParam> &GetPOSTParamVector() { return m_vecPostParams; }

	// Get direct access to request headers
	KeyValues *GetRequestHeadersKV() { return m_pkvRequestHeaders; }

	// writes the request into a protobuf for use in messages
	void SerializeIntoProtoBuf( CMsgHttpRequest & request ) const;

	// reads the request out of a protobuf from a message
	void DeserializeFromProtoBuf( const CMsgHttpRequest & apiKey );


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

protected:
	// Common initialization code
	void Init();

	// Method for this request
	EHTTPMethod m_eRequestMethod;

	// Header values and get param values.  Headers are defined with the name case-insensitive, so KV is a good fit.
	KeyValues *m_pkvRequestHeaders;

	// Get/Post params, these can be case sensitive, so keyvalues would be a bad fit.  They would also be a bad
	// fit for KV since we may use lots of random param names and use up space in the global string table.
	CUtlVector<CHTTPRequestParam> m_vecGetParams;
	CUtlVector<CHTTPRequestParam> m_vecPostParams;

	// Host to issue the request to (or host received on server side) 
	// Note: This is not the Host: header value, but rather the actual address the request is received on
	// is meant to be issued to.  The Host: header may be different containing a virtual hostname.
	CUtlString m_strHostname;

	// Relative URL
	CUtlString m_strURL;

	// Body of the request
	CUtlBuffer m_bufBody;
};


//-----------------------------------------------------------------------------
// Purpose: A wrapper to CHTTPRequest for Steam WebAPIs. Host, mode, and API key 
//	are set automatically
//-----------------------------------------------------------------------------
class CSteamAPIRequest : public CHTTPRequest
{
public:
	CSteamAPIRequest( EHTTPMethod eMethod, const char *pchInterface, const char *pchMethod, int nVersion );
};


class CHTTPResponse
{
public:
	CHTTPResponse();
	virtual ~CHTTPResponse();

	// Get a specific headers data 
	const char *GetResponseHeaderValue( const char *pchName, const char *pchDefault = NULL ) { return m_pkvResponseHeaders->GetString( pchName, pchDefault ); }

	// Set a specific headers data, will clobber any existing value for that header
	virtual void SetResponseHeaderValue( const char *pchName, const char *pchValue ) { m_pkvResponseHeaders->SetString( pchName, pchValue ); }

	// Set status code for response
	virtual void SetStatusCode( EHTTPStatusCode eStatusCode ) { m_eStatusCode = eStatusCode; }

	// Set the expiration header based on a time delta from now
	void SetExpirationHeaderDeltaFromNow( int32 nSecondsFromNow );

	// Set the expiration header based on a time delta from now
	void SetHeaderTimeValue( const char *pchHeaderName, RTime32 rtTimestamp );

	// Get the entire headers KV (in case you want to iterate them all or such)
	KeyValues *GetResponseHeadersKV() { return m_pkvResponseHeaders; }

	// Accessors to the body data in the response
	uint8 *GetPubBody() { return (uint8*)m_bufBody.Base(); }
	uint32 GetCubBody() { return m_bufBody.TellPut(); }
	CUtlBuffer *GetBodyBuffer() { return &m_bufBody; }

	// Accessor
	EHTTPStatusCode GetStatusCode() { return m_eStatusCode; }

	// writes the response into a protobuf for use in messages
	void SerializeIntoProtoBuf( CMsgHttpResponse & response ) const;

	// reads the response out of a protobuf from a message
	void DeserializeFromProtoBuf( const CMsgHttpResponse & response );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

protected:
	EHTTPStatusCode m_eStatusCode;
	CUtlBuffer m_bufBody;
	KeyValues *m_pkvResponseHeaders;
};

}
#include "tier0/memdbgoff.h"

#endif // HTTP_H
