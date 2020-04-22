//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
#include "cgutils.h"
#include "tier0/dbg.h"

struct DatatypeRec_t
{
	CGtype type;
	CGparameterclass parameterClass;
};


static DatatypeRec_t s_datatypeClassname[] = {
#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
    { enum_name, classname },
#include <Cg/cg_datatypes.h>
#undef CG_DATATYPE_MACRO
};


CGparameterclass vcgGetTypeClass( CGtype type )
{
	if( type <= CG_TYPE_START_ENUM || type > CG_TYPE_START_ENUM + sizeof( s_datatypeClassname ) / sizeof( s_datatypeClassname[0] ) )
	{	
		return CG_PARAMETERCLASS_UNKNOWN;
	}
	else
    {
		DatatypeRec_t & rec = s_datatypeClassname[type - CG_TYPE_START_ENUM - 1];
		Assert( rec.type == type );
		return rec.parameterClass;
    }
}
