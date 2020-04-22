#define HELPER_OVERLAPPED_SESSION_CALL_C_0(  ) 
#define HELPER_OVERLAPPED_SESSION_CALL_A_0(  ) 
#define HELPER_OVERLAPPED_SESSION_CALL_P_0(  ) 
#define HELPER_OVERLAPPED_SESSION_CALL_I_0(  ) 
#define HELPER_OVERLAPPED_SESSION_CALL_M_0(  ) 

#define DECLARE_OVERLAPPED_SESSION_CALL_0( XCallNameFN_T ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_0(  ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_0(  ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_0(  ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_0(  ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_0( XCallNameFN_T ) \
DECLARE_OVERLAPPED_SESSION_CALL_0( XCallNameFN_T ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_0(  ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_0(  ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_1( ArgType1, ArgName1 ) ArgType1 ArgName1, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_1( ArgType1, ArgName1 ) , m_##ArgName1( ArgName1 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_1( ArgType1, ArgName1 ) m_##ArgName1, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_1( ArgType1, ArgName1 ) ArgName1, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_1( ArgType1, ArgName1 ) ArgType1 m_##ArgName1;

#define DECLARE_OVERLAPPED_SESSION_CALL_1( XCallNameFN_T, ArgType1, ArgName1 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_1( ArgType1, ArgName1 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_1( ArgType1, ArgName1 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_1( ArgType1, ArgName1 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_1( ArgType1, ArgName1 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_1( XCallNameFN_T, ArgType1, ArgName1 ) \
DECLARE_OVERLAPPED_SESSION_CALL_1( XCallNameFN_T, ArgType1, ArgName1 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_1( ArgType1, ArgName1 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_1( ArgType1, ArgName1 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_2( ArgType1, ArgName1, ArgType2, ArgName2 ) ArgType1 ArgName1, ArgType2 ArgName2, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_2( ArgType1, ArgName1, ArgType2, ArgName2 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_2( ArgType1, ArgName1, ArgType2, ArgName2 ) m_##ArgName1, m_##ArgName2, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_2( ArgType1, ArgName1, ArgType2, ArgName2 ) ArgName1, ArgName2, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_2( ArgType1, ArgName1, ArgType2, ArgName2 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;

#define DECLARE_OVERLAPPED_SESSION_CALL_2( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_2( ArgType1, ArgName1, ArgType2, ArgName2 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_2( ArgType1, ArgName1, ArgType2, ArgName2 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_2( ArgType1, ArgName1, ArgType2, ArgName2 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_2( ArgType1, ArgName1, ArgType2, ArgName2 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_2( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2 ) \
DECLARE_OVERLAPPED_SESSION_CALL_2( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_2( ArgType1, ArgName1, ArgType2, ArgName2 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_2( ArgType1, ArgName1, ArgType2, ArgName2 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) ArgName1, ArgName2, ArgName3, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;

#define DECLARE_OVERLAPPED_SESSION_CALL_3( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_3( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
DECLARE_OVERLAPPED_SESSION_CALL_3( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_3( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, ArgType4 ArgName4, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 ), m_##ArgName4( ArgName4 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, m_##ArgName4, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) ArgName1, ArgName2, ArgName3, ArgName4, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;ArgType4 m_##ArgName4;

#define DECLARE_OVERLAPPED_SESSION_CALL_4( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_4( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
DECLARE_OVERLAPPED_SESSION_CALL_4( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_4( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, ArgType4 ArgName4, ArgType5 ArgName5, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 ), m_##ArgName4( ArgName4 ), m_##ArgName5( ArgName5 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, m_##ArgName4, m_##ArgName5, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) ArgName1, ArgName2, ArgName3, ArgName4, ArgName5, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;ArgType4 m_##ArgName4;ArgType5 m_##ArgName5;

#define DECLARE_OVERLAPPED_SESSION_CALL_5( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_5( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
DECLARE_OVERLAPPED_SESSION_CALL_5( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_5( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, ArgType4 ArgName4, ArgType5 ArgName5, ArgType6 ArgName6, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 ), m_##ArgName4( ArgName4 ), m_##ArgName5( ArgName5 ), m_##ArgName6( ArgName6 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, m_##ArgName4, m_##ArgName5, m_##ArgName6, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) ArgName1, ArgName2, ArgName3, ArgName4, ArgName5, ArgName6, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;ArgType4 m_##ArgName4;ArgType5 m_##ArgName5;ArgType6 m_##ArgName6;

#define DECLARE_OVERLAPPED_SESSION_CALL_6( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_6( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
DECLARE_OVERLAPPED_SESSION_CALL_6( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_6( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, ArgType4 ArgName4, ArgType5 ArgName5, ArgType6 ArgName6, ArgType7 ArgName7, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 ), m_##ArgName4( ArgName4 ), m_##ArgName5( ArgName5 ), m_##ArgName6( ArgName6 ), m_##ArgName7( ArgName7 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, m_##ArgName4, m_##ArgName5, m_##ArgName6, m_##ArgName7, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) ArgName1, ArgName2, ArgName3, ArgName4, ArgName5, ArgName6, ArgName7, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;ArgType4 m_##ArgName4;ArgType5 m_##ArgName5;ArgType6 m_##ArgName6;ArgType7 m_##ArgName7;

#define DECLARE_OVERLAPPED_SESSION_CALL_7( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_7( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
DECLARE_OVERLAPPED_SESSION_CALL_7( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_7( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, ArgType4 ArgName4, ArgType5 ArgName5, ArgType6 ArgName6, ArgType7 ArgName7, ArgType8 ArgName8, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 ), m_##ArgName4( ArgName4 ), m_##ArgName5( ArgName5 ), m_##ArgName6( ArgName6 ), m_##ArgName7( ArgName7 ), m_##ArgName8( ArgName8 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, m_##ArgName4, m_##ArgName5, m_##ArgName6, m_##ArgName7, m_##ArgName8, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) ArgName1, ArgName2, ArgName3, ArgName4, ArgName5, ArgName6, ArgName7, ArgName8, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;ArgType4 m_##ArgName4;ArgType5 m_##ArgName5;ArgType6 m_##ArgName6;ArgType7 m_##ArgName7;ArgType8 m_##ArgName8;

#define DECLARE_OVERLAPPED_SESSION_CALL_8( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_8( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
DECLARE_OVERLAPPED_SESSION_CALL_8( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_8( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8 ) \
		pxOverlapped ) ); \
};



#define HELPER_OVERLAPPED_SESSION_CALL_C_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) ArgType1 ArgName1, ArgType2 ArgName2, ArgType3 ArgName3, ArgType4 ArgName4, ArgType5 ArgName5, ArgType6 ArgName6, ArgType7 ArgName7, ArgType8 ArgName8, ArgType9 ArgName9, 
#define HELPER_OVERLAPPED_SESSION_CALL_A_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) , m_##ArgName1( ArgName1 ), m_##ArgName2( ArgName2 ), m_##ArgName3( ArgName3 ), m_##ArgName4( ArgName4 ), m_##ArgName5( ArgName5 ), m_##ArgName6( ArgName6 ), m_##ArgName7( ArgName7 ), m_##ArgName8( ArgName8 ), m_##ArgName9( ArgName9 )
#define HELPER_OVERLAPPED_SESSION_CALL_P_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) m_##ArgName1, m_##ArgName2, m_##ArgName3, m_##ArgName4, m_##ArgName5, m_##ArgName6, m_##ArgName7, m_##ArgName8, m_##ArgName9, 
#define HELPER_OVERLAPPED_SESSION_CALL_I_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) ArgName1, ArgName2, ArgName3, ArgName4, ArgName5, ArgName6, ArgName7, ArgName8, ArgName9, 
#define HELPER_OVERLAPPED_SESSION_CALL_M_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) ArgType1 m_##ArgName1;ArgType2 m_##ArgName2;ArgType3 m_##ArgName3;ArgType4 m_##ArgName4;ArgType5 m_##ArgName5;ArgType6 m_##ArgName6;ArgType7 m_##ArgName7;ArgType8 m_##ArgName8;ArgType9 m_##ArgName9;

#define DECLARE_OVERLAPPED_SESSION_CALL_9( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
struct XCallNameFN_T##_OverlappedCall_t : public XSessionCallStack::OverlappedSessionCall { \
	XCallNameFN_T##_OverlappedCall_t( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
		PXOVERLAPPED pxOverlapped ) : \
	XSessionCallStack::OverlappedSessionCall( hSession, pxOverlapped ) \
		HELPER_OVERLAPPED_SESSION_CALL_A_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) {} \
	virtual char const * Name() { return #XCallNameFN_T; } \
	virtual DWORD Run() { \
		DWORD ret = ::XCallNameFN_T( m_hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_P_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
		m_pxOverlapped ? &m_xOverlapped : NULL ); \
		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING ) { Warning( "XCall " #XCallNameFN_T " failed ( ret = %d, overlapped = %p )!\n", ret, m_pxOverlapped ); Assert( 0 ); } \
		return ret; } \
	HELPER_OVERLAPPED_SESSION_CALL_M_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
};

#define IMPLEMENT_OVERLAPPED_SESSION_CALL_9( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
DECLARE_OVERLAPPED_SESSION_CALL_9( XCallNameFN_T, ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
virtual DWORD XCallNameFN_T( HANDLE hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_C_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
		PXOVERLAPPED pxOverlapped ) { \
		return g_XSessionCallStack.ScheduleOverlappedSessionCall( new XCallNameFN_T##_OverlappedCall_t( hSession, \
		HELPER_OVERLAPPED_SESSION_CALL_I_9( ArgType1, ArgName1, ArgType2, ArgName2, ArgType3, ArgName3, ArgType4, ArgName4, ArgType5, ArgName5, ArgType6, ArgName6, ArgType7, ArgName7, ArgType8, ArgName8, ArgType9, ArgName9 ) \
		pxOverlapped ) ); \
};



