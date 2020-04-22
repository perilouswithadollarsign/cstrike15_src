//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects/dmetransformcontrol.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmerighandle.h"
#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmerig.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"
#include "phonemeconverter.h"
#include "tier1/utlstringmap.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "studio.h"
#include "tier3/tier3.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CDmePreset - container for preset control values
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePreset, CDmePreset );

void CDmePreset::OnConstruction()
{
	m_ControlValues.Init( this, "controlValues" );
}

void CDmePreset::OnDestruction()
{
}

CDmaElementArray< CDmElement > &CDmePreset::GetControlValues()
{
	return m_ControlValues;
}

const CDmaElementArray< CDmElement > &CDmePreset::GetControlValues() const
{
	return m_ControlValues;
}

int CDmePreset::FindControlValueIndex( const char *pControlName )
{
	int c = m_ControlValues.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *e = m_ControlValues.Get( i );
		if ( !Q_stricmp( e->GetName(), pControlName ) )
			return i;
	}
	return -1;
}

CDmElement *CDmePreset::FindControlValue( const char *pControlName )
{
	int i = FindControlValueIndex( pControlName );
	if ( i >= 0 )
		return m_ControlValues.Get(i);
	return NULL;
}

CDmElement *CDmePreset::FindOrAddControlValue( const char *pControlName )
{
	CDmElement *pControlValues = FindControlValue( pControlName );
	if ( !pControlValues )
	{
		// Create the default groups in order
		pControlValues = CreateElement< CDmElement >( pControlName, GetFileId() );
		m_ControlValues.AddToTail( pControlValues );
	}
	return pControlValues;
}

void CDmePreset::RemoveControlValue( const char *pControlName )
{
	int i = FindControlValueIndex( pControlName );
	if ( i >= 0 )
	{
		m_ControlValues.Remove( i );
	}
}


//-----------------------------------------------------------------------------
// Is the preset read-only?
//-----------------------------------------------------------------------------
bool CDmePreset::IsReadOnly()
{
	DmAttributeReferenceIterator_t h = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
	while ( h != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( h );
		CDmePresetGroup *pOwner = CastElement<CDmePresetGroup>( pAttribute->GetOwner() );
		if ( pOwner && pOwner->m_bIsReadOnly )
			return true;
		h = g_pDataModel->NextAttributeReferencingElement( h );
	}
	return false;
}

bool CDmePreset::IsAnimated()
{
	return GetValue< bool >( "animated", false );
}

//-----------------------------------------------------------------------------
// Copies control values
//-----------------------------------------------------------------------------
void CDmePreset::CopyControlValuesFrom( CDmePreset *pSource )
{
	m_ControlValues.RemoveAll();

	const CDmaElementArray< CDmElement > &sourceValues = pSource->GetControlValues();
	int nCount = sourceValues.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pCopy = sourceValues[i]->Copy( );
		m_ControlValues.AddToTail( pCopy );
	}
}


IMPLEMENT_ELEMENT_FACTORY( DmeProceduralPresetSettings, CDmeProceduralPresetSettings );

void CDmeProceduralPresetSettings::OnConstruction()
{
	m_flJitterScale.InitAndSet( this, "jitterscale", 1.0f );
	m_flSmoothScale.InitAndSet( this, "smoothscale", 1.0f );
	m_flSharpenScale.InitAndSet( this, "sharpenscale", 1.0f );
	m_flSoftenScale.InitAndSet( this, "softenscale", 1.0f );

	m_flJitterScaleVector.InitAndSet( this, "jitterscale_vector", 2.5f );
	m_flSmoothScaleVector.InitAndSet( this, "smoothscale_vector", 2.5f );
	m_flSharpenScaleVector.InitAndSet( this, "sharpenscale_vector", 2.5f );
	m_flSoftenScaleVector.InitAndSet( this, "softenscale_vector", 2.5f );

	m_nJitterIterations.InitAndSet( this, "jitteriterations", 5 );
	m_nSmoothIterations.InitAndSet( this, "smoothiterations", 5 );
	m_nSharpenIterations.InitAndSet( this, "sharpeniterations", 1 );
	m_nSoftenIterations.InitAndSet( this, "softeniterations", 1 );

	// 1/12 second now ( 833 ten thousandths )
	m_staggerInterval.InitAndSet( this, "staggerinterval", DmeTime_t( 1.0f / 12.0f ) );
}

void CDmeProceduralPresetSettings::OnDestruction()
{
}

float CDmeProceduralPresetSettings::GetJitterScale( DmAttributeType_t attType ) const
{
	if ( attType == AT_VECTOR3 )
		return m_flJitterScaleVector;
	return m_flJitterScale;
}

float CDmeProceduralPresetSettings::GetSmoothScale( DmAttributeType_t attType ) const
{
	if ( attType == AT_VECTOR3 )
		return m_flSmoothScaleVector;
	return m_flSmoothScale;
}

float CDmeProceduralPresetSettings::GetSharpenScale( DmAttributeType_t attType ) const
{
	if ( attType == AT_VECTOR3 )
		return m_flSharpenScaleVector;
	return m_flSharpenScale;
}

float CDmeProceduralPresetSettings::GetSoftenScale( DmAttributeType_t attType ) const
{
	if ( attType == AT_VECTOR3 )
		return m_flSoftenScaleVector;
	return m_flSoftenScale;
}


//-----------------------------------------------------------------------------
// CDmePresetGroup - container for animation set info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePresetGroup, CDmePresetGroup );

void CDmePresetGroup::OnConstruction()
{
	m_Presets.Init( this, "presets" );
	m_bIsVisible.InitAndSet( this, "visible", true );
	m_bIsReadOnly.Init( this, "readonly" );
}

void CDmePresetGroup::OnDestruction()
{
}

CDmaElementArray< CDmePreset > &CDmePresetGroup::GetPresets()
{
	return m_Presets;
}

const CDmaElementArray< CDmePreset > &CDmePresetGroup::GetPresets() const
{
	return m_Presets;
}

//-----------------------------------------------------------------------------
// Finds the index of a particular preset group
//-----------------------------------------------------------------------------
int CDmePresetGroup::FindPresetIndex( CDmePreset *pPreset )
{
	int c = m_Presets.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePreset *e = m_Presets.Get( i );
		if ( pPreset == e )
			return i;
	}
	return -1; 
}

int CDmePresetGroup::FindPresetIndex( const char *pPresetName )
{
	int c = m_Presets.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePreset *e = m_Presets.Get( i );
		if ( !Q_stricmp( e->GetName(), pPresetName ) )
			return i;
	}
	return -1; 
}


CDmePreset *CDmePresetGroup::FindPreset( const char *pPresetName )
{
	int i;
	int c = m_Presets.Count();
	for ( i = 0; i < c; ++i )
	{
		CDmePreset *e = m_Presets.Get( i );
		if ( !Q_stricmp( e->GetName(), pPresetName ) )
			return e;
	}
	return NULL;
}

