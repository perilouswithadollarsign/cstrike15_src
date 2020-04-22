//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dmserializerbinary.h"
#include "datamodel/idatamodel.h"
#include "datamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "dmattributeinternal.h"
#include "dmelementdictionary.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlbufferutil.h"
#include "DmElementFramework.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;
class CBaseSceneObject;


//-----------------------------------------------------------------------------
// special element indices
//-----------------------------------------------------------------------------
enum
{
	ELEMENT_INDEX_NULL = -1,
	ELEMENT_INDEX_EXTERNAL = -2,
};

// Versions
enum
{
	DM_BINARY_VER_STRINGTABLE = 2,
	DM_BINARY_VER_GLOBAL_STRINGTABLE = 4, // stringtable used for all strings, and count is an int (symbols still shorts)
	DM_BINARY_VER_STRINGTABLE_LARGESYMBOLS = 5,	// stringtable used for all strings, and count is an int (symbols are ints, too)
};


//-----------------------------------------------------------------------------
// Serialization class for Binary output
//-----------------------------------------------------------------------------
class CDmSerializerBinary : public IDmSerializer
{
public:
	CDmSerializerBinary() {}
	// Inherited from IDMSerializer
	virtual const char *GetName() const { return "binary"; }
	virtual const char *GetDescription() const { return "Binary"; }
	virtual bool StoresVersionInFile() const { return true; }
	virtual bool IsBinaryFormat() const { return true; }
	virtual int GetCurrentVersion() const { return DM_BINARY_VER_STRINGTABLE_LARGESYMBOLS; }
	virtual const char *GetImportedFormat() const { return NULL; }
 	virtual int GetImportedVersion() const { return 1; }
	virtual bool Serialize( CUtlBuffer &buf, CDmElement *pRoot );
	virtual bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
							  const char *pSourceFormatName, int nFormatVersion,
							  DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

private:

	// For serialize
	typedef CUtlDict< int, int > mapSymbolToIndex_t;
	// For unserialize
	typedef CUtlMap< int, CUtlSymbolLarge > mapIndexToSymbol_t;

	// Methods related to serialization
	void SerializeElementIndex( CUtlBuffer& buf, CDmElementSerializationDictionary& list, DmElementHandle_t hElement, DmFileId_t fileid );
	void SerializeElementAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute );
	void SerializeElementArrayAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute );
	bool SerializeAttributes( CUtlBuffer& buf, CDmElementSerializationDictionary& list, mapSymbolToIndex_t *pStringValueSymbols, CDmElement *pElement );
	bool SaveElementDict( CUtlBuffer& buf, mapSymbolToIndex_t *pStringValueSymbols, CDmElement *pElement );
	bool SaveElement( CUtlBuffer& buf, CDmElementSerializationDictionary& dict, mapSymbolToIndex_t *pStringValueSymbols, CDmElement *pElement);
	void GatherSymbols( CUtlSymbolTableLarge *pStringValueSymbols, CDmElement *pElement );

	// Methods related to unserialization
	DmElementHandle_t UnserializeElementIndex( CUtlBuffer &buf, CUtlVector<CDmElement*> &elementList );
	void UnserializeElementAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList );
	void UnserializeElementArrayAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList );
	void UnserializeStringArrayAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlString &tempString );
	bool UnserializeAttributes( CUtlBuffer &buf, CDmElement *pElement, CUtlVector<CDmElement*> &elementList, mapIndexToSymbol_t *pSymbolTable, int nEncodingVersion );
	bool UnserializeElements( CUtlBuffer &buf, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot, mapIndexToSymbol_t *pSymbolTable, int nEncodingVersion );
	void GetStringTable( CUtlBuffer &buf, int nStrings, int nEncodingVersion, mapIndexToSymbol_t *pMap );

	inline char const *Dme_GetStringFromBuffer( CUtlBuffer &buf, bool bUseLargeSymbols, mapIndexToSymbol_t *pMap )
	{
		unsigned int uSym = ( bUseLargeSymbols ) ? buf.GetInt() : buf.GetShort();
		return pMap->Element( uSym ).String();
	}

	inline CUtlSymbolLarge Dme_GetSymbolFromBuffer( CUtlBuffer &buf, bool bUseLargeSymbols, mapIndexToSymbol_t *pMap )
	{
		unsigned int uSym = ( bUseLargeSymbols ) ? buf.GetInt() : buf.GetShort();
		return pMap->Element( uSym );
	}
};

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CDmSerializerBinary s_DMSerializerBinary;

