//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: CSFMScriptMgr declaration, defines the interface of the sfm script
// manager class which is used to run scripts and provides and interface for
// script commands to perform operations.
//
//=============================================================================

#ifndef SFMSCRIPTMGR_H
#define SFMSCRIPTMGR_H
#ifdef _WIN32
#pragma once
#endif

#include "sfmobjects/sfmrigutils.h"
#include "movieobjects/dmechannel.h"
#include "tier1/UtlBuffer.h"
#include "tier1/UtlStack.h"
#include "tier1/fmtstr.h"


// Forward declarations
class CDmeDag;
class CDmeClip;
class CDmeFilmClip;
class CDmeAnimationSet;
class CDmeRigHandle;
class CDmeRigBaseConstraintOperator;
class CDmeRig;
class CDmeGameModel;
class CSFMAnimSetScriptContext;
class CDmePresetGroupInfo;


//-------------------------------------------------------------------------------------------------
// CSFMScriptContext -- The script context class is a storage container which provides information 
// to the script manager about the current state, including dag node and time selection.
//
//-------------------------------------------------------------------------------------------------
class CSFMScriptContext
{

public:

	enum SelectMode_t
	{
		SELECT_ADD,
		SELECT_REMOVE,
		SELECT_TOGGLE,	
	};

public:
	
	// Constructor
	CSFMScriptContext( CDmeClip *pMovie, CDmeFilmClip *pShot, DmeTime_t time, DmeFramerate_t framerate, CDmeAnimationSet *pActiveAnimSet = NULL, const CUtlVector< CDmeDag * > *pSelectedDagList = NULL, CDmElement *pSharedPresetGroupSettings = NULL );

	// Destructor, destroys the animation set context if created
	~CSFMScriptContext();

	// Get dag node the primary selection
	CDmeDag *PrimaryDagSelection() const;

	// Clear the current selection.
	void ClearSelection();

	// Add the specified node to the current selection.
	void AddToSelection( CDmeDag *pDagNode, SelectMode_t mode );

	// Remove the specified dag node from the current selection
	bool RemoveFromSelection( CDmeDag *pDagNode, bool preserveOrder = true );

	// Add the list of dag nodes to the current selection
	void AddListSelection( const CUtlVector< CDmeDag* > &selection );

	// Push the current selection on to the stack
	void PushSelection();

	// Pop the last selection from the stack
	void PopSelection();

	// Make the time selection include the entire shot
	void TimeSelectAll( DmeTime_t handleTime = DMETIME_ZERO );

	// Select time relative to the shot
	void TimeSelectShot( float leftFalloff, float leftHold, float rightHold, float rightFalloff, int leftFalloffType, int rightFalloffType );

	// Select time specified with in frames
	void TimeSelectFrames( int leftFalloff, int leftHold, int rightHold, int rightFalloff, int leftFalloffType, int rightFalloffType  );
	
	// Set the channel operating mode
	void SetChannelMode( ChannelMode_t mode );	
	
	// Get the current time a frame relative to the start of the shot
	void SetCurrentFrame( int frame );

	// Get the current frame relative to the start of the shot
	int GetCurrentFrame() const;

	// Set the current ( global ) time
	void SetCurrentTime( DmeTime_t globalTime );

	// Set the active animation set
	void SetActiveAnimationSet( CDmeAnimationSet *pAnimationSet );

	// Get the specified animation set context
	CSFMAnimSetScriptContext *GetAnimSetContext( int index ) const;
	
	// Find the animation set context for the specified animation set.
	CSFMAnimSetScriptContext *FindAnimationSetContext( CDmeAnimationSet *pAnimationSet ) const;

	// Get the first selected dag node in the selection list and initialize the iterator
	CDmeDag *GetFirstSelectedDag(); 

	// Get the next selected dag node in the selection list
	CDmeDag *GetNextSelectedDag();

	// Start a new rig or continue an existing one if a rig with the specified name already exists
	CDmeRig *BeginRig( const char *pchRigName, bool bAllowAppend );

	// End the current rig
	void EndRig();

