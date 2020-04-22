//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#ifndef PAINTABLE_ENTITY_BASE_H
#define PAINTABLE_ENTITY_BASE_H

#ifndef CLIENT_DLL
#include "paint_database.h"
#endif // ifndef CLIENT_DLL

#include "paint_color_manager.h"

abstract_class IPaintableEntity
{
public:
	virtual ~IPaintableEntity() {}
	virtual PaintPowerType GetPaintPowerAtPoint( const Vector& contact ) const = 0;
	virtual void Paint( PaintPowerType type, const Vector& worldContactPt ) = 0;
	virtual void CleansePaint() = 0;
	virtual PaintPowerType GetPaintedPower() const = 0;
};


template< typename BaseEntityType >
class CPaintableEntity : public BaseEntityType, public IPaintableEntity
{
	DECLARE_CLASS( CPaintableEntity, BaseEntityType );
	DECLARE_DATADESC();
	static const datamap_t DataMapInit();

public:
	CPaintableEntity();
	virtual ~CPaintableEntity();
	virtual PaintPowerType GetPaintPowerAtPoint( const Vector& worldContactPt ) const;
	virtual void Paint( PaintPowerType type, const Vector& worldContactPt );
	virtual void CleansePaint();
	virtual PaintPowerType GetPaintedPower() const;

private:
	PaintPowerType m_iPaintPower;
};

// OMFG HACK: Define the data description table. The current macros don't work with templatized classes.
// OMFG TODO: Write a generic macro to work with templatized classes.
template< typename BaseEntityType >
datamap_t CPaintableEntity<BaseEntityType>::m_DataMap = CPaintableEntity<BaseEntityType>::DataMapInit();

template< typename BaseEntityType >
datamap_t* CPaintableEntity<BaseEntityType>::GetDataDescMap()
{
	return &m_DataMap;
}


template< typename BaseEntityType >
datamap_t* CPaintableEntity<BaseEntityType>::GetBaseMap()
{
	datamap_t *pResult;
	DataMapAccess((BaseClass *)NULL, &pResult);
	return pResult;
}

const char PAINTABLE_ENTITY_USER_DATA_CLASS_NAME[] = "PaintableEntity";

template< typename BaseEntityType >
const datamap_t CPaintableEntity<BaseEntityType>::DataMapInit()
{
	typedef CPaintableEntity<BaseEntityType> classNameTypedef;
	static CDatadescGeneratedNameHolder nameHolder(PAINTABLE_ENTITY_USER_DATA_CLASS_NAME);
	static typedescription_t dataDesc[] =
	{
		DEFINE_FIELD( m_iPaintPower, FIELD_INTEGER )
	};

	datamap_t dataMap = { dataDesc, SIZE_OF_ARRAY(dataDesc), PAINTABLE_ENTITY_USER_DATA_CLASS_NAME, CPaintableEntity<BaseEntityType>::GetBaseMap() };
	return dataMap;
}

template< typename BaseEntityType >
CPaintableEntity< BaseEntityType >::CPaintableEntity()
			: m_iPaintPower( NO_POWER )
{
}

template< typename BaseEntityType >
CPaintableEntity< BaseEntityType >::~CPaintableEntity()
{
	#ifndef CLIENT_DLL
	//PaintDatabase.RemovePaintedEntity( this );
	#endif // ifndef CLIENT_DLL
}


template< typename BaseEntityType >
PaintPowerType CPaintableEntity< BaseEntityType >::GetPaintPowerAtPoint( const Vector& worldContactPt ) const
{
	return m_iPaintPower;
}


template< typename BaseEntityType >
void CPaintableEntity< BaseEntityType >::Paint( PaintPowerType type, const Vector& worldContactPt )
{
	m_iPaintPower = type;
}


template< typename BaseEntityType >
void CPaintableEntity< BaseEntityType >::CleansePaint()
{
	this->Paint( NO_POWER, vec3_origin );
}

template< typename BaseEntityType >
PaintPowerType CPaintableEntity< BaseEntityType >::GetPaintedPower() const
{
	return m_iPaintPower;
}


#endif //PAINTABLE_ENTITY_BASE_H
