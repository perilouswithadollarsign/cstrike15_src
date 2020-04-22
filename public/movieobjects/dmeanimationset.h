//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMEANIMATIONSET_H
#define DMEANIMATIONSET_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "movieobjects/dmephonememapping.h"
#include "movieobjects/proceduralpresets.h"
#include "movieobjects/dmecontrolgroup.h"
#include "movieobjects/dmetransformcontrol.h"
#include "tier1/utldict.h"

class CDmeDag;
class CDmeFilmClip;
class CDmeTransform;
class CDmeRig;


// names of control value attributes and single-valued (non-animated) preset value attributes
#define AS_VALUE_ATTR				"value"
#define AS_VALUE_LEFT_ATTR			"leftValue"
#define AS_VALUE_RIGHT_ATTR			"rightValue"
#define AS_VALUE_POSITION_ATTR		"valuePosition"
#define AS_VALUE_ORIENTATION_ATTR	"valueOrientation"

// names of animated preset value attributes
#define AS_VALUES_ATTR				"values"
#define AS_VALUES_LEFT_ATTR			"leftValues"
#define AS_VALUES_RIGHT_ATTR		"rightValues"
#define AS_VALUES_POSITION_ATTR		"valuePositions"
#define AS_VALUES_ORIENTATION_ATTR	"valueOrientations"

// names of animated preset time attributes
#define AS_TIMES_ATTR				"times"
#define AS_TIMES_LEFT_ATTR			"leftTimes"
#define AS_TIMES_RIGHT_ATTR			"rightTimes"
#define AS_TIMES_POSITION_ATTR		"timePositions"
#define AS_TIMES_ORIENTATION_ATTR	"timeOrientations"



//-----------------------------------------------------------------------------
// A preset is a list of values to be applied to named controls in the animation set
//-----------------------------------------------------------------------------
class CDmePreset : public CDmElement
{
	DEFINE_ELEMENT( CDmePreset, CDmElement );

public:
	CDmaElementArray< CDmElement > &GetControlValues();	
	const CDmaElementArray< CDmElement > &GetControlValues() const;

	CDmElement *FindControlValue( const char *pControlName );
	CDmElement *FindOrAddControlValue( const char *pControlName );
	void RemoveControlValue( const char *pControlName );
	bool IsReadOnly();
	void CopyControlValuesFrom( CDmePreset *pSource );
	bool IsAnimated();

private:
	int FindControlValueIndex( const char *pControlName );

	CDmaElementArray< CDmElement > m_ControlValues;
};


class CDmeProceduralPresetSettings : public CDmElement
{
	DEFINE_ELEMENT( CDmeProceduralPresetSettings, CDmElement );
public:
	
	float	GetJitterScale( DmAttributeType_t attType ) const;
	float	GetSmoothScale( DmAttributeType_t attType ) const;
	float	GetSharpenScale( DmAttributeType_t attType ) const;
	float	GetSoftenScale( DmAttributeType_t attType ) const;

	CDmaVar< float > m_flJitterScale;
	CDmaVar< float > m_flSmoothScale;
	CDmaVar< float > m_flSharpenScale;
	CDmaVar< float > m_flSoftenScale;

	CDmaVar< float > m_flJitterScaleVector;
	CDmaVar< float > m_flSmoothScaleVector;
	CDmaVar< float > m_flSharpenScaleVector;
	CDmaVar< float > m_flSoftenScaleVector;

	CDmaVar< int > m_nJitterIterations;
	CDmaVar< int > m_nSmoothIterations;
	CDmaVar< int > m_nSharpenIterations;
	CDmaVar< int > m_nSoftenIterations;

	CDmaTime m_staggerInterval;
};


class CDmeAnimationSet;
class CDmeCombinationOperator;

//-----------------------------------------------------------------------------
// A preset group is a list of presets, with shared visibility + readonly settings
//-----------------------------------------------------------------------------
class CDmePresetGroup : public CDmElement
{
	DEFINE_ELEMENT( CDmePresetGroup, CDmElement );

public:
	CDmaElementArray< CDmePreset > &GetPresets();			// raw access to the array
	const CDmaElementArray< CDmePreset > &GetPresets() const;
	CDmePreset *FindPreset( const char *pPresetName );
	CDmePreset *FindOrAddPreset( const char *pPresetName );
	bool RemovePreset( CDmePreset *pPreset );
	bool RemovePreset( const char *pPresetName );
	void MovePresetInFrontOf( CDmePreset *pPreset, CDmePreset *pInFrontOf );

	CDmaVar< bool > m_bIsVisible;
	CDmaVar< bool > m_bIsReadOnly;

	// Exports this preset group to a faceposer .txt expression file
	bool ExportToTXT( const char *pFilename, CDmeAnimationSet *pAnimationSet = NULL, CDmeCombinationOperator *pComboOp = NULL ) const;

