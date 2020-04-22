//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: See notes below
//
//=============================================================================

#include "sfmobjects/sfmobjects.h"
#include "datamodel/dmelementfactoryhelper.h"

// This hack causes the class factories for the element types to be imported into the compiled code...

USING_ELEMENT_FACTORY( DmeGraphEditorState );
