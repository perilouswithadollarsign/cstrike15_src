//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef RNDEBUGNAME_HDR
#define RNDEBUGNAME_HDR

#ifdef _DEBUG
#define RUBIKON_DEBUG_NAMES 1
#else
#define RUBIKON_DEBUG_NAMES 0
#endif

class CRnDebugName
{
public:
	CRnDebugName() { Init( ); }
	~CRnDebugName();
public:
	void SetV( const char* pNameFormat, va_list args );
	const char *Get() const;
	const char *GetSafe( ) const; // return either name or "", but not NULL
	void Init();
public:
#if RUBIKON_DEBUG_NAMES
	char *m_pName; // please keep the name at the top if possible: it's much easier to debug in VS that way
#endif
};

inline void CRnDebugName::Init()
{
#if RUBIKON_DEBUG_NAMES
	m_pName = NULL;
#endif
}

inline CRnDebugName::~CRnDebugName()
{
#if RUBIKON_DEBUG_NAMES
	if( m_pName )
	{
		delete[]m_pName;
	}
#endif
}

//---------------------------------------------------------------------------------------
inline void CRnDebugName::SetV( const char *pNameFormat, va_list args )
{
#if RUBIKON_DEBUG_NAMES
	if( m_pName )
	{
		delete[]m_pName;
		m_pName = NULL;
	}

	if( pNameFormat )
	{
        CReuseVaList dup_args( args );
		int nLen = vsnprintf( NULL, 0, pNameFormat, dup_args.m_ReuseList );
		if( nLen > 0 )
		{
			m_pName =  new char[nLen + 2];
			m_pName[nLen] = '\xFF';
			m_pName[nLen + 1] = '\xFF';
			vsnprintf( m_pName, nLen + 1, pNameFormat, args );
			AssertDbg( m_pName[nLen + 1] == '\xFF' && m_pName[nLen] == '\0' );
		}
	}
#endif
}


inline const char *CRnDebugName::Get() const
{
#if RUBIKON_DEBUG_NAMES
	return m_pName;
#else
	return NULL;
#endif
}

inline const char *CRnDebugName::GetSafe() const
{
	const char *p = Get( );
	return p ? p : "";
}


#endif
