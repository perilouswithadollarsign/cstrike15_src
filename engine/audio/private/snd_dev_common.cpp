//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Device Common Base Class.
//
//=====================================================================================//

#include "audio_pch.h"
#include "../../cl_splitscreen.h"
#include "snd_dma.h"
#include "../../debugoverlay.h"
#include "server.h"
#include "client.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


#define ISPEAKER_RIGHT_FRONT	0
#define ISPEAKER_LEFT_FRONT		1
#define ISPEAKER_RIGHT_REAR		2
#define ISPEAKER_LEFT_REAR		3
#define ISPEAKER_CENTER_FRONT	4
#define ISPEAKER_DIR_STEREO		99

extern Vector		listener_right[ MAX_SPLITSCREEN_CLIENTS ];

extern void DEBUG_StartSoundMeasure(int type, int samplecount );
extern void DEBUG_StopSoundMeasure(int type, int samplecount );
extern bool MIX_ScaleChannelVolume( paintbuffer_t *pPaint, channel_t *pChannel, float volume[CCHANVOLUMES], int mixchans );

inline bool FVolumeFrontNonZero( float *pvol )
{
	return (pvol[IFRONT_RIGHT] || pvol[IFRONT_LEFT]);
}

inline bool FVolumeRearNonZero( float *pvol )
{
	return (pvol[IREAR_RIGHT] || pvol[IREAR_LEFT]);
}

inline bool FVolumeCenterNonZero( float *pvol )
{
	return (pvol[IFRONT_CENTER] != 0);
}

// fade speaker volumes to mono, based on xfade value.
// ie: xfade 1.0 is full mono.
// ispeaker is speaker index, cspeaker is total # of speakers
// fmix2channels causes mono mix for 4 channel mix to mix down to 2 channels
//    this is used for the 2 speaker outpu case, which uses recombined 4 channel front/rear mixing

static float XfadeSpeakerVolToMono( float scale, float xfade, int ispeaker, int cspeaker, bool fmix2channels )
{
	float scale_out;
	float scale_target;

	if (cspeaker == 4 )
	{
		// mono sound distribution:
		float scale_targets[]    = {0.9, 0.9, 0.9, 0.9};	// RF, LF, RR, LR
		float scale_targets2ch[] = {0.9, 0.9, 0.0, 0.0};	// RF, LF, RR, LR

		if ( fmix2channels )
			scale_target = scale_targets2ch[(int)clamp(ispeaker, 0, 3)];
		else
			scale_target = scale_targets[(int)clamp(ispeaker, 0, 3)];

		goto XfadeExit;
	}

	if (cspeaker == 5 )
	{
		// mono sound distribution:
		float scale_targets[] = {0.9, 0.9, 0.5, 0.5, 0.9};	// RF, LF, RR, LR, FC
		scale_target = scale_targets[(int)clamp(ispeaker, 0, 4)];
		goto XfadeExit;
	}

	// if (cspeaker == 2 )
	scale_target = 0.9; // front 2 speakers in stereo each get 50% of total volume in mono case
	
XfadeExit:
	scale_out = scale + (scale_target - scale) * xfade;
	return scale_out;
}

// given:
//  2d yaw angle to sound source (0-360), where 0 is listener_right
//  pitch angle to source
//  angle to speaker position (0-360), where 0 is listener_right
//  speaker index
//  speaker total count,
// return: scale from 0-1.0 for speaker volume.
// NOTE: as pitch angle goes to +/- 90, sound goes to mono, all speakers.

#define PITCH_ANGLE_THRESHOLD	45.0
#define REAR_VOL_DROP			0.5
#define VOLCURVEPOWER			1.5		// 1.0 is a linear crossfade of volume between speakers.
										// 1.5 provides a smoother, nonlinear volume transition - this is done
										// because a volume of 255 played in a single speaker is
										// percieved as louder than 128 + 128 in two speakers
										// separated by at least 45 degrees.  The nonlinear curve
										// gives the volume boost needed.



