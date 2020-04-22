//======= Copyright © 1996-2009, Valve Corporation, All rights reserved. ======
//
//  Python wrapper for datamodel
//
//=============================================================================

#ifndef DATAMODEL_LIB_MOVIEOBJECTS_ONLY_H
#define DATAMODEL_LIB_MOVIEOBJECTS_ONLY_H

// Call this from application code to load up the datamodel python
// bindings when they are linked into the application directly
// Python must already be initialized
// This is a mode for ifm which only initializes and loads
// the datamodel & movieobjects python bindings
bool EmbedPyDataModel_movieobjects_only();

#endif // DATAMODEL_LIB_MOVIEOBJECTS_ONLY_H
