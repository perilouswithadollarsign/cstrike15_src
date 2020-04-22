// Copyright © 2008-, Valve LLC, All rigStahts reserved. 
//
//  cegclient.h
//
//	This file is to be imported by client applications that wish to link with our DRM library !
//	
//	Performance Note : 
//
//	There are several points within cegclient.h where we'll place a 'Performance Note' to indicate
//	the potential performance consequences of particular CEG functions.
//
//	
//
//
//	CEG Initialization - 
//
//	The first and last functions within the CEG SDK that must be called are : 
//		Steamworks_InitCEGLibrary()
//		Steamworks_TermCEGLibrary()
//
//	There are alternate versions of these functions, for people who restrict their usage of the 
//	CEG SDK to a subset of the available API's  (more on the subset later).  These functions are : 
//
//		Steamworks_MinimumInitCEGLibrary()
//		Steamworks_MinimumTermCEGLibrary().
//
//	In the event that the CEG library is initialized through Steamworks_InitCEGLibrary(), then the 
//	following functions must also be called, on each thread on which CEG code may execute, before any
//	other CEG code code executes on the thread (these functions are not necessary if Steamworks_MinimumInitCEGLibrary() is called).
//
//		Steamworks_RegisterThread()
//		Steamworks_UnRegisterThread().
//
//	The only function which can be called before Steamworks_RegisterThread() is Steamworks_InitCEGLibrary().
//	Steamworks_RegisterThread() must be called on the thread that calls Steamworks_InitCEGLibrary(), if it calls any 
//	other CEG functions.
//	For example, your main() will probably look like : 
//
//	int main( int argc, char* argv[] ) 
//	{
//		Steamworks_InitCEGLibrary();
//		Steamworks_RegisterThread();
//
//		/* Real work here */
//
//		Steamworks_UnRegisterThread();
//		Steamworks_TermCEGLibrary();
//		return	0;
//	}
//
//
//
//	CEG Minimum API : 
//
//		Steamworks_SimpleCheck()
//
//	This API is provided for people who are trying to combine CEG with other DRM technology.
//	This function does a very basic check that the machine is executing on the machine it was produced for.
//	This function doesn't conceal any of the data it is using to evaluate the machine, and therefore should not be 
//	used with the full CEG SDK.   It it not necessary to call Steamworks_RegisterThread()/Steamworks_UnRegisterThread()
//	if CEG API usage is restricted to this function.
//
//	CEG Full API's : 
//
//	The full set of CEG API's takes the form of direct function calls, who's performance impacts may be easily analyzed
//	versus macros, which cause sophisticated manipulations of your .obj files before the final executable is linked.
//
//
//	Direct Functions Calls -
//		Steamworks_TestSecret()		
//		Steamworks_TestSecretAlways(), 
//		Steamorks_SelfCheck().
//
//	Steamworks_TestSecret() and Steamworks_TestSecretAlways() are focused on evaluating the current 
//	computer to determine whether it is the machine the binary was produced for.
//	Because a hacker may be 'tracing' all windows API calls with a tool such as sysinternal's Process Monitor 
//	( http://technet.microsoft.com/en-us/sysinternals/bb896645 ), Steamworks_TestSecret() will not always 
//	evaluate it's secret.   The intention is to make it harder to record CEG activity.
//	Steamworks_TestSecretAlways() will always evaluate it's portion of secret data.
//
//	Steamworks_SelfCheck() - this function is focused on determining whether the executable has been modified.
//
//
//	Macro Mechanisms:
//
//	CEG's macro mechanisms attempt to entwine machine specific information into the correct operation of the game 
//	product.  As a result, unlike CEG function calls (which a hacker can simply stub out), a hacker must 
//	take the time to determine how the CEG code is entwined within the product, and make a more significant effort
//	to modify the game.
//
//	Specific Macro Mechanisms: 
//
//	CEG_EncryptFunction()
//
//	The function which contains this macro will be modified, so that the opcodes comprising the function 
//	are encrypted within the executable file.  The decryption key is derived from machine specific data.
//	If decryption of the code fails, the function will not be executed, and the game will to continue to run.
//	Whether the executable immediately terminates or not depends on what the encrypted function does for the game, 
//	as well as calling convention and architecture issues, as the CEG code may not clean up the stack correctly 
//	when the function fails to execute.
//	(Please note : Many CEG functions are themselves encrypted using this mechanism).
//
//	This mechanism can be used an unlimited number of times - the following macros have restrictions on 
//	the number of times they can be used within your executable.
//
//	Performance Note :
//	Every time a function containing CEG_EncryptFunction() macro is invoked, CEG code must execute to 
//	produce the function in a ready to run form in temporarily allocated memory.
//	CEG will keep a cached version of the function for up to 10 seconds, so that the full cost of decryption 
//	is not paid on every invocation.  However, every invocation does incur some CEG overhead to 
//	either decrypt or locate the cached form and to properly martial the call to the decrypted form of the function.
//
//-----------------------------------------
//	CEG_ProtectFunction()
//
//	The function which contains this macro will be modified, so that it's address must be computed
//	by every caller of the function.   The address computation will involve machine specific data, 
//	as well as checksum computations which verify that the executable has not been modified.
//	
//	This mechanism cannot be used (in conjunction with CEG_Define_Constant_Function) more then 130 times.
//	
//	Performance Note:
//
//	The full cost of the CEG_ProtectFunction() will be paid on every call of the target function.
//	Because this mechanism involves computing checksums of the executable image, this will be more 
//	expensive then CEG_EncryptFunction().  However, this mechanism is much more likely to cause problems
//	for somebody who is attacking the executable with a debugger, as the checksums are likely to find
//	the breakpoints being used by the debugger. 
//
//----------------------------------------
//	CEG_Define_Constant_Function( name, value )
//	CEG_Declare_Constant_Function( name )
//	CEG_GetConstantValue( name )
//
//	These macros create and declare a functions that the game can use to get interesting constants 
//	for game play.  (i.e. Field Of View angle, maximum player velocity etc...)
//
//	Computation of CEG_GetConstantValue() involves machine specific information as well as checksums
//	of the executing binary (same as CEG_ProtectFunction).
//	If the machine is different, or the executable has been modified, then the results returned 
//	by CEG_GetConstantValue() will be different.   The results in the game will depend on how
//	the constant is used.
//
//	The same Performance Note as for CEG_ProtectFunction() apply to this mechanism.
//
//	
//	Some General Notes on CEG's macro mechanisms:
//
//	The Compiler can eliminate CEG_ProtectFunction() and CEG_EncryptFunction() if it chooses to 
//	inline your function !!!  In general, it's better to use these macros only in function which appear in 
//	.cpp files (not in inline functions which appear in header files).   It's also better if the function 
//	in which CEG_ProtectFunction() or CEG_EncryptFunction() is placed is not called from within the same file!
//
//	C++ Constructors and Destructors : 
//
//	You can use CEG_ProtectFunction() and CEG_EncryptFunction() within your C++ constructors.
//	HOWEVER - if there is a global variable that invokes the constructor, you will be invoking 
//	the CEG mechanisms before you have executed Steamworks_InitCEGLibrary() - and this will probably crash your game.
//
//
//


