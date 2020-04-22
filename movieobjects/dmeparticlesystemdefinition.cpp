//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmeparticlesystemdefinition.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "toolutils/enginetools_int.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier1/convar.h"
#include "particles/particles.h"
#include "dme_controls/attributeintchoicepanel.h"
#include "dme_controls/attributeboolchoicepanel.h"
#include "dme_controls/attributestringchoicepanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//-----------------------------------------------------------------------------
// Human readable string for the particle functions
//-----------------------------------------------------------------------------
static const char *s_pParticleFuncTypeName[PARTICLE_FUNCTION_COUNT] =
{
	"Renderer",												// FUNCTION_RENDERER = 0,
	"Operator",												// FUNCTION_OPERATOR,
	"Initializer",											// FUNCTION_INITIALIZER,
	"Emitter",												// FUNCTION_EMITTER,
	"Children",												// FUNCTION_CHILDREN,
	"ForceGenerator",										// FUNCTION_FORCEGENERATOR
	"Constraint",											// FUNCTION_CONSTRAINT
};

const char *GetParticleFunctionTypeName( ParticleFunctionType_t type )
{
	return s_pParticleFuncTypeName[type];
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY_INSTALL_EXPLICITLY( DmeParticleFunction, CDmeParticleFunction );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeParticleFunction::OnConstruction()
{
	m_bSkipNextResolve = false;
	m_hTypeDictionary.Init( this, "type_dictionary", FATTRIB_DONTSAVE | FATTRIB_HIDDEN );
}

void CDmeParticleFunction::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Construct an appropriate editor attribute info
//-----------------------------------------------------------------------------
static void CreateEditorAttributeInfo( CDmeEditorType *pEditorType, const char *pAttributeName, const char *pWidgetInfo )
{
	if ( !pWidgetInfo )
		return;

	CCommand parse;
	parse.Tokenize( pWidgetInfo );
	if ( parse.ArgC() == 1 )
	{
		CDmeEditorAttributeInfo *pInfo = CreateElement< CDmeEditorAttributeInfo >( "field info", DMFILEID_INVALID );
		pEditorType->AddAttributeInfo( pAttributeName, pInfo ); 
		pInfo->m_Widget = parse[0];
		return;
	}

	if ( parse.ArgC() == 2 )
	{
		CDmeEditorChoicesInfo *pInfo = NULL;
		if ( !Q_stricmp( parse[0], "intchoice" ) )
		{
			pInfo = CreateElement< CDmeEditorIntChoicesInfo >( "field info", DMFILEID_INVALID );
		}

		if ( !Q_stricmp( parse[0], "boolchoice" ) )
		{
			pInfo = CreateElement< CDmeEditorBoolChoicesInfo >( "field info", DMFILEID_INVALID );
		}

		if ( !Q_stricmp( parse[0], "stringchoice" ) )
		{
			pInfo = CreateElement< CDmeEditorStringChoicesInfo >( "field info", DMFILEID_INVALID );
		}

		if ( !Q_stricmp( parse[0], "elementchoice" ) )
		{
			pInfo = CreateElement< CDmeEditorChoicesInfo >( "field info", DMFILEID_INVALID );
		}

		if ( pInfo )
		{
			pInfo->SetChoiceType( parse[1] );
			pEditorType->AddAttributeInfo( pAttributeName, pInfo ); 
			pInfo->m_Widget = parse[0];
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Used for backward compat
//-----------------------------------------------------------------------------
void CDmeParticleFunction::AddMissingFields( const DmxElementUnpackStructure_t *pUnpack )
{
	DestroyElement( m_hTypeDictionary, TD_DEEP );
	m_hTypeDictionary = CreateElement< CDmeEditorTypeDictionary >( "particleFunctionDict", DMFILEID_INVALID );
	CDmeEditorType *pEditorType = CreateElement< CDmeEditorType >( GetTypeString(), DMFILEID_INVALID );

	for ( ; pUnpack->m_pAttributeName; ++pUnpack )
	{
		CreateEditorAttributeInfo( pEditorType, pUnpack->m_pAttributeName, (const char *)pUnpack->m_pUserData );

		// Can happen if 'name' or 'functionName' is used
		if ( HasAttribute( pUnpack->m_pAttributeName ) )
			continue;

		CDmAttribute *pAttribute = AddAttribute( pUnpack->m_pAttributeName, pUnpack->m_AttributeType );
		if ( pUnpack->m_pDefaultString )
		{
			int nLen = Q_strlen( pUnpack->m_pDefaultString );
			CUtlBuffer bufParse( pUnpack->m_pDefaultString, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
			pAttribute->Unserialize( bufParse );
		}
	}

	m_hTypeDictionary->AddEditorType( pEditorType );
}


//-----------------------------------------------------------------------------
// Sets the particle operator
//-----------------------------------------------------------------------------
void CDmeParticleFunction::UpdateAttributes( const DmxElementUnpackStructure_t *pUnpack )
{
	// Delete all old attributes
	CDmAttribute *pNext;
	for( CDmAttribute *pAttr = FirstAttribute(); pAttr; pAttr = pNext )
	{
		pNext = pAttr->NextAttribute();
		if ( pAttr->IsStandard() || pAttr->IsFlagSet( FATTRIB_EXTERNAL ) )
			continue;

		RemoveAttributeByPtr( pAttr );
	}

	AddMissingFields( pUnpack );
}


//-----------------------------------------------------------------------------
// Marks a particle system as a new instance
// This is basically a workaround to prevent newly-copied particle functions
// from recompiling themselves a zillion times
//-----------------------------------------------------------------------------
void CDmeParticleFunction::MarkNewInstance()
{
	m_bSkipNextResolve = true;
}


//-----------------------------------------------------------------------------
// Don't bother resolving during unserialization, the owning def will handle it
//-----------------------------------------------------------------------------
void CDmeParticleFunction::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();
	MarkNewInstance();
}


//-----------------------------------------------------------------------------
// Recompiles the particle system when a change occurs
//-----------------------------------------------------------------------------
void CDmeParticleFunction::Resolve()
{
	BaseClass::Resolve();

	if ( m_bSkipNextResolve )
	{
		m_bSkipNextResolve = false;
		return;
	}

	for( CDmAttribute* pAttr = FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
	{
		if ( !pAttr->IsFlagSet( FATTRIB_DIRTY ) )
			continue;

		// Find all CDmeParticleSystemDefinitions referring to this function 
		DmAttributeReferenceIterator_t i = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
		while ( i != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
		{
			CDmAttribute *pAttribute = g_pDataModel->GetAttribute( i );

			// NOTE: This could cause the same particle system definition to recompile
			// multiple times if it refers to the same function multiple times,
			// but we don't expect that to happen, so we won't bother checking for it
			CDmeParticleSystemDefinition *pDef = CastElement<CDmeParticleSystemDefinition>( pAttribute->GetOwner() );
			if ( pDef && pDef->GetFileId() == GetFileId() )
			{
				pDef->RecompileParticleSystem();
			}
			i = g_pDataModel->NextAttributeReferencingElement( i );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Returns the editor type dictionary
//-----------------------------------------------------------------------------
CDmeEditorTypeDictionary* CDmeParticleFunction::GetEditorTypeDictionary()
{
	return m_hTypeDictionary;
}

void CDmeParticleFunction::InstanceTypeDictionary()
{
	if ( m_hTypeDictionary != DMELEMENT_HANDLE_INVALID )
	{
		m_hTypeDictionary = m_hTypeDictionary->Copy();
	}
}

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY_INSTALL_EXPLICITLY( DmeParticleOperator, CDmeParticleOperator );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeParticleOperator::OnConstruction()
{
	m_FunctionName.Init( this, "functionName" );
}

void CDmeParticleOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the particle operator
//-----------------------------------------------------------------------------
void CDmeParticleOperator::SetFunction( IParticleOperatorDefinition *pDefinition )
{
	m_FunctionName = pDefinition->GetName();
	const DmxElementUnpackStructure_t *pUnpack = pDefinition->GetUnpackStructure();
	UpdateAttributes( pUnpack );
}

const char *CDmeParticleOperator::GetFunctionType() const
{
	return m_FunctionName;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY_INSTALL_EXPLICITLY( DmeParticleChild, CDmeParticleChild );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeParticleChild::OnConstruction()
{
	m_Child.Init( this, "child", FATTRIB_NEVERCOPY );
/*
	CDmeEditorType *pEditorType = CreateElement< CDmeEditorType >( "DmeParticleChild", DMFILEID_INVALID );
	CDmeEditorAttributeInfo *pInfo = CreateElement< CDmeEditorAttributeInfo >( "field info", DMFILEID_INVALID );
	pEditorType->AddAttributeInfo( "child", pInfo ); 
	pInfo->m_Widget = "particle_picker";
	m_hTypeDictionary->AddEditorType( pEditorType );
*/
}

void CDmeParticleChild::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets the particle system child
//-----------------------------------------------------------------------------
void CDmeParticleChild::SetChildParticleSystem( CDmeParticleSystemDefinition *pDef, IParticleOperatorDefinition *pDefinition )
{
	// FIXME: Convert system name into a 
	m_Child = pDef;
	const DmxElementUnpackStructure_t *pUnpack = pDefinition->GetUnpackStructure();
	UpdateAttributes( pUnpack );
}

const char *CDmeParticleChild::GetFunctionType() const
{
	const CDmeParticleSystemDefinition *pChild = m_Child;
	return pChild ? pChild->GetName() : "";
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY_INSTALL_EXPLICITLY( DmeParticleSystemDefinition, CDmeParticleSystemDefinition );



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeParticleSystemDefinition::OnConstruction()
{
	m_ParticleFunction[FUNCTION_RENDERER].Init( this, "renderers" );
	m_ParticleFunction[FUNCTION_OPERATOR].Init( this, "operators" );
	m_ParticleFunction[FUNCTION_INITIALIZER].Init( this, "initializers" );
	m_ParticleFunction[FUNCTION_EMITTER].Init( this, "emitters" );
	m_ParticleFunction[FUNCTION_CHILDREN].Init( this, "children" );
	m_ParticleFunction[FUNCTION_FORCEGENERATOR].Init( this, "forces" );
	m_ParticleFunction[FUNCTION_CONSTRAINT].Init( this, "constraints" );
	m_bPreventNameBasedLookup.Init( this, "preventNameBasedLookup" );

	m_hTypeDictionary = CreateElement< CDmeEditorTypeDictionary >( "particleSystemDefinitionDict", DMFILEID_INVALID );
	CDmeEditorType *pEditorType = CreateElement< CDmeEditorType >( "DmeParticleSystemDefinition", DMFILEID_INVALID );

	const DmxElementUnpackStructure_t *pUnpack = g_pParticleSystemMgr->GetParticleSystemDefinitionUnpackStructure();
	for ( ; pUnpack->m_pAttributeName; ++pUnpack )
	{
		CreateEditorAttributeInfo( pEditorType, pUnpack->m_pAttributeName, (const char *)pUnpack->m_pUserData );

		CDmAttribute *pAttribute = AddAttribute( pUnpack->m_pAttributeName, pUnpack->m_AttributeType );
		if ( pUnpack->m_pDefaultString )
		{
			int nLen = MAX( 1, Q_strlen( pUnpack->m_pDefaultString ) );
			CUtlBuffer bufParse( pUnpack->m_pDefaultString, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
			pAttribute->Unserialize( bufParse );
		}
	}

	m_hTypeDictionary->AddEditorType( pEditorType );
}

void CDmeParticleSystemDefinition::OnDestruction()
{
	DestroyElement( m_hTypeDictionary, TD_DEEP );
}


//-----------------------------------------------------------------------------
// Returns the editor type dictionary
//-----------------------------------------------------------------------------
CDmeEditorTypeDictionary* CDmeParticleSystemDefinition::GetEditorTypeDictionary()
{
	return m_hTypeDictionary;
}


//-----------------------------------------------------------------------------
// Remove obsolete attributes
//-----------------------------------------------------------------------------
static void RemoveObsoleteAttributes( CDmElement *pElement, const DmxElementUnpackStructure_t *pUnpack )
{
	// Delete all obsolete attributes
	CDmAttribute *pNext;
	for( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pNext )
	{
		pNext = pAttr->NextAttribute();
		if ( pAttr->IsStandard() || pAttr->IsFlagSet( FATTRIB_EXTERNAL ) )
			continue;

		bool bFound = false;
		for ( const DmxElementUnpackStructure_t *pTrav = pUnpack; pTrav->m_pAttributeName; ++pTrav )
		{
			if ( !Q_stricmp( pTrav->m_pAttributeName, pAttr->GetName() ) )
			{
				bFound = true;
				break;
			}
		}

		if ( !bFound )
		{
			pElement->RemoveAttributeByPtr( pAttr );
		}
	}
}

// Remove all attributes from the element that are either:
// A) Not in the unpack structure (eg. stale)
// B) Equal to their default value
static void CompactElement( CDmElement *pElement, const DmxElementUnpackStructure_t *pUnpack )
{
	CDmAttribute *pNext;
	for( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pNext )
	{
		pNext = pAttr->NextAttribute();
		if ( pAttr->IsStandard() || pAttr->IsFlagSet( FATTRIB_EXTERNAL ) )
			continue;

		const DmxElementUnpackStructure_t *pFoundUnpack = NULL;
		for ( const DmxElementUnpackStructure_t *pTrav = pUnpack; pTrav->m_pAttributeName; ++pTrav )
		{
			if ( !Q_stricmp( pTrav->m_pAttributeName, pAttr->GetName() ) )
			{
				pFoundUnpack = pTrav;
				break;
			}
		}

		if ( !pFoundUnpack )
		{
			// wasn't found in the unpack - attribute is stale
			pElement->RemoveAttributeByPtr( pAttr );
		}
		else if ( pFoundUnpack && pFoundUnpack->m_pDefaultString )
		{
			int nLen = Q_strlen( pFoundUnpack->m_pDefaultString );
			CUtlBuffer bufParse( pFoundUnpack->m_pDefaultString, nLen, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );

			if ( pAttr->IsIdenticalToSerializedValue( bufParse ) )
			{
				// equal to the default value - safe to remove
				pElement->RemoveAttributeByPtr( pAttr );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Used for automatic handling of backward compatability
//-----------------------------------------------------------------------------
void CDmeParticleSystemDefinition::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	RemoveObsoleteAttributes( this, g_pParticleSystemMgr->GetParticleSystemDefinitionUnpackStructure() );

	// Add missing fields that are new
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		ParticleFunctionType_t type = (ParticleFunctionType_t)i;
		CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( type );
		int nAvailType = list.Count();
		int nCount = GetParticleFunctionCount( type );
		for ( int j = 0; j < nCount; ++j )
		{
			CDmeParticleFunction *pFunction = GetParticleFunction( type, j );

			if ( i == FUNCTION_CHILDREN )
			{
				RemoveObsoleteAttributes( pFunction, list[0]->GetUnpackStructure() );
				pFunction->AddMissingFields( list[0]->GetUnpackStructure() );
				continue;
			}

			for ( int k = 0; k < nAvailType; ++k )
			{
				if ( Q_stricmp( pFunction->GetName(), list[k]->GetName() ) ) 
					continue;

				RemoveObsoleteAttributes( pFunction, list[k]->GetUnpackStructure() );
				pFunction->AddMissingFields( list[k]->GetUnpackStructure() );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Check to see if any attributes changed
//-----------------------------------------------------------------------------
void CDmeParticleSystemDefinition::Resolve()
{
	BaseClass::Resolve();
	for( CDmAttribute* pAttr = FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
	{
		if ( pAttr->IsFlagSet( FATTRIB_DIRTY ) )
		{
			RecompileParticleSystem();
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Add, remove
//-----------------------------------------------------------------------------
CDmeParticleFunction* CDmeParticleSystemDefinition::AddOperator( ParticleFunctionType_t type, const char *pFunctionName )
{
	CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( type );

	int nCount = list.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( Q_stricmp( pFunctionName, list[i]->GetName() ) ) 
			continue;

		CDmeParticleOperator *pFunction = CreateElement< CDmeParticleOperator >( pFunctionName, GetFileId() );
		m_ParticleFunction[type].AddToTail( pFunction );
		pFunction->SetFunction( list[i] );
		return pFunction;
	}
	return NULL;
}

void CDmeParticleSystemDefinition::OverrideAttributesFromOtherDefinition( CDmeParticleSystemDefinition *pDef )
{
	for ( const CDmAttribute *pAttr = pDef->FirstAttribute(); pAttr != NULL; pAttr = pAttr->NextAttribute() )
	{
		DmAttributeType_t type = pAttr->GetType();
		const char *pAttrName = pAttr->GetName();
		CDmAttribute *pCopyAttr = GetAttribute( pAttrName );

		if ( !V_stricmp( pAttrName, "name" ) )
			continue;

		if ( pCopyAttr == NULL )
		{
			pCopyAttr = AddAttribute( pAttrName, type );

			int flags = pAttr->GetFlags();
			Assert( ( flags & FATTRIB_EXTERNAL ) == 0 );
			flags &= ~FATTRIB_EXTERNAL;

			pCopyAttr->ClearFlags();
			pCopyAttr->AddFlag( flags );
		}

		// Temporarily remove the read-only flag from the copy while we copy into it
		bool bReadOnly = pCopyAttr->IsFlagSet( FATTRIB_READONLY );
		if ( bReadOnly )
		{
			pCopyAttr->RemoveFlag( FATTRIB_READONLY );
		}

		if ( type == AT_ELEMENT )
		{
			// nothing.
		}
		else if ( type == AT_ELEMENT_ARRAY )
		{
			// nothing.
		}
		else
		{
			pCopyAttr->SetValue( pAttr );
		}

		if ( bReadOnly )
		{
			pCopyAttr->AddFlag( FATTRIB_READONLY );
		}

	}
}

CDmeParticleFunction* CDmeParticleSystemDefinition::AddCopyOfOperator( CDmeParticleFunction *pFunc )
{
	for ( int nType = 0; nType < PARTICLE_FUNCTION_COUNT; ++nType )
	{
		CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( (ParticleFunctionType_t)nType );

		int nCount = list.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( nType == FUNCTION_CHILDREN )
			{
				if ( !pFunc->IsA<CDmeParticleChild>() )
					continue;
			}
			else
			{
				if ( Q_stricmp( pFunc->GetFunctionType(), list[i]->GetName() ) ) 
					continue;
			}

			CDmeParticleFunction *pCopy = pFunc->Copy(TD_SHALLOW);
			pCopy->SetFileId( GetFileId(), TD_SHALLOW );
			pCopy->InstanceTypeDictionary();
			m_ParticleFunction[nType].AddToTail( pCopy );

			return pCopy;
		}
	}

	return NULL;
}


CDmeParticleFunction* CDmeParticleSystemDefinition::AddChild( CDmeParticleSystemDefinition *pChild )
{
	Assert( pChild );

	CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( FUNCTION_CHILDREN );
	Assert( list.Count() == 1 );
	CDmeParticleChild *pFunction = CreateElement< CDmeParticleChild >( pChild->GetName(), GetFileId() );
	m_ParticleFunction[FUNCTION_CHILDREN].AddToTail( pFunction );
	pFunction->SetChildParticleSystem( pChild, list[0] );
	return pFunction;
}

void CDmeParticleSystemDefinition::RemoveFunction( ParticleFunctionType_t type, CDmeParticleFunction *pFunction )
{
	int nIndex = FindFunction( type, pFunction );
	if ( nIndex >= 0 )
	{
		m_ParticleFunction[type].Remove(nIndex);
	}
}


//-----------------------------------------------------------------------------
// Find
//-----------------------------------------------------------------------------
int CDmeParticleSystemDefinition::FindFunction( ParticleFunctionType_t type, CDmeParticleFunction *pParticleFunction )
{
	int nCount = m_ParticleFunction[type].Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pParticleFunction == m_ParticleFunction[type][i] )
			return i;
	}
	return -1;
}

int CDmeParticleSystemDefinition::FindFunction( ParticleFunctionType_t type, const char *pFunctionName )
{
	int nCount = m_ParticleFunction[type].Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pFunctionName, m_ParticleFunction[type][i]->GetFunctionType() ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Iteration
//-----------------------------------------------------------------------------
int CDmeParticleSystemDefinition::GetParticleFunctionCount( ParticleFunctionType_t type ) const
{
	return m_ParticleFunction[type].Count();
}

CDmeParticleFunction *CDmeParticleSystemDefinition::GetParticleFunction( ParticleFunctionType_t type, int nIndex )
{
	return m_ParticleFunction[type][nIndex];
}


//-----------------------------------------------------------------------------
// Reordering
//-----------------------------------------------------------------------------
void CDmeParticleSystemDefinition::MoveFunctionUp( ParticleFunctionType_t type, CDmeParticleFunction *pElement )
{
	int nIndex = FindFunction( type, pElement );
	if ( nIndex > 0 )
	{
		m_ParticleFunction[type].Swap( nIndex, nIndex - 1 );
	}
}

void CDmeParticleSystemDefinition::MoveFunctionDown( ParticleFunctionType_t type, CDmeParticleFunction *pElement )
{
	int nIndex = FindFunction( type, pElement );
	int nLastIndex = m_ParticleFunction[type].Count() - 1;
	if ( nIndex >= 0 && nIndex < nLastIndex )
	{
		m_ParticleFunction[type].Swap( nIndex, nIndex + 1 );
	}
}


//-----------------------------------------------------------------------------
// Marks a particle system as a new instance
// This is basically a workaround to prevent newly-copied particle functions
// from recompiling themselves a zillion times
//-----------------------------------------------------------------------------
void CDmeParticleSystemDefinition::MarkNewInstance()
{
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		int nCount = m_ParticleFunction[i].Count();
		for ( int j = 0; j < nCount; ++j )
		{
			m_ParticleFunction[i][j]->MarkNewInstance();
		}
	}
}


//-----------------------------------------------------------------------------
// Recompiles the particle system when a change occurs
//-----------------------------------------------------------------------------
void CDmeParticleSystemDefinition::RecompileParticleSystem()
{
	const char *pFileFormat = "pcf";
	const char *pEncoding = g_pDataModel->GetDefaultEncoding( pFileFormat );
	int nFlags = g_pDataModel->IsEncodingBinary( pEncoding ) ? 0 : CUtlBuffer::TEXT_BUFFER;
	CUtlBuffer buf( 0, 0, nFlags );
	if ( g_pDataModel->Serialize( buf, pEncoding, pFileFormat, GetHandle() ) )
	{
		g_pParticleSystemMgr->ReadParticleConfigFile( buf, true );
	}
}


void CDmeParticleSystemDefinition::RemoveInvalidFunctions()
{
	for ( int f = 0; f < PARTICLE_FUNCTION_COUNT; ++f )
	{
		for ( int i = 0; i < m_ParticleFunction[f].Count(); )
		{
			CDmeParticleFunction* pFunc = m_ParticleFunction[f].Element(i);
			if ( pFunc == NULL )
			{
				m_ParticleFunction[f].Remove(i);
			}
			else
			{
				++i;
			}
		}
	}
}

void CDmeParticleSystemDefinition::Compact()
{
	// Traverse the entire definition and purge all the DM elements

	CompactElement( this, g_pParticleSystemMgr->GetParticleSystemDefinitionUnpackStructure() );

	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		ParticleFunctionType_t type = (ParticleFunctionType_t)i;
		CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( type );
		int nAvailType = list.Count();
		int nCount = GetParticleFunctionCount( type );
		for ( int j = 0; j < nCount; ++j )
		{
			CDmeParticleFunction *pFunction = GetParticleFunction( type, j );

			if ( i == FUNCTION_CHILDREN )
			{
				CompactElement( pFunction, list[0]->GetUnpackStructure() );
				continue;
			}

			for ( int k = 0; k < nAvailType; ++k )
			{
				if ( Q_stricmp( pFunction->GetName(), list[k]->GetName() ) ) 
					continue;

				CompactElement( pFunction, list[k]->GetUnpackStructure() );
				break;
			}
		}
	}
}
