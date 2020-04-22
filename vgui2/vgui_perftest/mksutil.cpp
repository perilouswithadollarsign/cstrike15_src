//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Material editor
//=============================================================================

#include "mksutil.h"
#include "tier2/fileutils.h"


void CMksUtil::Init( const char *objectName )
{
	m_SequenceCount = 0;
	m_Name = objectName;
}

void CMksUtil::CreateNewSequenceEntry()
{
	int index = m_MksEntries.AddToTail();
	m_MksEntries[index].entryType = ENTRYTYPE_SEQUENCE;
	m_MksEntries[index].sequenceNumber = m_SequenceCount;
	m_SequenceCount++;	
}

void CMksUtil::CreateNewFrameEntry( const char* pFrameName, float displayTime )
{
	int index = m_MksEntries.AddToTail();
	m_MksEntries[index].entryType = ENTRYTYPE_FRAME;
	m_MksEntries[index].pFrameName = pFrameName;
	m_MksEntries[index].displayTime = displayTime;
}

CUtlString CMksUtil::GetName()
{
	return m_Name;
}

void CMksUtil::WriteFile()
{
	if ( m_Name.IsEmpty() )
	{
		Msg( "Error: No mks output filename set!\n" );
		return;
	}

	CUtlString mksFileName = m_Name;
	mksFileName += ".mks";

	char pMksFileFullPath[ MAX_PATH ];
	if ( !GenerateFullPath( mksFileName, NULL, pMksFileFullPath, sizeof( pMksFileFullPath ) ) )
	{
		Warning( "CDataModel: Unable to generate full path for file %s\n", mksFileName );
		return;
	}

	COutputTextFile Outfile( pMksFileFullPath );
	if ( !Outfile.IsOk() )
	{
		Msg( "Error: failed to write MKS \"%s\"!\n", pMksFileFullPath );
		return;
	}

	char buffer[33];
	for ( int i = m_MksEntries.Head(); i < m_MksEntries.InvalidIndex(); i = m_MksEntries.Next( i ) )
	{
		if ( m_MksEntries[i].entryType == ENTRYTYPE_SEQUENCE )
		{
			Outfile.Write( "\n", sizeof( char ) );
			Outfile.Write( "sequence ", sizeof( char ) * Q_strlen("sequence ") );
			itoa( m_MksEntries[i].sequenceNumber, buffer, 10 );
			Outfile.Write( buffer, sizeof( char ) * Q_strlen(buffer) );
		}
		else if ( m_MksEntries[i].entryType == ENTRYTYPE_FRAME )
		{
			Outfile.Write( "frame ", sizeof( char ) * Q_strlen("frame ") );
			Outfile.Write( m_MksEntries[i].pFrameName, sizeof( char ) * Q_strlen(m_MksEntries[i].pFrameName) );
			Outfile.Write( " ", sizeof( char ) );
			sprintf( buffer, "%.1f", m_MksEntries[i].displayTime ); 
			Outfile.Write( buffer, sizeof( char ) * Q_strlen(buffer) );
		}

		Outfile.Write( "\n", sizeof( char ) );
	}

	Msg( "Ok: successfully saved MKS \"%s\"\n", pMksFileFullPath );		 
}