#ifndef _CEGCLIENT_H_
#define _CEGCLIENT_H_

#pragma once


//
//	Many of the CEG functions implement below compute CRC's over the executing image 
//	of the game when invoked.  This results in their performance impact varying with
//	the size of the game executable file.
//	A typical result for Steamworks_SelfCheck() and CEG_Protect_Member_Function() is 
//	3ms of overhead in a 6MB executable.  (Measured in L4D2).
//
//	In order to allow more fine grain control of which part of the executable is covered 
//	by CEG CRC's this macro is provided.
//
//	Executable code that is placed in this section WILL NOT have any CRC's covering the code.
//	To place code in this section on an entire source file basis use '#pragma code_seg( NO_CEG_SECTION )'
//	This places all of the code following the pragma into the NO_CEG_SECTION of the executable.
//
//	Care should be taken that the marked code cannot be modified to prevent execution of CEG checks.
//	Do not put direct CEG call sites into this section, or most of the call stack leading to a CEG call site.
//	This can be used to improve the performance of the Steamwork_SelfCheck() and CEG_Protect_Member_Function() mechanisms
//	as they will operate on a smaller portion of the game image.
//	Do not overuse, as placing all code into such sections would mean that the hackers could easily 
//	identify CEG library code - which would be the code not in NO_CEG_SECTION.
//
#define	NO_CEG_SECTION	".textnc"

//
//	This is the result code the DRM library passes to ExitProcess() when it terminates
//	a process that requires DRM processing.
//
#define	STEAM_PROCESS_TERMINATE_DRM_REQUIRED	0x8000DEAD

//
//	This function should be called exactly once by a client application.
//	The Steamworks_InitCEGLibrary() and Steamworks_TermCEGLibrary() will let steam know that a game was
//	running no matter what mechanism the user used to start the game.
//
extern	bool __cdecl Steamworks_InitCEGLibrary() ;

//
//	This function should be called exactly once by a client application - 
//	A client application should call only one of Steamworks_InitCEGLibrary or Steamworks_MinimumInitCEGLibrary() - NOT BOTH!
//
//	This function is provided for those applications incorporating additional DRM mechanisms into their application.
//	In those cases it may be appropriate to use Steamworks_MinimumInitCEGLibrary().
//	Otherwise, Steamworks_InitCEGLibrary() is the interface that should be used.
//
extern	bool __cdecl Steamworks_MinimumInitCEGLibrary();