	// Add an element to the currently active rig
	void AddElementToRig( CDmElement *pElement, CDmeAnimationSet *pAnimSet );

	
	// Accessors
	CDmeClip								*Movie() const						{ return m_pMovie;						}
	CDmeFilmClip							*Shot() const						{ return m_pShot;						}
	CDmeAnimationSet						*ActiveAnimationSet() const			{ return m_pActiveAnimSet;				}
	CDmeRig									*CurrentRig() const					{ return m_pCurrentRig;					}
	const DmeLog_TimeSelection_t			&TimeSelection() const				{ return m_TimeSelection;				}
	const CUtlVector< CDmeDag * >			&Selection() const					{ return m_Selection;					}
	DmeTime_t								CurrentTime() const					{ return m_CurrentTime;					}
	ChannelMode_t							ChannelMode() const					{ return m_ChannelMode;					}
	int										NumAnimationSets() const			{ return m_AnimSetContextList.Count();	}
	CDmElement								*SharedPresetGroupSettings() const	{ return m_pSharedPresetGroupSettings;	}

private:

	// Initialize the context.
	void InititalizeContext();

	// Destroy all of the animation set contexts
	void DestroyAnimSetContexts();

private:
	

	bool									m_bPythonInitalized;	// Flag indicating if python is initalized
	CDmeClip								*m_pMovie;				// Current movie in which the script is operating
	CDmeFilmClip							*m_pShot;				// Current shot in which the script is operating
	CDmeAnimationSet						*m_pActiveAnimSet;		// Currently selected animation set, used as a default
	CDmeRig									*m_pCurrentRig;			// Currently active rig
	ChannelMode_t							m_ChannelMode;			// Mode in which channels will be operated
	DmeTime_t								m_CurrentTime;			// Time at which the script is being run
	DmeFramerate_t							m_Framerate;			// The framerate setting of the current session
	DmeLog_TimeSelection_t					m_TimeSelection;		// Time selection to be used with any time dependent operations
	CUtlVector< CDmeDag * >					m_Selection;			// Set of selected dag nodes for script operation
	CUtlStack< CUtlVector< CDmeDag * > >	m_SelectionStack;		// Stack of selection sets used to store and restore selections
	CUtlVector< CSFMAnimSetScriptContext* >	m_AnimSetContextList;	// List of the animation set contexts, one for each animation set in the group
	int										m_CurrentControlIndex;	// Index used by the control iteration functions 
	int										m_CurrentDagIndex;		// Index used by the dag iteration functions
	CDmElement								*m_pSharedPresetGroupSettings; // Session's shared preset group settings
};


//-------------------------------------------------------------------------------------------------
// CSFMScriptMgr 
//
//-------------------------------------------------------------------------------------------------
class CSFMScriptMgr
{



public:

	// Constructor
	CSFMScriptMgr( CSFMScriptContext &scriptContext );

	// Destructor
	~CSFMScriptMgr();

	static void SetInterfaceFactory( CreateInterfaceFn factory );

	// Initialize the python script interface and register the sfm module.
	static void InitializeScriptInterface();

	// Shutdown the python script interface
	static void ShutdownScriptInterface();

	// Run the script provided in the buffer.
	static bool RunScript( const CUtlBuffer &scriptBuffer, CSFMScriptContext &scriptContext, CUtlString *pErrorMsg = NULL );

	// Find an animation set using the provided path
	CDmeAnimationSet *FindAnimationSet( const char *pchName, CDmeAnimationSet *pDefaultAnimSet = NULL ) const;

	// Set the currently active animation set that will be used when an animation set is not explicitly specified
	bool SetActiveAnimationSet( const char *pchAnimSetPath ) const;

	// Create an animation set for the specified element
	CDmeAnimationSet *CreateDagAnimationSet( CDmeDag *pElement, const char *pName ) const;

	// Create a game model referencing the specified mdl
	CDmeGameModel *CreateGameModel( const char *pRelativeModelName ) const;

	// Find a dag by name and animation set path
	CDmeDag *FindDag( const char *pchAnimSetDagPath, CDmeAnimationSet **pAnimationSet = NULL, const char **pDagName = NULL ) const;

	// Add all of the dag nodes of the animation set to the current selection
	bool SelectAnimSet( const char *pchAnimSetPath = NULL ) const;	

	// Add the controls and dag nodes within the specified selection group to the current selection set
	bool Select( const char *pchName, CSFMScriptContext::SelectMode_t mode = CSFMScriptContext::SELECT_ADD ) const;

	// Make the primary selection dag node the parent of all other selected dag nodes
	bool ParentDags( bool bMaintainWorldPos, bool bParentToWorld, ReParentLogMode_t logMode ) const;