void InstallBinarySerializer( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_DMSerializerBinary );
}


//-----------------------------------------------------------------------------
// Write out the index of the element to avoid looks at read time
//-----------------------------------------------------------------------------
void CDmSerializerBinary::SerializeElementIndex( CUtlBuffer& buf, CDmElementSerializationDictionary& list, DmElementHandle_t hElement, DmFileId_t fileid )
{
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		buf.PutInt( ELEMENT_INDEX_NULL ); // invalid handle
	}
	else
	{
		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		if ( pElement )
		{
			if ( pElement->GetFileId() == fileid )
			{
				buf.PutInt( list.Find( pElement ) );
			}
			else
			{
				buf.PutInt( ELEMENT_INDEX_EXTERNAL );
				char idstr[ 40 ];
				UniqueIdToString( pElement->GetId(), idstr, sizeof( idstr ) );
				buf.PutString( idstr );
			}
		}
		else
		{
			DmObjectId_t *pId = NULL;
			DmElementReference_t *pRef = g_pDataModelImp->FindElementReference( hElement, &pId );
			if ( pRef && pId )
			{
				buf.PutInt( ELEMENT_INDEX_EXTERNAL );
				char idstr[ 40 ];
				UniqueIdToString( *pId, idstr, sizeof( idstr ) );
				buf.PutString( idstr );
			}
			else
			{
				buf.PutInt( ELEMENT_INDEX_NULL ); // invalid handle
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Writes out element attributes
//-----------------------------------------------------------------------------
void CDmSerializerBinary::SerializeElementAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute )
{
	SerializeElementIndex( buf, list, pAttribute->GetValue< DmElementHandle_t >(), pAttribute->GetOwner()->GetFileId() );
}


//-----------------------------------------------------------------------------
// Writes out element array attributes
//-----------------------------------------------------------------------------
void CDmSerializerBinary::SerializeElementArrayAttribute( CUtlBuffer& buf, CDmElementSerializationDictionary& list, CDmAttribute *pAttribute )
{
	DmFileId_t fileid = pAttribute->GetOwner()->GetFileId();
	CDmrElementArray<> vec( pAttribute );

	int nCount = vec.Count();
	buf.PutInt( nCount );
	for ( int i = 0; i < nCount; ++i )
	{
		SerializeElementIndex( buf, list, vec.GetHandle(i), fileid );
	}
}


//-----------------------------------------------------------------------------
// Writes out all attributes
//-----------------------------------------------------------------------------
bool CDmSerializerBinary::SerializeAttributes( CUtlBuffer& buf, CDmElementSerializationDictionary& list, mapSymbolToIndex_t *pStringValueSymbols, CDmElement *pElement )
{
	// Collect the attributes to be written
	CDmAttribute **ppAttributes = ( CDmAttribute** )_alloca( pElement->AttributeCount() * sizeof( CDmAttribute* ) );
	int nAttributes = 0;
	for ( CDmAttribute *pAttribute = pElement->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( pAttribute->IsStandard() || pAttribute->IsFlagSet( FATTRIB_DONTSAVE ) )
			continue;

		ppAttributes[ nAttributes++ ] = pAttribute;
	}

	// Now write them all out in reverse order, since FirstAttribute is actually the *last* attribute for perf reasons
	buf.PutInt( nAttributes );
	for ( int i = nAttributes - 1; i >= 0; --i )
	{
		CDmAttribute *pAttribute = ppAttributes[ i ];
		Assert( pAttribute );

		buf.PutInt( pStringValueSymbols->Find(pAttribute->GetName()) );
		buf.PutChar( pAttribute->GetType() );
		switch( pAttribute->GetType() )
		{
		default:
 			pAttribute->Serialize( buf );
			break;

		case AT_ELEMENT:
			SerializeElementAttribute( buf, list, pAttribute );
			break;

		case AT_ELEMENT_ARRAY:
			SerializeElementArrayAttribute( buf, list, pAttribute );
			break;

		case AT_STRING:
			buf.PutInt( pStringValueSymbols->Find(pAttribute->GetValueString()) );
			break;
		}
	}

	return buf.IsValid();
}


bool CDmSerializerBinary::SaveElement( CUtlBuffer& buf, CDmElementSerializationDictionary& list, mapSymbolToIndex_t *pStringValueSymbols, CDmElement *pElement )
{
	SerializeAttributes( buf, list, pStringValueSymbols, pElement );
	return buf.IsValid();
}

bool CDmSerializerBinary::SaveElementDict( CUtlBuffer& buf, mapSymbolToIndex_t *pStringValueSymbols, CDmElement *pElement )
{
	buf.PutInt( pStringValueSymbols->Find( pElement->GetTypeString() ) );
	buf.PutInt( pStringValueSymbols->Find( pElement->GetName() ) );
	buf.Put( &pElement->GetId(), sizeof(DmObjectId_t) );
	return buf.IsValid();
}

void CDmSerializerBinary::GatherSymbols( CUtlSymbolTableLarge *pStringValueSymbols, CDmElement *pElement )
{
	pStringValueSymbols->AddString( pElement->GetTypeString() );
	pStringValueSymbols->AddString( pElement->GetName() );

	for ( CDmAttribute *pAttr = pElement->FirstAttribute(); pAttr; pAttr = pAttr->NextAttribute() )
	{
		pStringValueSymbols->AddString( pAttr->GetName() );

		if ( pAttr->GetType() == AT_STRING )
		{
			pStringValueSymbols->AddString( pAttr->GetValueString() );
		}
	}
}

bool CDmSerializerBinary::Serialize( CUtlBuffer &outBuf, CDmElement *pRoot )
{
	// Save elements, attribute links
	CDmElementSerializationDictionary dict;
	dict.BuildElementList( pRoot, true );

	DmElementDictHandle_t i;
	CUtlSymbolTableLarge stringSymbols;

	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		GatherSymbols( &stringSymbols, dict.GetRootElement( i ) );
	}

	// write out the symbol table for this file (may be significantly smaller than datamodel's full symbol table)
	int nSymbols = stringSymbols.GetNumStrings();

	outBuf.PutInt( nSymbols );

	CUtlVector< CUtlSymbolLarge > symbols;
	symbols.EnsureCount( nSymbols );
	stringSymbols.GetElements( 0, nSymbols, symbols.Base() );
	
	// It's case sensitive
	mapSymbolToIndex_t symbolToIndexMap( k_eDictCompareTypeCaseSensitive );

	// Build the helper map based on the gathered symbols
	for ( int si = 0; si < nSymbols; ++si )
	{
		CUtlSymbolLarge sym = symbols[ si ];
		const char *pStr = sym.String();
		symbolToIndexMap.Insert( pStr, si );
		outBuf.PutString( pStr );
	}

	// First write out the dictionary of all elements (to avoid later stitching up in unserialize)
	outBuf.PutInt( dict.RootElementCount() );
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		SaveElementDict( outBuf, &symbolToIndexMap, dict.GetRootElement( i ) );
	}

	// Now write out the attributes of each of those elements
	for ( i = dict.FirstRootElement(); i != ELEMENT_DICT_HANDLE_INVALID; i = dict.NextRootElement(i) )
	{
		SaveElement( outBuf, dict, &symbolToIndexMap, dict.GetRootElement( i ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Reads an element index and converts it to a handle (local or external)
//-----------------------------------------------------------------------------
DmElementHandle_t CDmSerializerBinary::UnserializeElementIndex( CUtlBuffer &buf, CUtlVector<CDmElement*> &elementList )
{
	int nElementIndex = buf.GetInt();
	Assert( nElementIndex < elementList.Count() );
	if ( nElementIndex == ELEMENT_INDEX_EXTERNAL )
	{
		char idstr[ 40 ];
		buf.GetString( idstr, sizeof( idstr ) );
		DmObjectId_t id;
		UniqueIdFromString( &id, idstr, sizeof( idstr ) );
		return g_pDataModelImp->FindOrCreateElementHandle( id );
	}

	Assert( nElementIndex >= 0 || nElementIndex == ELEMENT_INDEX_NULL );
	if ( nElementIndex < 0 || !elementList[ nElementIndex ] )
		return DMELEMENT_HANDLE_INVALID;

	return elementList[ nElementIndex ]->GetHandle();
}


//-----------------------------------------------------------------------------
// Reads an element attribute
//-----------------------------------------------------------------------------
void CDmSerializerBinary::UnserializeElementAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList )
{
	DmElementHandle_t hElement = UnserializeElementIndex( buf, elementList );
	if ( !pAttribute )
		return;

	pAttribute->SetValue( hElement );
}


//-----------------------------------------------------------------------------
// Reads an element array attribute
//-----------------------------------------------------------------------------
void CDmSerializerBinary::UnserializeElementArrayAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlVector<CDmElement*> &elementList )
{
	int nElementCount = buf.GetInt();

	if ( !pAttribute || pAttribute->GetType() != AT_ELEMENT_ARRAY )
	{
		// Parse past the data
		for ( int i = 0; i < nElementCount; ++i )
		{
			UnserializeElementIndex( buf, elementList );
		}
		return;
	}

	CDmrElementArray<> array( pAttribute );
	array.RemoveAll();
	array.EnsureCapacity( nElementCount );
	for ( int i = 0; i < nElementCount; ++i )
	{
		DmElementHandle_t hElement = UnserializeElementIndex( buf, elementList );
		array.AddToTail( hElement );
	}
}

//-----------------------------------------------------------------------------
// Read an array of strings
//-----------------------------------------------------------------------------
void CDmSerializerBinary::UnserializeStringArrayAttribute( CUtlBuffer &buf, CDmAttribute *pAttribute, CUtlString &tempString )
{

	CDmrStringArray stringArray( pAttribute );
	stringArray.RemoveAll();

	if ( buf.IsText() )
	{
		while ( true )
		{
			buf.EatWhiteSpace();
			if ( !buf.IsValid() )
				break;

			if ( !::Unserialize( buf, tempString ) )
				return;

			stringArray.AddToTail( tempString.Get() );
		}
	}
	else
	{
		int nNumStrings = buf.GetInt();
		if ( nNumStrings ) 
		{
			stringArray.EnsureCapacity( nNumStrings );
			for ( int i = 0; i < nNumStrings; ++i )
			{
				if ( !::Unserialize( buf, tempString ) )
					return;

				stringArray.AddToTail( tempString.Get() );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Reads a single element
//-----------------------------------------------------------------------------
bool CDmSerializerBinary::UnserializeAttributes( CUtlBuffer &buf, CDmElement *pElement, CUtlVector<CDmElement*> &elementList, mapIndexToSymbol_t *pSymbolTable, int nEncodingVersion )
{
	char nameBuf[ 1024 ];

	CUtlString tempString;
	tempString.SetLength( 1024 );

	bool bReadTimeAsObjectId = nEncodingVersion < 3; // version 3 and up put AT_TIME in the same slot that AT_OBJECTID used to be
	bool bUseLargeSymbols = nEncodingVersion >= DM_BINARY_VER_STRINGTABLE_LARGESYMBOLS;


	int nAttributeCount = buf.GetInt();
	for ( int i = 0; i < nAttributeCount; ++i )
	{
		const char *pName = NULL;
		{
			DMX_PROFILE_SCOPE( UnserializeAttributes_GetNameString );
			if ( pSymbolTable )
			{
				pName = Dme_GetStringFromBuffer( buf, bUseLargeSymbols, pSymbolTable );
			}
			else
			{
				buf.GetString( nameBuf, sizeof( nameBuf ) );
				pName = nameBuf;
			}
		}

		DmAttributeType_t nAttributeType = (DmAttributeType_t)buf.GetChar();
		Assert( pName != NULL && pName[ 0 ] != '\0' );
		Assert( nAttributeType != AT_UNKNOWN );

		if ( nAttributeType == AT_TIME && bReadTimeAsObjectId )
		{
			Warning( "CDmSerializerBinary: Removing deprecated objectid attribute '%s' of element '%s'\n", pName, pElement->GetName() );
			DmObjectId_t id;
			::Unserialize( buf, id );
			continue;
		}

		CDmAttribute *pAttribute = NULL;
		{
			DMX_PROFILE_SCOPE( UnserializeAttributes_AddAttribute );

			pAttribute = pElement ? pElement->AddAttribute( pName, nAttributeType ) : NULL;
			if ( pElement && !pAttribute )
			{
				CDmAttribute *pExistingAttr = pElement->GetAttribute( pName );
				if ( pExistingAttr )
				{
					Warning( "CDmSerializerBinary: Attribute '%s' of element '%s' read as '%s' but expected '%s'\n",
						pName, pElement->GetName(), GetTypeString( nAttributeType ), pExistingAttr->GetTypeString() );
				}
				else
				{
					Warning( "CDmSerializerBinary: Unknown error reading '%s' attribute '%s' of element '%s'\n",
						GetTypeString( nAttributeType ), pName, pElement->GetName() );
					return false;
				}
			}
		}


		switch( nAttributeType )
		{
		default:
			{
				DMX_PROFILE_SCOPE( UnserializeAttributes_Unserialize );
				if ( !pAttribute )
				{
					SkipUnserialize( buf, nAttributeType );
				}
				else
				{
					pAttribute->Unserialize( buf );
				}
			}
			break;

		case AT_ELEMENT:
			{
				DMX_PROFILE_SCOPE( UnserializeAttributes_UnserializeElementAttr );
				UnserializeElementAttribute( buf, pAttribute, elementList );
			}
			break;

		case AT_ELEMENT_ARRAY:
			{
				DMX_PROFILE_SCOPE( UnserializeAttributes_UnserializeElementArrayAttr );
				UnserializeElementArrayAttribute( buf, pAttribute, elementList );
			}
			break;

		case AT_STRING:
			{
				DMX_PROFILE_SCOPE( UnserializeAttributes_UnserializeStringAttr );

				if ( pSymbolTable && nEncodingVersion >= DM_BINARY_VER_GLOBAL_STRINGTABLE )
				{
					CUtlSymbolLarge symbol = Dme_GetSymbolFromBuffer( buf, bUseLargeSymbols, pSymbolTable );
					
					if ( pAttribute )
					{
						pAttribute->SetValue( symbol );
					}
				}
				else
				{
					if ( !pAttribute )
					{
						SkipUnserialize( buf, nAttributeType );
					}
					else
					{
						::Unserialize( buf, tempString );
						pAttribute->SetValue( tempString.Get() );
					}
				}
			}
			break;

		case AT_STRING_ARRAY:
			{
				DMX_PROFILE_SCOPE( UnserializeAttributes_UnserializeStringArrayAttr );
				if ( !pAttribute )
				{
					SkipUnserialize( buf, nAttributeType );
				}
				else
				{
					UnserializeStringArrayAttribute( buf, pAttribute, tempString );
				}
			}
			break;
		}
	}

	return buf.IsValid();
}

struct DmIdPair_t
{
	DmObjectId_t m_oldId;
	DmObjectId_t m_newId;
	DmIdPair_t &operator=( const DmIdPair_t &that )
	{
		CopyUniqueId( that.m_oldId, &m_oldId );
		CopyUniqueId( that.m_newId, &m_newId );
		return *this;
	}
	static unsigned int HashKey( const DmIdPair_t& that )
	{
		return *( unsigned int* )&that.m_oldId.m_Value;
	}
	static bool Compare( const DmIdPair_t& a, const DmIdPair_t& b )
	{
		return IsUniqueIdEqual( a.m_oldId, b.m_oldId );
	}
};

DmElementHandle_t CreateElementWithFallback( const char *pType, const char *pName, DmFileId_t fileid, const DmObjectId_t &id )
{
	DmElementHandle_t hElement = g_pDataModel->CreateElement( pType, pName, fileid, &id );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning("Binary: Element uses unknown element type %s\n", pType );
		hElement = g_pDataModel->CreateElement( "DmElement", pName, fileid, &id );
		Assert( hElement != DMELEMENT_HANDLE_INVALID );
	}
	return hElement;
}

void CDmSerializerBinary::GetStringTable( CUtlBuffer &buf, int nStrings, int nEncodingVersion, mapIndexToSymbol_t *pMap )
{
	char pStrBuf[2048];
	for ( int i = 0; i < nStrings; ++i )
	{
		buf.GetString( pStrBuf, sizeof(pStrBuf) );
		CUtlSymbolLarge sym = g_pDataModel->GetSymbol( pStrBuf );
		pMap->Insert( i, sym );
	}
}

//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CDmSerializerBinary::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion,
									   const char *pSourceFormatName, int nSourceFormatVersion,
									   DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	DMX_PROFILE_SCOPE( CDmSerializerBinary_Unserialize );
	Assert( !V_stricmp( pEncodingName, GetName() ) );
	if ( V_stricmp( pEncodingName, GetName() ) != 0 )
		return false;

	Assert( nEncodingVersion >= 0 && nEncodingVersion <= GetCurrentVersion() );
	if ( nEncodingVersion < 0 || nEncodingVersion > GetCurrentVersion() )
		return false;

	bool bReadSymbolTable = nEncodingVersion >= DM_BINARY_VER_STRINGTABLE;

	// Read string table
	int nStrings = 0;
	mapIndexToSymbol_t indexToSymbolMap( 0, 0, DefLessFunc( int ) );

	if ( bReadSymbolTable )
	{
		if ( nEncodingVersion >= DM_BINARY_VER_GLOBAL_STRINGTABLE )
		{
			nStrings = buf.GetInt();
		}
		else
		{
			nStrings = buf.GetShort();
		}

		GetStringTable( buf, nStrings, nEncodingVersion, &indexToSymbolMap );
	}

	bool bSuccess = UnserializeElements( buf, fileid, idConflictResolution, ppRoot, bReadSymbolTable?(&indexToSymbolMap):NULL, nEncodingVersion );
	if ( !bSuccess )
		return false;

	return g_pDataModel->UpdateUnserializedElements( pSourceFormatName, nSourceFormatVersion, fileid, idConflictResolution, ppRoot );
}

bool CDmSerializerBinary::UnserializeElements( CUtlBuffer &buf, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot, mapIndexToSymbol_t *pSymbolTable, int nEncodingVersion )
{
	DMX_PROFILE_SCOPE( CDmSerializerBinary_UnserializeElements );

	*ppRoot = NULL;

	bool bLargeSymbols = nEncodingVersion >= DM_BINARY_VER_STRINGTABLE_LARGESYMBOLS;

	// Read in the element count.
	int nElementCount = buf.GetInt();
	if ( !nElementCount )
		return true;

	CElementIdHash hashExistingElements;
	if ( CR_FORCE_COPY != idConflictResolution )
	{
		DMX_PROFILE_SCOPE( UnserializeElements_GetExistingElements );
		g_pDataModel->GetExistingElements( hashExistingElements );
	}
	UtlHashHandle_t hashInvalidHandle = hashExistingElements.InvalidHandle();

	// Read + create all elements
	char nameBuf[ 2048 ];
	char typeBuf[ 512 ];

	CUtlVector<CDmElement*> elementList( 0, nElementCount );
	for ( int i = 0; i < nElementCount; ++i )
	{
		const char *pName = NULL;
		const char *pType = NULL;
		DmObjectId_t id;

		{
			DMX_PROFILE_SCOPE( UnserializeElements_TypeAndName_GetString );
			if ( pSymbolTable )
			{
				pType = Dme_GetStringFromBuffer( buf, bLargeSymbols, pSymbolTable );
			}
			else
			{
				buf.GetString( typeBuf, sizeof(typeBuf) );
				pType = typeBuf;
			}


			if ( pSymbolTable && nEncodingVersion >= DM_BINARY_VER_GLOBAL_STRINGTABLE )
			{
				pName = Dme_GetStringFromBuffer( buf, bLargeSymbols, pSymbolTable );
			}
			else
			{
				buf.GetString( nameBuf, sizeof(nameBuf) );
				pName = nameBuf;
			}
		}

		buf.Get( &id, sizeof(DmObjectId_t) );

		DmElementHandle_t hElement = DMELEMENT_HANDLE_INVALID;
		if ( idConflictResolution == CR_FORCE_COPY )
		{
			CreateUniqueId( &id );
		}
		else
		{
			DMX_PROFILE_SCOPE( UnserializeElements_ConflictCheck );

			UtlHashHandle_t search = hashExistingElements.Find( id ); 
			DmElementHandle_t hExistingElement = ( search == hashInvalidHandle ? DMELEMENT_HANDLE_INVALID : hashExistingElements[ search ] );
			if ( hExistingElement != DMELEMENT_HANDLE_INVALID )
			{
				// id is already in use - need to resolve conflict

				if ( idConflictResolution == CR_DELETE_NEW )
				{
					elementList.AddToTail( g_pDataModel->GetElement( hExistingElement ) );
					continue; // just don't create this element
				}
				else if ( idConflictResolution == CR_DELETE_OLD )
				{
					g_pDataModelImp->DeleteElement( hExistingElement, HR_NEVER ); // keep the handle around until CreateElementWithFallback
					hElement = CreateElementWithFallback( pType, pName, fileid, id );
					Assert( hElement == hExistingElement );
				}
				else if ( idConflictResolution == CR_COPY_NEW )
				{
					CreateUniqueId( &id );
					hElement = CreateElementWithFallback( pType, pName, fileid, id );
				}
				else
				{
					Assert( 0 );
				}
			}
		}

		// if not found, then create it
		if ( hElement == DMELEMENT_HANDLE_INVALID )
		{
			DMX_PROFILE_SCOPE( UnserializeElements_CreateElementWithFallback );
			hElement = CreateElementWithFallback( pType, pName, fileid, id );
		}

		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		CDmeElementAccessor::DisableOnChangedCallbacks( pElement );
		elementList.AddToTail( pElement );
	}

	// The root is the 0th element
	*ppRoot = elementList[ 0 ];

	{
		DMX_PROFILE_SCOPE( UnserializeElements_UnserializeAttributes );
		// Now read all attributes
		for ( int i = 0; i < nElementCount; ++i )
		{
			CDmElement *pInternal = elementList[ i ];
			if ( !UnserializeAttributes( buf, pInternal->GetFileId() == fileid ? pInternal : NULL, elementList, pSymbolTable, nEncodingVersion ) )
				return false;
		}
	}

	{
		DMX_PROFILE_SCOPE( UnserializeElements_MarkNotBeingUnserializedAndResolve );
		for ( int i = 0; i < nElementCount; ++i )
		{
			CDmElement *pElement = elementList[ i ];
			if ( pElement->GetFileId() == fileid )
			{
				// mark all unserialized elements as done unserializing, and call Resolve()
				CDmeElementAccessor::EnableOnChangedCallbacks( pElement );
				CDmeElementAccessor::FinishUnserialization( pElement );
			}
		}
	}

	g_pDmElementFrameworkImp->RemoveCleanElementsFromDirtyList( );
	return buf.IsValid();
}