CDmePreset *CDmePresetGroup::FindOrAddPreset( const char *pPresetName )
{
	CDmePreset *pPreset = FindPreset( pPresetName );
	if ( !pPreset )
	{
		// Create the default groups in order
		pPreset = CreateElement< CDmePreset >( pPresetName, GetFileId() );
		m_Presets.AddToTail( pPreset );
	}
	return pPreset;
}

bool CDmePresetGroup::RemovePreset( CDmePreset *pPreset )
{
	int i = FindPresetIndex( pPreset );
	if ( i >= 0 )
	{
		m_Presets.Remove( i );
		return true;
	}
	return false;
}

bool CDmePresetGroup::RemovePreset( const char *pPresetName )
{
	int i = FindPresetIndex( pPresetName );
	if ( i >= 0 )
	{
		m_Presets.Remove( i );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Reorder presets
//-----------------------------------------------------------------------------
void CDmePresetGroup::MovePresetInFrontOf( CDmePreset *pPreset, CDmePreset *pInFrontOf )
{
	if ( pPreset == pInFrontOf )
		return;

	int nEnd = pInFrontOf ? FindPresetIndex( pInFrontOf ) : m_Presets.Count();
	Assert( nEnd >= 0 );
	 
	RemovePreset( pPreset );
	if ( nEnd > m_Presets.Count() )
	{
		nEnd = m_Presets.Count();
	}
	m_Presets.InsertBefore( nEnd, pPreset );
}


//-----------------------------------------------------------------------------
// Finds a control index
//-----------------------------------------------------------------------------
struct ExportedControl_t
{
	CUtlString m_Name;
	bool m_bIsStereo;
	int m_nFirstIndex;
};


//-----------------------------------------------------------------------------
// Builds a unique list of controls found in the presets
//-----------------------------------------------------------------------------
static int FindExportedControlIndex( const char *pControlName, CUtlVector< ExportedControl_t > &uniqueControls )
{
	int nCount = uniqueControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pControlName, uniqueControls[i].m_Name ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Builds a unique list of controls found in the presets
//-----------------------------------------------------------------------------
static int BuildExportedControlList( CDmeAnimationSet *pAnimationSet, const CDmePresetGroup *pPresetGroup, CUtlVector< ExportedControl_t > &uniqueControls )
{
	int nGlobalIndex = 0;
	const CDmrElementArrayConst< CDmePreset > &presets = pPresetGroup->GetPresets();
	int nPresetCount = presets.Count();
	for ( int i = 0; i < nPresetCount; ++i )
	{
		CDmePreset *pPreset = presets[i];
		Assert( !pPreset->IsAnimated() ); // deal with this after GDC
		if ( pPreset->IsAnimated() )
			continue;

		const CDmrElementArray< CDmElement > &controls = pPreset->GetControlValues();

		int nControlCount = controls.Count();
		for ( int i = 0; i < nControlCount; ++i )
		{
			CDmElement *pControlValue = controls[ i ];
			if ( !pControlValue )
				continue;

			const char *pControlName = pControlValue->GetName();
			int nIndex = FindExportedControlIndex( pControlName, uniqueControls );
			if ( nIndex >= 0 )
				continue;

			CDmAttribute *pValueAttribute = pControlValue->GetAttribute( "value" );
			if ( !pValueAttribute || pValueAttribute->GetType() != AT_FLOAT )
				continue;

			if ( pAnimationSet )
			{
				CDmElement *pControl = pAnimationSet->FindControl( pControlName );
				if ( !pControl )
					continue;

				int j = uniqueControls.AddToTail();
				ExportedControl_t &control = uniqueControls[j];
				control.m_Name = pControlName;
				control.m_bIsStereo = IsStereoControl( pControl );
				control.m_nFirstIndex = nGlobalIndex;
				nGlobalIndex += 1 + control.m_bIsStereo;
			}
			else
			{
				int j = uniqueControls.AddToTail();
				ExportedControl_t &control = uniqueControls[j];
				control.m_Name = pControlName;
				// this isn't quite as reliable as querying the animation set but if we don't have one...
				control.m_bIsStereo = pControlValue->HasAttribute( "leftValue" );
				control.m_nFirstIndex = nGlobalIndex;
				nGlobalIndex += 1 + control.m_bIsStereo;
			}
		}
	}
	return nGlobalIndex;
}


//-----------------------------------------------------------------------------
// Exports this preset group to a faceposer .txt expression file
// Either an animation set or a combination operator are required so that
// the default value for unspecified 
//-----------------------------------------------------------------------------
bool CDmePresetGroup::ExportToTXT( const char *pFileName, CDmeAnimationSet *pAnimationSet /* = NULL */, CDmeCombinationOperator *pComboOp /* = NULL */ ) const
{
	const CDmePresetGroup *pPresetGroup = this;

	// find all used controls
	CUtlVector< ExportedControl_t > exportedControls;
	BuildExportedControlList( pAnimationSet, pPresetGroup, exportedControls );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// Output the unique keys
	buf.Printf( "$keys " );
	int nExportedControlCount = exportedControls.Count();
	for ( int i = 0; i < nExportedControlCount; ++i )
	{
		char pTempBuf[MAX_PATH];

		ExportedControl_t &control = exportedControls[i];
		if ( !control.m_bIsStereo )
		{
			buf.Printf("%s ", control.m_Name.String() );
		}
		else
		{
			V_sprintf_safe( pTempBuf, "right_%s", control.m_Name.String() );
			buf.Printf("%s ", pTempBuf );
			V_sprintf_safe( pTempBuf, "left_%s", control.m_Name.String() );
			buf.Printf("%s ", pTempBuf );
		}
	}
	buf.Printf( "\n" );
	buf.Printf( "$hasweighting\n" );
	buf.Printf( "$normalized\n" );

	// Output all presets
	const CDmrElementArrayConst< CDmePreset > &presets = pPresetGroup->GetPresets();
	int nPresetCount = presets.Count();
	for ( int i = 0; i < nPresetCount; ++i )
	{
		CDmePreset *pPreset = presets[i];
		Assert( !pPreset->IsAnimated() ); // deal with this after GDC
		if ( pPreset->IsAnimated() )
			continue;

		const char *pPresetName = pPreset->GetName();

		// Hack for 'silence' and for p_ naming scheme
		if ( !Q_stricmp( pPresetName, "p_silence" ) )
		{
			pPresetName = "<sil>";
		}
		if ( pPresetName[0] == 'p' && pPresetName[1] == '_' )
		{
			pPresetName = &pPresetName[2];
		}

		buf.Printf( "\"%s\" \t", pPresetName );

		int nPhonemeIndex = TextToPhonemeIndex( pPresetName );
		int nCode = CodeForPhonemeByIndex( nPhonemeIndex );
		if ( nCode < 128 )
		{
			buf.Printf( "\"%c\" \t", nCode );
		}
		else
		{
			buf.Printf( "\"0x%x\"\t", nCode );
		}

		for ( int i = 0; i < nExportedControlCount; ++i )
		{
			ExportedControl_t &control = exportedControls[i];
			CDmElement *pControlValue = pPreset->FindControlValue( control.m_Name );
			if ( !pControlValue )
			{
				CDmElement *pControl = pAnimationSet ? pAnimationSet->FindControl( control.m_Name ) : NULL;
				if ( !pControl )
				{
					bool bIsMulti;
					const ControlIndex_t nIndex = FindComboOpControlIndexForAnimSetControl( pComboOp, control.m_Name, &bIsMulti );
					if ( nIndex >= 0 )
					{
						float flDefaultValue = bIsMulti ? 0.5f : pComboOp->GetControlDefaultValue( nIndex );
						buf.Printf( "%.5f\t0.000\t", flDefaultValue );
						if ( control.m_bIsStereo )
						{
							buf.Printf( "%.5f\t0.000\t", flDefaultValue );
						}
					}
					else
					{
						buf.Printf( "0.000\t0.000\t" );
						if ( control.m_bIsStereo )
						{
							buf.Printf( "0.000\t0.000\t" );
						}
					}

					continue;
				}

				float flDefaultValue = pControl->GetValue< float >( "defaultValue" );
				if ( !control.m_bIsStereo )
				{
					buf.Printf( "%.5f\t1.000\t", flDefaultValue );
				}
				else
				{
					buf.Printf( "%.5f\t1.000\t", flDefaultValue );
					buf.Printf( "%.5f\t1.000\t", flDefaultValue );
				}

				continue;
			}

			if ( !control.m_bIsStereo )
			{
				buf.Printf( "%.5f\t1.000\t", pControlValue->GetValue<float>( "value" ) );
			}
			else
			{
				buf.Printf( "%.5f\t1.000\t", pControlValue->GetValue<float>( "rightValue" ) );
				buf.Printf( "%.5f\t1.000\t", pControlValue->GetValue<float>( "leftValue" ) );
			}

		}
		const char *pDesc = DescForPhonemeByIndex( nPhonemeIndex );
		buf.Printf( "\"%s\"\n", pDesc ? pDesc : pPresetName );
	}

	return g_pFullFileSystem->WriteFile( pFileName, NULL, buf );
}


#undef ALIGN4
#define ALIGN4( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)


//-----------------------------------------------------------------------------
// Exports this preset group to a faceposer .vfe expression file
//-----------------------------------------------------------------------------
bool CDmePresetGroup::ExportToVFE( const char *pFileName, CDmeAnimationSet *pAnimationSet /* = NULL */, CDmeCombinationOperator *pComboOp /* = NULL */ ) const
{	  
	const CDmePresetGroup *pPresetGroup = this;

	int i;
	const CDmrElementArrayConst< CDmePreset > &presets = pPresetGroup->GetPresets();

	// find all used controls
	CUtlVector< ExportedControl_t > exportedControls;
	int nTotalControlCount = BuildExportedControlList( pAnimationSet, pPresetGroup, exportedControls );
	const int nExportedControlCount = exportedControls.Count();

	byte *pData = (byte *)calloc( 1024 * 1024, 1 );
	byte *pDataStart = pData;

	flexsettinghdr_t *fhdr = (flexsettinghdr_t *)pData;

	fhdr->id = ('V' << 16) + ('F' << 8) + ('E');
	fhdr->version = 0;
	if ( !g_pFullFileSystem->FullPathToRelativePathEx( pFileName, "GAME", fhdr->name, sizeof(fhdr->name) ) )
	{
		Q_strncpy( fhdr->name, pFileName, sizeof(fhdr->name) );
	}

	// allocate room for header
	pData += sizeof( flexsettinghdr_t );
	ALIGN4( pData );

	// store flex settings
	flexsetting_t *pSetting = (flexsetting_t *)pData;
	fhdr->numflexsettings = presets.Count();
	fhdr->flexsettingindex = pData - pDataStart;
	pData += sizeof( flexsetting_t ) * fhdr->numflexsettings;
	ALIGN4( pData );
	for ( i = 0; i < fhdr->numflexsettings; i++ )
	{
		CDmePreset *pPreset = presets[i];
		Assert( pPreset );
		Assert( !pPreset->IsAnimated() ); // deal with this after GDC
		if ( pPreset->IsAnimated() )
			continue;

		pSetting[i].index = i;
		pSetting[i].settingindex = pData - (byte *)(&pSetting[i]);

		flexweight_t *pFlexWeights = (flexweight_t *)pData;

		for ( int j = 0; j < nExportedControlCount; j++ )
		{
			ExportedControl_t &control = exportedControls[ j ];
			CDmElement *pControlValue = pPreset->FindControlValue( control.m_Name );
			if ( !pControlValue )
			{
				bool bIsMulti;
				const ControlIndex_t nIndex = FindComboOpControlIndexForAnimSetControl( pComboOp, control.m_Name, &bIsMulti );
				if ( nIndex >= 0 )
				{
					float flDefaultValue = bIsMulti ? 0.5f : pComboOp->GetControlDefaultValue( nIndex );
					if ( !control.m_bIsStereo )
					{
						pSetting[i].numsettings++;
						pFlexWeights->key = control.m_nFirstIndex;
						pFlexWeights->weight = flDefaultValue;
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
					}
					else
					{
						pSetting[i].numsettings += 2;
						pFlexWeights->key = control.m_nFirstIndex;
						pFlexWeights->weight = flDefaultValue;
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
						pFlexWeights->key = control.m_nFirstIndex + 1;
						pFlexWeights->weight = flDefaultValue;
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
					}
				}
				else
				{
					pSetting[i].numsettings++;
					pFlexWeights->key = control.m_nFirstIndex;
					pFlexWeights->weight = bIsMulti ? 0.5f : 0.0f;
					pFlexWeights->influence = 0.0f;
					pFlexWeights++;

					if ( control.m_bIsStereo )
					{
						pSetting[i].numsettings++;
						pFlexWeights->key = control.m_nFirstIndex + 1;
						pFlexWeights->weight = 0.0f;
						pFlexWeights->influence = 0.0f;
						pFlexWeights++;
					}
				}

				continue;
			}

			if ( !control.m_bIsStereo )
			{
				pSetting[i].numsettings++;
				pFlexWeights->key = control.m_nFirstIndex;
				pFlexWeights->weight = pControlValue->GetValue<float>( "value" );
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
			}
			else
			{
				pSetting[i].numsettings += 2;
				pFlexWeights->key = control.m_nFirstIndex;
				pFlexWeights->weight = pControlValue->GetValue<float>( "rightValue" );
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
				pFlexWeights->key = control.m_nFirstIndex + 1;
				pFlexWeights->weight = pControlValue->GetValue<float>( "leftValue" );
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
			}
		}

		pData = (byte *)pFlexWeights;
		ALIGN4( pData );
	}

	int numindexes = 1;
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		if ( pSetting[i].index >= numindexes )
		{
			numindexes = pSetting[i].index + 1;
		}
	}

	// store indexed table
	int *pIndex = (int *)pData;
	fhdr->numindexes = numindexes;
	fhdr->indexindex = pData - pDataStart;
	pData += sizeof( int ) * numindexes;
	ALIGN4( pData );
	for (i = 0; i < numindexes; i++)
	{
		pIndex[i] = -1;
	}
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		pIndex[pSetting[i].index] = i;
	}

	// store flex setting names
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		CDmePreset *pPreset = presets[i];
		const char *pPresetName = pPreset->GetName();

		// Hack for 'silence' and for p_ naming scheme
		if ( pPresetName[0] == 'p' && pPresetName[1] == '_' )
		{
			pPresetName = &pPresetName[2];
		}
		if ( !Q_stricmp( pPresetName, "silence" ) )
		{
			pPresetName = "<sil>";
		}

		pSetting[i].nameindex = pData - (byte *)(&pSetting[i]);
		strcpy( (char *)pData, pPresetName );
		pData += Q_strlen( pPresetName ) + 1;
	}
	ALIGN4( pData );

	// store key names
	char **pKeynames = (char **)pData;
	fhdr->numkeys = nTotalControlCount;
	fhdr->keynameindex = pData - pDataStart;
	pData += sizeof(char *) * nTotalControlCount;
	int j = 0;
	for ( i = 0; i < nExportedControlCount; ++i )
	{
		char pTempBuf[MAX_PATH];

		ExportedControl_t &control = exportedControls[i];
		if ( !control.m_bIsStereo )
		{
			pKeynames[j++] = (char *)(pData - pDataStart);
			strcpy( (char *)pData, control.m_Name );
			pData += Q_strlen( control.m_Name ) + 1;
		}
		else
		{
			pKeynames[j++] = (char *)(pData - pDataStart);
			V_sprintf_safe( pTempBuf, "right_%s", control.m_Name.String() );
			strcpy( (char *)pData, pTempBuf );
			pData += Q_strlen( pTempBuf ) + 1;

			pKeynames[j++] = (char *)(pData - pDataStart);
			V_sprintf_safe( pTempBuf, "left_%s", control.m_Name.String() );
			strcpy( (char *)pData, pTempBuf );
			pData += Q_strlen( pTempBuf ) + 1;
		}
	}
	Assert( j == nTotalControlCount );
	ALIGN4( pData );

	// allocate room for remapping
	int *keymapping = (int *)pData;
	fhdr->keymappingindex = pData - pDataStart;
	pData += sizeof( int ) * nTotalControlCount;
	for (i = 0; i < nTotalControlCount; i++)
	{
		keymapping[i] = -1;
	}
	ALIGN4( pData );

	fhdr->length = pData - pDataStart;

	FileHandle_t fh = g_pFullFileSystem->Open( pFileName, "wb" );
	if ( !fh )
	{
		Warning( "Unable to write to %s (read-only?)\n", pFileName );
		free( pDataStart );
		return false;
	}

	g_pFullFileSystem->Write( pDataStart, fhdr->length, fh );
	g_pFullFileSystem->Close( fh );
	free( pDataStart );
	return true;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// CDmeAnimationSet - container for animation set info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAnimationSet, CDmeAnimationSet );

void CDmeAnimationSet::OnConstruction()
{
	m_Controls.Init( this, "controls", FATTRIB_HAS_CALLBACK );
	m_PresetGroups.Init( this, "presetGroups" );
	m_PhonemeMap.Init( this, "phonememap" );
	m_Operators.Init( this, "operators" );
	m_RootControlGroup.InitAndCreate( this, "rootControlGroup" );
}

void CDmeAnimationSet::OnDestruction()
{
	m_ControlNameMap.RemoveAll();
}

void CDmeAnimationSet::OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem )
{
	if ( pAttribute != m_Controls.GetAttribute() )
		return;

	for ( int i = nFirstElem; i <= nLastElem; ++i )
	{
		CDmElement *pControl = m_Controls[ i ];
		if ( !pControl )
			continue;

		int slot = m_ControlNameMap.Find( pControl->GetName() );
		if ( slot == m_ControlNameMap.InvalidIndex() )
		{
			m_ControlNameMap.Insert( pControl->GetName(), pControl->GetHandle() );
		}
	}
}

