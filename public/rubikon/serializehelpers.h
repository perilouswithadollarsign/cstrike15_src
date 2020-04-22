//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef SERIALIZE_HELPERS_HDR
#define SERIALIZE_HELPERS_HDR

#ifdef __clang__
# define CLANG_ATTR(ATTR) __attribute__((annotate( ATTR )))
#else
# define CLANG_ATTR(ATTR)
#endif

#define AUTO_SERIALIZE_AS( TYPE ) CLANG_ATTR( "auto_serialize_as:" #TYPE )
#define SERIALIZE_ARRAY_SIZE( SIZE ) CLANG_ATTR( "array_size:" #SIZE )
#define SERIALIZE_SHARED_ARRAY_SIZE( SIZE ) CLANG_ATTR( "shared_data;array_size:" #SIZE )


class CRnObjectStats;
class CRnSnooper;
class CRnUnserializer;
class CRnSerializer;

#define AUTO_SERIALIZE_(RET,ATTR) public: RET Serialize( CRnSerializer* pOut ) const ATTR; RET Unserialize( CRnUnserializer* pIn ) ATTR; RET Serialize( CRnObjectStats* pOut ) const ATTR; RET Snoop( CRnSnooper* pIn, const void *pLocalCopy ) ;
#define AUTO_SERIALIZE_BASE AUTO_SERIALIZE_( virtual bool, CLANG_ATTR( "auto_serialize base" ) ); virtual CUtlStringToken GetClassName() const = 0
#define AUTO_SERIALIZE AUTO_SERIALIZE_( bool, CLANG_ATTR( "auto_serialize" ) )
#define AUTO_SERIALIZE_LEAF AUTO_SERIALIZE_( virtual bool, CLANG_ATTR( "auto_serialize leaf" ) ); virtual CUtlStringToken GetClassName() const OVERRIDE;
#define AUTO_SERIALIZE_POSTINIT() CLANG_ATTR( "auto_serialize_postinit" )
#define AUTO_SERIALIZE_STRUCT(NAME) bool Serialize( CRnSerializer *pOut, const NAME &ref ) CLANG_ATTR("auto_serialize"); bool Unserialize( CRnUnserializer* pIn, NAME &ref ) CLANG_ATTR("auto_serialize"); bool Serialize( CRnObjectStats *pOut, const NAME &ref ) CLANG_ATTR("auto_serialize"); bool Snoop( CRnSnooper *pIn, const NAME *pLocalCopy, NAME &ref );
#define DECL_SERIALIZE_STRUCT(NAME) bool Serialize( CRnSerializer *pOut, const NAME &ref ); bool Unserialize( CRnUnserializer* pIn, NAME &ref ); bool Serialize( CRnObjectStats *pOut, const NAME &ref ) ; bool Snoop( CRnSnooper *pIn, const NAME *pLocalCopy, NAME &ref );
#define DECL_SERIALIZE_CLASS(NAME) void Serialize( CRnObjectStats *pOut, const NAME * const pObj ); void Serialize( CRnSerializer *pOut, const NAME * const pObj ); void Unserialize( CRnUnserializer *pIn, NAME *& refPtr );

#define SKIP_SERIALIZE CLANG_ATTR( "skip_serialize" )



#include "tier1/utlbufferstrider.h"
#include "serializehelpers.inl"

#endif