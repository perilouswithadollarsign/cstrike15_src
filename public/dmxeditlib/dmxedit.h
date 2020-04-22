//======== Copyright © 1996-2009, Valve, L.L.C., All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// A library of functions to edit Datamodel elements intended to be wrapped
// into a scripting langauge
//;
//=============================================================================


#ifndef DMXEDIT_H
#define DMXEDIT_H


// Valve includes
#include "appframework/IAppSystem.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeflexrules.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmeselection.h"
#include "movieobjects/dmmeshcomp.h"
#include "dmeutils/dmmeshutils.h"

#ifdef SWIG

%import( package="vs", module="datamodel" ) "datamodel/idatamodel.h"
%import( package="vs", module="datamodel" ) "datamodel/dmelement.h"
%import( package="vs", module="datamodel" ) "datamodel/dmattribute.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmedag.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmemesh.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmevertexdata.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmetransform.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmecombinationoperator.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmeanimationset.h"
%import( package="vs", module="movieobjects" ) "movieobjects/dmeselection.h"
%import( package="vs", module="mathlib" ) "mathlib/vector.h"

#endif // #ifdef SWIG

#ifdef SWIG
%warnfilter(401) IDmxEdit;	// Nothing known about base class 'IAppSystem'.  Ignored.
%immutable;
extern IDmxEdit *g_pDmxEdit;
%mutable;

#define SWIG_DOC( x ) %feature( "docstring" ) x
#else // SWIG
#define SWIG_DOC( x )
#endif // SWIG


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
enum DmxEditErrorState_t
{
	DMXEDIT_OK = 0,
	DMXEDIT_WARNING,
	DMXEDIT_ERROR
};