//
//	This function should be called exactly once by a client application, immediately 
//	before it is going to exit the game.
//
extern	bool __cdecl Steamworks_TermCEGLibrary() ;
//
//	This function pairs with Steamworks_MinimumInitCEGLibrary().
//
extern	bool __cdecl Steamworks_MinimumTermCEGLibrary();

//
//	This pair of functions must be called on each thread that uses any CEG mechanism.
//	Steamworks_RegisterThread() must be called before any CEG mechanism executes on 
//	the thread, including all of the CEG_Protect_Member* macros.
//	Steamworks_UnRegisterThread() must be called AFTER all CEG mechanisms have been 
//	executed on a particular thread.
//
//	IMPORTANT NOTE REGARDING CEG IN MULTIPLE COMPONENTS - 
//	This Comment ONLY applies if you are using CEG in multiple DLL's (or a .DLL and .EXE)
//	which are loaded in the same process.
//	If Steamworks_RegisterThread() is called in DLL A, which invokes DLL B which also 
//	contains CEG code, then DLL B must call Steamworks_RegisterThread() before any CEG 
//	function within DLL B is executed, and must call Steamworks_UnRegisterThread() before 
//	it returns to DLL A.
//
//	If a thread executes exclusively in DLL A without calling DLL B, then Steamworks_RegisterThread() 
//	only needs to be called in DLL A.
//
extern	bool __cdecl Steamworks_RegisterThread() ;
extern	bool __cdecl Steamworks_UnRegisterThread() ; 

//
//	This function performs the most basic CEG checks for ownership, and makes no attempt
//	to conceal it's inner workings, unlike the remaining API's
//
//	If  bTerminateIfUnowned is non zero then the CEG will crash the process 
//	and attempt to return STEAM_PROCESS_TERMINATE_DRM_REQUIRED to the steam client.
//	The caller may pass zero (false) to the function, and take responsibility for terminating the game.
//	This allows the caller to separate the legitimacy test from the enforcement.
//	It is very important that the process have an exit code of STEAM_PROCESS_TERMINATE_DRM_REQUIRED
//	if this is done - the Steam Client will use that to report CEG errors to Steam
//	and to repair a customer installation.  This may arise if the customer 
//	has upgraded their Operating System, in which case the first time the game 
//	is launched after the upgrade, CEG may fail to identify the system and cause the game to exit.
//	In this situation, the steam client will repair the customer's installation before 
//	the game is launched again.
//
//	The variable argument list to the function is ignored by the steam client, 
//	but may be used to mask the signature of the function.
//
extern bool	__cdecl	Steamworks_SimpleCheck( DWORD bTerminateIfUnowned, ... );
//
//	This function checks to see whether an application has been 'stolen' - it does 
//	so by examining properties of the system on which we are running and checking if 
//	those properties match those expected through the encoding we've done into the binary !
//
//	IMPORTANT NOTE : This function has internal metering, it does not always perform its 
//	full check.   The goal is to make it more difficult to discover for hackers to locate 
//	all of these checks.
//
extern bool	__cdecl Steamworks_TestSecret() ; 

//
//	This function performs the same type of checks as Steamworks_TestSecret(), however 
//	it has no internal metering - it will always perform a check.
//
extern bool	__cdecl	Steamworks_TestSecretAlways() ; 

//
//	This function checks to see whether an application has been 'stolen' - it does
//	so by examining the PE file (.dll or .exe) in which the DRM library has been linked
//	and determines whether the PE file has been modified.
//
extern bool __cdecl Steamworks_SelfCheck() ;

//
//	This function takes an interface pointer returned from a steam dll, and verifies 
//	that it actually references a legitimate steam client.
//
extern bool	__cdecl	Steamworks_ValidateComponentInterface(void** lpv) ; 

//
//	By default - we use WinCrypt API's to do signature validation - 
//	if somebody desires, they can provide their own implementation.
//
class	ISignatureImplementation {
public : 
	virtual	bool 
	VerifyHash(	
				//
				//	First - we specify the data structure which has been 'signed' - 
				//	We sign our signature using Wincrypt's PROV_RSA_FULL provider 
				//	with the CALG_SHA1 algorithm.
				//	
				LPBYTE	lpbSignedData, 
				DWORD	cbSignedData, 
				//
				//	The signature block is generated using 
				//	CryptCreateHash( RSA FULL PROVIDER, CALG_SHA1 ) and CryptHashData().
				//	The Hash is then signed using CryptSignHash( Hash(), AT_SIGNATURE, .... ) 
				//	generating the specified SignatureBlock !
				//
				LPBYTE	lpbSignatureBlock, 
				DWORD	cbSignatureBlock, 
				//
				//	The public key associated with the private key that signed the Hash data !
				//
				LPBYTE	lpbPublicKey, 
				DWORD	cbPublicKey
				) = 0 ; 
} ; 