static float GetSpeakerVol( float yaw_source, float pitch_source, float mono, float yaw_speaker, int ispeaker, int cspeaker, bool fmix2channels )
{
	float adif = fabs(yaw_source - yaw_speaker);
	float pitch_angle = pitch_source;
	float scale = 0.0;
	float xfade = 0.0;

	if (adif > 180)
		adif = 360 - adif;

	// mono goes from 0.0 to 1.0 as listener moves into 'mono' radius of sound source.
	// Also, as pitch_angle to sound source approaches 90 (sound above/below listener), sounds become mono.

	// convert pitch angle to 0-90 absolute pitch
	if (pitch_angle < 0)
		pitch_angle += 360;

	if (pitch_angle > 180)
		pitch_angle = 360 - pitch_angle;

	if (pitch_angle > 90)
		pitch_angle = 90 - (pitch_angle - 90);

	// calculate additional mono crossfade due to pitch angle
	if (pitch_angle > PITCH_ANGLE_THRESHOLD)
	{
		xfade = (pitch_angle - PITCH_ANGLE_THRESHOLD) / (90.0 - PITCH_ANGLE_THRESHOLD);	// 0.0 -> 1.0 as angle 45->90	

		mono += xfade;
		mono = clamp(mono, 0.0, 1.0);
	}

	if (cspeaker == 2)
	{
		// 2 speaker (headphone) mix: speakers opposing, at 0 & 180 degrees

		scale = (1.0 - FastPow(adif / 180.0, VOLCURVEPOWER));

		goto GetVolExit;
	}

	if (adif >= 90.0)
		goto GetVolExit;	// 0.0 scale

	if (cspeaker == 4)
	{
		// 4 ch surround: all speakers on 90 degree angles, 
		// scale ranges from 0.0 (at 90 degree difference between source and speaker)
		// to 1.0 (0 degree difference between source and speaker)

		if (ispeaker == ISPEAKER_DIR_STEREO)
		{
			scale = (1.0 - FastPow(adif / 135.0f, VOLCURVEPOWER));
		}
		else
		{
			scale = (1.0 - FastPow(adif / 90.0, VOLCURVEPOWER));
		}

		goto GetVolExit;
	}

	// 5 ch surround: 

	// rear speakers are on 90 degree angles and return 0.0->1.0 range over +/- 90 degrees each
	// center speaker is on 45 degree angle to left/right front speaker
	// center speaker has 0.0->1.0 range over 45 degrees

	switch (ispeaker)
	{
	default:
	case ISPEAKER_RIGHT_REAR:
	case ISPEAKER_LEFT_REAR:
	{
		// rear speakers get +/- 90 degrees of linear scaling...
		scale = (1.0 - FastPow(adif / 90.0, VOLCURVEPOWER));
		break;
	}

	case ISPEAKER_CENTER_FRONT:
	{
		// center speaker gets +/- 45 degrees of linear scaling...
		if (adif > 45.0)
			goto GetVolExit;	// 0.0 scale

		scale = (1.0 - FastPow(adif / 45.0, VOLCURVEPOWER));
		break;
	}
	case ISPEAKER_RIGHT_FRONT:
	{
		if (yaw_source > yaw_speaker)
		{
			// if sound source is between right front speaker and center speaker, 
			// apply scaling over 75 degrees...

			if (adif > 75.0)
				goto GetVolExit;	// 0.0 scale

			scale = (1.0 - FastPow(adif / 75.0, VOLCURVEPOWER));
		}
		/*
		if (yaw_source > yaw_speaker && yaw_source < (yaw_speaker + 90.0))
		{
		// if sound source is between right front speaker and center speaker,
		// apply scaling over 45 degrees...
		if (adif > 45.0)
		goto GetVolExit;	// 0.0 scale

		scale = (1.0 - FastPow(adif/45.0, VOLCURVEPOWER));
		}
		*/
		else
		{
			// sound source is CW from right speaker, apply scaling over 90 degrees...
			scale = (1.0 - FastPow(adif / 90.0, VOLCURVEPOWER));
		}

		break;
	}

	case ISPEAKER_LEFT_FRONT:
	{
		if (yaw_source < yaw_speaker)
		{
			// if sound source is between left front speaker and center speaker, 
			// apply scaling over 75 degrees...

			if (adif > 75.0)
				goto GetVolExit;	// 0.0 scale

			scale = (1.0 - FastPow(adif / 75.0, VOLCURVEPOWER));

		}
		/*
		if (yaw_source < yaw_speaker && yaw_source > (yaw_speaker - 90.0))
		{
		// if sound source is between left front speaker and center speaker,
		// apply scaling over 45 degrees...
		if (adif > 45.0)
		goto GetVolExit;	// 0.0 scale

		scale = (1.0 - FastPow(adif/45.0, VOLCURVEPOWER));

		}
		*/
		else
		{
			// sound source is CW from right speaker, apply scaling over 90 degrees...
			scale = (1.0 - FastPow(adif / 90.0, VOLCURVEPOWER));
		}
		break;
	}
	}

GetVolExit:
	Assert(mono <= 1.0 && mono >= 0.0);
	Assert(scale <= 1.0 && scale >= 0.0);

	// crossfade speaker volumes towards mono with increased pitch angle of sound source

	scale = XfadeSpeakerVolToMono(scale, mono, ispeaker, cspeaker, fmix2channels);

	Assert(scale <= 1.0 && scale >= 0.0);

	return scale;
}

extern ConVar sv_cheats;

// counterstrike options
#define SND_SVACTIVE_CONCMD( VARIABLENAME, DEFAULTVALUE, DESCSTRING ) \
	float g_##VARIABLENAME = DEFAULTVALUE; \