void CDmeAnimationSet::OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem )
{
	if ( pAttribute != m_Controls.GetAttribute() )
		return;

	for ( int i = nFirstElem; i <= nLastElem; ++i )
	{
		CDmElement *pControl = m_Controls[ i ];
		if ( !pControl )
			continue;

		m_ControlNameMap.Remove( pControl->GetName() );
	}
}

CDmaElementArray< CDmElement > &CDmeAnimationSet::GetControls()
{
	return m_Controls;
}

CDmaElementArray< CDmePresetGroup > &CDmeAnimationSet::GetPresetGroups()
{
	return m_PresetGroups;
}

CDmaElementArray< CDmeOperator > &CDmeAnimationSet::GetOperators()
{
	return m_Operators;
}

void CDmeAnimationSet::AddOperator( CDmeOperator *pOperator )
{
	m_Operators.AddToTail( pOperator );
}

void CDmeAnimationSet::RemoveOperator( CDmeOperator *pOperator )
{
	int nCount = m_Operators.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Operators[i] == pOperator )
		{
			m_Operators.Remove(i);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Finds the index of a particular preset group
//-----------------------------------------------------------------------------
void CDmeAnimationSet::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	// Build control dictionary
	for ( int i = 0; i < m_Controls.Count(); ++i )
	{
		CDmElement *pControl = m_Controls[ i ];
		if ( !pControl )
			return;

		int slot = m_ControlNameMap.Find( pControl->GetName() );
		if ( slot == m_ControlNameMap.InvalidIndex() )
		{
			m_ControlNameMap.Insert( pControl->GetName(), pControl->GetHandle() );
		}
	}
}