	// Exports this preset group to a faceposer .vfe expression file
	bool ExportToVFE( const char *pFilename, CDmeAnimationSet *pAnimationSet = NULL, CDmeCombinationOperator *pComboOp = NULL ) const;

private:
	int FindPresetIndex( CDmePreset *pPreset );
	int FindPresetIndex( const char *pPresetName );

	CDmaElementArray< CDmePreset > m_Presets; // "presets"
};


//-----------------------------------------------------------------------------
// The main controlbox for controlling animation for a single model
//-----------------------------------------------------------------------------
class CDmeAnimationSet : public CDmElement
{
	DEFINE_ELEMENT( CDmeAnimationSet, CDmElement );

public:
	virtual void OnAttributeArrayElementAdded  ( CDmAttribute *pAttribute, int nFirstElem, int nLastElem );
	virtual void OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem );

	CDmaElementArray< CDmElement > &GetControls();				// raw access to the array
	CDmaElementArray< CDmePresetGroup > &GetPresetGroups();		// raw access to the array
	CDmaElementArray< CDmePhonemeMapping > &GetPhonemeMap();	// raw access to the array
	CDmaElementArray< CDmeOperator > &GetOperators();			// raw access to the array

	void RestoreDefaultPhonemeMap();
	CDmePhonemeMapping *FindMapping( const char *pRawPhoneme );

	// Control methods
	void		AddControl( CDmElement *pControl );
	void		RemoveControl( CDmElement *pControl );
	CDmElement *FindControl( const char *pControlName ) const;
	CDmElement *FindOrAddControl( const char *pControlName, bool bTransformControl, bool bMustBeNew = false );
	CDmElement *CreateNewControl( const char *pControlName, bool bTransformControl );

	// Methods pertaining to preset groups
	CDmePresetGroup *FindPresetGroup( const char *pGroupName );
	CDmePresetGroup *FindOrAddPresetGroup( const char *pGroupName );
	void AddPresetGroup( CDmePresetGroup *pPresetGroup );
	bool RemovePresetGroup( CDmePresetGroup *pPresetGroup );
	bool RemovePresetGroup( const char *pGroupName );
	void MovePresetGroupInFrontOf( CDmePresetGroup *pPresetGroup, CDmePresetGroup *pInFrontOf );

	CDmePreset *FindOrAddPreset( const char *pGroupName, const char *pPresetName );
	bool RemovePreset( CDmePreset *pPreset );
	bool RemovePreset( const char *pPresetName );


	// Get the root selection group
	CDmeControlGroup *GetRootControlGroup() const;

	// Find the control group with the specified name.
	CDmeControlGroup *FindControlGroup( const char *pControlGroupName ) const;

	// Find the control group with the specified name or add it if does not exist
	CDmeControlGroup *FindOrAddControlGroup( CDmeControlGroup *pParentGroup, const char *pControlGroupName );

	// Find the control with the specified name, remove it from the group it belongs to and destroy it
	void RemoveControlFromGroups( char const *pchControlName, bool removeEmpty = false );

	// Build a list of the root dag nodes of the animation set
	void FindRootDagNodes( CUtlVector< CDmeDag* > &dagNodeList ) const;
	

	virtual void OnElementUnserialized();

	void CollectDagNodes( CUtlVector< CDmeDag* > &dagNodeList ) const;
	void CollectOperators( CUtlVector< DmElementHandle_t > &operators );
	void AddOperator( CDmeOperator *pOperator );
	void RemoveOperator( CDmeOperator *pOperator );

	void UpdateTransformDefaults() const;

private:
	int FindPresetGroupIndex( CDmePresetGroup *pGroup );
	int FindPresetGroupIndex( const char *pGroupName );

	CDmaElementArray< CDmElement >			m_Controls;				// "controls"
	CDmaElementArray< CDmePresetGroup >		m_PresetGroups;			// "presetGroups"
	CDmaElementArray< CDmePhonemeMapping >	m_PhonemeMap;			// "phonememap"
	CDmaElementArray< CDmeOperator >		m_Operators;			// "operators"	
	CDmaElement< CDmeControlGroup >			m_RootControlGroup;		// "rootControlGroup"

	// Helper for searching for controls by name
	CUtlDict< DmElementHandle_t, int >		m_ControlNameMap;
};

CDmeAnimationSet *FindAnimationSetForDag( CDmeDag *pDagNode );


//-----------------------------------------------------------------------------
// Utility class to migrate from traversing all animationsets within an animationsetgroup to a filmclip
//-----------------------------------------------------------------------------
class CAnimSetGroupAnimSetTraversal
{
public:
	CAnimSetGroupAnimSetTraversal( CDmeFilmClip *pFilmClip ) : m_pFilmClip( pFilmClip ), m_nIndex( 0 ) {}
	void Reset( CDmeFilmClip *pFilmClip ) { m_pFilmClip = pFilmClip; m_nIndex = 0; }