CON_COMMAND( VARIABLENAME , DESCSTRING ) \
{ \
	if ( args.ArgC () < 2 ) \
	{ \
	ConMsg ("%s = %f\n", #VARIABLENAME, g_##VARIABLENAME ); \
	} \
	else if ( sv.IsActive() && !sv_cheats.GetBool() ) \
	{ \
	ConMsg( "Cannot change %s while the server is running and sv_cheats = 0. To change settings set sv_cheats 1\n", #VARIABLENAME ); \
		return; \
	} \
	else \
	{ \
	g_##VARIABLENAME = V_atof( args[1] ); \
	} \
} \

// headphones
SND_SVACTIVE_CONCMD( snd_front_headphone_position, 90.0, "Specifies the position (in degrees) of the virtual front left/right headphones." )
SND_SVACTIVE_CONCMD( snd_rear_headphone_position, 90.0, "Specifies the position  (in degrees) of the virtual rear left/right headphones." )

// speakers 
SND_SVACTIVE_CONCMD( snd_front_stereo_speaker_position, 90.0, "Specifies the position (in degrees) of the virtual front left/right speakers." );
SND_SVACTIVE_CONCMD( snd_rear_stereo_speaker_position, 90.0, "Specifies the position (in degrees) of the virtual rear left/right speakers." );

// speakers 4, 5 or 7
SND_SVACTIVE_CONCMD( snd_front_surround_speaker_position, 45.0, "Specifies the position (in degrees) of the virtual front left/right speakers." );
SND_SVACTIVE_CONCMD( snd_rear_surround_speaker_position, 135.0,"Specifies the position (in degrees) of the virtual rear left/right speakers." );

// given unit vector from listener to sound source,
// determine proportion of volume for sound in FL, FC, FR, RL, RR quadrants
// Scale this proportion by the distance scalar 'gain'
// If sound has 'mono' radius, blend sound to mono over 50% of radius.
SND_SVACTIVE_CONCMD( snd_headphone_pan_exponent, 1.0, "Specifies the exponent for the pan xfade from phone to phone if the \"exp\" pan law is being used." );
SND_SVACTIVE_CONCMD( snd_stereo_speaker_pan_exponent, 1.5, "Specifies the exponent for the pan xfade from speaker to speaker if the \"exp\" pan law is being used." );
SND_SVACTIVE_CONCMD( snd_surround_speaker_pan_exponent, 1.5, "Specifies the exponent for the pan xfade from speaker to speaker if the \"exp\" pan law is being used." );


SND_SVACTIVE_CONCMD( snd_headphone_pan_radial_weight, 1.0, "Apply cos(angle) * weight before pan law" );
SND_SVACTIVE_CONCMD( snd_stereo_speaker_pan_radial_weight, 0.0, "Apply cos(angle) * weight before pan law" );
SND_SVACTIVE_CONCMD( snd_surround_speaker_pan_radial_weight, 0.0, "Apply cos(angle) * weight before pan law" );

enum snd_pan_mode_t
{
	SND_PAN_EXP = 0,
	SND_PAN_EQ_POW,
	SND_PAN_GLDSRC

};

snd_pan_mode_t g_snd_headphone_pan_mode = SND_PAN_EXP;
snd_pan_mode_t g_snd_stereo_speaker_pan_mode = SND_PAN_EXP;
snd_pan_mode_t g_snd_surround_speaker_pan_mode = SND_PAN_EXP;
// SND_SVACTIVE_CONCMD( snd_headphone_pan_mode, 0.0, "Specifies which pan law to use when monitoring through headphones. < 0 = exp >, < 1 = equal power >, < 2 = goldsrc >", int );
// SND_SVACTIVE_CONCMD( snd_stereo_speaker_pan_mode, 0.0, "Specifies which pan law to use when monitoring through stereo speakers. < 0 = exp >, < 1 = equal power >, < 2 = goldsrc >", int );
// SND_SVACTIVE_CONCMD( snd_surround_speaker_pan_mode, 0.0, "Specifies which pan law to use when monitoring through stereo speakers. < 0 = exp >, < 1 = equal power >, < 2 = goldsrc >", int );



extern  ConVar snd_debug_panlaw;
void DEBUG_DrawPanCurvesLocation( float flYawSource, float flSpeakerMin, float flSpeakerMax, float flFactor, float flMinValue, float flMaxValue )
{

	float startY = 0.66;
	float totalY = 0.25;
	float startX = 0.0;
	float endX = 0.25 * ( 9.0 / 16.0 );
	float stepX = (float)(endX / (float)180);
	float nSource = flFactor * 180;

	float flMinY = flMinValue * totalY;
	float flMaxY = flMaxValue * totalY;

	CDebugOverlay::AddScreenTextOverlay( startX + ( stepX * nSource ), startY - ( flMinY ) - .01, .01, 255, 0, 0, 255, "#" );
	CDebugOverlay::AddScreenTextOverlay( startX + ( stepX * nSource ), startY - ( flMinY ) - .02, .01, 255, 0, 0, 255, "#" );
	CDebugOverlay::AddScreenTextOverlay( startX + ( stepX * nSource ), startY - ( flMaxY ) + .01, .01, 255, 0, 0, 255, "#" );
	CDebugOverlay::AddScreenTextOverlay( startX + ( stepX * nSource ), startY - ( flMaxY ) + .02, .01, 255, 0, 0, 255, "#" );

	startX += (((float) 1.5 * endX) + 0.01 );
	float flMidX = startX + ( endX * 0.5 );
	float flMidY = startY - ( totalY * 0.5);

	float flSin, flCos;
	FastSinCos( flYawSource * 0.01745329251994329500 , &flSin, &flCos );
	CDebugOverlay::AddScreenTextOverlay(flMidX +  flCos * endX *0.53, flMidY + -(flSin*totalY*0.53), .01, 255, 0, 0, 255,  "#");

}

// given unit vector from listener to sound source,
// determine proportion of volume for sound in FL, FC, FR, RL, RR quadrants
// Scale this proportion by the distance scalar 'gain'
// If sound has 'mono' radius, blend sound to mono over 50% of radius.
void Device_SpatializeChannel( int nSlot, float volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono, int nWaveType )
{
	VPROF("Device_SpatializeChannel");
	float rfscale, rrscale, lfscale, lrscale, fcscale;

	fcscale = rfscale = lfscale = rrscale = lrscale = 0.0;	
 	bool bSurround = g_AudioDevice->IsSurround();
 	bool bSurroundCenter = g_AudioDevice->IsSurroundCenter();
 	bool bHeadphone = g_AudioDevice->IsHeadphone();

	// clear volumes

	for (int i = 0; i < CCHANVOLUMES/2; i++)
		volume[i] = 0;

	// linear crossfader for 2, 4 or 5 speakers, using polar coord. separation angle as linear basis

	// get pitch & yaw angle from listener origin to sound source

	QAngle angles;
	float pitch;
	float source_yaw;
	float yaw;

	VectorAngles(sourceDir, angles);

	pitch		= angles[PITCH];
	source_yaw	= angles[YAW];

	// get 2d listener yaw angle from listener right

	QAngle angles2d;
	Vector source2d;
	float listener_yaw;

	source2d.x = listener_right[ nSlot ].x;
	source2d.y = listener_right[ nSlot ].y;
	source2d.z = 0.0;

	VectorNormalize(source2d);

	// convert right vector to euler angles (yaw & pitch)

	VectorAngles(source2d, angles2d);

	listener_yaw = angles2d[YAW];
	
	// get yaw of sound source, with listener_yaw as reference 0.

	yaw = source_yaw - listener_yaw;

	if (yaw < 0)
		yaw += 360;

	if ( !bSurround )
	{
		// 2 ch stereo mixing

		if ( bHeadphone )
		{
			// headphone mix: (NO HRTF)

			rfscale = GetSpeakerVol( yaw, pitch, mono, 0.0,  ISPEAKER_RIGHT_FRONT, 2, false);
			lfscale = GetSpeakerVol( yaw, pitch, mono, 180.0, ISPEAKER_LEFT_FRONT, 2, false );
		}
		else
		{
			// stereo speakers at 45 & 135 degrees: (mono sounds mix down to 2 channels)
		
			rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0,  ISPEAKER_RIGHT_FRONT, 4, true );
			lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_LEFT_FRONT, 4, true );
			rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_RIGHT_REAR, 4, true );
			lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_LEFT_REAR, 4, true );

			// add sounds coming from rear (quieter)

			rfscale = clamp((rfscale + rrscale * 0.75), 0.0, 1.0); 
			lfscale = clamp((lfscale + lrscale * 0.75), 0.0, 1.0);		
			
			rrscale = 0;
			lrscale = 0;

			//DevMsg("lfscale=%f rfscale=%f lrscale=%f rrscale=%f\n",lfscale,rfscale,lrscale,rrscale);
			//DevMsg("pitch=%f yaw=%f \n",pitch, yaw);
		}
		goto SpatialExit;
	}

	if ( bSurround && !bSurroundCenter )
	{
		// 4 ch surround

		// linearly scale with radial distance from asource to FR, FL, RR, RL
		// where FR = 45 degrees, FL = 135, RR = 315 (-45), RL = 225 (-135)

		if ( nWaveType == CHAR_DIRSTEREO )
		{
			// select a different speaker falloff curve specifically for this mode
			rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0,  ISPEAKER_DIR_STEREO, 4, false );
			lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_DIR_STEREO, 4, false );
			rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_DIR_STEREO, 4, false );
			lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_DIR_STEREO, 4, false );
		}
		else
		{
			rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0,  ISPEAKER_RIGHT_FRONT, 4, false );
			lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_LEFT_FRONT, 4, false );
			rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_RIGHT_REAR, 4, false );
			lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_LEFT_REAR, 4, false );
		}


		// DevMsg("lfscale=%f rfscale=%f lrscale=%f rrscale=%f\n",lfscale,rfscale,lrscale,rrscale);
		// DevMsg("pitch=%f yaw=%f \n",pitch, yaw);

		goto SpatialExit;
	}

	if ( bSurround && bSurroundCenter )
	{
		// 5 ch surround

		// linearly scale with radial distance from asource to FR, FC, FL, RR, RL
		// where FR = 45 degrees, FC = 90, FL = 135, RR = 315 (-45), RL = 225 (-135)

		rfscale = GetSpeakerVol( yaw, pitch, mono, 45.0, ISPEAKER_RIGHT_FRONT, 5, false );
		fcscale = GetSpeakerVol( yaw, pitch, mono, 90.0, ISPEAKER_CENTER_FRONT, 5, false );
		lfscale = GetSpeakerVol( yaw, pitch, mono, 135.0, ISPEAKER_LEFT_FRONT, 5, false );
		rrscale = GetSpeakerVol( yaw, pitch, mono, 315.0, ISPEAKER_RIGHT_REAR, 5, false );
		lrscale = GetSpeakerVol( yaw, pitch, mono, 225.0, ISPEAKER_LEFT_REAR, 5, false );
		
		//DevMsg("lfscale=%f center= %f rfscale=%f lrscale=%f rrscale=%f\n",lfscale,fcscale, rfscale,lrscale,rrscale);
		//DevMsg("pitch=%f yaw=%f \n",pitch, yaw);

		goto SpatialExit;
	}

