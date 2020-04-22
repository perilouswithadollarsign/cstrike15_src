//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef DATA_LINKER_INTERFACE_H
#define DATA_LINKER_INTERFACE_H


#include "tier0/platform.h"
#include "tier0/basetypes.h"
#include "tier0/dbg.h"



template <uint nAlignment = 1, uint nCookie = 0>
struct DataLinkerClassProperties
{
	enum {kDataAlignment = nAlignment};
	enum {kDataCookie = nCookie};// 0 means no cookie
};

template <typename T>
struct DataLinkerClassPropertiesSelector: public DataLinkerClassProperties<sizeof(T)&1?1:sizeof(T)&3?2:4> // heuristic: 1-, 2- and 4-byte aligned type size calls for 1-, 2- and 4-byte alignment by default
{
};



//#define DATALINKER_ALIGNMENT(ALIGNMENT) enum {kDataAlignment = ALIGNMENT}


#define DATALINKER_CLASS_ALIGNMENT(CLASS, ALIGNMENT) template <> struct DataLinkerClassPropertiesSelector<CLASS> {enum {kDataAlignment = ALIGNMENT};}

DATALINKER_CLASS_ALIGNMENT(float,4);
DATALINKER_CLASS_ALIGNMENT(int32,4);
DATALINKER_CLASS_ALIGNMENT(int16,2);
DATALINKER_CLASS_ALIGNMENT(int8,1);
DATALINKER_CLASS_ALIGNMENT(uint32,4);
DATALINKER_CLASS_ALIGNMENT(uint16,2);
DATALINKER_CLASS_ALIGNMENT(uint8,1);

// undefine DATALINKER_CHECK_COOKIES if you don't want runtime cookie checks
#define DATALINKER_CHECK_COOKIES

namespace DataLinker
{


inline byte *ResolveOffset(int32 *pOffset)
{
	int offset = *pOffset;
	return offset ? ((byte*)pOffset) + offset : NULL;
}

inline byte *ResolveOffsetFast(const int32 *pOffset)
{
	int offset = *pOffset;
	Assert(offset != 0);
	return ((byte*)pOffset) + offset;
}




// AT RUN-TIME ONLY the offset converts automatically into pointers to the appropritate type 
// at tool-time, you should use LinkSource_t and LinkTarget_t
template <typename T, int32 nCookie = 0>
struct Offset_t
{
	enum {kDataAlignment = 4};

	int32 offset;
	bool operator == (int zero)const {Assert(zero == 0); return offset == zero;}
	bool IsNull()const {return offset == 0;}
	int32 NotNull()const {return offset;}
	const T* GetPtr()const
	{
		// validate
		byte *ptr = ResolveOffsetFast(&offset);
#ifdef DATALINKER_CHECK_COOKIES
		if(nCookie)
		{
			if(nCookie != ((int*)ptr)[-1])
			{
				Error("Invalid data cookie %d != %d\n", ((int*)ptr)[-1], nCookie);
			}
		}
#endif
		return (const T*)ptr;
	}
	//const T* GetPtrFast()const {return (const T*)ResolveOffsetFast(&offset);}
	operator const T* ()const{return GetPtr();}
	const T* operator ->() const{return GetPtr();}
};

template <typename T,int32 nCookie = 0>
struct OffsetAndSize_t: public Offset_t<T,nCookie>
{
	int32 size;