	bool IsValid();
	CDmeAnimationSet *Next();

private:
	CDmeFilmClip *m_pFilmClip;
	int m_nIndex;
};


//-----------------------------------------------------------------------------
// Utility class for finding the dependencies of a control with in the 
// animation set.
//-----------------------------------------------------------------------------
class CAnimSetControlDependencyMap
{
	public:

		// Add the controls of the specified animation set to the dependency map
		void AddAnimationSet( const CDmeAnimationSet* pAnimSet );

		// Get the list of controls which the specified control is dependent on.
		const CUtlVector< const CDmElement * > *GetControlDepndencies( const CDmElement *pControl ) const;


	private:


		struct DependencyList_t
		{
			const CDmElement				*m_pElement;
			CUtlVector< const CDmElement * > m_Dependencies;
		};

		DependencyList_t *FindDependencyList( const CDmElement* pElement );

		CUtlVector< DependencyList_t >					m_DependencyData;

};

static const char DEFAULT_POSITION_ATTR[] = "defaultPosition";
static const char DEFAULT_ORIENTATION_ATTR[] = "defaultOrientation";
static const char DEFAULT_FLOAT_ATTR[] = "defaultValue";

#define MULTI_CONTROL_FORMAT_STRING "multi_%s"

typedef int ControlIndex_t;
ControlIndex_t FindComboOpControlIndexForAnimSetControl( CDmeCombinationOperator *pComboOp, const char *pControlName, bool *pIsMulti = NULL );


inline bool IsMonoControl( const CDmElement *pControl )
{
//	static CUtlSymbolLarge sym = g_pDataModel->GetSymbol( "value" );
	return pControl->HasAttribute( "value" );
}

inline bool IsStereoControl( const CDmElement *pControl )
{
//	static CUtlSymbolLarge sym = g_pDataModel->GetSymbol( "rightValue" );
	return pControl->HasAttribute( "rightValue" );
}

inline bool IsTransformControl( const CDmElement *pControl )
{
	return pControl->IsA( CDmeTransformControl::GetStaticTypeSymbol() );
}


//-----------------------------------------------------------------------------
// Utility methods to convert between L/R and V/B
//-----------------------------------------------------------------------------
inline void ValueBalanceToLeftRight( float *pLeft, float *pRight, float flValue, float flBalance, float flDefaultValue )
{
	*pLeft  = ( flBalance <= 0.5f ) ? flValue : ( ( 1.0f - flBalance ) / 0.5f ) * ( flValue - flDefaultValue ) + flDefaultValue;
	*pRight = ( flBalance >= 0.5f ) ? flValue : (          flBalance   / 0.5f ) * ( flValue - flDefaultValue ) + flDefaultValue;
}


//-----------------------------------------------------------------------------
// CDmePresetGroupInfo - container for shared preset groups
//-----------------------------------------------------------------------------

class CDmePresetGroupInfo : public CDmElement
{
	DEFINE_ELEMENT( CDmePresetGroupInfo, CDmElement );

public:
	void LoadPresetGroups();

	const char *GetFilenameBase() const { return m_filenameBase.Get(); }
	void SetFilenameBase( const char *pFilenameBase ) { m_filenameBase = pFilenameBase; }

	int GetPresetGroupCount() const { return m_presetGroups.Count(); }
	CDmePresetGroup *GetPresetGroup( int i ) { return m_presetGroups[ i ]; }
	int AddPresetGroup( CDmePresetGroup *pPresetGroup ) { return m_presetGroups.AddToTail( pPresetGroup ); }

	static void FilenameBaseForModelName( const char *pModelName, char *pFileNameBase, int nFileNameBaseLen );
	static CDmePresetGroupInfo *FindPresetGroupInfo( const char *pFilenameBase, CDmrElementArray< CDmePresetGroupInfo > &presetGroupInfos );
	static CDmePresetGroupInfo *FindOrCreatePresetGroupInfo( const char *pFilenameBase, CDmrElementArray< CDmePresetGroupInfo > &presetGroupInfos );
	static CDmePresetGroupInfo *CreatePresetGroupInfo( const char *pFilenameBase, DmFileId_t fileid );

	static void LoadPresetGroups( const char *pFilenameBase, CDmaElementArray< CDmePresetGroup > &presetGroups );

protected:
	CDmaString m_filenameBase; // "filenameBase"
	CDmaElementArray< CDmePresetGroup > m_presetGroups; // "presetGroups"
};


#endif // DMEANIMATIONSET_H