//
//	This function checks a signature of all the read-only portions of the executing image.
//	Unlike other CEG mechanisms, this uses cryptographic mechanisms that are not easily concealed from reverse engineering.
//	Additionally, this is a higher cost mechanism as it involves the entire executing image.
//	This check can be used at game launch, (before any user interface is displayed) where it will serve as a stalking horse, 
//	and occasionally throughout execution.
//
//	Additionally, neither of Steamworks_ValidateComponentInterface() or Steamworks_SelfValidate() provide multiple implementations.
//	Steamworks_TestSecret(), Steamworks_TestSecretAlways(), Steamworks_SelfCheck() will all be multi-plexed across many distinct
//	implementations (this is done by mingle.exe).   Additional call sites for Steamworks_SelfValidate() are not as valuable as 
//	additional call sites for the previously mentioned CEG functions.
//
extern bool __cdecl Steamworks_SelfValidate(ISignatureImplementation*	pISignatureImplementation) ; 


////////////////////////////////////////////////////
//
//	DO NOT CALL THIS FUNCTION !
//	This is referenced only through the CEG_Define_Constant_Function 
//
typedef	DWORD	(*PFNSteamworks_ComputeDwordValue)();
extern DWORD __cdecl Steamworks_ComputeDwordValue() ; 
//
//	DO NOT DO NOT CALL THIS FUNCTION !
//
//	This is only referenced indirectly through the CEG_Protect_Virtual_Function macro !
//
extern void*	__cdecl Steamworks_ProtectFunction( void*, void* ) ; 
//
//	DO NOT CALL THIS FUNCTION !
//
//	This declaration is used within CEG Concealed function mechanisms to incorporate 
//	machine specific data into the function concealment mechanism.   It's not to be 
//	directly called.
//
extern	void __cdecl Steamworks_GetSecretData( unsigned __int32 /* 32bit unsigned int - all platforms.*/, union _16BYTES_U& );
//
//	DO NOT DO NOT CALL THIS FUNCTION !
//
//	This function should only be referenced through the CEG_EncryptFunction macro !
//
extern bool	__cdecl	Steamworks_EncryptedFunctionLoad();
//
//	DO NOT DO NOT CALL THIS FUNCTION !
//
//	This symbol exists for the benefit of mingle.exe when it processes your .obj files
//	and should only appear in the initializers of a CEGProtectedFunction structure !
//
extern	void __cdecl Steamworks_InvalidReference() ; 
//
//	DO NOT CALL THIS FUNCTION - Exists solely for use in macros defined below !
//
//	This function exists so we can force optimizing compilers to emit all the CEG 
//	relevant data, as otherwise the compiler would discard them as unreferenced.
//
extern	void __cdecl CEG_ForceReference(const struct CEG_Protection_Thunk&, const char* ) ; 
//
//	This class is defined purely so we can create derived classes in the CEG_Protect_Member_Function() macro.
//	Because that macro must be used within a function, defining a class allows us to generate the necessary 
//	compiler references to ensure that the compiler does not optimize out CEG functions and symbols.
//
class	I_CEG_ForceReference_Interface 
{
	virtual	const CEG_Protection_Thunk*	ReferencingFunction(unsigned index) = 0 ; 
} ; 
////////////////////////////////////////////////////////////////////////////////
//	DO NOT CALL THIS FUNCTION - Exists solely for use in macros defined below !
//
//	Together with the I_CEG_ForceReference_Interface allows us to force compilation 
//	of some of our fancy CEG symbols !
//
extern	int __cdecl CEG_ForceReference( class I_CEG_ForceReference_Interface* p ) ; 
//
//
///////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
//
//	CEG Internal Macros - 
//
//	The following are not intended for direct use, but will be instantiated
//	by CEG_EncryptFunction()/CEG_ProtectFunction()/CEG_GetConstantValue().
//
//
//	These handy macros help to generate unique variable names, as well as messages
//	through __pragma( message() ) !
//
#define CEG_UNIQNAME2(x,y) x##y
#define CEG_UNIQNAME1(x,y) CEG_UNIQNAME2(x,y)
#define CEG_UNIQNAME(x) CEG_UNIQNAME1(x,__COUNTER__)
#define	CEG_STRING(z)	#z
#define CEG_LINE_NUMBER(x) CEG_STRING(x)
#if	defined( _M_X64 )
#define	CEG_UNIQNAME3(x, y) CEG_STRING( x##y ) 
#else
#define	CEG_UNIQNAME3(x, y) CEG_STRING( _##x##y )
#endif

//
//	Too bad there's no hexadecimal 'G', F+1 will do
//
#define	CEG_PROTECT_SIGNATURE	0xCEF1CEF1
#define CEG_PROTECT_VIRTUAL_SIGNATURE	0xCEF10000