//-----------------------------------------------------------------------------
// Finds the index of a particular preset group
//-----------------------------------------------------------------------------
int CDmeAnimationSet::FindPresetGroupIndex( CDmePresetGroup *pPresetGroup )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePresetGroup *e = m_PresetGroups.Get( i );
		if ( pPresetGroup == e )
			return i;
	}
	return -1; 
}

int CDmeAnimationSet::FindPresetGroupIndex( const char *pGroupName )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePresetGroup *e = m_PresetGroups.Get( i );
		if ( e && !Q_stricmp( e->GetName(), pGroupName ) )
			return i;
	}
	return -1; 
}


//-----------------------------------------------------------------------------
// Find by name
//-----------------------------------------------------------------------------
CDmePresetGroup *CDmeAnimationSet::FindPresetGroup( const char *pGroupName )
{
	int nIndex = FindPresetGroupIndex( pGroupName );
	if ( nIndex >= 0 )
		return m_PresetGroups[nIndex];
	return NULL;
}


//-----------------------------------------------------------------------------
// Find or add by name
//-----------------------------------------------------------------------------
CDmePresetGroup *CDmeAnimationSet::FindOrAddPresetGroup( const char *pGroupName )
{
	CDmePresetGroup *pPresetGroup = FindPresetGroup( pGroupName );
	if ( !pPresetGroup )
	{
		// Create the default groups in order
		pPresetGroup = CreateElement< CDmePresetGroup >( pGroupName, GetFileId() );
		m_PresetGroups.AddToTail( pPresetGroup );
	}
	return pPresetGroup;
}

