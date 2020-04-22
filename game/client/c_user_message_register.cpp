//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_user_message_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUserMessageRegisterBase *CUserMessageRegisterBase::s_pHead = NULL;

CUserMessageRegisterBase::CUserMessageRegisterBase()
{	
	// Link it in.
	m_pNext = s_pHead;
	s_pHead = this;
}


void CUserMessageRegisterBase::RegisterAll()
{
	for ( CUserMessageRegisterBase *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		pCur->Register();
	}
}