SpatialExit:

	// scale volumes in each quadrant by distance attenuation.

	// volumes are 0-255:
	// gain is 0.0->1.0, rscale is 0.0->1.0, so scale is 0.0->1.0
	// master_vol is 0->255, so rightvol is 0->255

	volume[IFRONT_RIGHT] = master_vol * gain * rfscale;
	volume[IFRONT_LEFT] =  master_vol * gain * lfscale;
	
	volume[IFRONT_RIGHT] = clamp( volume[IFRONT_RIGHT], 0, 255 );
	volume[IFRONT_LEFT]  = clamp( volume[IFRONT_LEFT], 0, 255 );

	if ( bSurround )
	{
		volume[IREAR_RIGHT] = master_vol * gain * rrscale;
		volume[IREAR_LEFT] =  master_vol * gain * lrscale;

		volume[IREAR_RIGHT] = clamp( volume[IREAR_RIGHT], 0, 255 );
		volume[IREAR_LEFT] = clamp( volume[IREAR_LEFT], 0, 255 );

		if ( bSurroundCenter )
		{
			volume[IFRONT_CENTER] = master_vol * gain * fcscale;
			volume[IFRONT_CENTER0] = 0.0;

			volume[IFRONT_CENTER] = clamp( volume[IFRONT_CENTER], 0, 255);
		}
	}
}

void DEBUG_DrawSpeakerValues( float volume[CCHANVOLUMES/2] )
{

	float startY = 0.66;
	float totalY = 0.25;
	float startX = 0.0;
	float endX = 0.25 * ( 9.0 / 16.0 );
/*	float stepX = (float)(endX / (float)180);*/

	
	char valueString[32];

	startX += (((float) 1.5 * endX) + 0.01 );
	float flMidX = startX + ( endX * 0.5 );
/*	float flMidY = startY - ( totalY * 0.5);*/

	CDebugOverlay::AddScreenTextOverlay(flMidX , startY - totalY -0.04, .01, 0, 0, 255, 255,  "#");
	CDebugOverlay::AddScreenTextOverlay(flMidX , startY + 0.04, .01, 0, 0, 255, 255,  "#");

	sprintf( valueString, "%.3f", volume[IFRONT_CENTER] );
	CDebugOverlay::AddScreenTextOverlay(flMidX -0.01 , startY - totalY -0.015, .01, 255, 255, 255, 255,  valueString );

	int nCenterVol = volume[IFRONT_CENTER] * 15;
	for(int i = 1; i <= nCenterVol; i++ )
	{
		CDebugOverlay::AddScreenTextOverlay(flMidX, startY - totalY -0.04 - (float)i * 0.005, .01, 0, 255, 0, 255,  "#");
	}


	sprintf( valueString, "%.3f", volume[IFRONT_RIGHT] );
	CDebugOverlay::AddScreenTextOverlay(flMidX + endX * 0.5, startY - totalY -0.015, .01, 255, 255, 255, 255,  valueString );

	int nFrontRightVol = volume[IFRONT_RIGHT] * 15;
	for(int i = 1; i <= nFrontRightVol; i++ )
	{
		CDebugOverlay::AddScreenTextOverlay(flMidX + (float)i * 0.005, startY - totalY -0.04, .01, 0, 255, 0, 255,  "#");
	}
	

	sprintf( valueString, "%.3f", volume[IFRONT_LEFT] );
	CDebugOverlay::AddScreenTextOverlay(flMidX - endX * 0.5, startY - totalY -0.015, .01, 255, 255, 255, 255,  valueString );

	int nFrontLeftVol = volume[IFRONT_LEFT] * 15;
	for(int i = 1; i <= nFrontLeftVol; i++ )
	{
		CDebugOverlay::AddScreenTextOverlay(flMidX - (float)i * 0.005, startY - totalY -0.04, .01, 0, 255, 0, 255,  "#");
	}

	sprintf( valueString, "%.3f", volume[IREAR_RIGHT] );
	CDebugOverlay::AddScreenTextOverlay(flMidX + endX * 0.5, startY + 0.015, .01, 255, 255, 255, 255,  valueString );

	int nRearRightVol = volume[IREAR_RIGHT] * 15;
	for(int i = 1; i <= nRearRightVol; i++ )
	{
		CDebugOverlay::AddScreenTextOverlay(flMidX + (float)i * 0.005, startY + 0.04, .01, 0, 255, 0, 255,  "#");
	}

	sprintf( valueString, "%.3f", volume[IREAR_LEFT] );
	CDebugOverlay::AddScreenTextOverlay(flMidX - endX * 0.5, startY + 0.015, .01, 255, 255, 255, 255,  valueString );

	int nRearLeftVol = volume[IREAR_LEFT] * 15;
	for(int i = 1; i <= nRearLeftVol; i++ )
	{
		CDebugOverlay::AddScreenTextOverlay(flMidX - (float)i * 0.005, startY + 0.04, .01, 0, 255, 0, 255,  "#");
	}


}

