#ifndef __PS3_RTTI__
#define __PS3_RTTI__

#include <stdio.h>

namespace ps3_rtti
{
	template<class Klass>
	class type_trait
	{
	private:
		static const size_t m_nameSize=16;

	public:
		static const int m_typeTag;
		static char m_name[m_nameSize];
		static int TypeInfo() {return int(&m_typeTag);}
		static const char* TypeName() 
		{
			snprintf(m_name,m_nameSize,"%x",TypeInfo());
			return m_name;
		}
	};
}

#define TYPE_INFO int
#define TYPEID(t) ps3_rtti::type_trait<t>::TypeInfo()
#define TYPENAME(t) ps3_rtti::type_trait<t>::TypeName()
#define TYPE_INFO_EQUAL(i1, i2) i1 == i2 
#define INSTANCTIATE_TYPE(t)									\
	namespace ps3_rtti											\
	{															\
		template <> const int type_trait<t>::m_typeTag=0;		\
		template <> char type_trait<t>::m_name[16] = "";		\
	}

#define INVALID_TYPE_INFO int(0)

#endif