//
//	Define various strings used by CEG as section names through __pragma() directives.
//
#if	!defined(CEG_PUBLIC_SECTION_STRINGS)
#define	CEG_PUBLIC_SECTION_STRINGS
//
//	These macros correspond to section names used by CEG.
//
//	The following macros will use C++ pragma's to allocate objects into these 
//	various sections.   No direct reference to these macros should be necessary.
//
#define	CEG_META_GRAPH_START	".cegm$ga"
#define	CEG_META_GRAPH_END		".cegm$gz"
//
//	This section will contain the signature tested by the Steamworks_ValidateSelf() interface
//
#define	CEG_SIGNATURE_SECTION	".crsig"

//
//	This section will contain the data CEG uses to generate the Constant Function mechanisms !
//
#define	CEG_EXTERNAL_VALUE		".cegi$d"
#endif

extern	DWORD	__cdecl	Steamworks_RevealFunction( DWORD_PTR, DWORD_PTR );
extern	void	__cdecl	Steamworks_ForceRef( struct	CEG_SwapStruct* );

//
//	We use this macro to generate identifiers that Mingle may modify.
//	In some .obj manipulations, Mingle will change identifiers in the .obj file - 
//	And will expect the identifiers to be produced by this macro!
//
//	NOTE that the __XXXXXXXX will be turned into digits (essentially a counter)
//	by mingle !
//
#define	Steamworks_CEG_MingleModifiableName_Prefix	Steamworks_CEGIdentifier_
#define	Steamworks_CEG_MingleModifiableName_Suffix	__XXXXXXXX
#define	Steamworks_CEG_MingleModifiableName(name)	Steamworks_CEGIdentifier_	 ## name ## __XXXXXXXX

#define	CEG_TEMPLATE_USE_START_SECTION	".cegtu$a"
#define	CEG_TEMPLATE_USE_SECTION		".cegtu$m"
#define	CEG_TEMPLATE_USE_END_SECTION	".cegtu$z"

//
//	Reserve scratch space for the CEG Implementation to cache results!
//	
//	
struct	Steamworks_ThunkFunction_Scratch	
{
	DWORD	m_rgdw[16] ;
};


//
//	Identify the version number of the CEG_SwapStruct structure.
//
#define	Steamworks_CEG_SwapVersion			1
//
//	This mask specifies that the function specified by CEG_SwapStruct should be prepped by mingle
//	to have runtime encryption operations applied to it.
//
#define	Steamworks_CEG_EncryptSectionMask	1
//
//	This mask specifies that the function specified by CEG_SwapStruct should be marked as COMDAT Select
//	Associative to the specification section.   The select associative option is only used if 
//	the original section was already COMDAT Select Any (common for templates and inline functions).
//	This flag is used by code within the CEG library implementing various secrets - where we want to 
//	ensure that the target executable file links the absolute minimum amount of stuff, and does not 
//	pull in any un-necessary objects.
//
#define	Steamworks_CEG_SelectAssocMask		2


//
//	This is the macro we use to cause the 'consumption' of a template !
//
#define	Steamworks_CEG_InvokeTemplate(  arg, flags )	\
	static	char	CEG_SwapName_Local[] = __FUNCDNAME__ ;	\
	__pragma( section(CEG_TEMPLATE_USE_SECTION, read, write) )	\
	static	CEG_SwapStruct	__declspec(align(1)) __declspec(allocate(CEG_TEMPLATE_USE_SECTION))	\
	LocalSwap = {	\
		Steamworks_CEG_SwapVersion,		\
		CEG_SwapName_Local,		\
		Steamworks_CEG_MingleModifiableName( arg ), 	\
		flags, NULL, 0, 0 \
	}	;	\
	Steamworks_ForceRef(&LocalSwap)	
	
#define	Steamworks_DeclareCEGTemplate( arg )	\
	extern	DWORD_PTR	__cdecl	Steamworks_CEG_MingleModifiableName( arg )( DWORD_PTR )

Steamworks_DeclareCEGTemplate( CacheConcealThunk );
Steamworks_DeclareCEGTemplate( ProtectFunction );

#pragma	pack(push, 1)

#if	!defined(CEG_LINKOBJECT_DEFINED)
#define	CEG_LINKOBJECT_DEFINED
//
//	The 'ID' of an object identified by mingle - this 
//	is large enough to be a GUID or an MD5 computation !
//
struct	LinkObjectId 
{
	BYTE		m_rgb[16];
};
#endif


