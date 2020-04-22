//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: A particle system definition
//
//=============================================================================

#ifndef DMEPARTICLESYSTEMDEFINITION_H
#define DMEPARTICLESYSTEMDEFINITION_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmattributevar.h"
#include "particles/particles.h"


// key names for clipboard keyvalues entries
#define PARTICLE_CLIPBOARD_FUNCTIONS_STR "fnc"
#define PARTICLE_CLIPBOARD_DEFINITION_STR "pcf"
#define PARTICLE_CLIPBOARD_DEF_BODY_STR "def"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeEditorTypeDictionary;
class CDmeParticleSystemDefinition;


//-----------------------------------------------------------------------------
// Base class for particle functions inside a particle system definition
//-----------------------------------------------------------------------------
class CDmeParticleFunction : public CDmElement
{
	DEFINE_ELEMENT( CDmeParticleFunction, CDmElement );

public:
	virtual const char *GetFunctionType() const { return NULL; }
	virtual void Resolve();
	virtual void OnElementUnserialized();

	// Used for backward compat
	void AddMissingFields( const DmxElementUnpackStructure_t *pUnpack );

	// Returns the editor type dictionary
	CDmeEditorTypeDictionary* GetEditorTypeDictionary();

	// Marks a particle system as a new instance
	// This is basically a workaround to prevent newly-copied particle functions
	// from recompiling themselves a zillion times
	void MarkNewInstance();

	// should be called if a function was shallow-copied, so it has its own type dictionary
	void InstanceTypeDictionary();

protected:
	void UpdateAttributes( const DmxElementUnpackStructure_t *pUnpack );

private:
	// Defines widgets to edit this bad boy
	CDmaElement< CDmeEditorTypeDictionary > m_hTypeDictionary;
	bool m_bSkipNextResolve;
};


//-----------------------------------------------------------------------------
// Something that updates particles
//-----------------------------------------------------------------------------
class CDmeParticleOperator : public CDmeParticleFunction
{
	DEFINE_ELEMENT( CDmeParticleOperator, CDmeParticleFunction );

public:
	// Sets the particle operator
	void SetFunction( IParticleOperatorDefinition *pDefinition );

	// Returns the function type
	virtual const char *GetFunctionType() const;

private:
	CDmaString m_FunctionName;
};


//-----------------------------------------------------------------------------
// A child of a particle system
//-----------------------------------------------------------------------------
class CDmeParticleChild : public CDmeParticleFunction
{
	DEFINE_ELEMENT( CDmeParticleChild, CDmeParticleFunction );

public:
	// Sets the particle operator
	void SetChildParticleSystem( CDmeParticleSystemDefinition *pDef, IParticleOperatorDefinition *pDefinition );

	// Returns the function type
	virtual const char *GetFunctionType() const;

	CDmaElement< CDmeParticleSystemDefinition > m_Child;
};



//-----------------------------------------------------------------------------
// Represents an editable entity; draws its helpers
//-----------------------------------------------------------------------------
class CDmeParticleSystemDefinition : public CDmElement
{
	DEFINE_ELEMENT( CDmeParticleSystemDefinition, CDmElement );

public:
	virtual void OnElementUnserialized();
	virtual void Resolve();

	// Add, remove
	CDmeParticleFunction* AddOperator( ParticleFunctionType_t type, const char *pFunctionName );
	CDmeParticleFunction* AddCopyOfOperator( CDmeParticleFunction *pFunc );
	CDmeParticleFunction* AddChild( CDmeParticleSystemDefinition *pChild );
	void RemoveFunction( ParticleFunctionType_t type, CDmeParticleFunction *pParticleFunction );
	void OverrideAttributesFromOtherDefinition( CDmeParticleSystemDefinition *pDef );

	// Find
	int FindFunction( ParticleFunctionType_t type, CDmeParticleFunction *pParticleFunction );
	int FindFunction( ParticleFunctionType_t type, const char *pFunctionName );

	// Iteration
	int GetParticleFunctionCount( ParticleFunctionType_t type ) const;
	CDmeParticleFunction *GetParticleFunction( ParticleFunctionType_t type, int nIndex );

	// Reordering
	void MoveFunctionUp( ParticleFunctionType_t type, CDmeParticleFunction *pElement );
	void MoveFunctionDown( ParticleFunctionType_t type, CDmeParticleFunction *pElement );

	// Returns the editor type dictionary
	CDmeEditorTypeDictionary* GetEditorTypeDictionary();

	// Recompiles the particle system when a change occurs
	void RecompileParticleSystem();

	// Marks a particle system as a new instance
	// This is basically a workaround to prevent newly-copied particle functions
	// from recompiling themselves a zillion times
	void MarkNewInstance();

	// Should we use name-based lookup?
	bool UseNameBasedLookup() const;

	void RemoveInvalidFunctions();

	// Remove DM attributes that aren't needed
	void Compact();

private:
	CDmaElementArray< CDmeParticleFunction > m_ParticleFunction[PARTICLE_FUNCTION_COUNT];
	CDmaVar< bool > m_bPreventNameBasedLookup;

	// Defines widgets to edit this bad boy
	CDmeHandle< CDmeEditorTypeDictionary > m_hTypeDictionary;
};


//-----------------------------------------------------------------------------
// Should we use name-based lookup?
//-----------------------------------------------------------------------------
inline bool CDmeParticleSystemDefinition::UseNameBasedLookup() const
{
	return !m_bPreventNameBasedLookup;
}


//-----------------------------------------------------------------------------
// Human readable string for the particle functions
//-----------------------------------------------------------------------------
const char *GetParticleFunctionTypeName( ParticleFunctionType_t type );

//-----------------------------------------------------------------------------
// Helper function

template<class T> T* ReadParticleClassFromKV( KeyValues *pKV, const char *pKeyName )
{
	const char *pData = pKV->GetString( pKeyName );
	int nLen = pData ? Q_strlen( pData ) : 0;
	if ( nLen )
	{
		CUtlBuffer buf( pData, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );

		DmElementHandle_t hRoot;
		if ( !g_pDataModel->Unserialize( buf, "keyvalues2", "pcf", NULL, "paste", CR_FORCE_COPY, hRoot ) )
		{
			return NULL;
		}

		return GetElement<T>( hRoot );
	}

	return NULL;
}



#endif // DMEPARTICLESYSTEMDEFINITION_H