void CDmeAnimationSet::AddPresetGroup( CDmePresetGroup *pPresetGroup )
{
	m_PresetGroups.AddToTail( pPresetGroup );
}

//-----------------------------------------------------------------------------
// Remove preset group
//-----------------------------------------------------------------------------
bool CDmeAnimationSet::RemovePresetGroup( CDmePresetGroup *pPresetGroup )
{
	int i = FindPresetGroupIndex( pPresetGroup );
	if ( i >= 0 )
	{
		m_PresetGroups.Remove( i );
		return true;
	}
	return false;
}

bool CDmeAnimationSet::RemovePresetGroup( const char *pPresetGroupName )
{
	int i = FindPresetGroupIndex( pPresetGroupName );
	if ( i >= 0 )
	{
		m_PresetGroups.Remove( i );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Reorder preset groups
//-----------------------------------------------------------------------------
void CDmeAnimationSet::MovePresetGroupInFrontOf( CDmePresetGroup *pPresetGroup, CDmePresetGroup *pInFrontOf )
{
	if ( pPresetGroup == pInFrontOf )
		return;

#ifdef _DEBUG
	int nStart = FindPresetGroupIndex( pPresetGroup );
#endif

	int nEnd = pInFrontOf ? FindPresetGroupIndex( pInFrontOf ) : m_PresetGroups.Count();
	Assert( nStart >= 0 && nEnd >= 0 );

	RemovePresetGroup( pPresetGroup );
	if ( nEnd > m_PresetGroups.Count() )
	{
		nEnd = m_PresetGroups.Count();
	}
	m_PresetGroups.InsertBefore( nEnd, pPresetGroup );
}


CDmePreset *CDmeAnimationSet::FindOrAddPreset( const char *pGroupName, const char *pPresetName )
{
	CDmePresetGroup *pPresetGroup = FindOrAddPresetGroup( pGroupName );
	return pPresetGroup->FindOrAddPreset( pPresetName );
}

bool CDmeAnimationSet::RemovePreset( const char *pPresetName )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( m_PresetGroups[i]->RemovePreset( pPresetName ) )
			return true;
	}
	return false;
}

bool CDmeAnimationSet::RemovePreset( CDmePreset *pPreset )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( m_PresetGroups[i]->RemovePreset( pPreset ) )
			return true;
	}
	return false;
}

CDmaElementArray< CDmePhonemeMapping > &CDmeAnimationSet::GetPhonemeMap()
{
	return m_PhonemeMap;
}

void CDmeAnimationSet::RestoreDefaultPhonemeMap()
{
	CUndoScopeGuard guard( "RestoreDefaultPhonemeMap" );

	int i;
	int c = m_PhonemeMap.Count();
	for ( i = 0; i < c; ++i )
	{
		g_pDataModel->DestroyElement( m_PhonemeMap[ i ]->GetHandle() );
	}
	m_PhonemeMap.Purge();

	int phonemeCount = NumPhonemes();
	for ( i = 0; i < phonemeCount; ++i )
	{
		const char *pName = NameForPhonemeByIndex( i );
		CDmePhonemeMapping *mapping = CreateElement< CDmePhonemeMapping >( pName, GetFileId() );
		char presetName[ 256 ];
		Q_snprintf( presetName, sizeof( presetName ), "p_%s", pName );
		mapping->m_Preset = presetName;
		mapping->m_Weight = 1.0f;

		m_PhonemeMap.AddToTail( mapping );
	}
}

