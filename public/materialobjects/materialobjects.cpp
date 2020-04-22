//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: See notes below
//
//=============================================================================

#include "materialobjects/materialobjects.h"
#include "datamodel/dmelementfactoryhelper.h"

// YOU MUST INCLUDE THIS FILE INTO ANY PROJECT WHICH USES THE materialobjects.lib FILE
// This hack causes the class factories for the element types to be imported into the compiled code...

// Material types
USING_ELEMENT_FACTORY( DmeAmalgamatedTexture );
USING_ELEMENT_FACTORY( DmeSheetSequence );
USING_ELEMENT_FACTORY( DmeSheetSequenceFrame );
USING_ELEMENT_FACTORY( DmeSheetImage );
USING_ELEMENT_FACTORY( DmeTexture );
USING_ELEMENT_FACTORY( DmeTextureFrame );
USING_ELEMENT_FACTORY( DmeImageArray );
USING_ELEMENT_FACTORY( DmeImage );
USING_ELEMENT_FACTORY( DmePrecompiledTexture );
USING_ELEMENT_FACTORY( DmeTP_ComputeMipmaps );
USING_ELEMENT_FACTORY( DmeTP_ChangeColorChannels );