#define PI 3.14159265
static void InterpSpeakerVol( float flRadialWeight, snd_pan_mode_t nPanMode, float flExponent, float flYawSource, float flSpeakerMin, float flSpeakerMax, float &flMinValue, float &flMaxValue, bool bDraw = true)
{
	float flFactor = ( flYawSource - flSpeakerMin ) / (flSpeakerMax - flSpeakerMin );
	float flOriginalFactor = flFactor;
	if( flRadialWeight > 0.0 && nPanMode != SND_PAN_GLDSRC )
	{
		float flRadialFactor = 1.0 - ( FastCos( flFactor * PI ) + 1.0 ) * 0.5;
		float flRadialDiff = flRadialFactor - flFactor;
		flFactor = flFactor + ( flRadialDiff * flRadialWeight );
	}

	switch( nPanMode )
	{
		default:
		case SND_PAN_EXP:
		{
			flMinValue = 1.0 - FastPow( flFactor, flExponent );
			flMaxValue = 1.0 - FastPow( 1.0 - flFactor, flExponent );
			break;
		}
		case SND_PAN_EQ_POW:
		{
			// equal power pan
			float flFactorAngle = flFactor * ( PI * 0.5);
			FastSinCos( flFactorAngle, &flMaxValue, &flMinValue );
			break;
		}
		case SND_PAN_GLDSRC:
		{
			// goldsrc pan
			flMinValue = ( FastCos( flFactor * PI ) + 1.0 ) * 0.5;
			flMaxValue = 1.0 - flMinValue;
			break;
		}
	}
	flMinValue = clamp( flMinValue, 0.0, 1.0 );
	flMaxValue = clamp( flMaxValue, 0.0, 1.0 );
	if( snd_debug_panlaw.GetInt() && bDraw )
	{
			DEBUG_DrawPanCurvesLocation( flYawSource, flSpeakerMin, flSpeakerMax, flOriginalFactor, flMinValue, flMaxValue );
	}
}

void DEBUG_DrawPanCurves(void)
{

	float startY = 0.66;
	float totalY = 0.25;
	float startX = 0.0;
	float endX = 0.25 * ( 9.0 / 16.0 );
	float stepX = (float)(endX / (float)180);
	float flMinValue, flMaxValue;
// 	for( int nOption = 0; nOption < 2; nOption++ )
// 	{
		snd_pan_mode_t nPanMode = (snd_pan_mode_t ) g_snd_headphone_pan_mode;
		float flExponent = g_snd_headphone_pan_exponent;
		float flRadialPan = g_snd_headphone_pan_radial_weight;
		float flFrontSpeakerPos = g_snd_front_headphone_position;
		float flRearSpeakerPos = g_snd_rear_headphone_position;
		if( snd_surround.GetInt() > 0 )
		{
			nPanMode = (snd_pan_mode_t ) g_snd_stereo_speaker_pan_mode;
			flExponent = g_snd_stereo_speaker_pan_exponent;
			flRadialPan = g_snd_stereo_speaker_pan_radial_weight;
			CDebugOverlay::AddScreenTextOverlay(startX + (endX * 0.2), startY + 0.01, .1, 250, 250, 200, 255,  "Speaker Pan Law");
			if( snd_surround.GetInt() == 2 )
			{
				flFrontSpeakerPos = g_snd_front_stereo_speaker_position;
				flRearSpeakerPos = g_snd_rear_stereo_speaker_position;
			}
			else
			{
				flExponent = g_snd_surround_speaker_pan_exponent;
				flRadialPan = g_snd_surround_speaker_pan_radial_weight;
				flFrontSpeakerPos = g_snd_front_surround_speaker_position;
				flRearSpeakerPos = g_snd_rear_surround_speaker_position;
				nPanMode = (snd_pan_mode_t ) g_snd_surround_speaker_pan_mode;
			}

		}
		else
		{
			CDebugOverlay::AddScreenTextOverlay(startX + (endX * 0.2), startY + 0.01, .1, 250, 250, 200, 255,  "Headphone Pan Law");
		}
		if(nPanMode == SND_PAN_EQ_POW )
		{
			CDebugOverlay::AddScreenTextOverlay(startX + (endX * 0.2), startY + 0.03, .1, 250, 250, 200, 255,  "Equal Power");
		}
		else if(nPanMode == SND_PAN_EXP )
		{
			CDebugOverlay::AddScreenTextOverlay(startX + (endX * 0.2), startY + 0.03, .1, 250, 250, 200, 255,  "Exponential");
		}
		else if(nPanMode == SND_PAN_GLDSRC )
		{
			CDebugOverlay::AddScreenTextOverlay(startX + (endX * 0.2), startY + 0.03, .1, 250, 250, 200, 255,  "Gold Source");
		}

		//startX += (((float) nOption * endX) + 0.01 );
		for( int nSource = 0; nSource <= 180; nSource++ )
		{
			InterpSpeakerVol( flRadialPan, nPanMode, flExponent, (float) nSource, 0.0, 180.0, flMinValue, flMaxValue, false );
			CDebugOverlay::AddScreenTextOverlay(startX +  (stepX * (float) nSource), startY - (flMinValue*totalY), .01, 0, 255, 0, 255,  "+");
			CDebugOverlay::AddScreenTextOverlay(startX + (stepX * (float) nSource), startY - (flMaxValue*totalY), .01, 0, 255, 0, 255,  "+");
		}
		startX += (((float) 1.5 * endX) + 0.01 );
		float flMidX = startX + ( endX * 0.5 );
		float flMidY = startY - ( totalY * 0.5);
		for( int nSource = 0; nSource <= 180; nSource++ )
		{
			FastSinCos( (float)nSource * 0.01745329251994329500 * 2.0, &flMaxValue, &flMinValue );
			CDebugOverlay::AddScreenTextOverlay(flMidX +  flMaxValue * endX *0.5, flMidY + -(flMinValue*totalY*0.5), .01, 0, 255, 0, 255,  "*");
		}
		CDebugOverlay::AddScreenTextOverlay(flMidX +  1.0 * endX * 0.38, flMidY + (0.0*totalY*0.38), .01, 0, 255, 0, 255,  ">");
		CDebugOverlay::AddScreenTextOverlay(flMidX -  1.0 * endX * 0.38, flMidY + (0.0*totalY*0.38), .01, 0, 255, 0, 255,  "<");
		CDebugOverlay::AddScreenTextOverlay(flMidX +  0.0 * endX *0.38, flMidY - (1.0*totalY*0.38), .01, 0, 255, 0, 255,  "^");

		float flSin, flCos;
		FastSinCos( (flFrontSpeakerPos+90 ) * 0.01745329251994329500, &flSin, &flCos );
		CDebugOverlay::AddScreenTextOverlay(flMidX +  flCos * (endX * 0.46), flMidY - (flSin*totalY*0.46), .01, 255, 255, 0, 255,  "*");
		FastSinCos( -(flFrontSpeakerPos+270 ) * 0.01745329251994329500, &flSin, &flCos );
		CDebugOverlay::AddScreenTextOverlay(flMidX +  flCos * (endX * 0.46), flMidY - (flSin*totalY*0.46), .01, 255, 255, 0, 255,  "*");

		FastSinCos( (flRearSpeakerPos+90 ) * 0.01745329251994329500, &flSin, &flCos );
		CDebugOverlay::AddScreenTextOverlay(flMidX +  flCos * (endX * 0.42), flMidY - (flSin*totalY * 0.42), .01, 0, 255, 255, 255,  "*");
		FastSinCos( -(flRearSpeakerPos+270 ) * 0.01745329251994329500, &flSin, &flCos );
		CDebugOverlay::AddScreenTextOverlay(flMidX +  flCos * (endX * 0.42), flMidY - (flSin*totalY * 0.42), .01, 0, 255, 255, 255,  "*");


//	}
}