CDmePhonemeMapping *CDmeAnimationSet::FindMapping( const char *pRawPhoneme )
{
	int c = m_PhonemeMap.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePhonemeMapping *e = m_PhonemeMap.Get( i );
		Assert( e );
		if ( !e )
			continue;

		if ( !Q_stricmp( e->GetName(), pRawPhoneme ) )
			return e;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Add the specified control to the animation set's list of controls
//-----------------------------------------------------------------------------
void CDmeAnimationSet::AddControl( CDmElement *pControl )
{
	m_Controls.AddToTail( pControl );
	if ( pControl )
	{
		int slot = m_ControlNameMap.Find( pControl->GetName() );
		if ( slot == m_ControlNameMap.InvalidIndex() )
		{
			m_ControlNameMap.Insert( pControl->GetName(), pControl->GetHandle() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove the specified control from the animation set's list of 
// controls and remove any selection elements referring to the control.
//-----------------------------------------------------------------------------
void CDmeAnimationSet::RemoveControl( CDmElement *pControl )
{	

	// Search the for the control in the list and remove
	// the corresponding element from the list if found.
	int index = m_Controls.Find( pControl );
	if ( index != m_Controls.InvalidIndex() )
	{
		// Remove the control from any selection groups it may be in.
		RemoveControlFromGroups( pControl->GetName(), true );

		// Remove the control from the list
		m_Controls.Remove( index );

		// Remove from name mapping
		m_ControlNameMap.Remove( pControl->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Finds a control 
//-----------------------------------------------------------------------------
CDmElement *CDmeAnimationSet::FindControl( const char *pControlName ) const
{
	int idx = m_ControlNameMap.Find( pControlName );
	if ( idx == m_ControlNameMap.InvalidIndex() )
		return NULL;
	return g_pDataModel->GetElement( m_ControlNameMap[ idx ] );
}

//-----------------------------------------------------------------------------
// Finds or adds a control 
//-----------------------------------------------------------------------------
CDmElement *CDmeAnimationSet::FindOrAddControl( const char *pControlName, bool transformControl, bool bMustBeNew )
{
	CDmElement *pControl = FindControl( pControlName );

	if ( bMustBeNew && ( pControl != NULL ) )
		return NULL;

	if ( !pControl )
	{
		// If not, then create one
		if ( transformControl )
		{
			pControl = CreateElement< CDmeTransformControl >( pControlName, GetFileId() );
		}
		else
		{
			pControl = CreateElement< CDmElement >( pControlName, GetFileId() );
		}
		AddControl( pControl );		
	}

	return pControl;
}


//-----------------------------------------------------------------------------
// Create a new control with the specified name, if a control with the 
// specified name already exists returns NULL.
//-----------------------------------------------------------------------------
CDmElement *CDmeAnimationSet::CreateNewControl( const char *pControlName, bool bTransformControl )
{
	return FindOrAddControl( pControlName, bTransformControl, true );
}


//-----------------------------------------------------------------------------
// Purpose: Get the root control group
//-----------------------------------------------------------------------------
CDmeControlGroup *CDmeAnimationSet::GetRootControlGroup() const
{
	return m_RootControlGroup;
}


//-----------------------------------------------------------------------------
// Purpose: Find the control group with the specified name.
//-----------------------------------------------------------------------------
CDmeControlGroup *CDmeAnimationSet::FindControlGroup( const char *pControlGroupName ) const
{
	return m_RootControlGroup->FindChildByName( pControlGroupName, true );
}


//-----------------------------------------------------------------------------
// Purpose: Find the control group with the specified name or add it if does
// not exist.
//-----------------------------------------------------------------------------
CDmeControlGroup *CDmeAnimationSet::FindOrAddControlGroup( CDmeControlGroup *pParentGroup, const char *pControlGroupName )
{
	CDmeControlGroup *pRootGroup = ( pParentGroup != NULL ) ? pParentGroup : m_RootControlGroup;

	// Search for the group to see if it already exists.
	CDmeControlGroup *pControlGroup = pRootGroup->FindChildByName( pControlGroupName, true );
	
	// If the selection group was not found, create it.
	if ( pControlGroup == NULL )
	{
		pControlGroup = pRootGroup->CreateControlGroup( pControlGroupName );
	}

	return pControlGroup;
}


//-----------------------------------------------------------------------------
// Purpose: Find the control with the specified name, remove it from the group 
// it belongs to and destroy it
//-----------------------------------------------------------------------------
void CDmeAnimationSet::RemoveControlFromGroups( char const *pchControlName, bool bRemoveEmpty )
{
	CDmeControlGroup *pGroup = NULL;
	CDmElement *pControl = m_RootControlGroup->FindControlByName( pchControlName, true, &pGroup );

	if ( pGroup )
	{
		// Remove the control from the group
		pGroup->RemoveControl( pControl );

		// If the flag is set to remove empty groups and the group is empty, remove it.
		if ( bRemoveEmpty && pGroup->IsEmpty() && ( pGroup != m_RootControlGroup ) )
		{
			CDmeControlGroup::DestroyGroup( pGroup, NULL, false );
		}
	}
}



//-----------------------------------------------------------------------------
// Build a list of the root dag nodes of the animation set
//-----------------------------------------------------------------------------
void CDmeAnimationSet::FindRootDagNodes( CUtlVector< CDmeDag* > &rootDagNodeList ) const
{
	rootDagNodeList.EnsureCapacity( rootDagNodeList.Count() + 4 );

	int nControls = m_Controls.Count();
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		// Check to see if the control is a transform control
		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( m_Controls[ iControl ] );
		if ( pTransformControl == NULL )
			continue;

		// Get the dag node associated with the transform control
		CDmeDag *pDagNode = pTransformControl->GetDag();
		if ( pDagNode == NULL )
			continue;

		// Check to see if the parent of the dag is also in the animation set.
		CDmeDag *pParent = pDagNode->GetParent();
		if ( pParent )
		{
			CDmeTransformControl *pTransformControl = pParent->FindTransformControl();
			if ( pTransformControl )
			{
				if ( FindReferringElement< CDmeAnimationSet >( pTransformControl, "controls" ) == this )
					continue;
			}
		}
		
		// If the dag node has no parent or the parent is not in the
		// animation set add the dag node to the list of root nodes.
		if ( rootDagNodeList.Find( pDagNode ) == rootDagNodeList.InvalidIndex() )
		{
			rootDagNodeList.AddToTail( pDagNode );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find all of the dag nodes within the animation set.
//-----------------------------------------------------------------------------
void CDmeAnimationSet::CollectDagNodes( CUtlVector< CDmeDag* > &dagNodeList ) const
{	
	// Reserve space for the maximum number of dag nodes that may be added to the list.
	int nControls = m_Controls.Count();
	dagNodeList.EnsureCapacity( dagNodeList.Count() + nControls );

	// Iterate through all of the controls in the animation set. For the 
	// controls which are transform controls get the associated dag node and 
	// add it to the dag node list if it is not already in the dag node list.
	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		// Check to see if the control is a transform control
		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( m_Controls[ iControl ] );
		if ( pTransformControl )
		{
			// Get the dag node associated with the transform control
			CDmeDag *pDagNode = pTransformControl->GetDag();
			if ( pDagNode )
			{
				// Check to see if the dag node is already in the list, if not add it.
				if ( dagNodeList.Find( pDagNode ) == dagNodeList.InvalidIndex() )
				{
					dagNodeList.AddToTail( pDagNode );
				}
			}
		}
	}
}


void CDmeAnimationSet::CollectOperators( CUtlVector< DmElementHandle_t > &operators )
{
	int numOperators = m_Operators.Count();
	for ( int i = 0; i < numOperators; ++i )
	{
		DmElementHandle_t h = m_Operators.GetHandle( i );
		if ( h != DMELEMENT_HANDLE_INVALID )
		{
			operators.AddToTail( h );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Make sure all of the transform controls of the animation set have
// an appropriate default attribute.
//-----------------------------------------------------------------------------
void CDmeAnimationSet::UpdateTransformDefaults() const
{	
	// Get the game model associated with the animation set, this will be used to determine the default 
	// values for each of the controls, if it is not specified then the defaults cannot be set.
	CDmeGameModel *pGameModel = GetValueElement< CDmeGameModel >( "gameModel" );
	if ( pGameModel == NULL )
		return;

	// Iterate through each of the controls in the animation set, if it is a transform control see if 
	// it has a default value, if it does not have a default value, find the bone associated with the
	// transform and get the default position or rotation for that bone from the model.
	int nControls = m_Controls.Count();

	for ( int iControl = 0; iControl < nControls; ++iControl )
	{
		CDmElement *pControl = m_Controls[ iControl ];
		if ( pControl == NULL )
			continue;

		// Determine if the control is a transform control and if it has a default value.
		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pControl );
		if ( pTransformControl == NULL )
			continue;

		CDmeTransform *pTransform = pTransformControl->GetTransform();
		if ( pTransform == NULL )
			continue;
		
		// Find the bone associated with the transform and get the default position of the bone.			
		if ( !pTransformControl->HasDefaultPosition() )
		{
			Vector position = vec3_origin;
			int boneIndex = pGameModel->FindBone( pTransform );
			if ( pGameModel->GetBoneDefaultPosition( boneIndex, position ) )
			{			
				pTransformControl->SetDefaultPosition( position );
			}
		}

		// Find the bone associated with the transform and get the default rotation of the bone.
		if ( !pTransformControl->HasDefaultOrientation() )
		{			
			Quaternion orientation;
			int boneIndex = pGameModel->FindBone( pTransform );
			if ( pGameModel->GetBoneDefaultOrientation( boneIndex, orientation) )
			{
				pTransformControl->SetDefaultOrientation( orientation );
			}				
		}
	}
}


//-----------------------------------------------------------------------------
// Find the animation set to which the dag belongs, if any. This function 
// operates by checking to see of the dag node is referenced by an animation 
// set either directly or by a control. If the specified dag node is not 
// immediately referenced by an animation set the function checks the 
// hierarchical ancestors of the dag node to see if they are referenced by 
// an animation set. If the node is somehow referenced by multiple animation
// sets this function will simply return the first one it encounters as the 
// assumption is that a dag node will only be referenced by one animation set.
//-----------------------------------------------------------------------------
CDmeAnimationSet *FindAnimationSetForDag( CDmeDag *pDagNode )
{	
	while ( pDagNode )
	{
		// Check to see if an animation set directly references this dag node
		CDmeAnimationSet *pAnimationSet = FindAncestorReferencingElement< CDmeAnimationSet >( pDagNode );
		if ( pAnimationSet != NULL )
			return pAnimationSet;

		// Check to see if an animation set references the dag node through a control 
		CDmeTransformControl *pTransformControl = pDagNode->FindTransformControl();
		if ( pTransformControl )
		{
			CDmeAnimationSet *pAnimationSet = FindAncestorReferencingElement< CDmeAnimationSet >( pTransformControl );
			if ( pAnimationSet != NULL )
				return pAnimationSet;
		}
	
		// Failed to find the animation set, go on to the parent of the dag node and
		// try again, repeat until an animation set is found or the root is reached.
		pDagNode = pDagNode->GetParent();
	}

	return NULL;
}



//-----------------------------------------------------------------------------
//
// CAnimSetControlDependencyMpa implementation, a utility class for finding the
// dependencies of a control with in the animation set.
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Add the controls of the specified animation set to the dependency map
//-----------------------------------------------------------------------------
void CAnimSetControlDependencyMap::AddAnimationSet( const CDmeAnimationSet* pAnimSet )
{
	if ( pAnimSet == NULL )
		return;

	CDmeGameModel *pGameModel = pAnimSet->GetValueElement< CDmeGameModel >( "gameModel" );
	if ( pGameModel == NULL )
		return;

	// Construct a table which maps the global flex controllers to the control elements that are driving them.
	int numFlexControllers = pGameModel->NumGlobalFlexControllers();
	CUtlVector< CDmElement* > controlMap;
	controlMap.EnsureCount( numFlexControllers );

	for ( int iFlex = 0; iFlex < numFlexControllers; ++iFlex )
	{
		controlMap[ iFlex ] = NULL;

		CDmeGlobalFlexControllerOperator* pFlexOp = pGameModel->GetGlobalFlexController( iFlex );
		if ( pFlexOp )
		{
			CUtlVector< CDmeChannel* > channels( 0, 4 );
			FindAncestorsReferencingElement( pFlexOp, channels );

			int nChannels = channels.Count();
			for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
			{
				CDmeChannel *pChannel = channels[ iChannel ];
				if ( pChannel && pChannel->GetToElement() == pFlexOp )
				{
					CDmElement* pElement = pChannel->GetFromElement();	 
					controlMap[ iFlex ] = pElement;
					break;
				}
			}
		}
	}


	// Get a list of the flex controllers that each flex controller is dependent on.
	CUtlVector < CUtlVector< int > > flexDependencyList;
	pGameModel->FindFlexControllerDependencies( flexDependencyList );
	int nLists = flexDependencyList.Count();

	for ( int iList = 0; iList < nLists; ++iList )
	{
		CUtlVector< int > &dependencyList = flexDependencyList[ iList ];
		int listCount = dependencyList.Count();

		if ( listCount > 0 )
		{
			// Construct the control dependency list from the flex controller dependency 
			// list. The first element in flex controller list is the dependent element, 
			// the following elements are the dependencies.
			CDmElement *pElement = controlMap[ dependencyList[ 0 ] ];
			if ( pElement )
			{
				DependencyList_t *pDependencySet = FindDependencyList( pElement );

				if ( pDependencySet == NULL)
				{
					int arrayIndex = m_DependencyData.AddToTail();
					pDependencySet = &m_DependencyData[ arrayIndex ];
					pDependencySet->m_pElement = pElement;
					pDependencySet->m_Dependencies.EnsureCapacity( listCount );
				}

				for ( int iDep = 1; iDep < listCount; ++iDep )
				{
					CDmElement* pDependency = controlMap[ dependencyList[ iDep ] ];
					if ( pDependency != pElement )
					{
						if ( pDependencySet->m_Dependencies.Find( pDependency ) == pDependencySet->m_Dependencies.InvalidIndex() )
						{
							pDependencySet->m_Dependencies.AddToTail( pDependency );
						}
					}
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Get the list of controls which the specified control is dependent on.
//-----------------------------------------------------------------------------
const CUtlVector< const CDmElement * > *CAnimSetControlDependencyMap::GetControlDepndencies( const CDmElement *pControl ) const
{
	if ( pControl == NULL )
		return NULL;

	const DependencyList_t *pDependencyList = NULL;
	int nSets = m_DependencyData.Count();
	for ( int i = 0; i < nSets; ++i )
	{
		if ( m_DependencyData[ i ].m_pElement == pControl )
		{
			pDependencyList = &m_DependencyData[ i ];
			break;
		}
	}

	if ( pDependencyList == NULL )
		return NULL;

	if ( pDependencyList->m_pElement != pControl )
	{
		// If this assert is hit, something has gone wrong with the control look up.
		Assert( pDependencyList->m_pElement == pControl );
		return NULL;
	}
	
	return &pDependencyList->m_Dependencies;
}


//-----------------------------------------------------------------------------
// Purpose: Find the dependency list for the specified control
//-----------------------------------------------------------------------------
CAnimSetControlDependencyMap::DependencyList_t *CAnimSetControlDependencyMap::FindDependencyList( const CDmElement* pControl )
{
	DependencyList_t *pDependencyList = NULL;
	int nSets = m_DependencyData.Count();
	for ( int i = 0; i < nSets; ++i )
	{
		if ( m_DependencyData[ i ].m_pElement == pControl )
		{
			pDependencyList = &m_DependencyData[ i ];
			break;
		}
	}

	return pDependencyList;
}


//-----------------------------------------------------------------------------
// CDmePresetGroupInfo - container for shared preset groups
//-----------------------------------------------------------------------------

IMPLEMENT_ELEMENT_FACTORY( DmePresetGroupInfo, CDmePresetGroupInfo );

void CDmePresetGroupInfo::OnConstruction()
{
	m_filenameBase.Init( this, "filenameBase" );
	m_presetGroups.Init( this, "presetGroups" );
}

void CDmePresetGroupInfo::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// Loads model presets from .pre files matching the filename base
//-----------------------------------------------------------------------------
void CDmePresetGroupInfo::LoadPresetGroups()
{
	LoadPresetGroups( m_filenameBase, m_presetGroups );
}

void CDmePresetGroupInfo::FilenameBaseForModelName( const char *pModelName, char *pFileNameBase, int nFileNameBaseLen )
{
	V_FixupPathName( pFileNameBase, nFileNameBaseLen, pModelName );
	V_StripExtension( pFileNameBase, pFileNameBase, nFileNameBaseLen );
}

CDmePresetGroupInfo *CDmePresetGroupInfo::FindPresetGroupInfo( const char *pFilenameBase, CDmrElementArray< CDmePresetGroupInfo > &presetGroupInfos )
{
	// find an existing CDmePresetGroupInfo
	int nPresetGroupInfos = presetGroupInfos.Count();
	for ( int i = 0; i < nPresetGroupInfos; ++i )
	{
		CDmePresetGroupInfo *pPresetGroupInfo = presetGroupInfos[ i ];
		if ( !pPresetGroupInfo )
			continue;

		if ( V_stricmp( pFilenameBase, pPresetGroupInfo->GetFilenameBase() ) == 0 )
			return pPresetGroupInfo;
	}

	return NULL;
}

CDmePresetGroupInfo *CDmePresetGroupInfo::FindOrCreatePresetGroupInfo( const char *pFilenameBase, CDmrElementArray< CDmePresetGroupInfo > &presetGroupInfos )
{
	// find or create an CDmePresetGroupInfo
	CDmePresetGroupInfo *pPresetGroupInfo = CDmePresetGroupInfo::FindPresetGroupInfo( pFilenameBase, presetGroupInfos );
	if ( !pPresetGroupInfo )
	{
		pPresetGroupInfo = CreatePresetGroupInfo( pFilenameBase, presetGroupInfos.GetOwner()->GetFileId() );
		presetGroupInfos.AddToTail( pPresetGroupInfo );
	}
	return pPresetGroupInfo;
}

CDmePresetGroupInfo *CDmePresetGroupInfo::CreatePresetGroupInfo( const char *pFilenameBase, DmFileId_t fileid )
{
	char presetGroupInfoName[ MAX_PATH ];
	V_FileBase( pFilenameBase, presetGroupInfoName, sizeof( presetGroupInfoName ) );

	CDmePresetGroupInfo *pPresetGroupInfo = CreateElement< CDmePresetGroupInfo >( presetGroupInfoName, fileid );
	pPresetGroupInfo->SetFilenameBase( pFilenameBase );
	pPresetGroupInfo->LoadPresetGroups();
	return pPresetGroupInfo;
}

void CDmePresetGroupInfo::LoadPresetGroups( const char *pFilenameBase, CDmaElementArray< CDmePresetGroup > &presetGroups )
{
	char presetPath[MAX_PATH];
	Q_ExtractFilePath( pFilenameBase, presetPath, sizeof( presetPath ) );

	char presetNameFilter[MAX_PATH];
	V_strncpy( presetNameFilter, pFilenameBase, sizeof( presetNameFilter ) );
	int nLen = Q_strlen( presetNameFilter );
	Q_snprintf( &presetNameFilter[ nLen ], MAX_PATH - nLen, "*.pre" );

	DmFileId_t parentFileId = presetGroups.GetOwner()->GetFileId();

	FileFindHandle_t fh;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( presetNameFilter, "GAME", &fh );
	for ( ; pFileName; pFileName = g_pFullFileSystem->FindNext( fh ) )
	{
		char relativePresetPath[MAX_PATH];
		Q_ComposeFileName( presetPath, pFileName, relativePresetPath, sizeof( relativePresetPath ) );

		CDmElement* pRoot = NULL;
		DmFileId_t fileid = g_pDataModel->RestoreFromFile( relativePresetPath, "GAME", NULL, &pRoot, CR_FORCE_COPY ); // TODO - change this to CR_DELETE_OLD, since we'll want the loaded presets to replace the existing ones
		if ( fileid == DMFILEID_INVALID || !pRoot )
			continue;

		CDmePresetGroup *pPresetGroup = CastElement<CDmePresetGroup>( pRoot );
		if ( !pPresetGroup )
		{
			if ( pRoot )
			{
				g_pDataModel->RemoveFileId( pRoot->GetFileId() );
			}
			continue;
		}

		pPresetGroup->SetFileId( parentFileId, TD_DEEP );

		// Presets used through the model preset manager must be read only + shared
		pPresetGroup->m_bIsReadOnly = true;
		pPresetGroup->SetShared( true );

		presetGroups.AddToTail( pPresetGroup );
	}

	g_pFullFileSystem->FindClose( fh );
}


ControlIndex_t FindComboOpControlIndexForAnimSetControl( CDmeCombinationOperator *pComboOp, const char *pControlName, bool *pIsMulti /*= NULL*/ )
{
	const char *pMultiControlBaseName = pControlName ? StringAfterPrefix( pControlName, "multi_" ) : NULL;
	if ( pIsMulti )
	{
		*pIsMulti = pMultiControlBaseName != NULL;
	}

	if ( !pComboOp || !pControlName )
		return -1;

	ControlIndex_t index = pComboOp->FindControlIndex( pControlName );
	if ( index >= 0 )
		return index;

	if ( !pMultiControlBaseName )
		return -1;

	index = pComboOp->FindControlIndex( pMultiControlBaseName );
	if ( index < 0 )
		return -1;

	Assert( pComboOp->IsMultiControl( index ) );

	return index;
}


//-----------------------------------------------------------------------------
// Utility class to migrate from traversing all animationsets within an animationsetgroup to a filmclip
//-----------------------------------------------------------------------------

bool CAnimSetGroupAnimSetTraversal::IsValid()
{
	return m_pFilmClip && m_nIndex < m_pFilmClip->GetAnimationSets().Count();
}

CDmeAnimationSet *CAnimSetGroupAnimSetTraversal::Next()
{
	CDmeAnimationSet *pAnimSet = NULL;
	while ( pAnimSet == NULL && IsValid() )
	{
		pAnimSet = m_pFilmClip->GetAnimationSets()[ m_nIndex++ ];
	}
	return pAnimSet;
}
