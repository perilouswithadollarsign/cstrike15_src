//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "tier1/convar.h"
#include "jigglebones.h"
#ifdef CLIENT_DLL
#include "engine/ivdebugoverlay.h"
#include "cdll_client_int.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
ConVar JiggleBoneDebug( "cl_jiggle_bone_debug", "0", FCVAR_CHEAT, "Display physics-based 'jiggle bone' debugging information" );
ConVar JiggleBoneDebugYawConstraints( "cl_jiggle_bone_debug_yaw_constraints", "0", FCVAR_CHEAT, "Display physics-based 'jiggle bone' debugging information" );
ConVar JiggleBoneDebugPitchConstraints( "cl_jiggle_bone_debug_pitch_constraints", "0", FCVAR_CHEAT, "Display physics-based 'jiggle bone' debugging information" );
ConVar JiggleBoneInvert( "cl_jiggle_bone_invert", "0", FCVAR_CHEAT );
ConVar JiggleBoneSanity( "cl_jiggle_bone_sanity", "1", 0, "Prevent jiggle bones from pointing directly away from their target in case of numerical instability." );

static int s_id = 2;

#ifdef CLIENT_DLL
char *VarArgs( const char *format, ... );
#else
class CDummyOverlay
{
public:
	void AddLineOverlay(const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration) {};
	void AddTextOverlay(const Vector &origin, float duration, const char *format, ...) {}
};

SELECTANY CDummyOverlay *debugoverlay = new CDummyOverlay;

char *VarArgs( const char *format, ... )
{
	return NULL;
}
#endif

//-----------------------------------------------------------------------------
JiggleData *CJiggleBones::GetJiggleData( int bone, float currenttime, const Vector &initBasePos, const Vector &initTipPos )
{
	FOR_EACH_LL( m_jiggleBoneState, it )
	{
		if ( m_jiggleBoneState[it].bone == bone )
		{
			JiggleData *data = &m_jiggleBoneState[it];
			if ( !IsFinite( data->lastUpdate ) )
			{
				Warning( "lastUpdate NaN\n" );
			}
			if ( !data->basePos.IsValid() )
			{
				Warning( "basePos NaN\n" );
			}
			if ( !data->baseLastPos.IsValid() )
			{
				Warning( "baseLastPos NaN\n" );
			}
			if ( !data->baseVel.IsValid() )
			{
				Warning( "baseVel NaN\n" );
			}
			if ( !data->baseAccel.IsValid() )
			{
				Warning( "baseAccel NaN\n" );
			}
			if ( !data->tipPos.IsValid() )
			{
				Warning( "tipPos NaN\n" );
			}
			if ( !data->tipVel.IsValid() )
			{
				Warning( "tipVel NaN\n" );
			}
			if ( !data->tipAccel.IsValid() )
			{
				Warning( "tipAccel NaN\n" );
			}
			return &m_jiggleBoneState[it];
		}
	}

	JiggleData data;
	data.Init( bone, currenttime, initBasePos, initTipPos );
	data.id = s_id++;

	int idx = m_jiggleBoneState.AddToHead( data );
	if ( idx == m_jiggleBoneState.InvalidIndex() )
		return NULL;

	return &m_jiggleBoneState[idx];
}


//-----------------------------------------------------------------------------
/**
 * Do spring physics calculations and update "jiggle bone" matrix
 * (Michael Booth, Turtle Rock Studios)
 */
