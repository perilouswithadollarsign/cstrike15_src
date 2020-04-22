//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: See notes below
//
//=============================================================================

#include "mdlobjects/mdlobjects.h"
#include "datamodel/dmelementfactoryhelper.h"

// YOU MUST INCLUDE THIS FILE INTO ANY PROJECT WHICH USES THE mdlobjects.lib FILE
// This hack causes the class factories for the element types to be imported into the compiled code...

// MDL types
USING_ELEMENT_FACTORY( DmeMdlList );
USING_ELEMENT_FACTORY( DmeBBox );
USING_ELEMENT_FACTORY( DmeHitbox );
USING_ELEMENT_FACTORY( DmeHitboxSet );
USING_ELEMENT_FACTORY( DmeHitboxSetList );
USING_ELEMENT_FACTORY( DmeBodyPart );
USING_ELEMENT_FACTORY( DmeBlankBodyPart );
USING_ELEMENT_FACTORY( DmeLOD );
USING_ELEMENT_FACTORY( DmeLODList );
USING_ELEMENT_FACTORY( DmeCollisionModel );
USING_ELEMENT_FACTORY( DmeJointConstrain );
USING_ELEMENT_FACTORY( DmeJointAnimatedFriction );
USING_ELEMENT_FACTORY( DmeCollisionJoint );
USING_ELEMENT_FACTORY( DmeCollisionJoints );
USING_ELEMENT_FACTORY( DmeBodyGroup );
USING_ELEMENT_FACTORY( DmeBodyGroupList );
USING_ELEMENT_FACTORY( DmeBoneWeight );
USING_ELEMENT_FACTORY( DmeBoneMask );
USING_ELEMENT_FACTORY( DmeBoneMaskList );
USING_ELEMENT_FACTORY( DmeMotionControl );
USING_ELEMENT_FACTORY( DmeIkChain );
USING_ELEMENT_FACTORY( DmeIkRange );
USING_ELEMENT_FACTORY( DmeIkLock );
USING_ELEMENT_FACTORY( DmeIkRule );
USING_ELEMENT_FACTORY( DmeIkTouchRule );
USING_ELEMENT_FACTORY( DmeIkFootstepRule );
USING_ELEMENT_FACTORY( DmeIkReleaseRule );
USING_ELEMENT_FACTORY( DmeIkAttachmentRule );
USING_ELEMENT_FACTORY( DmeAnimCmd );
USING_ELEMENT_FACTORY( DmeAnimCmdFixupLoop );
USING_ELEMENT_FACTORY( DmeAnimCmdWeightList );
USING_ELEMENT_FACTORY( DmeAnimCmdSubtract );
USING_ELEMENT_FACTORY( DmeAnimCmdAlign );
USING_ELEMENT_FACTORY( DmeAnimCmdRotateTo );
USING_ELEMENT_FACTORY( DmeAnimCmdWalkFrame );
USING_ELEMENT_FACTORY( DmeAnimCmdCompress );
USING_ELEMENT_FACTORY( DmeAnimCmdDerivative );
USING_ELEMENT_FACTORY( DmeAnimCmdLinearDelta );
USING_ELEMENT_FACTORY( DmeAnimCmdSplineDelta );
USING_ELEMENT_FACTORY( DmeAnimCmdNumFrames );
USING_ELEMENT_FACTORY( DmeAnimCmdPreSubtract );
USING_ELEMENT_FACTORY( DmeAnimCmdLocalHierarchy );
USING_ELEMENT_FACTORY( DmeAnimCmdNoAnimation );
USING_ELEMENT_FACTORY( DmeAnimationEvent );
USING_ELEMENT_FACTORY( DmeSequenceActivity );
USING_ELEMENT_FACTORY( DmeSequenceBlendBase );
USING_ELEMENT_FACTORY( DmeSequenceBlend );
USING_ELEMENT_FACTORY( DmeSequenceCalcBlend );
USING_ELEMENT_FACTORY( DmeSequenceBase );
USING_ELEMENT_FACTORY( DmeSequence );
USING_ELEMENT_FACTORY( DmeMultiSequence );
USING_ELEMENT_FACTORY( DmeSequenceList );
USING_ELEMENT_FACTORY( DmeIncludeModelList );
USING_ELEMENT_FACTORY( DmeDefineBone );
USING_ELEMENT_FACTORY( DmeDefineBoneList );
USING_ELEMENT_FACTORY( DmeMaterialGroup );
USING_ELEMENT_FACTORY( DmeMaterialGroupList );
USING_ELEMENT_FACTORY( DmeEyeballGlobals );
USING_ELEMENT_FACTORY( DmeEyeball );
USING_ELEMENT_FACTORY( DmeSkinnerVolume );
USING_ELEMENT_FACTORY( DmeSkinnerJoint );
USING_ELEMENT_FACTORY( DmeSkinner );
USING_ELEMENT_FACTORY( DmePoseParameter );
USING_ELEMENT_FACTORY( DmePoseParameterList );
USING_ELEMENT_FACTORY( DmeAnimBlockSize );
USING_ELEMENT_FACTORY( DmeSequenceLayerBase );
USING_ELEMENT_FACTORY( DmeSequenceAddLayer );
USING_ELEMENT_FACTORY( DmeSequenceBlendLayer );
USING_ELEMENT_FACTORY( DmeAssetRoot );
USING_ELEMENT_FACTORY( DmeRelatedAsset );
USING_ELEMENT_FACTORY( DmeElementGroup );
USING_ELEMENT_FACTORY( DmeBoneFlexDriverControl );
USING_ELEMENT_FACTORY( DmeBoneFlexDriver );
USING_ELEMENT_FACTORY( DmeBoneFlexDriverList );
USING_ELEMENT_FACTORY( DmeProceduralBone );
USING_ELEMENT_FACTORY( DmeJiggleBone );
USING_ELEMENT_FACTORY( DmeMatSysPanelSettings );
USING_ELEMENT_FACTORY( DmeMatSysRoot );
USING_ELEMENT_FACTORY( DmeMatSysMDLDag );
USING_ELEMENT_FACTORY( DmeMatSysDMXDag );
USING_ELEMENT_FACTORY( DmeMatSysMPPDag );
USING_ELEMENT_FACTORY( DmeAssemblyCommand );
USING_ELEMENT_FACTORY( DmeAnimationAssemblyCommand );
USING_ELEMENT_FACTORY( DmeFixupLoop );
USING_ELEMENT_FACTORY( DmeSubtract );
USING_ELEMENT_FACTORY( DmePreSubtract );
USING_ELEMENT_FACTORY( DmeRotateTo );
USING_ELEMENT_FACTORY( DmeBoneMaskCmd );
USING_ELEMENT_FACTORY( DmeEyelid );
USING_ELEMENT_FACTORY( DmeMouth );
