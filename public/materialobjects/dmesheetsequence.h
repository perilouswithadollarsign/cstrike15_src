//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMESHEETSEQUENCE_H
#define DMESHEETSEQUENCE_H
#ifdef _WIN32
#pragma once
#endif
	
#include "materialobjects/amalgtexturevars.h"
#include "bitmap/floatbitmap.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


class CDmeSheetSequence;
class CDmeSheetImage : public CDmElement
{
	DEFINE_ELEMENT( CDmeSheetImage, CDmElement );

public:

	// Called when attributes change
	virtual	void OnAttributeChanged( CDmAttribute *pAttribute ) {}
	virtual void OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}
	virtual void OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}

	CDmeSheetSequence *FindSequence( int index );

	FloatBitMap_t *m_pImage;

	// where it ended up packed
	CDmaVar< int > m_XCoord;
	CDmaVar< int > m_YCoord;	

	CDmaElementArray< CDmeSheetSequence > m_mapSequences;
};

class CDmeSheetSequenceFrame : public CDmElement
{
	DEFINE_ELEMENT( CDmeSheetSequenceFrame, CDmElement );

public:

	// Called when attributes change
	virtual	void OnAttributeChanged( CDmAttribute *pAttribute ) {}
	virtual void OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}
	virtual void OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}

	// Up to 4 images can be used in a frame
	CDmaElementArray< CDmeSheetImage > m_pSheetImages;
	CDmaVar< float > m_fDisplayTime;
};

class CDmeSheetSequence : public CDmElement
{
	DEFINE_ELEMENT( CDmeSheetSequence, CDmElement );

public:

	// Called when attributes change
	virtual	void OnAttributeChanged( CDmAttribute *pAttribute ) {}
	virtual void OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}
	virtual void OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}

	CDmaVar< int > m_nSequenceNumber;	 
	CDmaVar< bool > m_Clamp; // as opposed to loop
	CDmaVar< int > m_eMode;
	CDmaElementArray< CDmeSheetSequenceFrame > m_Frames;

};



#endif // DMESHEETSEQUENCE_H