struct	CEG_SwapStruct	{
	//
	//	Tell Mingle what version of this structure the file was compiled with !
	//
	DWORD_PTR	m_Version;
	//
	//	This field is the decorated name of the function that is being manipulated.
	//	This is generated through the __FUNCDNAME__ preprocessor directive of Visual Studio.
	//
	char*		m_szDecoratedName;
	//
	//	This points to the function that will 'replace' the specified function.
	//
	void*		m_pfnOriginal;
	//
	//	Reserve space in the structure for communication with mingle.exe and 
	//	cegdll.dll regarding the details of what happens to the swapped code !
	//
	DWORD_PTR	m_SectionOptions;
	//
	//	Based on the Section Options, these fields will be interpreted by Mingle and the CEG runtime.
	//
	void*		m_Reserved1;
	DWORD_PTR	m_Reserved2;
	//
	//	Mingle will give us the size of the Swap Target function, in bytes !
	//
	DWORD_PTR	m_cbTarget;
	//
	//	This is a mingle generated identifier, that will be used by cegdll.dll and drmpe.exe 
	//	to identify the template used and it's associated metadata !
	//
	LinkObjectId	m_LinkObjectId;
};

//
//	This structure must have the exact same size as CEG_SwapStruct, and will be the runtime interpretation 
//	of the structure !   Mingle will modify relocation records so that the final linked form stores 'distances'
//	to the actual referenced items instead of pointers.   We do this to reduce relocation records in the final
//	target executable, which we believe give data structure feedback to hackers.
//
struct	CEG_Runtime_SwapStruct	{
	//
	//	This field carries the version of the structure.
	//
	DWORD_PTR	m_Version;
	//
	//	This is the 'distance' to the function that Mingle placed into the .obj file.
	//	This function is called directly instead of the original 'TargetFunction'
	//
	//	Note that 'distances' are stored by mingle.exe using a relocation type that is appropriate for code.
	//	That means that distances on both 32 and 64bit platforms are 32 bit signed integers - 
	//	However, this field is 64bits on a 64bit platform.   The linker will have stored a 32 bit signed
	//	integer into the field, so we must do appropriate casting, because we don't have a genuine 64 bit signed integer in here.
	//	When manipulating the structures, we will at a time turn this field into a full 'pointer like' value
	//	(i.e. during the CEG process) - so it is convenient for us that it can hold that.
	//
	INT_PTR		m_OffsetSubstitute;
	//
	//	NOTE : All the same comments that apply to m_OffsetSubstitute apply to this field !
	//
	//	This is the distance to the 'Target' function - the function containing the Steamworks_CEG_InvokeTemplate macro
	//	This TargetFunction may be in an encrypted form, depending on the flags specified in m_SectionOptions!
	//
	INT_PTR		m_OffsetOriginal;
	//
	//	These options specify manipulations that mingle will perform on the 'Original' function
	//	before it is linked.  In particular, it may be turned into a 'Concealed' function - which 
	//	is a self-contained data structure containing the functions op-codes and metadata that can 
	//	be encrypted during the CEG process.
	//
	DWORD_PTR	m_SectionOptions;
	//
	//	We allow the macro to request what Encryption thunk is used in the event that the 
	//	Section Options specify 
	//
	DWORD_PTR	m_SectionThunk;
	//
	//	For future use ....
	//
	DWORD_PTR	m_Reserved2;
	//
	//	Mingle will give us the size of the Substitute function, in bytes !
	//	It is usefull to know the Substitute function size - as during runtime we may 
	//	need to 'search' for it.
	//
	DWORD_PTR	m_cbTarget;
	//
	//	This is a mingle generated identifier, that will be used by cegdll.dll and drmpe.exe 
	//	to identify the template used and it's associated metadata !
	//
	LinkObjectId	m_LinkObjectId;

