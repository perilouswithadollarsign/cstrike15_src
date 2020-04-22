// The dtlibwrapper is an annoying workaround to make Sony's dtlib dll play
// nicely with our exes (such as vxconsole). 
// 
// dtlib is Sony's library for bidirectional communication with the PS3 console.
// (ie, it is the PC side of the DECI3 interface.)
// dtlib.dllis linked against Microsoft’s dynamic C Runtime library.
// VXConsole and all our libs are of course statically linked to the CRT so that 
// we can subvert  malloc(). Ordinarily this wouldn’t really be a problem, except 
// that some functions in dtlib.dll pass and return STL strings by value:
//	HCONNECT dt_connect( std::string hostname, int portNo, std::string programName )
//	std::string dt_geterrormsg( int errorno )
// This causes a crash because it guarantees that memory will be allocated on one 
// side of the DLL boundary and deallocated on the other (because of the pass-by-
// value semantics). 
//
// Therefore we construct this tiny DLL which links against the DLL version of
// msft's runtime, and whose only purpose is to reexport functions which expect
// a std::string parameter into ones that use char *s. By ensuring that 
// std::strings are only constructed or destructed on this side of the DLL boundary,
// we avoid trouble with the free store.
#ifndef DTLIBWRAPPER_H
#define DTLIBWRAPPER_H
#pragma once 


#ifdef DTLIBWRAPPER_DLL_EXPORT
#define DTLIBWRAP_API __declspec(dllexport)
#else
#define DTLIBWRAP_API __declspec(dllimport)
#endif

#include "dtlib.h"
// #include <string>
// #include <vector>

// This is the API for the dtlib; you get a pointer
// to this class instance and then call its functions.
// Call the static GetSingleton() to get the instance
// of this.

class DTLIBWRAP_API Idtwrap
{
public:
	/*
	// local typedefs to shadow (but be equal to) the global ones
	// copied from dtlib.h
	typedef HANDLE HCONNECT;
	typedef HANDLE HDECI;
	typedef HANDLE HSELECT;
	typedef HANDLE HDRIVER;
	typedef HANDLE HPROTO;
	typedef BOOL (CALLBACK DECIRECVPROC)(HDECI, BYTE*, int size, int status);
	typedef BOOL (CALLBACK DCMPRECVPROC)(HCONNECT, BYTE*, int size);
	typedef BOOL (CALLBACK DRIVERRECVPROC)(HDRIVER, BYTE*, int size);
	typedef enum DT_DESTINATION {
		HOST   = 0, // Host Application
		MGR    = 1, // Communication Processor
		TARGET = 2, // Protocol Driver
	};
	*/

	virtual UINT32 dt_set_option(UINT32 value) = 0 ;

	virtual HCONNECT dt_connect(const char * hostname, int portNo,
		const char * programName) = 0;
	virtual void dt_disconnect(HCONNECT connect) = 0 ;

	virtual HDECI dt_register(HCONNECT connectH, int protocol, int port,
		deci3_lib::DT_DESTINATION dst, const char * lparName,
		int priority=128) = 0;
	virtual int dt_unregister(HDECI deciHandle) = 0 ;

	virtual int dt_send(HDECI deciHandle, BYTE* data, int length) = 0 ;
	virtual int dt_receive(HANDLE deciHandle, BYTE* data, int length) = 0 ;

	// error & error messages
	virtual int dt_getlasterror() = 0 ;

	// signature differs from dtlib:
	// you pass in a pointer to a (preallocated) buffer in which to store the error.
	// returns the length of the original string ( if it's greater than buflen, you 
	// didn't get the whole string )
	virtual int dt_geterrormsg( char *pOutBuf, unsigned int bufLen, int error) = 0 ;
	virtual int dt_get_protocol(HANDLE handle, int &protocol, int &port) = 0 ; // deprecated

	// signatures on these change from dtlib:
	// instead of returning a std::vector, these
	// function accept a pointer to a (preallocated
	// by you) array of the underlying structs,
	// and the max length of that array. 
	// It will copy the structs into the array and return
	// the number of structs it wanted to copy. If that
	// number is greater than the size of the buffer you fed
	// in, you didn't get the whole result.
	virtual int dt_get_registered_list( DtRegisteredInfo* pOutArray, unsigned int outLen, HCONNECT connectH) = 0 ;
	virtual int dt_get_protocol_list( DtProtocolInfo *pOutArray, unsigned int outLen, HCONNECT connectH, const char * lparName) = 0 ;

	enum { kLPAR_NAME_LENGTH = 64 };
	// as above, but note that you are passing in an array of
	// arrays: the inner dimension must be the constant kLPAR_NAME_LENGTH
	virtual int dt_get_lparlist( char pOutBufs[][kLPAR_NAME_LENGTH], unsigned int numBufs, HCONNECT connectH) = 0 ;

	virtual int dt_get_version( char *pOutBuf, unsigned int bufLen, HCONNECT connectH) = 0 ;
	virtual int dt_power_status(HCONNECT connH, int &status) = 0 ;

	virtual HANDLE dt_select(HCONNECT connH, int waitTime) = 0 ;
	virtual int dt_set_dcmp_status_function(HCONNECT connectH, DCMPRECVPROC* func) = 0 ;
	virtual int dt_add_recv_function(HDECI deciH, DECIRECVPROC* func) = 0 ;
	virtual int dt_delete_recv_function(HDECI deciH) = 0 ;

	virtual int dt_send_dcmp(HCONNECT connectH, const char * lparName, BYTE *data, int dataSize) = 0 ;
	virtual int dt_set_dcmp_echo_function(HCONNECT connectH, DCMPRECVPROC* func) = 0 ;

	static Idtwrap *GetSingleton();
};



#endif // DTLIBWRAPPER_H