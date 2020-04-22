//========= Copyright (c), Valve Corporation, All rights reserved. ============//
//
// Purpose: CEconCoupons - Limited time offers to purchase an econ item
//
//=============================================================================//
#pragma once

#include "gcsdk/protobufsharedobject.h"
#include "cstrike15_gcmessages.pb.h"

class CEconCoupon : public GCSDK::CProtoBufSharedObject< CSOEconCoupon, k_EEconTypeCoupon >
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL_MT( CEconCoupon );

	enum { k_MaxNumCoupons = 4 };	// Must match SCH declaration
#endif

public:
	const static int k_nTypeID = k_EEconTypeCoupon;
	virtual int GetTypeID() const
	{
		return k_nTypeID;
	}

#ifdef GC
//  virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess &sqlAccess, const CUtlVector< int > &fields );
// 	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess );
// 	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess &sqlAccess );
// 	void WriteToRecord( CSchCoupons *pSchRecord );
// 	void ReadFromRecord( const CSchCoupons &rSchRecord );
#endif

};
