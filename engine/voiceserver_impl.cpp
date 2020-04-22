//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: This module implements the IVoiceServer interface.
//
// $NoKeywords: $
//=============================================================================//

#include "quakedef.h"
#include "server.h"
#include "ivoiceserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CVoiceServer : public IVoiceServer
{
public:
	
	virtual bool	GetClientListening(int iReceiver, int iSender)
	{
		// Make into client indices..
		--iReceiver;
		--iSender;

		if(iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 || iSender >= sv.GetClientCount() )
			return false;

		return sv.GetClient(iSender)->IsHearingClient( iReceiver );
	}
	
	virtual bool	SetClientListening(int iReceiver, int iSender, bool bListen)
	{
		// Make into client indices..
		--iReceiver;
		--iSender;
		
		if(iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 || iSender >= sv.GetClientCount() )
			return false;

		CGameClient *cl = sv.Client(iSender);
			
		ConVarRef voice_verbose( "voice_verbose" );
		if ( voice_verbose.GetBool() && ( !!cl->m_VoiceStreams.Get( iReceiver ) != !!bListen ) )
		{
			Msg( "* CVoiceServer::SetClientListening:  %s m_VoiceStreams from %s (%s) to %s (%s)\n", bListen ? "Enable" : "Disable", cl->GetClientName(), cl->GetNetChannel() ? cl->GetNetChannel()->GetAddress() : "null", sv.Client(iReceiver)->GetClientName(), sv.Client(iReceiver)->GetNetChannel() ? sv.Client(iReceiver)->GetNetChannel()->GetAddress() : "null" );
		}

		cl->m_VoiceStreams.Set( iReceiver, bListen?1:0 );

		return true;
	}	
	virtual bool	SetClientProximity(int iReceiver, int iSender, bool bUseProximity)
	{
		// Make into client indices..
		--iReceiver;
		--iSender;

		if(iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 || iSender >= sv.GetClientCount() )
			return false;

		CGameClient *cl = sv.Client(iSender);

		cl->m_VoiceProximity.Set( iReceiver, bUseProximity );

		return true;
	}	
};


EXPOSE_SINGLE_INTERFACE(CVoiceServer, IVoiceServer, INTERFACEVERSION_VOICESERVER);
