// This contains stubs that emulate the HL2 engine for places where
// the response rules expect to find it.

// temporarily make unicode go away as we deal with Valve types
#ifdef _UNICODE
#define PUT_UNICODE_BACK 
#undef _UNICODE
#endif

#include "platform.h"
#include "wchartypes.h"
#include <ctype.h>
struct datamap_t;
template <typename T> datamap_t *DataMapInit(T *);

#include "appframework/appframework.h"
#include "filesystem.h"
#include "vstdlib/random.h"
#include "icommandline.h"

#include "responserules/response_types.h"
#include "../../responserules/runtime/response_types_internal.h"
#include "response_system.h"

#include "cli_appsystem_unmanaged_wrapper.h"
#include "cli_appsystem_adapter.h"

#include "characterset.h"

#ifdef PUT_UNICODE_BACK
#define _UNICODE 
#undef PUT_UNICODE_BACK
#endif


class CLI_SourceEngineEmulator;

const char *COM_Parse (const char *data);
byte *UTIL_LoadFileForMe( const char *filename, int *pLength, IFileSystem *filesystem );

int TestRandomNumberGeneration( int bottom, int top ) 
{
	return ResponseRules::IEngineEmulator::Get()->GetRandomStream()->RandomInt(bottom,top);
}

const char *TestFileSystemHook( ) 
{
	// return ResponseRules::IEngineEmulator::Get()->GetFilesystem() ? "present" : "absent" ;
	return ResponseRules::IEngineEmulator::Get()->GetFilesystem()->IsSteam() ? "steam" : "not steam";
}


class CLI_SourceEngineEmulator : public ResponseRules::IEngineEmulator
{
public:
	/// Given an input text buffer data pointer, parses a single token into the variable token and returns the new
	///  reading position
	virtual const char			*ParseFile( const char *data, char *token, int maxlen );

	/// Return a pointer to an IFileSystem we can use to read and process scripts.
	virtual IFileSystem *GetFilesystem();

	/// Return a pointer to an instance of an IUniformRandomStream
	virtual IUniformRandomStream *GetRandomStream() ;

	/// Return a pointer to a tier0 ICommandLine
	virtual ICommandLine *GetCommandLine();

	/// Emulates the server's UTIL_LoadFileForMe
	virtual byte *LoadFileForMe( const char *filename, int *pLength );

	/// Emulates the server's UTIL_FreeFile
	virtual void  FreeFile( byte *buffer );

	CLI_SourceEngineEmulator();
	virtual ~CLI_SourceEngineEmulator();

	void LocalInit();
// protected:
};

CLI_SourceEngineEmulator g_EngineEmulator;


CLI_SourceEngineEmulator::CLI_SourceEngineEmulator()
{
	LocalInit();
}

CLI_SourceEngineEmulator::~CLI_SourceEngineEmulator()
{
}


ResponseRules::IEngineEmulator *ResponseRules::IEngineEmulator::s_pSingleton = &g_EngineEmulator;

/// Return a pointer to an IFileSystem we can use to read and process scripts.
 IFileSystem *CLI_SourceEngineEmulator::GetFilesystem()
{
	return AppSystemWrapper_Unmanaged::Get()->GetFilesytem();
}

/// Return a pointer to an instance of an IUniformRandomStream
 IUniformRandomStream *CLI_SourceEngineEmulator::GetRandomStream()
{
	return AppSystemWrapper_Unmanaged::Get()->GetRandomStream();
}

/// Return a pointer to a tier0 ICommandLine
 ICommandLine *CLI_SourceEngineEmulator::GetCommandLine()
{
	return AppSystemWrapper_Unmanaged::Get()->GetCommandLine();
}


/// Emulates the server's UTIL_LoadFileForMe
 byte *CLI_SourceEngineEmulator::LoadFileForMe( const char *filename, int *pLength )
{
	return UTIL_LoadFileForMe( filename, pLength, GetFilesystem() );
}

/// Emulates the server's UTIL_FreeFile
 void  CLI_SourceEngineEmulator::FreeFile( byte *buffer )
{
	GetFilesystem()->FreeOptimalReadBuffer( buffer );
}

/*
===================================
  STUFF COPIED FROM SOURCE ENGINE
===================================
*/

 
// wordbreak parsing set
static characterset_t	g_BreakSet, g_BreakSetIncludingColons;
bool com_ignorecolons = false; 
#define COM_TOKEN_MAX_LENGTH 1024
char	com_token[COM_TOKEN_MAX_LENGTH] = {0} ;



/// Given an input text buffer data pointer, parses a single token into the variable token and returns the new
///  reading position
 const char	*CLI_SourceEngineEmulator::ParseFile( const char *data, char *token, int maxlen )
{
	Assert( data );

	const char *return_data = COM_Parse(data);
	Q_strncpy(token, com_token, maxlen);
	return return_data;	
}


/*
==============
COM_Parse (from Quake engine)

Parse a token out of a string
==============
*/
static 
const char *COM_Parse (const char *data)
{
	unsigned char    c;
	int             len;
	characterset_t	*breaks;
	
	breaks = &g_BreakSetIncludingColons;
	if ( com_ignorecolons )
		breaks = &g_BreakSet;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if ( IN_CHARACTERSET( *breaks, c ) )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if ( IN_CHARACTERSET( *breaks, c ) )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*pLength - 
// Output : byte
//-----------------------------------------------------------------------------
static byte *UTIL_LoadFileForMe( const char *filename, int *pLength, IFileSystem *filesystem )
{
	void *buffer = NULL;

	int length = filesystem->ReadFileEx( filename, "GAME", &buffer, true, true );

	if ( pLength )
	{
		*pLength = length;
	}

	return (byte *)buffer;
}


void CLI_SourceEngineEmulator::LocalInit()
{
	CharacterSetBuild( &g_BreakSet, "{}()'" );
	CharacterSetBuild( &g_BreakSetIncludingColons, "{}()':" );
}