enum SelectOp_t
{
	ADD_SELECT_OP,
	SUBTRACT_SELECT_OP,
	TOGGLE_SELECT_OP,
	INTERSECT_SELECT_OP,
	REPLACE_SELECT_OP,
	INVALID_SELECT_OP
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class IDmxEdit : public IAppSystem
{
public:
	virtual DmxEditErrorState_t GetErrorState() const = 0;

	virtual void ResetErrorState() = 0;

	virtual const char *GetErrorString() const = 0;
};



//-----------------------------------------------------------------------------
// The argument names don't follow conventions so they look nice for Python
// which doesn't have pointers, etc...
//-----------------------------------------------------------------------------
extern IDmxEdit *g_pDmxEdit;

#ifdef SWIG
//-----------------------------------------------------------------------------
// Generalized error handler for dmxedit
//-----------------------------------------------------------------------------


%pythoncode %{

import filesystem

import mathlib
Vector = mathlib.Vector

SetMod = filesystem.setMod
SetGame = filesystem.setMod

def ContentDir():
	return str( filesystem.Path( filesystem.content() ) / filesystem.mod() )

%}

%typemap( in ) CUtlVector< const char * > &
{
	/* Check if is a list */
	if ( PyList_Check( $input ) )
	{
		const int nListCount = PyList_Size( $input );
		$1 = new CUtlVector< const char * >;
		for ( int i = 0; i < nListCount; i++) {
			PyObject *pPyListItem = PyList_GetItem( $input, i );
			if ( PyString_Check( pPyListItem ) )
			{
				$1->AddToTail( PyString_AsString( PyList_GetItem( $input, i ) ) );
			}
			else
			{
				PyErr_SetString( PyExc_TypeError, "Non string attempted to be converted into CUtlVector< const char * > &" );
				return NULL;
			}
		}
	}
	else
	{
		PyErr_SetString( PyExc_TypeError, "Non-list attempted to be converted into CUtlVector< const char * > &" );
		return NULL;
	}
}


// This cleans up the char ** array we malloc'd before the function call
%typemap( freearg ) CUtlVector< const char * > &
{
	delete $1;
}


%typemap( argout ) CUtlVector< const char * > *pOutStringList
{
	const int nListCount = $1->Count();

	PyObject *pPyStringList = PyList_New( nListCount );
	if ( !pPyStringList )
	{
		PyErr_SetString( PyExc_TypeError, "Couldn't allocate new python list to return CUtlVector< const char * > *" );
	}

	for ( int i = 0; i < nListCount; i++ )
	{
		PyList_SetItem( pPyStringList, i, PyString_FromString( $1->Element( i ) ) );
	}

	if ( ( !$result ) || ( $result == Py_None ) )
	{
		// Result not yet set, set it to a singleton of the data
		$result = pPyStringList;
	}
	else
	{
		// Result already set
		if ( !PyTuple_Check( $result ) )
		{
			// Result wasn't a tuple, so make one and set the first element
			// to old result
			PyObject *o2 = $result;
			$result = PyTuple_New( 1 );
			PyTuple_SetItem( $result, 0, o2 );
		}

		// Append data to result tuple
		PyObject *o3 = PyTuple_New( 1 );
		PyTuple_SetItem( o3, 0, pPyStringList );
		PyObject *o2 = $result;
		$result = PySequence_Concat( o2, o3 );
		Py_DECREF( o2 );
		Py_DECREF( o3 );
	}
}

%typemap( in, numinputs=0 ) CUtlVector< const char * > *pOutStringList( CUtlVector< const char * > tmpStringList )
{
	$1 = &tmpStringList;
}


%pythoncode %{
def MeshIt( root ):
	assert isinstance( root, vs.datamodel.CDmElement )
	mesh = FirstMesh
%}


// Declare it for SWIG
bool RemapMaterial( CDmeMesh *pDmemesh, const char *pszPattern, const char *pszRepl );

//-----------------------------------------------------------------------------
// Finds all materials bound to the mesh and replaces ones which match the
// source name with the destination name
//-----------------------------------------------------------------------------
%{
bool RemapMaterial( CDmeMesh *pDmeMesh, const char *pszPattern, const char *pszRepl )
{
	bool bRetVal = false;

	char szMatName[ MAX_PATH ];

	PyObject *pPyReObj = PyImport_ImportModule( "re" ); // New
	if ( pPyReObj )
	{
		PyObject *pPyReDict = PyModule_GetDict( pPyReObj ); // Borrowed
		if ( pPyReDict )
		{
			PyObject *pPyReCompile = PyDict_GetItemString( pPyReDict, "compile" ); // Borrowed
			PyObject *pPyReSub = PyDict_GetItemString( pPyReDict, "sub" ); // Borrowed
			PyObject *pPyReIGNORECASE = PyDict_GetItemString( pPyReDict, "IGNORECASE" ); // Borrowed

			if ( PyCallable_Check( pPyReCompile ) && PyCallable_Check( pPyReSub ) && pPyReIGNORECASE )
			{
				PyObject *pPyPattern = PyString_FromString( pszPattern ); // New
				if ( pPyPattern )
				{
					PyObject *pPyArgs0 = PyTuple_New( 2 );
					if ( pPyArgs0 )
					{
						Py_XINCREF( pPyReIGNORECASE );
						PyTuple_SetItem( pPyArgs0, 0, pPyPattern );
						PyTuple_SetItem( pPyArgs0, 1, pPyReIGNORECASE );

						PyObject *pPyRe = PyObject_CallObject( pPyReCompile, pPyArgs0 );	// New
						if ( pPyRe )
						{
							PyObject *pPyRepl = PyString_FromString( pszRepl );

							PyObject *pPyArgs1 = PyTuple_New( 3 );
							if ( pPyArgs1 )
							{
								PyTuple_SetItem( pPyArgs1, 0, pPyRe );
								PyTuple_SetItem( pPyArgs1, 1, pPyRepl );

								const int nFaceSetCount = pDmeMesh->FaceSetCount();
								for ( int i = 0; i < nFaceSetCount; ++i )
								{
									CDmeFaceSet *pDmeFaceSet = pDmeMesh->GetFaceSet( i );
									if ( !pDmeFaceSet )
										continue;

									CDmeMaterial *pDmeMaterial = pDmeFaceSet->GetMaterial();
									if ( !pDmeMaterial )
										continue;

									const char *pszMaterialName = pDmeMaterial->GetMaterialName();
									Q_StripExtension( pszMaterialName, szMatName, sizeof( szMatName ) );
									Q_FixSlashes( szMatName, '/' );

									PyObject *pPyMaterialName = PyString_FromString( szMatName );
									if ( pPyMaterialName )
									{
										PyTuple_SetItem( pPyArgs1, 2, pPyMaterialName );

										PyObject *pPyNewMat = PyObject_CallObject( pPyReSub, pPyArgs1 );	// New
										if ( pPyNewMat )
										{
											const char *pszNewMat = PyString_AsString( pPyNewMat );
											if ( Q_stricmp( szMatName, pszNewMat ) )
											{
												pDmeMaterial->SetMaterial( pszNewMat );
												pDmeMaterial->SetName( pszNewMat );

												bRetVal = true;
											}

											Py_XDECREF( pPyNewMat );
										}
									}
								}
							}
							else
							{
								Py_XDECREF( pPyRe );
								Py_XDECREF( pPyRepl );
							}
						}

						Py_XDECREF( pPyRe );
					}
					else
					{
						Py_XDECREF( pPyPattern );
					}
				}
			}
		}
	}

	Py_XDECREF( pPyReObj );

	return bRetVal;
}
%}


%pythoncode %{
def ImportQciFaceRules( dmeMesh, qciFilename ):
	import qcmodelblock

	qcmodelblock.CQcModelBlock( dmeMesh ).Parse( filesystem.resolve( qciFilename ) )

%}

#endif // #ifdef SWIG


SWIG_DOC( "Loads the specified DMX file from disk" );
CDmElement *LoadDmx( const char *filename );

SWIG_DOC( \
	"Loads the specified OBJ file(s) from disk.\n" \
	"objLoadType is one of \"ABSOLUTE\" or \"RELATIVE\".\n" \
	"Absolute is the shape as it will appear.\n" \
	"Relative will look strange as is, because they have had their dependent deltas subtracted from them." \
);
CDmElement *LoadObj( const char *filename, const char *objLoadType = "ABSOLUTE" );

SWIG_DOC( \
	"Saves the current scene to the specified dmx file\n" \
);
bool SaveDmx( CDmElement *pDmeRoot, const char *filename );

SWIG_DOC( \
	"Saves the current scene to a sequence of obj files.\n" \
	"$saveType is one of \"absolute\" or \"relative\".  If not specified, \"absolute\" is assumed.\n" \
	"If deltaName is passed, then only a single OBJ of that delta is saved.  \"base\" is the base state." \
);
bool SaveObj( CDmElement *pDmeRoot, const char *filename, const char *objSaveType = "ABSOLUTE", const char *deltaName = NULL );


SWIG_DOC( \
	"Returns the first mesh under the specfied element which can be a DmeDag or a DmeElement with\n" \
	"a \"model\" attribute pointing to the root of the model" \
);
CDmeMesh *GetFirstComboMesh( CDmElement *dmeRoot );


SWIG_DOC( \
	"Returns the first mesh under the specfied element which can be a DmeDag or a DmeElement\n" \
	"that matches the specified mesh name.  Name matching is case sensitive" \
); 
CDmeMesh *GetNamedMesh( CDmElement *dmeRoot, const char *meshName );


SWIG_DOC( "Returns the first mesh under the specfied element which can be a DmeDag or a DmeElement" );
CDmeMesh *GetFirstMesh( CDmElement *dmeRoot );


SWIG_DOC( "Returns the next mesh in the scene restarting the search at the specified mesh" );
CDmeMesh *GetNextMesh( CDmeMesh *currentDmeMesh );


SWIG_DOC( \
	"Prints a list of all of the deltas present in the specified mesh\n" \
	"\n" \
	"SEE ALSO\n" \
	"GetDeltaNames" \
);
bool ListDeltas( CDmeMesh *dmeMesh );

SWIG_DOC( \
	"Returns the number of delta states in the specified mesh\n" \
	"\n" \
	"Equivalent Python: len( dmeMesh.deltaStates )" \
);
int DeltaCount( CDmeMesh *dmeMesh );


SWIG_DOC( \
	"Returns the name of the delta state at the specified index of the specified mesh\n" \
	"\n" \
	"Equivalent Python: dmeMesh.deltaStates[ nDeltaIndex ].name" \
);
const char *DeltaName( CDmeMesh *dmeMesh, int nDeltaIndex );


SWIG_DOC( \
	"Returns the delta state at the specified index of the specified mesh\n" \
	"\n" \
	"Equivalent Python: dmeMesh.deltaStates[ nDeltaIndex ]" \
);
CDmeVertexDeltaData *GetDeltaState( CDmeMesh *dmeMesh, int nDeltaIndex );


SWIG_DOC( \
	"Returns the delta on the specified mesh that \n" \
	"\n" \
	"Equivalent Python: dmeMesh.deltaStates[ nDeltaIndex ].name" \
);
CDmeVertexDeltaData *GetDeltaState( CDmeMesh *dmeMesh, const char *pszDeltaName );


SWIG_DOC( \
	"Resets the current mesh back to the default base state, i.e. no deltas active" \
	"\n" \
	"dmeMesh.SetBaseStateToDelta( None, dmeMesh.FindBaseState( baseStateName ) )" \
);
bool ResetState( CDmeMesh *dmeMesh );


SWIG_DOC( \
	"Sets the specified base state on the specified mesh to the specified delta" \
);
bool SetState( CDmeMesh *dmeMesh, const char *deltaName );


SWIG_DOC( \
	"Removes faces from the specified mesh which have the specified material"
);
bool RemoveFacesWithMaterial( CDmeMesh *pDmeMesh, const char *pMaterialName );


SWIG_DOC( \
	"Removes faces from the specified mesh which have more than the specified number of faces"
);
bool RemoveFacesWithMoreThanNVerts( CDmeMesh *pDmeMesh, int nVertexCount );


SWIG_DOC( \
	"Mirrors the specified mesh in the specified axis.\n" \
	"$pszAxis is one of \"x\", \"y\", \"z\".\n" \
	"If $pszAxis is not specified, \"x\" is assumed (i.e. \"x\" == mirror across YZ plane).\n" \
	"Mirror() does the equivalent of a ResetState()."
);
bool Mirror( CDmeMesh *pDmeMesh, const char *pszAxis = "x" );


SWIG_DOC( \
	"Computes new smooth normals for the specified mesh and all of its delta states." \
);
bool ComputeNormals( CDmeMesh *pDmeMesh );


SWIG_DOC( \
	"Sets the wrinkle scale of the raw control of the specified control.\n" \
	"\n" \
	"NOTE: Only for use with ComputeWrinkles() (i.e. with the s, plural)\n" \
	"\n" \
	"      SetWrinkleScale merely sets the wrinkle scale value of the specified delta\n" \
	"      on the combination operator.  This value is only used if ComputeWrinkles()\n" \
	"      is called after all SetWrinkleScale() calls are made.\n" \
	"\n"
	"A wrinkle scale of 0 means there will be no wrinkle deltas.\n" \
	"The function to compute a single delta's wrinkle values,\n" \
	"ComputeWrinkle() does not use the wrinkle scale value at all.\n" \
	"\n" \
	"NOTE: Negative (-) is compress, Positive (+) is stretch.\n" \
	"\n" \
	"SEE ALSO\n" \
	"ComputeWrinkles, ComputeWrinkle, ComputeNormalWrinkles, ComputeNormalWrinkle" \
);
bool SetWrinkleScale( CDmeMesh *pDmeMesh, const char *pszControlName, const char *pszRawControlName, float flScale );


SWIG_DOC( \
	"Uses the current combo rules ( i.e. The wrinkleScales, via SetWrinkleScale())\n" \
	"to compute wrinkle delta values.\n" \
	"If #bOverwrite is false, only wrinkle data that doesn't currently exist on the mesh will be computed.\n" \
	"If #bOverwrite is true then all wrinkle data will be computed from the combo rules\n" \
	"wiping out any existing data computed using ComputeWrinkle().\n" \
	"\n" \
	"SEE ALSO\n" \
	"SetWrinkleScale, ComputeWrinkle, ComputeNormalWrinkles, ComputeNormalWrinkle()\n" \
);
bool ComputeWrinkles( CDmeMesh *pDmeMesh, bool bOverwrite = false );


SWIG_DOC( \
	"Updates the wrinkle stretch/compress data for the specified delta based on the position deltas\n" \
	"for the current state scaled by the scale value.  If $operation isn't specified then \"REPLACE\"\n" \
	"is assumed. $operation is one of: \"REPLACE\", \"ADD\".\n" \
	"\n" \
	"NOTE: Negative (-) is compress, Positive (+) is stretch.\n" \
	"\n" \
	"NOTE: This implies a call of: SetWrinkleScale( $delta, 1.0 )\n" \
	"\n" \
	"NOTE: This uses the current state of the mesh to compute the delta, it doesn't have to be the actual\n " \
	"      delta but to use the actual delta call it as below:\n" \
	"\n" \
	"for delta in GetDeltaList( mesh ):\n" \
	"    SetState( mesh, delta )\n" \
	"    ComputeWrinkle( mesh, delta, 1 )\n" \
	"\n" \
	"SEE ALSO\n" \
	"SetWrinkleScale, ComputeWrinkles, ComputeNormalWrinkles, ComputeNormalWrinkle\n" \
);
bool ComputeWrinkle( CDmeMesh *pDmeMesh, const char *pszDeltaName, float flScale, const char *pszOperation = "REPLACE" );


SWIG_DOC( \
	"Computes wrinkles for each delta based on the distance the vertex moves in each delta.\n" \
	"The sign (i.e. compress or stretch) is derived by comparing the delta vector with the\n" \
	"vertex normal.  Deltas moving in the same direction as the normal are stretch, deltas\n" \
	"moving in the opposite direction of the normal are compress\n" \
	"\n" \
	"NOTE: Opposite the normal, Negative (-) is compress, With the normal, Positive (+) is stretch.\n" \
	"\n" \
	"If #bOverwrite is false, only wrinkle data that doesn't currently exist on the mesh will be computed.\n" \
	"If #bOverwrite is true then all wrinkle data will be computed wiping out any existing wrinkle data.\n" \
	"\n" \
	"SEE ALSO\n"
	"SetWrinkleScale, ComputeWrinkles, ComputeWrinkle, ComputeNormalWrinkle\n" \
);
bool ComputeNormalWrinkles( CDmeMesh *pDmeMesh, float flScale, bool bOverwrite = false );


SWIG_DOC( \
	"Updates the wrinkle stretch/compress data for the specified delta based on the position deltas\n" \
	"for the current state scaled by the scale value.  If $operation isn't specified then \"REPLACE\"\n" \
	"is assumed. $operation is one of: \"REPLACE\", \"ADD\".\n" \
	"\n" \
	"The sign (i.e. compress or stretch) is derived by comparing the delta vector with the\n" \
	"vertex normal.  Deltas moving in the same direction as the normal are stretch, deltas\n" \
	"moving in the opposite direction of the normal are compress\n" \
	"\n" \
	"NOTE: Opposite the normal, Negative (-) is compress, With the normal, Positive (+) is stretch.\n" \
	"\n" \
	"NOTE: This calls SetWrinkleScale( $delta, 1.0 ) prior to running.\n" \
	"\n" \
	"NOTE: This uses the current state of the mesh to compute the delta, it doesn't have to be the actual\n" \
	"      delta but to use the actual delta call it as below:\n" \
	"\n" \
	"for delta in GetDeltaList( mesh ):\n" \
	"    SetState( mesh, delta )\n" \
	"    ComputeNormalWrinkle( mesh, delta, 1 )\n"
	"\n" \
	"SEE ALSO\n"
	"SetWrinkleScale, ComputeWrinkles, ComputeWrinkle, ComputeNormalWrinkles\n" \
);
bool ComputeNormalWrinkle( CDmeMesh *pDmeMesh, const char *pszDeltaName, float flScale, const char *pszOperation = "REPLACE" );


SWIG_DOC( \
	"Saves the current state of the specified mesh to the named delta state\n" \
);
bool SaveDelta( CDmeMesh *pDmeMesh, const char *pszDeltaName );


SWIG_DOC( \
	"Deletes the named delta state from the specified mesh\n" \
);
bool DeleteDelta( CDmeMesh *pDmeMesh, const char *pszDeltaName );


SWIG_DOC( \
	"Scales the position values by the specified amount.\n" \
	"If only 1 scale value is supplied a uniform scale in all three dimensions is applied\n" \
);
bool Scale( CDmeMesh *pDmeMesh, float flScaleX, float flScaleY, float flScaleZ );
bool Scale( CDmeMesh *pDmeMesh, float flScale );


SWIG_DOC( \
	"Sets the specified morph control on the specified mesh to be stereo if the 3rd argument is true.\n" \
	"If the 3rd argument is omitted, true is assumed\n" \
);
bool SetStereoControl( CDmeMesh *pDmeMesh, const char *pszControlName, bool bStereo = true );


SWIG_DOC( \
	"Sets the specified morph control on the specified mesh to be an eyelid control if the 3rd argument is true.\n" \
	"If the 3rd argument is omitted, true is assumed\n" \
);
bool SetEyelidControl( CDmeMesh *pDmeMesh, const char *pControlName, bool bEyelid = true );


SWIG_DOC( \
	"Returns the maximum distance any vertex moves in the specified delta.\n" \
	"Returns 0 if the delta specified doesn't exist\n" \
);
float MaxDeltaDistance( CDmeMesh *pDmeMesh, const char *pszDeltaName );


SWIG_DOC( \
	"Groups the specified raw controls under a common group control.\n"
	"If the group control already exists, the raw controls are added to it\n" \
);
bool GroupControls( CDmeMesh *pDmeMesh, const char *pszGroupName, CUtlVector< const char * > &rawControlNames );


SWIG_DOC( \
	"Returns a list of delta states present on the mesh.\n" \
	"For a delta state to be controllable, there must be a raw control\n" \
	"with the same name on combination operator controlling the mesh.\n" \
	"\n" \
	"SEE ALSO\n" \
	"GetRawControlNames\n" \
);
void GetDeltaNames( CDmeMesh *pDmeMesh, CUtlVector< const char * > *pOutStringList );


SWIG_DOC( \
	"Returns a list of raw controls on the combination operator controlling the mesh.\n" \
	"A raw control is the internal value that directly drives a delta state on a mesh.\n" \
	"Raw controls are not visible to the user.\n" \
	"For a raw control to work, it must be grouped under a control and there must\n" \
	"be a delta on the mesh with the same name.\n" \
	"If pszControlName is specified, it will return a list of the raw controls\n" \
	"groups under that control, otherwise it will return a list of all of the raw controls.\n" \
	"\n" \
	"SEE ALSO\n" \
	"GetDeltaNames, GetControlNames, GroupControls\n" \
);
void GetRawControlNames( CDmeMesh *pDmeMesh, CUtlVector< const char * > *pOutStringList, const char *pszControlName = NULL );

SWIG_DOC( \
	"Returns a list of control names on the combination opertator controlling the mesh.\n" \
	"These are the user visible controls.\n" \
	"For a control to work there must be at least one raw control grouped under the control.\n" \
	"\n" \
	"SEE ALSO\n" \
	"GetRawControlNames\n" \
);
void GetControlNames( CDmeMesh *pDmeMesh, CUtlVector< const char * > *pOutStringList );


SWIG_DOC( \
	"Reorders the controls in the specified order.\n" \
	"The specified controls will be moved to the front of the list of controls.\n" \
	"Not all of the controls need to be specified.\n" \
	"Unspecified controls will be left in the order they already are after the specified controls.\n" \
);
bool ReorderControls( CDmeMesh *pDmeMesh, CUtlVector< const char * > &controlNames );


SWIG_DOC( \
	"Create a domination rule.  A list of strings is passed as the 1st argument which are\n" \
	"the controls which are the dominators and a lua array of strings is passed as the 2nd\n" \
	"argument which are the controls to be supressed by the dominators.\n" \
	"EXAMPLE\n"
	"AddDominationRule( comboMesh, [ \"OpenLowerLip\", \"OpenUpperLip\" ], [ \"OpenLips\" ] )\n" \
);
bool AddDominationRule( CDmeMesh *pDmeMesh, CUtlVector< const char * > &dominators, CUtlVector< const char * > &supressed );


SWIG_DOC( \
	"Selects vertices on the specified mesh based on the specified string.\n" \
	"\n" \
	"  pszSelectString is one of:\n" \
	"\n" \
	"  ALL  -    Select all vertices in the mesh\n" \
	"  NONE -    Select nothing, i.e. clear the selection\n" \
	"  <delta> - Selects all vertices used in the specified delta\n" \
	"\n" \
	"If no CDmeSingleIndexedComponent is specified, the selection is stored on the mesh itself\n" \
	"and used between multiple operations.  The CDmeSingleIndexedComponent used is returned\n" \
	"and can be passed into subsequent calls.\n" \
);
CDmeSingleIndexedComponent *Select( CDmeMesh *pDmeMesh, const char *pzSelectOpString, const char *pszSelectString, CDmeSingleIndexedComponent *pPassedSelection = NULL );
CDmeSingleIndexedComponent *Select( CDmeMesh *pDmeMesh, const char *pszSelectString, CDmeSingleIndexedComponent *pPassedSelection = NULL );

SWIG_DOC( \
	"Returns the radius of the specified delta, if it exists.\n" \
	"Radius of a delta is defined as the radius of the tight bounding sphere\n" \
	"containing all of the deltas added to their base state values\n" \
);
float DeltaRadius( CDmeMesh *pDmeMesh, const char *pszDeltaName );

SWIG_DOC( \
	"Returns the radius of the specified selection, if no selection is specified, the current selection on the mesh is used if it exists, if not 0 is returned\n" \
);
float SelectionRadius( CDmeMesh *pDmeMesh, CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC( \
	"Grows the specified or current selection on the mesh by the specified size" \
);
bool GrowSelection( CDmeMesh *pDmeMesh, int nSize = 1, CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC( \
	"Shrinks the specified or current selection on the mesh by the specified size" \
);
bool ShrinkSelection( CDmeMesh *pDmeMesh, int nSize = 1, CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC( \
	"Sets the way distances will be interpreted after the command.  $distanceType is one of \"absolute\" or \"relative\".  By default distances are \"absolute\".  All functions that work with distances (Add, Interp and Translate) work on the currently selected vertices.  \"absolute\" means use the distance as that number of units, \"relative\" means use the distance as a scale of the radius of the bounding sphere of the selected vertices." \
);
bool SetDistanceType( const char *pszDistanceType );

SWIG_DOC( \
	"Interpolates the current state of the mesh towards the specified state, $weight, #featherDistance and $falloffType.\n" \
	"$falloffType is one of \"STRAIGHT\", \"SPIKE\", \"DOME\", \"BELL\"" \
);
bool Interp(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight = 1.0f,
	float flFeatherDistance = 0.0f,
	const char *pszFalloffType = "STRAIGHT",
	const char *pszDistanceType = "DEFAULT",
	CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC(  \
	"Adds specified state to the current state of the mesh weighted and feathered by the specified #weight, #featherDistance & $falloffType.  " \
	"$falloffType is one of \"STRAIGHT\", \"SPIKE\", \"DOME\", \"BELL\".  Note that only the specified delta is added.  " \
	"i.e. If Add( \"A_B\" ); is called then just A_B is added, A & B are not.  See AddCorrected();" \
);
bool Add(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight = 1.0f,
	float flFeatherDistance = 0.0f,
	const char *pszFalloffType = "STRAIGHT",
	const char *pszDistanceType = "DEFAULT",
	CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC(  \
	"Same as Add() except that the corrected delta is added. i.e. If AddCorrected( \"A_B\" ); is called " \
	"then A, B & A_B are all added.  This works similarly to SetState() whereas Add() just adds " \
	"the named delta." \
);
bool AddCorrected(
	CDmeMesh *pDmeMesh,
	const char *pszDeltaName,
	float flWeight = 1.0f,
	float flFeatherDistance = 0.0f,
	const char *pszFalloffType = "STRAIGHT",
	const char *pszDistanceType = "DEFAULT",
	CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC( \
	"Translates the selected vertices of the mesh by the specified amount" \
);
bool Translate(
    CDmeMesh *pDmeMesh,
    float flTx,
    float flTy,
    float flTz,
    float flFeatherDistance = 0.0f,
	const char *pszFalloffType = "STRAIGHT",
	const char *pszDistanceType = "DEFAULT",
    CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC( \
	"Rotates the selected vertices of the mesh by the specified amount" \
);
bool Rotate(
    CDmeMesh *pDmeMesh,
    float flRx,
    float flRy,
    float flRz,
    float flOx = 0.0f,
    float flOy = 0.0f,
    float flOz = 0.0f,
    float flFeatherDistance = 0.0f,
	const char *pszFalloffType = "STRAIGHT",
	const char *pszDistanceType = "DEFAULT",
    CDmeSingleIndexedComponent *pDmePassedSelection = NULL );

SWIG_DOC( \
	"Remaps the material at the specified index while looking at the face sets, to the new material path name" \
);
bool RemapMaterial( CDmeMesh *pDmeMesh, int nMaterialIndex, const char *pszNewMaterialName );

SWIG_DOC( \
	"Takes the data from the Src mesh and combines it into the specified mesh. " \
	"If a skinning bone name is specified, it will skin all of the vertices added to the mesh to that bone. " \
	"Otherwise... something else will happen." \
	"NOTE: MergeMeshAndSkeleton should probably be used instead of this function as it handles skinning better.  TODO: Overlapping vertices merged via a new function\n" \
	"SEE ALSO\n" \
	"MergeMeshAndSkeleton"
);
bool Combine( CDmeMesh *pDstMesh, CDmeMesh *pSrcMesh, const char *pszDstSkinningBoneName = NULL );

SWIG_DOC( \
	"Takes the specified src mesh and merges onto the dst mesh at a merge socket.  " \
	"Also copies skin weights from the closest vertices in the dst mesh.\n" \
	"NOTE: The dst can be either a mesh or a root to a loaded scene.\n" \
	"If a scene root is specified as the dst, then it is searched for the best matching merge socket.\n" \
	"NOTE: MergeMeshAndSkeleton should probably be used instead of this function.  TODO: Overlapping vertices merged via a new function\n" \
	"SEE ALSO\n" \
	"MergeMeshAndSkeleton"
);
bool Merge( CDmElement *pDmDstRoot, CDmeMesh *pSrcMesh );


SWIG_DOC( \
	"Merges all of the mesh data from the SRC mesh into the DST mesh.  " \
	"Any joints that SRC mesh is skinned to that don't exist in the DST mesh scene are " \
	"also merged as necessary.  SRC & DST joints are matched by name.\n" \
	"This function should probably be used instead of Combine or Merge\n"
);
bool MergeMeshAndSkeleton( CDmeMesh *pDstMesh, CDmeMesh *pSrcMesh );


SWIG_DOC( \
	"Takes a delta and applies a selection mask to it.  Where the mask is zero, the delta is turned off" \
	"and where the mask is one, the delta is maintained fully with values inbetween dampening the effect" \
	"of the delta."
);
bool ApplyMaskToDelta( CDmeVertexDeltaData *pTheDelta, CDmeSingleIndexedComponent *pDmePassedSelection );


SWIG_DOC( \
	"Creates a delta on the base mesh based on the diff between the base mesh and the"\
	"\"pszMeshToUseAsDelta\" with the name given."
);
bool CreateDeltaFromMesh( CDmeMesh *pBaseMesh, CDmeMesh *pszMeshToUseAsDelta, const char *pszDeltaName, CDmeSingleIndexedComponent *pDmePassedSelection = NULL );


SWIG_DOC( \
	"Returns the DmeCombinationOperator driving the specified mesh, if no DmeCombinationOperator " \
	"is driving the mesh, one is created and returned."
);
CDmeCombinationOperator *FindOrCreateComboOp( CDmeMesh *pDmeMesh );


SWIG_DOC( \
	"Moves the mesh vertices on the specified base state to the current position as computed " \
	"from the skin weights and current skeleton position.  If no base state is specified then " \
	"the standard dmxedit base state is used"
);
bool SetMeshFromSkeleton( CDmeMesh *pDmeMesh );


SWIG_DOC( \
	"Sets the wrinkle map value in the specified delta of the selected vertices on the specified mesh to the scale " \
	"value multipled by the vertex selection weight" \
);
bool SetWrinkleWeight( CDmeMesh *pDmeMesh, const char *pszDeltaName, CDmeSingleIndexedComponent *pDmePassedSelection, float flScale = 1.0 );

SWIG_DOC( \
	"Computes a wrap deformation of the target mesh based on the bind and current base states" \
	"of the influence mesh." \
);
bool WrapDeform( CDmeMesh *pDmeInfluenceMesh, CDmeMesh *pDmeTargetMesh );

SWIG_DOC( \
	"Compute the convex 3D hull of specified mesh and return it as a new mesh" \
);
CDmeMesh *ComputeConvexHull3D( CDmeMesh *pDmeMesh, float flCoplanarEpsilon = 1.0f / 32.0f );

SWIG_DOC( \
	"Compute the mean point for the specified point list" \
);
Vector ComputeMean( const CUtlVector< Vector > &pointList );

SWIG_DOC( \
	"Compute the covariant matrix for the specified point list" \
);
matrix3x4_t ComputeCovariantMatrix( const CUtlVector< Vector > &pointList, const Vector &vMean );
matrix3x4_t ComputeCovariantMatrix( const CUtlVector< Vector > &pointList );

SWIG_DOC( \
	"Like QC '$model localvar <var> [ var ...]'"
);
bool FlexLocalVar( CDmeMesh *pDmeMesh, const char *pszFlexRuleLocalVar );

SWIG_DOC( \
	"Like QC '$model <expression>'"
);
bool FlexRuleExpression( CDmeMesh *pDmeMesh, const char *pszExpression );

SWIG_DOC( \
	"Find the DmeFlexRules for the specified mesh or create one if one doesn't exist"
);
CDmeFlexRules *FindOrAddDmeFlexRules( CDmeMesh *pDmeMesh );

SWIG_DOC( \
	"Creates the named flex control with the specified min/max, returns -1 on failure."
);
ControlIndex_t FlexControl( CDmeMesh *pDmeMesh, const char *pszName, float flMin = 0.0f, float flMax = 1.0f );

SWIG_DOC( \
	"Set the specified flex control's min/max value"
);
bool SetControlMinMax( CDmeMesh *pDmeMesh, ControlIndex_t nControlIndex, float flMin, float flMax );


SWIG_DOC( \
	"Adds an eyeball like QC $model eyeball"
);
bool Eyeball(
	CDmeMesh *pDmeMesh,
	const char *pszName,
	const char *pszBoneName,
	const Vector &vPosition,
	const char *pszMaterialName,
	const float flDiameter,
	const float flAngle,
	const float flPupilScale );


SWIG_DOC( \
	"Adds an eyeball like QC $model dmxeyelid"
);
bool Eyelid(
	CDmeMesh *pDmeMesh,
	bool bUpper,
	const char *pszLowererFlex,
	float flLowererHeight,
	const char *pszNeutralFlex,
	float flNeutralHeight,
	const char *pszRaiserFlex,
	float flRaiserHeight,
	const char *pszRightMaterialName,
	const char *pszLeftMaterialName );

SWIG_DOC( \
	"Adds an mouth like QC $model mouth"
);
bool Mouth(
	CDmeMesh *pDmeMesh,
	int nMouthNumber,
	const char *pszFlexControllerName,
	const char *pszBoneName,
	const Vector &vForward );


SWIG_DOC( \
	"Removes all flex/face rules"
);
bool ClearFlexRules( CDmeMesh *pDmeMesh );

#if 0

// Do not do these until requested
bool SelectHalf( const CSelectOp &selectOp, const CHalfType &halfType, CDmeSingleIndexedComponent *pPassedSelection = NULL, CDmeMesh *pPassedMesh = NULL );

bool SelectHalf( const CHalfType &halfType, CDmeSingleIndexedComponent *pPassedSelection = NULL, CDmeMesh *pPassedMesh = NULL );

bool ImportComboRules( const char *pFilename, bool bOverwrite = true, bool bPurgeDeltas = true );

bool CreateDeltasFromPresets( const char *pPresetFilename, bool bDeleteNonPresetDeltas = true, const CUtlVector< CUtlString > *pPurgeAllButThese = NULL, const char *pExpressionFilename = NULL );

bool CachePreset( const char *pPresetFilename, const char *pExpressionFilename = NULL );

bool ClearPresetCache();

bool CreateDeltasFromCachedPresets( bool bDeleteNonPresetDeltas = true, const CUtlVector< CUtlString > *pPurgeAllButThese = NULL ) const;

bool CreateExpressionFileFromPresets( const char *pPresetFilename, const char *pExpressionFilename );

bool CreateExpressionFilesFromCachedPresets() const;

bool FixPresetFile( const char *pPresetFilename );

#endif // #if 0


#ifdef SWIG


%pythoncode %{
def MeshIt( root ):
	import vs.datamodel
	assert isinstance( root, vs.datamodel.CDmElement )
	mesh = GetFirstMesh( root )
	while True:
		if mesh is None:
			break
		yield mesh
		mesh = GetNextMesh( mesh )

def ComboMeshIt( root ):
	import vs.datamodel
	assert isinstance( root, vs.datamodel.CDmElement )

	for mesh in MeshIt( root ):
		if mesh.DeltaStateCount() > 0:
			yield mesh

def FirstComboMesh( root ):
	import vs.datamodel
	assert isinstance( root, vs.datamodel.CDmElement )

	for mesh in MeshIt( root ):
		if mesh.DeltaStateCount() > 0:
			return mesh

	return None

%}

#endif // #ifdef SWIG

#endif // DMXEDIT_H