static float InterpPitchAngle( float flPitchAngle, float flMono )
{
	float flNewMono = flMono;

	// mono goes from 0.0 to 1.0 as listener moves into 'mono' radius of sound source.
	// Also, as pitch_angle to sound source approaches 90 (sound above/below listener), sounds become mono.
	
	// convert pitch angle to 0-90 absolute pitch
	if ( flPitchAngle < 0)
		flPitchAngle += 360;

	if ( flPitchAngle > 180)
		flPitchAngle = 360 - flPitchAngle;
	
	if ( flPitchAngle > 90)
		flPitchAngle = 90 - (flPitchAngle - 90);
		
	// calculate additional mono crossfade due to pitch angle
	if ( flPitchAngle > PITCH_ANGLE_THRESHOLD )	
	{
		float xfade  = ( flPitchAngle - PITCH_ANGLE_THRESHOLD ) / ( 90.0 - PITCH_ANGLE_THRESHOLD );	// 0.0 -> 1.0 as angle 45->90	

		flNewMono = clamp( flNewMono + xfade, 0.0, 1.0 );
	}

	return flNewMono;
}

static float InterpMonoSpread( float flMono, float flScale, int ispeaker, int cspeaker, bool fmix2channels )
{
	Assert(flMono <= 1.0 && flMono >= 0.0);

	Assert( flScale <= 1.0 && flScale >= 0.0 );
 	
	// crossfade speaker volumes towards mono with increased pitch angle of sound source

	float flNewScale = XfadeSpeakerVolToMono( flScale, flMono, ispeaker, cspeaker, fmix2channels );  
	
	Assert( flScale <= 1.0 && flScale >= 0.0);
	
	return flNewScale;

}



