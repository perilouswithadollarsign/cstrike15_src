//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "importkeyvaluebase.h"
#include "dmserializers.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "tier1/KeyValues.h"
#include "tier1/UtlBuffer.h"
#include "datamodel/dmattribute.h"


//-----------------------------------------------------------------------------
// Serialization class for Key Values
//-----------------------------------------------------------------------------
class CImportCommentary : public CImportKeyValueBase
{
public:
	virtual const char *GetName() const { return "commentary"; }
	virtual const char *GetDescription() const { return "Commentary File"; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version
	virtual const char *GetImportedFormat() const { return "commentary"; }
	virtual int GetImportedVersion() const { return 1; }

	virtual bool Serialize( CUtlBuffer &outBuf, CDmElement *pRoot );
	virtual bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
		const char *pSourceFormatName, int nSourceFormatVersion,
		DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

protected:
	// Main entry point for derived classes to implement unserialization
	virtual CDmElement* UnserializeFromKeyValues( KeyValues *pKeyValues ) { Assert( 0 ); return NULL; }
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportCommentary s_ImportCommentary;

void InstallCommentaryImporter( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_ImportCommentary );
}


bool CImportCommentary::Serialize( CUtlBuffer &buf, CDmElement *pRoot )
{
	IDmSerializer *pKVSerializer = g_pDataModel->FindSerializer( "keyvalues" );
	if ( !pKVSerializer )
		return false;

	return pKVSerializer->Serialize( buf, pRoot );
}

//-----------------------------------------------------------------------------
// Handles creation of the right element for a keyvalue
//-----------------------------------------------------------------------------
class CElementForKeyValueCallback : public IElementForKeyValueCallback
{
public:
	const char *GetElementForKeyValue( const char *pszKeyName, int iNestingLevel )
	{
		if ( iNestingLevel == 1 && !Q_strncmp(pszKeyName, "entity", 6) )
			return "DmeCommentaryNodeEntity";

		return NULL;
	}
};

bool CImportCommentary::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
									  const char *pSourceFormatName, int nSourceFormatVersion,
									  DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	*ppRoot = NULL;

	IDmSerializer *pKVSerializer = g_pDataModel->FindSerializer( "keyvalues" );
	if ( !pKVSerializer )
		return false;

	CElementForKeyValueCallback KeyValuesCallback;
	g_pDataModel->SetKeyValuesElementCallback( &KeyValuesCallback );

	bool bSuccess = pKVSerializer->Unserialize( buf, "keyvalues", nEncodingVersion, pSourceFormatName, nSourceFormatVersion, fileid, idConflictResolution, ppRoot );

	g_pDataModel->SetKeyValuesElementCallback( NULL );

	return bSuccess;
}