	void*	GetOriginalFunction()	const
	{
		return	reinterpret_cast<void*>(reinterpret_cast<INT_PTR>(&m_OffsetOriginal)+static_cast<INT32>(m_OffsetOriginal)+sizeof(INT32));
	}
	void*	GetSubstituteFunction()	const
	{
		return	reinterpret_cast<void*>(reinterpret_cast<INT_PTR>(&m_OffsetSubstitute)+static_cast<INT32>(m_OffsetSubstitute)+sizeof(INT32));
	}
};
#pragma pack(pop)
//
//	End of CEG Internal Macros - 
//
////////////////////////////////////////////////////////////////////////////////

	
//
//	The flowing three macros :
//		CEG_Define_Constant_Function( name, value ), 
//		CEG_Declare_Constant_Function( name ), 
//		CEG_GetConstantValue( name ) 
//
//	Operate together to provide another CEG mechanism for protecting your binary.
//
//	Example usage : 
//	
//	header.h : 
//  /* macro is at File Scope !*/
//	CEG_Declare_Constant_Function( ZombiesPerSecond ) ; 
//
//	main.cpp : 
//	/* macro is at File Scope !*/
//	CEG_Define_Constant_Function( ZombiesPerSecond, 17 ) ; 
//
//	zombies.cpp : 
//	/* CEG_GetConstantValue( ZombiesPerSecond ) must be used within a  function !*/
//
//	void
//	ClockTick() 
//	{
//		if( ElapsedTime > 1 ) /* Has 1 second passed ? */
//		{
//			DWORD	cZombies = CEG_GetConstantValue( ZombiesPerSecond ) ; 
//			for( DWORD i=0; i<cZombies; ++i ) 
//			{
//				/* Create a Zombie !*/
//			}
//		}
//	}
//
//	Explanation : 
//
//	CEG_Define_Constant_Fucntion( ZombiesPerSecond, 17 ) defines a function 
//	that will return 17 (as a DWORD) every time it is called, IF your executable
//	has not been tampered with, or copied.   This function must NOT be inlined, 
//	when your game is CEG'd, the code bytes of the function will be modified.
//	Putting the Definition in the same file as the usage may allow aggressive compiler 
//	settings to inline some usages of the function - we recommend that the definition 
//	never appear with the usage.
//
//	CEG_Declare_Constant_Function( ZombiesPerSecond ) generates an extern declaration
//	of this function.
//
//	CEG_GetConstantValue( ZombiesPerSecond ) calls the function defined by 
//	CEG_Define_Constant_Function() and declared by CEG_Declare_Constant_Function().
//
//	How it Works : 
//	
//	This mechanism is very similar to that provided by CEG_Protect_Member_Function()
//	The CEG code will compute checksums over your executing image, and these checksums 
//	will come to the result specified in the CEG_Declare_Constant_Function() if the 
//	image has not been tampered with.   If a hacker has tampered, this will return a 
//	random number.    By choosing appropriate constants, your game may be rendered 
//	unplayable, although CEG will not crash the game.
//
//	Recommendation : 
//
//	This CEG functionality degrades the game when the game is tampered with.
//	It should be used in conjunction with all the other CEG mechanisms.
//	The other CEG mechanisms will immediately crash the game when tampered, 
//	which makes these mechanisms easy to identify in support calls, online forums etc...
//
//	Make sure you include a small number of Steamworks_TestSecretAlways() call 
//	during the start/load of the game.  These will catch Casual Piracy.
//	The game degradation caused through this the CEG_Define_Constant_Function() is 
//	directed at sophisticated pirates, who will have a hard time tracking 
//	down the results of their modifications of the game image.
//
//	IMPORTANT NOTE ABOUT THE VALUE ZERO !
//
//	Don't use :
//	CEG_Define_Constant_Function( Zero, 0 ) 
//	or : 
//	CEG_Define_ConstantFloat_Function( FloatZero, 0.0f ) 
//
//	The .obj and .exe parsing mechanisms assume 0 (also known as NULL) is an unbound 
//	usage - and the resulting calls to CEG_GetConstantValue() will not return 0 (zero)
//	(The string 'zero' is fine, of course).
//
//

#define	CEG_GetConstantValue( name )	\
	CEG_UNIQNAME1( CEG_ConstantValue_, name )()

#define	CEG_Declare_ConstantFloat_Function( name ) \
	extern	float	__cdecl	CEG_UNIQNAME1( CEG_ConstantValue_, name )() 

#define	Steamworks_CEG_ValueBind_Signature	0xCEF1DDEE

#define	CEG_Define_Constant_Function( name, value )		\
	extern	"C"	{	\
		__pragma( section(CEG_EXTERNAL_VALUE, read, write) )	\
		Steamworks_CEG_ValueBinding	__declspec(align(8)) __declspec(allocate(CEG_EXTERNAL_VALUE))	\
		CEG_UNIQNAME1( Steamworks_CEG_ValueBinding_, name ) = {	\
			Steamworks_CEG_ValueBind_Signature,		\
			0,	\
			value,		\
			reinterpret_cast<DWORD_PTR>( Steamworks_ComputeDwordValue ) , 	\
		}	;	\
		__pragma( comment( linker, "/INCLUDE:" CEG_UNIQNAME3( Steamworks_CEG_ValueBinding_, name ) ) )	\
	};	\
	extern	DWORD	CEG_UNIQNAME1( CEG_ConstantValue_, name )( void );	\
	DWORD	CEG_UNIQNAME1( CEG_ConstantValue_, name )( void )	\
	{	\
		INT32*	pOffset = reinterpret_cast<INT32*>( & CEG_UNIQNAME1( Steamworks_CEG_ValueBinding_, name ).m_ImplementationFunction ) ;	\
		PFNSteamworks_ComputeDwordValue	pfn = reinterpret_cast<PFNSteamworks_ComputeDwordValue>( reinterpret_cast<BYTE*>(pOffset+1) + *pOffset );	\
		return	(*pfn)();	\
	}

#define	CEG_Declare_Constant_Function( name )	\
	extern	DWORD	CEG_UNIQNAME1( CEG_ConstantValue_, name )()	

//
//	These forms of CEG_Define_Constant_Function() appeared in older CEG SDK's, but are no longer available !
//
//
#define	CEG_Define_Constant_Function2( name, value )	\
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Defined_Constant_Function2 has been retired and can no longer be used! - use CEG_Define_Constant_Function!") ) 