	// Create a rig handle with the specified name, add it to the active rig if any.
	CDmeRigHandle *CreateRigHandle( char const *pchHandleName, char const *pchRigSubGroup, const Vector &p, const Quaternion &q, bool bPositionControl, bool bRotationControl ) const;

	// Create a dag and associated control for the specified attachment position.
	CDmeDag *CreateAttachmentHandle( const char *pchAttachmentName ) const;

	// Create a dag node and add controls for it to the specified animation set
	CDmeDag *CreateDag( const char *pName, const Vector &pos, const Quaternion &orient, CDmeAnimationSet *pAnimSet = NULL, CDmeDag *pParent = NULL ) const;

	// Create a constraint of the specified type using the current selection.
	CDmeConstraintTarget *CreateConstraint( char const *pchConstraintName, char const *pchRigSubGroup, int constrainType, bool bPreserveOffset, float weight, bool bCreateControls, bool bOperate ) const;

	// Create an aim constraint using the current selection.
	CDmeConstraintTarget *CreateAimConstraint( char const *pchConstraintName, char const *pchRigSubGroup, bool bPreserveOffset, float weight, bool bCreateControls, const Vector &upVector, TransformSpace_t upSpace, const CDmeDag *pRefDag ) const;

	// Create an IK constraint using he current selection
	CDmeConstraintTarget *CreateIKConstraint( char const *pchConstraintName, char const *pchRigSubGroup, bool bPreserveOffset, bool bCreateControls, const Vector &poleVector, CDmeDag *pPoleVectorTarget ) const;

	// Remove all constraints from the currently selected dag nodes.
	void RemoveConstraints() const;

	// Generate position and orientation logs for the currently selected dag nodes.
	bool GenerateLogSamples( CDmeDag *pParent, bool bPosition, bool bOrientation, bool bWorld ) const;

	// Get the averaged position of the selected dag nodes
	void GetDagPosition( Vector &position, TransformSpace_t space, const CDmeDag *pReferenceSpaceDag = NULL) const;

	// Get the average orientation of the selected dag nodes
	void GetDagRotation( Vector &rotation, TransformSpace_t space, const CDmeDag *pReferenceSpaceDag = NULL ) const;

	// Move the selected dag nodes to the specified position or by specified amount
	void MoveDags( const Vector &offset, TransformSpace_t space, bool bRelative, bool bOffsetMode, CDmeDag *pReferenceDag );

	// Rotate the selected dag nodes to the specified position or by specified amount
	void RotateDags( const Vector &offset, TransformSpace_t space, bool bRelative, bool bOffsetMode, CDmeDag *pReferenceDag );

	// Set the current position and rotation as the default for the transforms of the selected dag nodes
	void SetDagTransformDefaults( bool bPosition, bool bOrientation ) const;

	// Set the currently selected dag nodes to their reference pose position.
	void SetReferencePose() const;
	
	// Clear the error message buffer
	void ClearErrorMessage();

	// Append the provided string to the current error message
	void AppendErrorMessage( const char *errorString );

	// Get the current error message string
	const char *GetErrorMessage() const;
	
	// Get the script context
	const CSFMScriptContext &Context() const	{ return m_Context; }
	CSFMScriptContext &Context()				{ return m_Context;	}


private:	

	// Execute the specified script
	bool ExecuteScript( const CUtlBuffer &scriptBuffer );

	// Separate the target name from the path of a animation set / dag name path.
	static const char *SeparatePath( const char *pchAnimSetDagPath, char *pchAnimSetPath, int maxLength );

	// Construct a name for constraint on the specified dag 
	void ConstructNameForConstraint( EConstraintType constraintType, CDmeDag *pDag, const char *pchConstraintName, CFmtStr &name ) const;

	// Perform processing which is done on all newly created constraints
	CDmeConstraintTarget *ProcessNewConstraint( CDmeDag *pSalveDag, CDmeRigBaseConstraintOperator* pConstraint, const char *pchRigSubGroup, bool bCreateControls ) const;

private:

	CSFMScriptContext	&m_Context;
	CUtlString			m_ErrorBuffer;

	static CreateInterfaceFn sm_pToolsFactory;
};
//-------------------------------------------------------------------------------------------------


// Global pointer to the active script manager, created and destroyed by RunScript()
extern CSFMScriptMgr *g_pScriptMgr;


#endif // SFMSCRIPTMGR_H
