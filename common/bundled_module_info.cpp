//=========== Copyright Valve Corporation, All rights reserved. =============//
//
// Purpose: Common location for hard-coded knowledge about module
// bundles, such as tier2_bundle and tier3_bundle.
//
//===========================================================================//

#include "bundled_module_info.h"

#include "tier0/icommandline.h"
#include "tier1/strtools.h"

static const char * const s_pTier2BundleModules[] =
{
    "filesystem_stdio",
    "imemanager",
    "inputsystem",
    "localize",
    "materialsystem2",
    "networksystem",
    "resourcesystem",
    "schemasystem",
    "soundsystem",
};

static const char * const s_pTier3BundleModules[] =
{
    "animationsystem",
    "meshsystem",
    "particles",
    "renderingpipelines",
    "scenesystem",
    "worldrenderer",
};

static bool FindBundledModuleName( const char *pCheck, const char * const *pTable, int nTable )
{
    for ( int i = 0; i < nTable; i++ )
    {
        if ( V_stricmp_fast( pCheck, pTable[i] ) == 0 )
        {
            return true;
        }
    }

    return false;
}

const char *RemapBundledModuleName( const char *pModuleName )
{
    static bool s_bCheckedCmd;
    // Default to using bundles.
    static bool s_bNoTier2Bundle = true;
    static bool s_bNoTier3Bundle = true;
    static const char *s_pNoBundleModule;

    if ( !s_bCheckedCmd )
    {
        if ( Plat_GetEnv( "SOURCE2_USE_BUNDLES" ) != NULL )
        {
            s_bNoTier2Bundle = false;
            s_bNoTier3Bundle = false;
        }

        if ( Plat_GetEnv( "SOURCE2_NO_BUNDLES" ) != NULL )
        {
            s_bNoTier2Bundle = true;
            s_bNoTier3Bundle = true;
        }
        
        if ( CommandLine()->HasParm( "-use_tier2_bundle" ) )
        {
            s_bNoTier2Bundle = false;
        }
        if ( CommandLine()->HasParm( "-use_tier3_bundle" ) )
        {
            s_bNoTier3Bundle = false;
        }
        
        if ( CommandLine()->HasParm( "-no_tier2_bundle" ) )
        {
            s_bNoTier2Bundle = true;
        }
        if ( CommandLine()->HasParm( "-no_tier3_bundle" ) )
        {
            s_bNoTier3Bundle = true;
        }
        
        s_pNoBundleModule = CommandLine()->ParmValue( "-no_bundle_module", "" );
        
        s_bCheckedCmd = true;
    }

    if ( s_pNoBundleModule &&
         s_pNoBundleModule[0] &&
         V_stricmp_fast( pModuleName, s_pNoBundleModule ) == 0 )
    {
        return pModuleName;
    }
    
    if ( !s_bNoTier2Bundle &&
         FindBundledModuleName( pModuleName, s_pTier2BundleModules, ARRAYSIZE( s_pTier2BundleModules ) ) )
    {
        return "tier2_bundle";
    }

    if ( !s_bNoTier3Bundle &&
         FindBundledModuleName( pModuleName, s_pTier3BundleModules, ARRAYSIZE( s_pTier3BundleModules ) ) )
    {
        return "tier3_bundle";
    }

    return pModuleName;
}
