//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
// 
// Purpose: 
//
//=====================================================================================//

#include "blackbox.h"
#include "tier0/basetypes.h"
#include "cmd.h"
#include "tier1/utllinkedlist.h"
#include "tier1/convar.h"
#include "tier1/fmtstr.h"
#include "tier1/interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// development only, off by default for 360
ConVar blackbox( "blackbox", IsX360() ? "0" : "1" );

#define MAX_MESSAGE_SIZE 1024
#define DEFAULT_RECORD_LIMIT 16

static const char* gTypeMap[] = 
{
	"VCD",
	"WAV",
	"BOT",
	NULL
};

struct CBlackBoxRecord
{
	CBlackBoxRecord(const char *new_message) 
	{
		m_time = Plat_FloatTime();
		m_message = new char[strlen(new_message)+1];
		strcpy(m_message, new_message);
	}
	~CBlackBoxRecord() { if (m_message) delete m_message; };

	operator const char *() const	
	{
		static CFmtStrN<MAX_MESSAGE_SIZE+16> buf;

		double temp = m_time;
		int hh = int(temp/(60*60));
		int mm = int(temp/60) % 60;
		float ss = temp - ((mm + (hh * 60)) * 60);

		buf.sprintf( "[%02d:%02d:%02.3f]: %s", hh, mm, ss, m_message );

		return buf;
	}

	double m_time;
	char *m_message;
};

class CBlackBox: public IBlackBox
{
public:
	CBlackBox();
	~CBlackBox();

	virtual void Record(int type, const char *msg);
	virtual void SetLimit(int type, unsigned int count);
	virtual const char *Get(int type, unsigned int index);
	virtual int Count(int type);
	virtual void Flush(int type);

	virtual const char *GetTypeName(int type);
	virtual int GetTypeCount();

	enum Types {
		VCD,
		WAV,
		BOT,
		TYPE_COUNT
	};

private:
	bool ValidType(int type) { return (type >= 0 && type < TYPE_COUNT); };

	CUtlVector<CBlackBoxRecord *> m_records[TYPE_COUNT];
	uint m_record_limits[TYPE_COUNT];
};

CBlackBox gCBlackBox;
IBlackBox *gBlackBox = &gCBlackBox;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBlackBox, IBlackBox, BLACKBOX_INTERFACE_VERSION, gCBlackBox );

CBlackBox::CBlackBox()
{
	for ( int i = 0; i < TYPE_COUNT; i++ )
	{
		m_record_limits[i] = DEFAULT_RECORD_LIMIT;
	}
}

CBlackBox::~CBlackBox()
{
	for (int i = 0; i < TYPE_COUNT; i++)
	{
		Flush(i);
	}
}

void CBlackBox::Record( int type, const char *msg )
{
	if ( IsX360() || !blackbox.GetBool() )
		return;

	if ( !ValidType(type) )	
		return;

	CBlackBoxRecord *new_record = new CBlackBoxRecord( msg );
	
	m_records[type].AddToTail( new_record );
	if ( (uint)m_records[type].Count() > m_record_limits[type] )
	{
		CBlackBoxRecord *old_record = m_records[type].Head();		 
		m_records[type].Remove( 0 );
		delete old_record;
	}
}

void CBlackBox::SetLimit(int type, unsigned int count)
{
	if ( !ValidType( type ))
		return;

	m_record_limits[type] = count;
}

const char *CBlackBox::Get( int type, unsigned int index )
{
	if ( !ValidType( type ))
		return NULL;
	
	return (*m_records[type][index]);
}

const char *CBlackBox::GetTypeName( int type )
{
	if ( !ValidType( type ))
		return NULL;

	return gTypeMap[type];
}

int CBlackBox::GetTypeCount()
{
	return TYPE_COUNT;
}

int CBlackBox::Count( int type )
{
	if ( !ValidType( type ))
		return -1;

	return m_records[type].Count();
}

void CBlackBox::Flush( int type )
{
	if ( !ValidType( type ))
		return;

	m_records[type].PurgeAndDeleteElements();
}

CON_COMMAND_F( blackbox_record, "Record an entry into the blackbox", FCVAR_DONTRECORD )
{
	if ( IsX360() || !blackbox.GetBool() )
		return;

	if ( args.ArgC() < 2 )
	{
		Msg( "Insufficient arguments to blackbox_record. Usage: blackbox_record <type> <message>\n" );
		return;
	}

	BlackBox_Record( args[1], args[2] );
}

CON_COMMAND_F( blackbox_dump, "Dump the contents of the blackbox", FCVAR_DONTRECORD )
{
	if ( IsX360() )
		return;

	for ( int type = 0; type < gBlackBox->GetTypeCount(); type++ )
	{
		for ( int i = 0; i < gBlackBox->Count(type); i++ )
		{
			Msg( "%s[%d]: %s\n", gBlackBox->GetTypeName( type ), i+1, gBlackBox->Get( type, i ) );				
		}
	}
}

void BlackBox_Record( const char *type, const char *pFormat, ... )
{
	if ( IsGameConsole() || !blackbox.GetBool() )
		return;

	int type_num;
	for ( type_num = 0; type_num < gBlackBox->GetTypeCount(); type_num++ )
	{
		if ( !V_strcasecmp( gBlackBox->GetTypeName( type_num ), type ) )
			break;
	}

	if ( type_num >= gBlackBox->GetTypeCount() )
	{
		Msg( "Invalid blackbox type: %s\n", type );
		return;
	}

	char szMessage[1024];	
	va_list marker;

	va_start( marker, pFormat);
	Q_vsnprintf( szMessage, sizeof( szMessage ), pFormat, marker);
	va_end( marker );	

	//Msg( "Record: %s: %s\n", type, szMessage );
	gBlackBox->Record( type_num, szMessage );
}
