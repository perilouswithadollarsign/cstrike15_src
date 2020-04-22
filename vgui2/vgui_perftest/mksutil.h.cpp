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

#include "vstdlib/cvar.h"
#include "appframework/vguimatsysapp.h"
#include "nowindows.h"
#include "FileSystem.h"
#include "materialsystem/IMaterialSystem.h"
#include "vgui/IVGui.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "tier0/dbg.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/AnimationController.h"
#include "tier0/icommandline.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "filesystem_init.h"
#include "vstdlib/iprocessutils.h"
#include "matsys_controls/matsyscontrols.h"
#include "matsys_controls/mdlpicker.h"
#include "IStudioRender.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vgui_controls/frame.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/materialsystemutil.h"
#include "tier3/tier3.h"
#include "vgui_controls/consoledialog.h"
#include "icvar.h"
#include "vgui/keycode.h"
#include "vguimaterial.h"
#include "tier0/vprof.h"  
#include "tier0/progressbar.h"
#include "amalg_texture_vars.h"
#include "amalg_texture.h"
#include "dmeamalgtexture.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier2/fileutils.h"

#include "amalg_texture_parser.h"
#include "dmserializers/idmserializers.h"


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMksUtil 
{
public:

	enum 
	{
		ENTRYTYPE_SEQUENCE,
		ENTRYTYPE_FRAME,
		ENTRYTYPE_MAX
	};

	struct sMKSInfo
	{
		int entryType;
		int sequenceNumber;
		const char *pFrameName; // array?
		float displayTime;
	};

	void GenerateMKSEntries();
	void GenerateMKSFile( const char *pMksFileName );
	void CreateNewSequenceEntry();
	void CreateNewFrameEntry( const char* pFrameName, float displayTime = 1.0f );

private:

	CUtlLinkedList< sMKSInfo > m_MksEntries;
	int m_SequenceCount;
};


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

void CMksUtil::GenerateMKSFile( const char *pMksFileName )
{
	if ( pMksFileName == NULL )
	{
		Msg( "Error: No mks output filename set!\n" );
		return;
	}

	char pMksFileFullPath[ MAX_PATH ];
	if ( !GenerateFullPath( pMksFileName, NULL, pMksFileFullPath, sizeof( pMksFileFullPath ) ) )
	{
		Warning( "CDataModel: Unable to generate full path for file %s\n", pMksFileName );
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