	const T& operator []( int index ) const
	{
		Assert(index < size);
		return this->GetPtr()[index];
	}
};

template <typename T,int32 nCookie = 0>
struct OffsetSizeAndStride_t : public OffsetAndSize_t<T,nCookie>
{
	int32 stride;
	const T& operator []( int index ) const
	{
		Assert(index < this->size);
		return *(const T*)(ResolveOffsetFast(&this->offset) + stride * index);
	}
};


enum {kSpecialTargetUndefined = -1};
enum {kSpecialTargetNull = 0};
enum {kSpecialTargetDefault = 1};// resolved pointer/offset right in the LinkSource 

// this is resolved or unresolved reference within the data block
// it's unique for the life cycle of the Stream and must either be resolved or
// never referenced by the end
struct LinkTarget_t
{
	int m_id;
	friend class Stream;
	LinkTarget_t(){m_id = kSpecialTargetUndefined;} // -1 is anonymous/undefined reference
};



struct LinkTargetNull_t: public LinkTarget_t
{
	LinkTargetNull_t(){m_id = kSpecialTargetNull;} // 0 is special default case meaning null pointer/offset 
};

/*
template <typename T>
struct Target_t
{
	T* m_ptr;
	operator T* () {return m_ptr;}
	T* operator -> () {return m_ptr;}
};
*/

struct LinkSource_t
{
protected:
	int m_offset;
	friend class Stream;
	LinkSource_t(){}
};




enum OffsetTypeEnum
{
	kOtRelative32bit,
	kOtRelative16bit,
	kOtAbsolute32bit
};


//////////////////////////////////////////////////////////////////////////
// this class may be useful at runtime to use fast serialize interface (without data linking)
// 
abstract_class IBasicStream
{
public:
	virtual void* WriteBytes(uint numBytes) = 0;

	virtual void Align(uint nAlignment, int nOffset = 0) = 0;
	inline void AlignPointer(){Align(4);}
	virtual long Tell() = 0;

	// the Begin/End semantics differ in different implementations
	// in some, it can be ignored, some others can use it for statistics and some others may set fixed page boundaries based on the names and/or flags 
	virtual void Begin(const char *nName, uint flags = 0) = 0;
	virtual void End() = 0;
	virtual void PrintStats() = 0;
	virtual void ClearStats() = 0;

	template <typename T>
	T* Write(uint count = 1);

	template <typename T, int32 nCookie>
	T* WriteWithCookie(uint count = 1);

	template <typename T, int32 nCookie>
	T* WriteWithCookieStrided(uint count, uint stride);

	template <typename T>
	void WriteSimple(const T x)
	{
		*Write<T>() = x;
	}

	virtual void EnsureAvailable(uint addCapacity) = 0;

	inline void WriteFloat(float x)
	{
		WriteSimple(x);
	}
	inline void WriteU32(uint32 x)
	{
		WriteSimple(x);
	}
	inline void WriteU16(uint16 x) 
	{
		WriteSimple(x);
	}
	inline void WriteByte(byte x)
	{
		WriteSimple(x);
	}

};


//////////////////////////////////////////////////////////////////////////
// this is light-weight version of IStream with Links
// It can link on-the-fly, has minimal overhead but cannot late-bind symbols
// you basically have to manage all your late-bindings by managing 
// pointers to offsets, which is fine in 99% of use cases.
// WARNING: there are no error-checking facilities here. If you forget to set 
//          a link, you'll have a NULL offset and no warnings or errors
//
abstract_class IStream: public IBasicStream
{
public:
	template <typename T,int32 nCookie>	T* WriteAndLink(Offset_t<T,nCookie>*pOffset, uint count = 1);
	template <typename T,int32 nCookie>	T* WriteAndLink(OffsetAndSize_t<T,nCookie>*pOffset, uint count = 1); // intentionally not implemented - use explicit WriteAndLinkArray()
	template <typename T,int32 nCookie>	T* WriteAndLinkArray(OffsetAndSize_t<T,nCookie>*pOffsetAndSize, uint count = 1);
	template <typename T,int32 nCookie>	T* WriteAndLinkStrided(OffsetSizeAndStride_t<T,nCookie>*pOffset, uint nStride, uint count = 1);
	template <typename T,int32 nCookie>	void Link(Offset_t<T,nCookie>*pOffset, const T* pTarget);

	virtual void Link(int32 *pOffset, const void *pTarget) = 0;
	virtual void Link(int16 *pOffset, const void *pTarget) = 0;

	virtual bool Compile(void *pBuffer) = 0;
	virtual uint GetTotalSize()const = 0;
};


//////////////////////////////////////////////////////////////////////////
// This is IStream with late-binding capabilities. You can link multiple sources to 
// the same target, then change your mind and reset the target somewhere and it'll
// automatically track all sources and reset them to the latest target at compilation.
// You can create unresolved targets to resolve them later.
// You can extend this to multiple object files linked at later stage (szDescription 
// must be unique identifiers)
// 
abstract_class ILinkStream: public IStream
{
public:
	virtual LinkSource_t WriteOffset(const char *szDescription) = 0;
	virtual LinkSource_t WriteOffset(LinkTarget_t linkTarget, const char *szDescription) = 0;
	virtual LinkSource_t WriteNullOffset(const char *szDescription) = 0;

	virtual void Link(LinkSource_t, LinkTarget_t, const char *szDescription) = 0;
	virtual LinkSource_t LinkToHere(int32 *pOffset, const char *szDescription) = 0;
	virtual LinkSource_t Link(int32 *pOffset, LinkTarget_t linkTarget, const char *szDescription) = 0;

	virtual LinkTarget_t NewTarget() = 0; // create new, unresolved target
	virtual LinkTarget_t NewTarget(void *pWhere) = 0;
	virtual LinkTarget_t NewTargetHere() = 0; // creates a target right here
	virtual void SetTargetHere(LinkTarget_t) = 0; // sets the given target to point to right here
	virtual void SetTargetNull(LinkTarget_t) = 0; // set this target to point to NULL
	virtual LinkSource_t NewOffset(int *pOffset, const char *szDescription) = 0;

	virtual bool IsDeclared(LinkTarget_t linkTarget)const = 0;
	virtual bool IsSet(LinkTarget_t linkTarget)const = 0;
	virtual bool IsDefined(LinkSource_t linkSource)const = 0;
	virtual bool IsLinked(LinkSource_t linkSource)const = 0;
};



template <typename T>
inline T* IBasicStream::Write(uint count)
{
	// TODO: insert reflection code here
	uint nAlignment = DataLinkerClassPropertiesSelector<T>::kDataAlignment;
	Align(nAlignment);
	return (T*)WriteBytes(count * sizeof(T));
}



template <typename T, int32 nCookie>
inline T* IBasicStream::WriteWithCookie(uint count)
{
	// TODO: insert reflection code here
	uint nAlignment = DataLinkerClassPropertiesSelector<T>::kDataAlignment;

	if(nCookie)
	{
		if(nAlignment > sizeof(int32))
		{
			Align(nAlignment, -4);
		}
		else
		{
			Align(sizeof(int32));// we want the cookie itself aligned properly
		}
		WriteSimple<int32>(nCookie);
		Assert((Tell()&(nAlignment-1)) == 0); // must be correctly aligned by now
	}
	else
	{
		if(nAlignment > 1)
		{
			Align(nAlignment);
		}
	}
	return (T*)WriteBytes(count * sizeof(T));
}


template <typename T, int32 nCookie>
inline T* IBasicStream::WriteWithCookieStrided(uint count, uint stride)
{
	// TODO: insert reflection code here
	uint nAlignment = DataLinkerClassPropertiesSelector<T>::kDataAlignment;

	if(nCookie)
	{
		if(nAlignment > sizeof(int32))
		{
			Align(nAlignment, -4);
		}
		else
		{
			Align(sizeof(int32));// we want the cookie itself aligned properly
		}
		WriteSimple<int32>(nCookie);
		Assert((Tell()&(nAlignment-1)) == 0); // must be correctly aligned by now
	}
	else
	{
		if(nAlignment > 1)
		{
			Align(nAlignment);
		}
	}
	if(count)
		return (T*)WriteBytes((count-1) * stride + sizeof(T));
	else
		return (T*)WriteBytes(0);
}


template <typename T,int32 nCookie>
inline void IStream::Link(Offset_t<T,nCookie>*pOffset, const T* pTarget)
{
	Link(&pOffset->offset, pTarget);
}


template <typename T, int32 nCookie>
inline T* IStream::WriteAndLink(Offset_t<T,nCookie>*pOffset, uint count)
{
	T* p = WriteWithCookie<T,nCookie>(count);
	Link(&pOffset->offset, p);
	return p;
}

template <typename T, int32 nCookie>
inline T* IStream::WriteAndLinkArray(OffsetAndSize_t<T,nCookie>*pOffsetAndSize, uint count)
{
	pOffsetAndSize->size = count;
	if(count)
	{
		T* p = WriteWithCookie<T,nCookie>(count);
		Link(&pOffsetAndSize->offset, p);
		return p;
	}
	else
	{
		pOffsetAndSize->offset = 0;
		return NULL;
	}
}

template <typename T, int32 nCookie>
inline T* IStream::WriteAndLinkStrided(OffsetSizeAndStride_t<T,nCookie>*pOffset, uint stride, uint count)
{
	pOffset->size = count;
	pOffset->stride = stride;
	if(count)
	{
		T* p = WriteWithCookieStrided<T,nCookie>(count, stride);
		Link(&pOffset->offset, p);
		return p;
	}
	else
	{
		pOffset->offset = 0;
		return NULL;
	}
}



}


struct DataLinkerBasicStreamRange_t
{
	DataLinker::IBasicStream *m_pStream;
	DataLinkerBasicStreamRange_t(DataLinker::IBasicStream  *pStream, const char *name): m_pStream(pStream){pStream->Begin(name);}
	~DataLinkerBasicStreamRange_t(){m_pStream->End();}
};

#define DATALINKER_RANGE(STREAM,NAME) DataLinkerBasicStreamRange_t dataLinker_basicStreamRange##__LINE((STREAM),(NAME))

#endif
