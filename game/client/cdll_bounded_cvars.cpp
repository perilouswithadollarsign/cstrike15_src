//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "cdll_bounded_cvars.h"
#include "convar_serverbounded.h"
#include "tier0/icommandline.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


bool g_bForceCLPredictOff = false;
bool g_bSpectatingForceCLPredictOff = false;
// ------------------------------------------------------------------------------------------ //
// cl_predict.
// ------------------------------------------------------------------------------------------ //

class CBoundedCvar_Predict : public ConVar_ServerBounded
{
public:
	CBoundedCvar_Predict() :
	  ConVar_ServerBounded( "cl_predict", 
		  "1.0", 
		  FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
		  "Perform client side prediction." )
	  {
	  }

	  virtual float GetFloat() const
	  {
		  // Used temporarily for CS kill cam.
		  if ( g_bForceCLPredictOff || g_bSpectatingForceCLPredictOff )
			  return 0;

		  static const ConVar *pClientPredict = g_pCVar->FindVar( "sv_client_predict" );
		  if ( pClientPredict && pClientPredict->GetInt() != -1 )
		  {
			  // Ok, the server wants to control this value.
			  return pClientPredict->GetFloat();
		  }
		  else
		  {
			  return GetBaseFloatValue();
		  }
	  }
};

static CBoundedCvar_Predict cl_predict_var;
ConVar_ServerBounded *cl_predict = &cl_predict_var;



// ------------------------------------------------------------------------------------------ //
// cl_interp_ratio.
// ------------------------------------------------------------------------------------------ //

class CBoundedCvar_InterpRatio : public ConVar_ServerBounded
{
public:
	CBoundedCvar_InterpRatio() :
	  ConVar_ServerBounded( "cl_interp_ratio", 
		  "2.0", 
		  FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
		  "Sets the interpolation amount (final amount is cl_interp_ratio / cl_updaterate)." )
	  {
	  }

	  virtual float GetFloat() const
	  {
		  static const ConVar *pMin = g_pCVar->FindVar( "sv_client_min_interp_ratio" );
		  static const ConVar *pMax = g_pCVar->FindVar( "sv_client_max_interp_ratio" );

		  float flBaseFloatValue = GetBaseFloatValue();
		  if ( engine->IsPlayingDemo() && engine->GetDemoPlaybackParameters() )
			  flBaseFloatValue = 2.0f; // use the default value when playing demos

		  if ( pMin && pMax && pMin->GetFloat() != -1 )
		  {
			  return clamp( flBaseFloatValue, pMin->GetFloat(), pMax->GetFloat() );
		  }
		  else
		  {
			  return flBaseFloatValue;
		  }
	  }
};

static CBoundedCvar_InterpRatio cl_interp_ratio_var;
ConVar_ServerBounded *cl_interp_ratio = &cl_interp_ratio_var;


// ------------------------------------------------------------------------------------------ //
// cl_interp
// ------------------------------------------------------------------------------------------ //

class CBoundedCvar_Interp : public ConVar_ServerBounded
{
public:
	CBoundedCvar_Interp() :
	  ConVar_ServerBounded( "cl_interp", 
		  "0.03125",		// 2 / 102.4
		  FCVAR_USERINFO | FCVAR_NOT_CONNECTED, 
		  "Sets the interpolation amount (bounded on low side by server interp ratio settings).", true, 0.0f, true, 0.5f )
	  {
	  }

	  virtual float GetFloat() const
	  {
		  static const ConVar *pUpdateRate = g_pCVar->FindVar( "cl_updaterate" );
		  static const ConVar *pMin = g_pCVar->FindVar( "sv_client_min_interp_ratio" );
		  
		  float flBaseFloatValue = GetBaseFloatValue();
		  if ( engine->IsPlayingDemo() && engine->GetDemoPlaybackParameters() && pUpdateRate )
			  flBaseFloatValue = 2.0f / pUpdateRate->GetFloat(); // use a default value 2/updaterate when playing demos

		  if ( pUpdateRate && pMin && pMin->GetFloat() != -1 )
		  {
			  return MAX( flBaseFloatValue, pMin->GetFloat() / pUpdateRate->GetFloat() );
		  }
		  else
		  {
			  return flBaseFloatValue;
		  }
	  }
};

static CBoundedCvar_Interp cl_interp_var;
ConVar_ServerBounded *cl_interp = &cl_interp_var;

float GetClientInterpAmount()
{
	static const ConVar *pUpdateRate = g_pCVar->FindVar( "cl_updaterate" );
	if ( pUpdateRate )
	{
		// #define FIXME_INTERP_RATIO
		float interp = cl_interp_ratio->GetFloat() / pUpdateRate->GetFloat();
		return interp;
	}
	else
	{
		AssertMsgOnce( false, "GetInterpolationAmount: can't get cl_updaterate cvar." );	
		return 0.1f;
	}
}