//////////////////////////////////////////////////////////////////////////
// float only version
//////////////////////////////////////////////////////////////////////////
void Device_SpatializeChannel( int nSlot, float volume[CCHANVOLUMES/2], const Vector& sourceDir, float mono, float flRearToStereoScale /* = 0.75 */ )
{
	VPROF("CAudioDeviceBase::SpatializeChannel - 2");

	float rfscale, rrscale, lfscale, lrscale, fcscale;
	bool bSurround = g_AudioDevice->IsSurround();
	bool bSurroundCenter = g_AudioDevice->IsSurroundCenter();
	bool bHeadphone = g_AudioDevice->IsHeadphone();
	fcscale = rfscale = lfscale = rrscale = lrscale = 0.0;	


	// clear volumes
	for (int i = 0; i < CCHANVOLUMES/2; i++)
		volume[i] = 0;

	// linear crossfader for 2, 4 or 5 speakers, using polar coord. separation angle as linear basis

	// get pitch & yaw angle from listener origin to sound source

	QAngle angles;
	float pitch;
	float source_yaw;
	float yaw;

	// sourceDir = sourcePos - playerPos
	VectorAngles(sourceDir, angles);

	pitch		= angles[PITCH];
	source_yaw	= angles[YAW];

	// adjust mono spread based on pitch angle
	float flMono = InterpPitchAngle( pitch, mono );

		// get 2d listener yaw angle from listener right

	QAngle angles2d;
	Vector source2d;
	float listener_yaw;

	source2d.x = listener_right[ nSlot ].x;
	source2d.y = listener_right[ nSlot ].y;
	source2d.z = 0.0;

	VectorNormalize(source2d);

	// convert right vector to euler angles (yaw & pitch)

	VectorAngles(source2d, angles2d);

	listener_yaw = angles2d[YAW];

	// get yaw of sound source, with listener_yaw as reference 0.

	yaw = AngleDiff( source_yaw, listener_yaw );
	if ( yaw < 0.0 )
	{
		yaw += 360;
	}

	// default to stereo
	float flFrontCenter = 90;

	// pre-rotation
	float flFront = 90;
	float flRear = 90;
	bool bMixToTwoChannels = true;
	int nSpkeakerCount = 4;
	
	snd_pan_mode_t nPanMode = ( snd_pan_mode_t ) g_snd_stereo_speaker_pan_mode;
	float flPanExponent = g_snd_stereo_speaker_pan_exponent;
	float flRadialPan = g_snd_stereo_speaker_pan_radial_weight;

	if ( bHeadphone )
	{
		// headphone mix: (NO HRTF)
		// override with phone settings
		flFront = clamp( g_snd_front_headphone_position, 0.0, 90.0 );
		flRear = clamp( g_snd_rear_headphone_position, flFront, 180.0 );

		bMixToTwoChannels = true;
		nSpkeakerCount = 4;
		nPanMode = ( snd_pan_mode_t ) g_snd_headphone_pan_mode;
		flPanExponent = g_snd_headphone_pan_exponent;
		flRadialPan = g_snd_headphone_pan_radial_weight;
	}
	else if( bSurround && !bSurroundCenter )
	{
		// override with quad settings
		flFront = clamp( g_snd_front_surround_speaker_position, 0.0, 90.0 );
		flRear = clamp( g_snd_rear_surround_speaker_position, flFront, 180.0 );
		flPanExponent = g_snd_surround_speaker_pan_exponent;
		flRadialPan = g_snd_surround_speaker_pan_radial_weight;
		nPanMode = ( snd_pan_mode_t ) g_snd_surround_speaker_pan_mode;

		bMixToTwoChannels = false;
		nSpkeakerCount = 4;
	}
	else if ( bSurround && bSurroundCenter )
	{
		// override with 5.1 settings
		flFront = clamp( g_snd_front_surround_speaker_position, 0.0, 90.0 );
		flRear = clamp( g_snd_rear_surround_speaker_position, flFront, 180.0 );
		flPanExponent = g_snd_surround_speaker_pan_exponent;
		flRadialPan = g_snd_surround_speaker_pan_radial_weight;
		nPanMode = ( snd_pan_mode_t ) g_snd_surround_speaker_pan_mode;

		bMixToTwoChannels = false;
		nSpkeakerCount = 5;
	}
	else // stereo
	{
		// override with stereo settings
		flFront = clamp( g_snd_front_stereo_speaker_position, 0.0, 90.0 );
		flRear = clamp( g_snd_rear_stereo_speaker_position, flFront, 180.0 );
		bMixToTwoChannels = true;
		nSpkeakerCount = 4; // stereo is processed as 4 speakers (just for backward compatibility
 	}

	float flFrontRight = 90.0 - flFront;
	float flFrontLeft = 90.0 + flFront;
	float flRearRight = 450.0 - flRear;
	float flRearLeft= 90.0 + flRear;

	// if there's a center speaker we interp from right->center->left
	if( bSurroundCenter )
	{
		if( yaw >= flFrontRight && yaw < flFrontCenter )
		{
			InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flFrontRight, flFrontCenter, rfscale, fcscale );
		}
		else if( yaw >= flFrontCenter && yaw < flFrontLeft )
		{
			InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flFrontCenter, flFrontLeft, fcscale, lfscale );
		}
	} // otw directly right->left
	else
	{
		if( yaw >= flFrontRight && yaw < flFrontLeft )
		{
			InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flFrontRight, flFrontLeft, rfscale, lfscale );
		}
	}

	if ( yaw >= flFrontLeft && yaw < flRearLeft )
	{
		InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flFrontLeft, flRearLeft, lfscale, lrscale );
	}
	else if ( yaw >= flRearLeft && yaw < flRearRight )
	{
		InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flRearLeft, flRearRight, lrscale, rrscale );
	}
	// the circle wraps between frontright and rearright
	else if ( yaw >= flRearRight ) // between RearRight & 0/360
	{
		InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flRearRight, 360 + flFrontRight, rrscale, rfscale );
	}
	else if ( yaw < flFrontRight ) // between 0/360 and FrontRight
	{
		InterpSpeakerVol( flRadialPan, nPanMode, flPanExponent, yaw, flRearRight - 360, flFrontRight, rrscale, rfscale );
	}

	Assert( fcscale <= 1.0 && fcscale >= 0.0 );

	if( bSurroundCenter )
	{
		fcscale = InterpMonoSpread( flMono, fcscale, ISPEAKER_CENTER_FRONT, nSpkeakerCount, bMixToTwoChannels  );
	}
	Assert( rfscale <= 1.0 && rfscale >= 0.0 );
	rfscale = InterpMonoSpread( flMono, rfscale, ISPEAKER_RIGHT_FRONT, nSpkeakerCount, bMixToTwoChannels  );
	Assert( lfscale <= 1.0 && lfscale >= 0.0 );
	lfscale = InterpMonoSpread( flMono, lfscale, ISPEAKER_LEFT_FRONT, nSpkeakerCount, bMixToTwoChannels );
	Assert( rrscale <= 1.0 && rrscale >= 0.0 );
	rrscale = InterpMonoSpread( flMono, rrscale, ISPEAKER_RIGHT_REAR, nSpkeakerCount, bMixToTwoChannels );
	Assert( lrscale <= 1.0 && lrscale >= 0.0 );
	lrscale = InterpMonoSpread( flMono, lrscale, ISPEAKER_LEFT_REAR, nSpkeakerCount, bMixToTwoChannels );



	// add sounds coming from rear (potentially scaled)
	if( !bSurround )
	{
		rfscale = rfscale + ( rrscale * flRearToStereoScale ); 
		lfscale = lfscale + ( lrscale * flRearToStereoScale );		
		rrscale = 0;
		lrscale = 0;
	}

	volume[IFRONT_RIGHT] = clamp( rfscale, 0.0, 1.0 );
	volume[IFRONT_LEFT] =  clamp( lfscale, 0.0, 1.0 );

	if ( bSurround )
	{
		volume[IREAR_RIGHT] = clamp( rrscale, 0.0, 1.0 );
		volume[IREAR_LEFT] =  clamp( lrscale, 0.0, 1.0 );

		if ( bSurroundCenter )
		{
			volume[IFRONT_CENTER] = clamp( fcscale, 0.0, 1.0 );
			volume[IFRONT_CENTER0] = 0.0;
		}
	}
	if(snd_debug_panlaw.GetInt())
	{
		DEBUG_DrawSpeakerValues( volume );
	}
}

// old skool integer version
ConVar snd_rear_speaker_scale("snd_rear_speaker_scale", "1.0", FCVAR_CHEAT, "How much to scale rear speaker contribution to front stereo output" );

void Device_ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount)
{
	VPROF("CAudioDeviceBase::ApplyDSPEffects");
	DEBUG_StartSoundMeasure( 1, samplecount );

	DSP_Process( idsp, pbuffront, pbufrear, pbufcenter, samplecount );

	DEBUG_StopSoundMeasure( 1, samplecount );
}

void Device_MixUpsample( int sampleCount, int filtertype )
{
	VPROF( "CAudioDeviceBase::MixUpsample" );

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();
	int ifilter = pPaint->ifilter;

	Assert (ifilter < CPAINTFILTERS);

	S_MixBufferUpsample2x( sampleCount, pPaint->pbuf, &(pPaint->fltmem[ifilter][0]), CPAINTFILTERMEM, filtertype );

	if ( pPaint->fsurround )
	{
		Assert( pPaint->pbufrear );
		S_MixBufferUpsample2x( sampleCount, pPaint->pbufrear, &(pPaint->fltmemrear[ifilter][0]), CPAINTFILTERMEM, filtertype );

		if ( pPaint->fsurround_center )
		{
			Assert( pPaint->pbufcenter );
			S_MixBufferUpsample2x( sampleCount, pPaint->pbufcenter, &(pPaint->fltmemcenter[ifilter][0]), CPAINTFILTERMEM, filtertype );
		}
	}

	// make sure on next upsample pass for this paintbuffer, new filter memory is used
	pPaint->ifilter++;
}

