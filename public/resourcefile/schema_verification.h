#ifndef SCHEMA_VERIFICATION_H_
#define SCHEMA_VERIFICATION_H_
/*
inline int SchemaVerificationMemberSize( const char *pStructName, const char *pMemberName )
{
	const CResourceStructIntrospection* pStructIntro = g_pResourceSystem->FindStructIntrospection( pStructName );

	if ( !pStructIntro )
	{
		return -1;
	}

	const CResourceFieldIntrospection* pFieldIntro = pStructIntro->FindField( pMemberName );

	if ( !pFieldIntro )
	{
		return -1;
	}

	return pFieldIntro->GetElementSize(0);
}

inline int SchemaVerificationMemberMemoryOffset( const char *pStructName, const char *pMemberName )
{
	const CResourceStructIntrospection* pStructIntro = g_pResourceSystem->FindStructIntrospection( pStructName );

	if ( !pStructIntro )
	{
		return -1;
	}

	const CResourceFieldIntrospection* pFieldIntro = pStructIntro->FindField( pMemberName );

	if ( !pFieldIntro )
	{
		return -1;
	}

	return pFieldIntro->m_nInMemoryOffset;
}
*/
template<class T> class CTAlignmentOfHelper
{
	T t; byte b; // the extra byte will force the compiler to pad it out by an additional alignment
};

#define schema_alignmentof( _className ) ( sizeof( CTAlignmentOfHelper<_className> ) - sizeof(_className) )


#define VERIFY_FOR_SCHEMA( _description, _expectedValue, _value ) \
{ \
	if ( ( (_expectedValue) != (_value) ) ) \
	{ \
		Warning( "[FAILED] - " _description " - Expected %d but got %d\n", (_expectedValue), (_value) ); \
		nLocalErrors++; \
	} \
	else \
	{ \
		Msg( "[  OK  ] - " _description "\n" ); \
	} \
}

#define VERIFY_SCHEMA_MEMBER_SIZE( _className, _memberName, _expectedSize ) \
	VERIFY_FOR_SCHEMA( "Member size of " #_className "::" #_memberName, _expectedSize, sizeof( (( _className *)(0))->_memberName ) );

#define VERIFY_SCHEMA_MEMBER_MEMORY_OFFSET( _className, _memberName, _expectedOffset ) \
	VERIFY_FOR_SCHEMA( "Member offset of " #_className "::" #_memberName, _expectedOffset, offsetof( _className , _memberName ) );

#define VERIFY_SCHEMA_TYPE_MEMORY_SIZE( _className, _expectedSize ) \
	VERIFY_FOR_SCHEMA( "Struct size of " #_className, _expectedSize, sizeof( _className ) );

#define VERIFY_SCHEMA_TYPE_ALIGNMENT( _className, _expectedAlignment ) \
	VERIFY_FOR_SCHEMA( "Struct alignment of " #_className, _expectedAlignment, schema_alignmentof( _className ) );

#define VERIFY_SCHEMA_MEMBER_FIX_ARRAY_ELEMENT_SIZE( _className, _memberName, _expectedSize ) \
	VERIFY_FOR_SCHEMA( "Member array element size of " #_className "::" #_memberName, _expectedSize, sizeof( (( _className *)(0))->_memberName [ 0 ] ) );

#define VERIFY_SCHEMA_MEMBER_FIX_ARRAY_LENGTH( _className, _memberName, _expectedLength ) \
	VERIFY_FOR_SCHEMA( "Member array length of " #_className "::" #_memberName, _expectedLength, ( sizeof( (( _className *)(0))->_memberName ) / sizeof( (( _className *)(0))->_memberName [ 0 ] ) ) );

#define BEGIN_SCHEMA_CLASS_VERIFY( _className ) \
	class CSchemaVerificationFor##_className { \
	public: static int DoVerify() { \
	int nLocalErrors = 0; \
	Msg( "Schema Verification: " #_className "\n" );

#define END_SCHEMA_CLASS_VERIFY( ) \
	Msg( "\n\n" ); \
	return nLocalErrors; \
	} };

#define PERFORM_SCHEMA_CLASS_VERIFY( _className ) nErrors += CSchemaVerificationFor##_className::DoVerify();

#endif