void CJiggleBones::BuildJiggleTransformations( int boneIndex, float currenttime, const mstudiojigglebone_t *jiggleInfo, const matrix3x4_t &goalMX, matrix3x4_t &boneMX, bool coordSystemIsFlipped )
{
	Vector goalBasePosition;
	MatrixPosition( goalMX, goalBasePosition );

	Vector goalForward, goalUp, goalLeft;
	MatrixGetColumn( goalMX, 0, goalLeft );
	MatrixGetColumn( goalMX, 1, goalUp );
	MatrixGetColumn( goalMX, 2, goalForward );

	// compute goal tip position
	Vector goalTip = goalBasePosition + jiggleInfo->length * goalForward;

	JiggleData *data = GetJiggleData( boneIndex, currenttime, goalBasePosition, goalTip );
	if ( !data )
	{
		return;
	}

	if ( currenttime - data->lastUpdate > 0.5f )
	{
		data->Init( boneIndex, currenttime, goalBasePosition, goalTip );
	}

	if ( JiggleBoneInvert.GetBool() )
	{
		data->basePos = -data->basePos;
		data->baseLastPos = -data->baseLastPos;
		data->baseVel = -data->baseVel;
		data->baseAccel = -data->baseAccel;
		data->tipPos = -data->tipPos;
		data->tipVel = -data->tipVel;
		data->tipAccel = -data->tipAccel;
	}

#ifdef CLIENT_DLL
	if ( data->id == JiggleBoneDebug.GetInt() )
	{
		int line = 20;
		engine->Con_NPrintf( line++, "basePos %f %f %f", data->basePos.x, data->basePos.y, data->basePos.z );
		engine->Con_NPrintf( line++, "baseLastPos %f %f %f", data->baseLastPos.x, data->baseLastPos.y, data->baseLastPos.z );
		engine->Con_NPrintf( line++, "baseVel %f %f %f", data->baseVel.x, data->baseVel.y, data->baseVel.z );
		engine->Con_NPrintf( line++, "baseAccel %f %f %f", data->baseAccel.x, data->baseAccel.y, data->baseAccel.z );
		engine->Con_NPrintf( line++, "tipPos %f %f %f", data->tipPos.x, data->tipPos.y, data->tipPos.z );
		engine->Con_NPrintf( line++, "tipVel %f %f %f", data->tipVel.x, data->tipVel.y, data->tipVel.z );
		engine->Con_NPrintf( line++, "tipAccel %f %f %f", data->tipAccel.x, data->tipAccel.y, data->tipAccel.z );
	}
#endif

	if ( JiggleBoneSanity.GetBool() )
	{
		Vector goalDir = goalTip - goalBasePosition;
		goalDir.NormalizeInPlace();
		Vector dataDir = data->tipPos - goalBasePosition;
		dataDir.NormalizeInPlace();

		float dot = goalDir.Dot( dataDir );
		if ( dot < -0.9f ) // if we end up pointing almost completely away, just reset
		{
			data->Init( boneIndex, currenttime, goalBasePosition, goalTip );
		}
	}

	//Vector bodyVel;
	//EstimateAbsVelocity( bodyVel );

	// limit maximum deltaT to avoid simulation blowups
	// if framerate gets very low, jiggle will run in slow motion
	const float thirtyHZ = 0.0333f;
	const float thousandHZ = 0.001f;
	float deltaT = clamp( currenttime - data->lastUpdate, thousandHZ, thirtyHZ );
	data->lastUpdate = currenttime;

	//
	// Bone tip flex
	//
	if (jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID))
	{
		// apply gravity in global space
		data->tipAccel.z -= jiggleInfo->tipMass;

		if (jiggleInfo->flags & JIGGLE_IS_FLEXIBLE)
		{
			// We want to run the simulation for this part of the jiggle with smaller time steps.
			static const float MIN_DELTA_T_PER_ITERATION = 1.0f / 120.0f;
			for (float deltaTLeft = deltaT; deltaTLeft > 0.0f; deltaTLeft -= MIN_DELTA_T_PER_ITERATION )
			{
				float usableDeltaT = deltaTLeft > MIN_DELTA_T_PER_ITERATION ? MIN_DELTA_T_PER_ITERATION : deltaTLeft;
				// decompose into local coordinates
				Vector error = goalTip - data->tipPos;

				Vector localError;
				localError.x = DotProduct( goalLeft, error );
				localError.y = DotProduct( goalUp, error );
				localError.z = DotProduct( goalForward, error );

				Vector localVel;
				localVel.x = DotProduct( goalLeft, data->tipVel );
				localVel.y = DotProduct( goalUp, data->tipVel );
				localVel.z = 0.0f; // TODO: this was uninitialized, but is being used

				// yaw spring
				float yawAccel = jiggleInfo->yawStiffness * localError.x - jiggleInfo->yawDamping * localVel.x;

				// pitch spring
				float pitchAccel = jiggleInfo->pitchStiffness * localError.y - jiggleInfo->pitchDamping * localVel.y;

				if (jiggleInfo->flags & JIGGLE_HAS_LENGTH_CONSTRAINT)
				{
					// drive tip towards goal tip position	
					data->tipAccel += yawAccel * goalLeft + pitchAccel * goalUp;
				}
				else
				{
					// allow flex along length of spring
					localVel.z = DotProduct( goalForward, data->tipVel );

					// along spring
					float alongAccel = jiggleInfo->alongStiffness * localError.z - jiggleInfo->alongDamping * localVel.z;

					// drive tip towards goal tip position	
					data->tipAccel += yawAccel * goalLeft + pitchAccel * goalUp + alongAccel * goalForward;
				}

				// simple euler integration		
				data->tipVel += data->tipAccel * usableDeltaT;
				data->tipPos += data->tipVel * usableDeltaT;
			}

		}

		// clear this timestep's accumulated accelerations
		data->tipAccel = vec3_origin;		

		//
		// Apply optional constraints
		//
		if (jiggleInfo->flags & (JIGGLE_HAS_YAW_CONSTRAINT | JIGGLE_HAS_PITCH_CONSTRAINT))
		{
			// find components of spring vector in local coordinate system
			Vector along = data->tipPos - goalBasePosition;
			Vector localAlong;
			localAlong.x = DotProduct( goalLeft, along );
			localAlong.y = DotProduct( goalUp, along );
			localAlong.z = DotProduct( goalForward, along );

			Vector localVel;
			localVel.x = DotProduct( goalLeft, data->tipVel );
			localVel.y = DotProduct( goalUp, data->tipVel );
			localVel.z = DotProduct( goalForward, data->tipVel );

			if (jiggleInfo->flags & JIGGLE_HAS_YAW_CONSTRAINT)
			{
				// enforce yaw constraints in local XZ plane
				float yawError = atan2( localAlong.x, localAlong.z );

				bool isAtLimit = false;
				float yaw = 0.0f;

				if (yawError < jiggleInfo->minYaw)
				{
					// at angular limit
					isAtLimit = true;
					yaw = jiggleInfo->minYaw;
				}
				else if (yawError > jiggleInfo->maxYaw)
				{
					// at angular limit
					isAtLimit = true;
					yaw = jiggleInfo->maxYaw;
				}

				if (isAtLimit)
				{
					float sy, cy;
					SinCos( yaw, &sy, &cy );

					// yaw matrix
					matrix3x4_t yawMatrix;

					yawMatrix[0][0] = cy;
					yawMatrix[1][0] = 0;
					yawMatrix[2][0] = -sy;

					yawMatrix[0][1] = 0;
					yawMatrix[1][1] = 1.0f;
					yawMatrix[2][1] = 0;

					yawMatrix[0][2] = sy;
					yawMatrix[1][2] = 0;
					yawMatrix[2][2] = cy;

					yawMatrix[0][3] = 0;
					yawMatrix[1][3] = 0;
					yawMatrix[2][3] = 0;

					// global coordinates of limit
					matrix3x4_t limitMatrix;					
					ConcatTransforms( goalMX, yawMatrix, limitMatrix );

					Vector limitLeft( limitMatrix.m_flMatVal[0][0], 
						limitMatrix.m_flMatVal[1][0],
						limitMatrix.m_flMatVal[2][0] );

					Vector limitUp( limitMatrix.m_flMatVal[0][1], 
						limitMatrix.m_flMatVal[1][1],
						limitMatrix.m_flMatVal[2][1] );

					Vector limitForward( limitMatrix.m_flMatVal[0][2], 
						limitMatrix.m_flMatVal[1][2],
						limitMatrix.m_flMatVal[2][2] );

					if (JiggleBoneDebugYawConstraints.GetBool())
					{
						float dT = 0.01f;
						const float axisSize = 10.0f;
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitLeft, 0, 255, 255, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitUp, 255, 255, 0, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitForward, 255, 0, 255, true, dT );
					}

					Vector limitAlong( DotProduct( limitLeft, along ), 
						DotProduct( limitUp, along ),
						DotProduct( limitForward, along ) );

					// clip to limit plane
					data->tipPos = goalBasePosition + limitAlong.y * limitUp + limitAlong.z * limitForward;

					// yaw friction - rubbing along limit plane
					Vector limitVel;
					limitVel.x = 0.0f; // TODO: this was uninitialized, and is used when yawBounce is non-zero!
					limitVel.y = DotProduct( limitUp, data->tipVel );
					limitVel.z = DotProduct( limitForward, data->tipVel );

					data->tipAccel -= jiggleInfo->yawFriction * (limitVel.y * limitUp + limitVel.z * limitForward);

					// update velocity reaction to hitting constraint
					data->tipVel = -jiggleInfo->yawBounce * limitVel.x * limitLeft + limitVel.y * limitUp + limitVel.z * limitForward;

					// update along vectors for use by pitch constraint
					along = data->tipPos - goalBasePosition;
					localAlong.x = DotProduct( goalLeft, along );
					localAlong.y = DotProduct( goalUp, along );
					localAlong.z = DotProduct( goalForward, along );

					localVel.x = DotProduct( goalLeft, data->tipVel );
					localVel.y = DotProduct( goalUp, data->tipVel );
					localVel.z = DotProduct( goalForward, data->tipVel );
				}
			}


			if (jiggleInfo->flags & JIGGLE_HAS_PITCH_CONSTRAINT)
			{
				// enforce pitch constraints in local YZ plane
				float pitchError = atan2( localAlong.y, localAlong.z );

				bool isAtLimit = false;
				float pitch = 0.0f;

				if (pitchError < jiggleInfo->minPitch)
				{
					// at angular limit
					isAtLimit = true;
					pitch = jiggleInfo->minPitch;
				}
				else if (pitchError > jiggleInfo->maxPitch)
				{
					// at angular limit
					isAtLimit = true;
					pitch = jiggleInfo->maxPitch;
				}

				if (isAtLimit)
				{
					float sp, cp;
					SinCos( pitch, &sp, &cp );

					// pitch matrix
					matrix3x4_t pitchMatrix;

					pitchMatrix[0][0] = 1.0f;
					pitchMatrix[1][0] = 0;
					pitchMatrix[2][0] = 0;

					pitchMatrix[0][1] = 0;
					pitchMatrix[1][1] = cp;
					pitchMatrix[2][1] = -sp;

					pitchMatrix[0][2] = 0;
					pitchMatrix[1][2] = sp;
					pitchMatrix[2][2] = cp;

					pitchMatrix[0][3] = 0;
					pitchMatrix[1][3] = 0;
					pitchMatrix[2][3] = 0;

					// global coordinates of limit
					matrix3x4_t limitMatrix;					
					ConcatTransforms( goalMX, pitchMatrix, limitMatrix );

					Vector limitLeft( limitMatrix.m_flMatVal[0][0], 
						limitMatrix.m_flMatVal[1][0],
						limitMatrix.m_flMatVal[2][0] );

					Vector limitUp( limitMatrix.m_flMatVal[0][1], 
						limitMatrix.m_flMatVal[1][1],
						limitMatrix.m_flMatVal[2][1] );

					Vector limitForward( limitMatrix.m_flMatVal[0][2], 
						limitMatrix.m_flMatVal[1][2],
						limitMatrix.m_flMatVal[2][2] );

					if (JiggleBoneDebugPitchConstraints.GetBool())
					{
						float dT = 0.01f;
						const float axisSize = 10.0f;
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitLeft, 0, 255, 255, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitUp, 255, 255, 0, true, dT );
						debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * limitForward, 255, 0, 255, true, dT );
					}

					Vector limitAlong( DotProduct( limitLeft, along ), 
						DotProduct( limitUp, along ),
						DotProduct( limitForward, along ) );

					// clip to limit plane
					data->tipPos = goalBasePosition + limitAlong.x * limitLeft + limitAlong.z * limitForward;

					// pitch friction - rubbing along limit plane
					Vector limitVel;
					limitVel.x = 0.0f; // TODO: this was uninitialized, but is being used
					limitVel.y = DotProduct( limitUp, data->tipVel );
					limitVel.z = DotProduct( limitForward, data->tipVel );

					data->tipAccel -= jiggleInfo->pitchFriction * (limitVel.x * limitLeft + limitVel.z * limitForward);

					// update velocity reaction to hitting constraint
					data->tipVel = limitVel.x * limitLeft - jiggleInfo->pitchBounce * limitVel.y * limitUp + limitVel.z * limitForward;
				}
			}
		}

		// needed for matrix assembly below
		Vector forward = data->tipPos - goalBasePosition;
		forward.NormalizeInPlace();

		if (jiggleInfo->flags & JIGGLE_HAS_ANGLE_CONSTRAINT)
		{
			// enforce max angular error
			Vector error = goalTip - data->tipPos;
			float dot = DotProduct( forward, goalForward );
			float angleBetween = acos( dot );
			if (dot < 0.0f)
			{
				angleBetween = 2.0f * M_PI - angleBetween;
			}

			if (angleBetween > jiggleInfo->angleLimit)
			{
				// at angular limit
				float maxBetween = jiggleInfo->length * sin( jiggleInfo->angleLimit );

				Vector delta = goalTip - data->tipPos;
				delta.NormalizeInPlace();

				data->tipPos = goalTip - maxBetween * delta;

				forward = data->tipPos - goalBasePosition;
				forward.NormalizeInPlace();
			}
		}

		if (jiggleInfo->flags & JIGGLE_HAS_LENGTH_CONSTRAINT)
		{
			// enforce spring length
			data->tipPos = goalBasePosition + jiggleInfo->length * forward;

			// zero velocity along forward bone axis
			data->tipVel -= DotProduct( data->tipVel, forward ) * forward;
		}

		//
		// Build bone matrix to align along current tip direction
		//
		Vector left, up;
		if ( coordSystemIsFlipped )
		{
			// If the coordinate system is flipped, use left handed rules.
			left = CrossProduct( forward, goalUp );
			left.NormalizeInPlace();
			up = CrossProduct( left, forward );
		}
		else
		{
			left = CrossProduct( goalUp, forward );
			left.NormalizeInPlace();
			up = CrossProduct( forward, left );
		}


		boneMX[0][0] = left.x;
		boneMX[1][0] = left.y;
		boneMX[2][0] = left.z;
		boneMX[0][1] = up.x;
		boneMX[1][1] = up.y;
		boneMX[2][1] = up.z;
		boneMX[0][2] = forward.x;
		boneMX[1][2] = forward.y;
		boneMX[2][2] = forward.z;

		boneMX[0][3] = goalBasePosition.x;
		boneMX[1][3] = goalBasePosition.y;
		boneMX[2][3] = goalBasePosition.z;
	}


	//
	// Bone base flex
	//
	if (jiggleInfo->flags & JIGGLE_HAS_BASE_SPRING)
	{
		// gravity
		data->baseAccel.z -= jiggleInfo->baseMass;

		// simple spring
		Vector error = goalBasePosition - data->basePos;
		data->baseAccel += jiggleInfo->baseStiffness * error - jiggleInfo->baseDamping * data->baseVel;

		data->baseVel += data->baseAccel * deltaT;
		data->basePos += data->baseVel * deltaT;

		// clear this timestep's accumulated accelerations
		data->baseAccel = vec3_origin;		

		// constrain to limits
		error = data->basePos - goalBasePosition;
		Vector localError;
		localError.x = DotProduct( goalLeft, error );
		localError.y = DotProduct( goalUp, error );
		localError.z = DotProduct( goalForward, error );

		Vector localVel;
		localVel.x = DotProduct( goalLeft, data->baseVel );
		localVel.y = DotProduct( goalUp, data->baseVel );
		localVel.z = DotProduct( goalForward, data->baseVel );

		// horizontal constraint
		if (localError.x < jiggleInfo->baseMinLeft)
		{
			localError.x = jiggleInfo->baseMinLeft;

			// friction
			data->baseAccel -= jiggleInfo->baseLeftFriction * (localVel.y * goalUp + localVel.z * goalForward);
		}
		else if (localError.x > jiggleInfo->baseMaxLeft)
		{
			localError.x = jiggleInfo->baseMaxLeft;

			// friction
			data->baseAccel -= jiggleInfo->baseLeftFriction * (localVel.y * goalUp + localVel.z * goalForward);
		}

		if (localError.y < jiggleInfo->baseMinUp)
		{
			localError.y = jiggleInfo->baseMinUp;

			// friction
			data->baseAccel -= jiggleInfo->baseUpFriction * (localVel.x * goalLeft + localVel.z * goalForward);
		}
		else if (localError.y > jiggleInfo->baseMaxUp)
		{
			localError.y = jiggleInfo->baseMaxUp;

			// friction
			data->baseAccel -= jiggleInfo->baseUpFriction * (localVel.x * goalLeft + localVel.z * goalForward);
		}

		if (localError.z < jiggleInfo->baseMinForward)
		{
			localError.z = jiggleInfo->baseMinForward;

			// friction
			data->baseAccel -= jiggleInfo->baseForwardFriction * (localVel.x * goalLeft + localVel.y * goalUp);
		}
		else if (localError.z > jiggleInfo->baseMaxForward)
		{
			localError.z = jiggleInfo->baseMaxForward;

			// friction
			data->baseAccel -= jiggleInfo->baseForwardFriction * (localVel.x * goalLeft + localVel.y * goalUp);
		}

		data->basePos = goalBasePosition + localError.x * goalLeft + localError.y * goalUp + localError.z * goalForward;


		// fix up velocity
		data->baseVel = (data->basePos - data->baseLastPos) / deltaT;
		data->baseLastPos = data->basePos;


		if (!(jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID)))
		{
			// no tip flex - use bone's goal orientation
			boneMX = goalMX;							
		}

		// update bone position
		MatrixSetColumn( data->basePos, 3, boneMX );
	}
	else if (!(jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID)))
	{
		// no flex at all - just use goal matrix
		boneMX = goalMX;
	}


	// debug display
	if ( JiggleBoneDebug.GetInt() == 1 || JiggleBoneDebug.GetInt() == data->id )
	{
		float dT = 0.01f;
		const float axisSize = 5.0f;
		debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * goalLeft, 255, 0, 0, true, dT );
		debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * goalUp, 0, 255, 0, true, dT );
		debugoverlay->AddLineOverlay( goalBasePosition, goalBasePosition + axisSize * goalForward, 0, 0, 255, true, dT );

		debugoverlay->AddTextOverlay( goalBasePosition, dT, "%d", data->id );

		const float sz = 1.0f;

		if (jiggleInfo->flags & (JIGGLE_IS_FLEXIBLE | JIGGLE_IS_RIGID))
		{
			debugoverlay->AddLineOverlay( goalBasePosition, 
				data->tipPos, 255, 255, 0, true, dT );

			debugoverlay->AddLineOverlay( data->tipPos + Vector( -sz, 0, 0 ), 
				data->tipPos + Vector( sz, 0, 0 ), 0, 255, 255, true, dT );
			debugoverlay->AddLineOverlay( data->tipPos + Vector( 0, -sz, 0 ), 
				data->tipPos + Vector( 0, sz, 0 ), 0, 255, 255, true, dT );
			debugoverlay->AddLineOverlay( data->tipPos + Vector( 0, 0, -sz ), 
				data->tipPos + Vector( 0, 0, sz ), 0, 255, 255, true, dT );
		}

		if (jiggleInfo->flags & JIGGLE_HAS_BASE_SPRING)
		{
			debugoverlay->AddLineOverlay( data->basePos + Vector( -sz, 0, 0 ), 
				data->basePos + Vector( sz, 0, 0 ), 255, 0, 255, true, dT );
			debugoverlay->AddLineOverlay( data->basePos + Vector( 0, -sz, 0 ), 
				data->basePos + Vector( 0, sz, 0 ), 255, 0, 255, true, dT );
			debugoverlay->AddLineOverlay( data->basePos + Vector( 0, 0, -sz ), 
				data->basePos + Vector( 0, 0, sz ), 255, 0, 255, true, dT );
		}
	}
}

