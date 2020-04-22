//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//=============================================================================

#ifndef SFMANIMSETSCRIPTCONTEXT_H
#define SFMANIMSETSCRIPTCONTEXT_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmechannel.h"
#include "tier1/timeutils.h"

// Forward declarations
class CDmElement;
class CDmeClip;
class CDmeFilmClip;
class CDmeAnimationSet;
class CDmeGameModel;
class CDmeChannelsClip;
class CDmeDag;
class CDmeRigHandle;
class CDmeChannel;
class CDmeTransformControl;
class CDmeRigBaseConstraintOperator;
class CDmeConstraintTarget;
class CDmeRig;
class CStudioHdr;

//-------------------------------------------------------------------------------------------------
// CSFMAnimSetScriptContext 
//
//-------------------------------------------------------------------------------------------------
class CSFMAnimSetScriptContext
{

public:
	
	// Constructor
	CSFMAnimSetScriptContext( CDmeAnimationSet *pAnimSet, CDmeFilmClip *pShot, CDmeClip *pMovie, DmeTime_t time, ChannelMode_t channelMode );

	// Destructor
	~CSFMAnimSetScriptContext();	

	// Add the specified dag node to the animation set
	void AddDagNode( CDmeDag *pDagNode, CDmeRig *pRig, const char *pchControlGroupPath = 0, bool bPositionControl = true, bool bOrientationControl = true ) const;

	// Remove the specified rig handle from the animation set
	void RemoveRigHandle( CDmeRigHandle *pRigHandle ) const;

	// Add the specified constraint to the animation set
	void AddConstraint( CDmeRigBaseConstraintOperator *pConstraint, CDmeRig *pRig, const char *pchControlGroupPath = 0 ) const;

	// Find any dag node within the animation set with the specified name
	CDmeDag *FindDagNode( const char *pchName ) const;

	// Find the dag node associated with the specified bone
	CDmeDag *FindBoneDag( const char *pchBoneName ) const;

	// Set the channel operation mode for all of the channels in the the clip associated with the animation set
	void SetChannelMode( ChannelMode_t mode );

	// Set the current time and update the time of the channels in the animation set.
	void SetCurrentTime( DmeTime_t time );
	
	// Get the animation set associated with the context
	CDmeAnimationSet *GetAnimationSet() const { return m_pAnimSet; }

	// Get the gameModel associated with the context
	CDmeGameModel *GetGameModel() const	{ return m_pGameModel; }

private:

	// Create the position and rotation channels for the specified dag node.
	//void CreateTransformChannels( CDmeDag *pDag, CDmeChannel *channels[ 2 ], bool bPosition, bool bRotation ) const;

	// Create a transform control of the specified type for the provided dag node and attach it to the specified channel.
	CDmeTransformControl *CreateTransformControl( CDmeDag *pDag ) const;

	// Create the weight channels for the specified constraint
	void CreateWeightChannels( CDmeRigBaseConstraintOperator *pConstraint, CDmeRig *pRig ) const;

	// Create a control for the specified weight channel and add it to the animation set
	CDmElement *CreateWeightControl( const char *pBaseName, CDmeConstraintTarget *pTarget, CDmeChannel *pDagChannel, bool rig ) const;
	
	// Add a control with the specified name at the specified location to the animation set
	void AddControl( CDmElement *pControl, const char *pchGroupPath ) const;



private:
	
	bool					m_bValid;			// Flag indicating if the context is ready to perform operations
	DmeTime_t				m_CurrentTime;		// Current time at which the script is operating
	ChannelMode_t			m_ChannelMode;		// Current channel operation mode
	CDmeClip				*m_pMovie;			// Current movie in which the script is operating
	CDmeFilmClip			*m_pShot;			// Current shot in which the script is operating
	CDmeAnimationSet		*m_pAnimSet;		// Animation set on which the script is operating
	CDmeChannelsClip		*m_pChannelsClip;	// Channels clip for the animation set
	CDmeGameModel			*m_pGameModel;		// Game model associated with the animation set
	CStudioHdr				*m_pStudioHdr;		// Pointer to a studio model header interface instance

};
//-------------------------------------------------------------------------------------------------


#endif // SFMANIMSETSCRIPTCONTEXT_H