#define	CEG_Define_ConstantFloat_Function( name, value )		\
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Defined_ConstantFloat_Function has been retired and can no longer be used! - use CEG_Define_Constant_Function!") ) 

#define	CEG_Define_ConstantFloat_Function2( name, value )	\
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Defined_ConstantFloat_Function2 has been retired and can no longer be used! - use CEG_Define_Constant_Function!") ) 



/***************************************************
****************************************************

	CEG_ProtectFunction and CEG_EncryptFunction
	
	These macros must be placed within the body of a function.
	Here is a small example, which describes how CEG_ProtectFunction() operates :


	class CMyClass {
					virtual   void MyVirtualFunction( int SomeArg ) ; 
	} ; 

	__declspec(noinline)	// This is necessary to prevent compiler optimizations from eliminating CEG check.
	void
	CMyClass::MyVirtualFunction( int SomeArg ) {
		CEG_ProtectFunction();

		//
		//            Now do the real work !
		//
	}

	Void ExampleCall(CMyClass * p )
	{
					p->MyVirtualFunction( EXAMPLE_CONSTANT ) ; }
	}


	Now what happens to your binary is the following : 

	The vtable for CMyClass has been changed to reference a CEG function.  When ExampleCall() is executed, 
	It will actually end up calling CEG code through the vtable.   This CEG code preserves the state of the stack 
	and arguments in a fashion similar to the CRT setjmp() function.
	
	The CEG code will then compute a checksum that incorporates machine identifying information into the checksum.

	If nobody has modified the executable (by setting breakpoints, or by changing the executable file) and the executable
	is running on the machine it was produced for, then the checksum results in the address of CMyClass::MyVirtualFunction().

	In this case, CEG will patch the CONTEXT it got from it's setjmp() operation and longjmp() to CMyClass::MyVirtualFunction().
	The setjmp()/longjmp() manipulations allow CEG to do it's work, without having to understand the calling convention 
	between ExampleCall() and CMyClass::MyVirtualFunction().

	Before the CEG code invokes longjmp(), it checks that the address it computed is correct, by computing an MD5 of the 
	address, and comparing this to a stored MD5.  If this comparison fail, CEG terminates the process directly.

	You can use CEG_ProtectFunction()/CEG_EncryptFunction() in any kind of function - 
		C++ member functions, C++ static member functions etc....


*******************************************************
******************************************************/



#define	CEG_ProtectFunction( )	\
	Steamworks_CEG_InvokeTemplate( ProtectFunction, 0 )


#define	CEG_EncryptFunction( )	\
	Steamworks_CEG_InvokeTemplate( CacheConcealThunk, Steamworks_CEG_EncryptSectionMask )


//
//	These are previus forms of CEG_ProtectFunction() that appeared in older SDK's - and should be replaced by either CEG_ProtectFunction()
//	or CEG_EncryptFunction().
//
#define	CEG_Protect_Function(x)	\
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Protect_Function has been retired and can no longer be used! - use CEG_ProtectFunction!") ) 

#define     CEG_Protect_StaticMemberFunction(x, y)    \
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Protect_StaticMemberFunction has been retired and can no longer be used! - use CEG_ProtectFunction!") ) 

#define	CEG_Protect_Virtual_Function3( name ) \
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Protect_Virtual_Function3 has been retired and can no longer be used! - use CEG_ProtectFunction!") ) 

#define	CEG_Protect_Member_Function( name ) \
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Protect_Member_Function has been retired and can no longer be used! - use CEG_ProtectFunction!") ) 

#define	CEG_Protect_Member_Function2( name ) 	\
	__pragma( message( __FILE__ "(" CEG_LINE_NUMBER(__LINE__) ") : CEG_Protect_Member_Function2 has been retired and can no longer be used! - use CEG_ProtectFunction!") ) 


//
//	This structure is declared to be a platform invariant size !
//
struct	Steamworks_CEG_ValueBinding
{
	DWORD32		m_Signature;
	DWORD32		m_BindingUse;
	DWORD64		m_DesiredResult;
	DWORD64		m_ImplementationFunction;
};



//////////////////////////////////////////////////////////////
//
//	Reference Macros !
//	
//	These macro's instantiate several CEG data structures as the CEG tools 
//	(mingle.exe and drmpe.exe) will see them in .obj and .exe files.
//
//	

#define	CEG_PROTECT_START_SIGNATURE	0xCCEEFF11
#define	CEG_PROTECT_END_SIGNATURE	0x11ffeeCC 

//
//	DO NOT REFERENCE 
//
//	We specify these symbols here so that the linker will properly 
//	concatenate the CEG sections together in the final image.
//
//	These symbols are only for use by internal code of the CEG library !
//	
//

//
//////////////////////////////////////////////////////////////



#endif	// _CEGCLIENT_H_