//====== Copyright (c) 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: See notes below
//
//=============================================================================

#include "movieobjects/movieobjects.h"

#include "movieobjects/movieobjects_compiletools.cpp"
#include "datamodel/dmelementfactoryhelper.h"

// YOU MUST INCLUDE THIS FILE INTO ANY PROJECT WHICH USES THE movieobjects.lib FILE
// This hack causes the class factories for the element types to be imported into the compiled code...

// Movie types
USING_ELEMENT_FACTORY( DmeCamera );
USING_ELEMENT_FACTORY( DmeSoundClip );
USING_ELEMENT_FACTORY( DmeFilmClip );
USING_ELEMENT_FACTORY( DmeMDL );
USING_ELEMENT_FACTORY( DmeMaterial );
USING_ELEMENT_FACTORY( DmeLight );
USING_ELEMENT_FACTORY( DmeGameModel );
USING_ELEMENT_FACTORY( DmeSound );
USING_ELEMENT_FACTORY( DmeGameSound );

USING_ELEMENT_FACTORY( DmeMorphOperator );
USING_ELEMENT_FACTORY( DmeTransformOperator );
USING_ELEMENT_FACTORY( DmeExpressionOperator );
USING_ELEMENT_FACTORY( DmeConnectionOperator );

USING_ELEMENT_FACTORY( DmeGamePortal );
USING_ELEMENT_FACTORY( DmeMouseInput );
USING_ELEMENT_FACTORY( DmeKeyboardInput );

USING_ELEMENT_FACTORY( DmeEditorAttributeInfo );
USING_ELEMENT_FACTORY( DmeEditorChoicesInfo );
USING_ELEMENT_FACTORY( DmeEditorType );
USING_ELEMENT_FACTORY( DmeEditorTypeDictionary );

USING_ELEMENT_FACTORY( DmePackColorOperator );
USING_ELEMENT_FACTORY( DmePackVector2Operator );
USING_ELEMENT_FACTORY( DmePackVector3Operator );
USING_ELEMENT_FACTORY( DmePackVector4Operator );
USING_ELEMENT_FACTORY( DmePackQAngleOperator );
USING_ELEMENT_FACTORY( DmePackQuaternionOperator );
USING_ELEMENT_FACTORY( DmePackVMatrixOperator );

USING_ELEMENT_FACTORY( DmeUnpackColorOperator );
USING_ELEMENT_FACTORY( DmeUnpackVector2Operator );
USING_ELEMENT_FACTORY( DmeUnpackVector3Operator );
USING_ELEMENT_FACTORY( DmeUnpackVector4Operator );
USING_ELEMENT_FACTORY( DmeUnpackQAngleOperator );
USING_ELEMENT_FACTORY( DmeUnpackQuaternionOperator );
USING_ELEMENT_FACTORY( DmeUnpackVMatrixOperator );

USING_ELEMENT_FACTORY( DmeAnimationSet );
USING_ELEMENT_FACTORY( DmePhonemeMapping );
USING_ELEMENT_FACTORY( DmeGlobalFlexControllerOperator );
USING_ELEMENT_FACTORY( DmeTransformControl );
USING_ELEMENT_FACTORY( DmeControlGroup );

USING_ELEMENT_FACTORY( DmeTimeSelection );

USING_ELEMENT_FACTORY( DmeConstraintTarget );
USING_ELEMENT_FACTORY( DmeConstraintSlave );
USING_ELEMENT_FACTORY( DmeRigPointConstraintOperator );
USING_ELEMENT_FACTORY( DmeRigOrientConstraintOperator );
USING_ELEMENT_FACTORY( DmeRigAimConstraintOperator );
USING_ELEMENT_FACTORY( DmeRigIKConstraintOperator );
USING_ELEMENT_FACTORY( DmeRigRotationConstraintOperator );
USING_ELEMENT_FACTORY( DmeRigHandle );
USING_ELEMENT_FACTORY( DmeRig );
USING_ELEMENT_FACTORY( DmeRigAnimSetElements );
USING_ELEMENT_FACTORY( DmeRigTwistSlave );
USING_ELEMENT_FACTORY( DmeRigTwistConstraintOperator );

USING_ELEMENT_FACTORY( DmeFlexRuleBase );
USING_ELEMENT_FACTORY( DmeFlexRulePassThrough );
USING_ELEMENT_FACTORY( DmeFlexRuleExpression );
USING_ELEMENT_FACTORY( DmeFlexRuleLocalVar );
USING_ELEMENT_FACTORY( DmeFlexRules );
