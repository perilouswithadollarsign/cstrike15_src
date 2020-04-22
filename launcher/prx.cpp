#include "platform.h"
#include "basetypes.h"
#include "dbg.h"
#include "logging.h"
#include <cellstatus.h>
#include <sys/prx.h>
#include "ps3/ps3_helpers.h"


#ifdef _DEBUG
#define launcher_ps3 launcher_dbg
#else
#define launcher_ps3 launcher_rel
#endif


PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER( launcher );
SYS_MODULE_START( _launcher_ps3_prx_entry );



int LauncherMain( int argc, char **argv );

void Launcher_ShutdownCallback()
{
	LoggingSystem_ResetCurrentLoggingState();	// tear down all logging listeners
}

// An exported function is needed to generate the project's PRX stub export library
extern "C" int _launcher_ps3_prx_entry( unsigned int args, void *pArg )
{
	Assert( args >= sizeof( PS3_LoadLauncher_Parameters_t ) );
	
	PS3_LoadLauncher_Parameters_t *pParams = reinterpret_cast< PS3_LoadLauncher_Parameters_t * >( pArg );
	Assert( pParams->cbSize >= sizeof( PS3_LoadLauncher_Parameters_t ) );
	
	pParams->pfnLauncherMain = LauncherMain;

	// Return tier0 shutdown callback
	pParams->pfnLauncherShutdown = Launcher_ShutdownCallback;

	return SYS_PRX_RESIDENT;
}