void Device_Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	VPROF( "CAudioDeviceBase::Mix8Mono" );

	float volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 1) )
		return;

	if ( FVolumeFrontNonZero(volume) )
	{
		Mix8MonoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount);
	}

	if ( pPaint->fsurround )
	{
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix8MonoWavtype( pChannel, pPaint->pbufrear + outputOffset, &volume[IREAR_LEFT], (byte *)pData, inputOffset, rateScaleFix, outCount  );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix8MonoWavtype( pChannel, pPaint->pbufcenter + outputOffset, &volume[IFRONT_CENTER], (byte *)pData, inputOffset, rateScaleFix, outCount  );
		}
	}
}

void Device_Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	VPROF( "CAudioDeviceBase::Mix8Stereo" );

	float volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 2 ) )
		return;

	if ( FVolumeFrontNonZero(volume) )
	{
		Mix8StereoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
	}

	if ( pPaint->fsurround )
	{
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix8StereoWavtype( pChannel, pPaint->pbufrear + outputOffset, &volume[IREAR_LEFT], (byte *)pData, inputOffset, rateScaleFix, outCount );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix8StereoWavtype( pChannel, pPaint->pbufcenter + outputOffset, &volume[IFRONT_CENTER], (byte *)pData, inputOffset, rateScaleFix, outCount );
		}
	}
}

void Device_Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	VPROF( "CAudioDeviceBase::Mix16Mono" );

	float volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 1 ) )
		return;
	
	if ( FVolumeFrontNonZero(volume) )
	{
		Mix16MonoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
	}

	if ( pPaint->fsurround )
	{		
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix16MonoWavtype( pChannel, pPaint->pbufrear + outputOffset, &volume[IREAR_LEFT], pData, inputOffset, rateScaleFix, outCount );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix16MonoWavtype( pChannel, pPaint->pbufcenter + outputOffset, &volume[IFRONT_CENTER], pData, inputOffset, rateScaleFix, outCount );
		}
	}
}

void Device_Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	VPROF( "CAudioDeviceBase::Mix16Stereo" );

	float volume[CCHANVOLUMES];

	paintbuffer_t *pPaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( pPaint, pChannel, volume, 2 ) )
		return;

	if ( FVolumeFrontNonZero(volume) )
	{
		Mix16StereoWavtype( pChannel, pPaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
	}

	if ( pPaint->fsurround )
	{
		if ( FVolumeRearNonZero(volume) )
		{
			Assert( pPaint->pbufrear );
			Mix16StereoWavtype( pChannel, pPaint->pbufrear  + outputOffset, &volume[IREAR_LEFT], pData, inputOffset, rateScaleFix, outCount );
		}

		if ( pPaint->fsurround_center && FVolumeCenterNonZero(volume) )
		{
			Assert( pPaint->pbufcenter );
			Mix16StereoWavtype( pChannel, pPaint->pbufcenter  + outputOffset, &volume[IFRONT_CENTER], pData, inputOffset, rateScaleFix, outCount );
		}
	}
}

#if USE_AUDIO_DEVICE_V1

// Null Audio Device
class CAudioDeviceNull : public CAudioDeviceBase
{
public:
	CAudioDeviceNull()
	{
		m_pName = "Sound Disabled";
		m_nChannels = 2;
		m_nSampleBits = 16;
		m_nSampleRate = SOUND_DMA_SPEED;
		m_bIsActive = false;
	}
	bool		IsActive( void ) { return false; }
	bool		Init( void ) { return true; }
	void		Shutdown( void ) {}
	void		Pause( void ) {} 
	void		UnPause( void ) {}
	
	int64		PaintBegin( float, int64, int64 ) { return 0; }
	void		PaintEnd( void ) {}

	int			GetOutputPosition( void ) { return 0; }
	void		ClearBuffer( void ) {}

	void		TransferSamples( int end ) {}
	
	int			DeviceSampleCount( void )	{ return 0; }
};

IAudioDevice *Audio_GetNullDevice( void )
{
	return new CAudioDeviceNull;
}
#else

void IAudioDevice2::TransferSamples( uint32 )
{
	CAudioMixBuffer mixBuffers[SOUND_DEVICE_MAX_CHANNELS];

	const portable_samplepair_t *pFront = PAINTBUFFER;
	const portable_samplepair_t *pRear = REARPAINTBUFFER;
	const portable_samplepair_t *pCenter = CENTERPAINTBUFFER;

	float flMasterVolume = S_GetMasterVolume();

	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		mixBuffers[0].m_flData[i] = pFront[i].left;
		mixBuffers[1].m_flData[i] = pFront[i].right;
	}
	ScaleBuffer( mixBuffers[0].m_flData, mixBuffers[0].m_flData, flMasterVolume );
	ScaleBuffer( mixBuffers[1].m_flData, mixBuffers[1].m_flData, flMasterVolume );
	if ( IsSurroundCenter() )
	{
		for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
		{
			mixBuffers[2].m_flData[i] = pCenter[i].left;
			mixBuffers[3].m_flData[i] = 0;
		}
		ScaleBuffer( mixBuffers[2].m_flData, mixBuffers[2].m_flData, flMasterVolume );
		// this is all zeros, so scaling it to the master volume isn't necessary
		//ScaleBuffer( mixBuffers[3].m_flData, mixBuffers[3].m_flData, flMasterVolume );
	}
	if ( IsSurround() )
	{
		for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
		{
			mixBuffers[4].m_flData[i] = pRear[i].left;
			mixBuffers[5].m_flData[i] = pRear[i].right;
		}
		ScaleBuffer( mixBuffers[4].m_flData, mixBuffers[4].m_flData, flMasterVolume );
		ScaleBuffer( mixBuffers[5].m_flData, mixBuffers[5].m_flData, flMasterVolume );
	}
	if ( ChannelCount() > 6 )
	{
		for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
		{
			mixBuffers[6].m_flData[i] = 0;
			mixBuffers[7].m_flData[i] = 0;
		}
	}
	OutputBuffer( ChannelCount(), mixBuffers );
}
#endif
