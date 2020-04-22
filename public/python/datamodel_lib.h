//======= Copyright © 1996-2009, Valve Corporation, All rights reserved. ======
//
//  Python wrapper for datamodel
//
//=============================================================================

#ifndef DATAMODEL_LIB_H
#define DATAMODEL_LIB_H

// Call this from application code to load up the datamodel python
// bindings when they are linked into the application directly
// Python must already be initialized
bool EmbedPyDataModel();

// Full register registers all _<module>.pyd and then the corresponding <module>.py files
void InitPyDataModelLib( bool bValvePythonInit );

#endif // DATAMODEL_LIB_H
