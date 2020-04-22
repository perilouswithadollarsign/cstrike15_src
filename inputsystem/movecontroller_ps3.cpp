//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

#include "movecontroller_ps3.h"
#include <cell/camera.h> // PS3 eye camera
#include <pthread.h>

#include <vjobs/root.h>
#include <tier1/convar.h>
#include <tier0/dbg.h>

// NOTE: This has to be the last file included!
#include <tier0/memdbgon.h>
#include "input_device.h"
#include "inputsystem.h"

#include <vectormath/cpp/vectormath_aos.h>

using namespace Vectormath::Aos;

#define MC_MAX_NUM_SAMPLES 30

enum FilterModeType { NONE, LOW_PASS, MOVING_AVG, EXP_MOVING_AVG, GYRO_ATTEN_LOW_PASS, GYRO_PREDICTOR_CORRECTOR, DISTANCE_FALLOFF };

// time in microseconds between gyro samples (gyro samples are updated at approximately 180 Hz = ~5556 usec)
#define GYRO_SAMPLE_SPACING 5625

// [dkorus] lifted this define from the samples/sdk/gem/sharpshooter demo
#define SHARP_SHOOTER_DEVICE_ID 0x8081

extern IVJobs * g_pVJobs;
CMoveController g_moveController;
CMoveController* g_pMoveController = &g_moveController;

ConVar ps3_move_roll_trigger( "ps3_move_roll_trigger", "45.0", FCVAR_ARCHIVE, "amount of roll to trigger R/L shoulder button press in degrees" );

ConVar ps3_move_enabled( "ps3_move_enabled", "1", FCVAR_DEVELOPMENTONLY, "0 => Disabled, 1 => Enabled." );
ConVar ps3_move_filter_method( "ps3_move_filter_method", "6", FCVAR_DEVELOPMENTONLY, "Which filter method to use. 0 to 6 (none, low_pass, moving_average, gyro_atten_low_pass, gyro_corrector, distance_falloff)." );
ConVar ps3_move_cursor_sampling( "ps3_move_cursor_sampling", "0.5", FCVAR_DEVELOPMENTONLY, "0 to 1. Larger numbers = more samples, smoother, more lag.", true, 0.0f, true, 1.0f );
ConVar mc_cursor_sensitivity( "mc_cursor_sensitivity", "0.5", FCVAR_ARCHIVE, "0.0 to 1.0", true, 0.0f, true, 1.0f );
ConVar mc_min_cursor_sensitivity( "mc_min_cursor_sensitivity", "0.25", FCVAR_DEVELOPMENTONLY, "0.0 to 1.0", true, 0.0f, true, 1.0f );
ConVar mc_max_cursor_sensitivity( "mc_max_cursor_sensitivity", "1.25", FCVAR_DEVELOPMENTONLY, "0.0 to 4.0", true, 0.0f, true, 4.0f );

static sys_ppu_thread_t			s_gemThread;
static bool						s_bGemThreadExit = false;
static sys_memory_container_t	s_camContainer;

struct { float left, right, bottom, top; } static s_tracker_plane_extents[MAX_PS3_MOVE_CONTROLLERS];	/**< @brief tracking region extents for each controller */
static int						s_nCalibrationStep = 0;



//--------------------------------------------------------------------------------------------------
// GemThreadState contains motion controller state
// written to by Gem update thread
// read by main thread
//--------------------------------------------------------------------------------------------------
struct GemThreadState
{
	CellGemInfo		m_CellGemInfo;
	CellGemState	m_aCellGemState[MAX_PS3_MOVE_CONTROLLERS];
	int32			m_aStatus[MAX_PS3_MOVE_CONTROLLERS]; // associated getState return values
	uint64			m_aStatusFlags[MAX_PS3_MOVE_CONTROLLERS];
	int32			m_camStatus;
	Vector			m_pos[MAX_PS3_MOVE_CONTROLLERS];
	Quaternion		m_quat[MAX_PS3_MOVE_CONTROLLERS];
	float			m_posX[MAX_PS3_MOVE_CONTROLLERS];
	float			m_posY[MAX_PS3_MOVE_CONTROLLERS];
};
static GemThreadState s_gemThreadState;

static void UpdateGemThread(uint64 args);


static float getPitch( vec_float4 q ) 
{
	Vectormath::Aos::Quat quat( q );
	Vectormath::Aos::Vector3 home_dir( 0, 0, -1 );
	Vectormath::Aos::Vector3 new_dir = Vectormath::Aos::rotate( quat, home_dir );
	float x = new_dir[0], y = new_dir[1], z = new_dir[2];
	return atan2f( y, sqrtf( x * x + z * z) );
}

// Yaw - Rotation of the controller pointing axis, if it were projected into the x-z plane
// Same as telescope azimuth angle.
static float getYaw( vec_float4 q )
{
	Vectormath::Aos::Quat quat( q );
	Vectormath::Aos::Vector3 home_dir( 0, 0, -1 );
	Vectormath::Aos::Vector3 new_dir = Vectormath::Aos::rotate( quat, home_dir );
	new_dir[1] = 0; // portion on x-z plane
	return atan2f( -new_dir[0], -new_dir[2] );
}

//Performs a raw ray cast into the tracker plane extents down the -Z axis of the controller
static void MoveKitPointerIntersectWithTrackerPlane(VmathVector3 position, VmathQuat orientation, float* pointerX, float* pointerY)
{
	// given the gem position and orientation, form a ray from the ball down the gem -Z axis (direction of pointing)
	VmathVector3 rayStart;	
	VmathVector3 rayDir;

	vmathV3Copy(&rayStart, &position);
	vmathV3MakeFromElems(&rayDir, 0.0f, 0.0f, -1.0f);

	// rotate the direction into the orientation of the controller
	vmathQRotate(&rayDir, &orientation, &rayDir);

	// intersect the ray with the display plane (at z=0):
	// isect.z = rayStart.z + rayDir.z * t, so t = (0 - rayStart.z) / rayDir.z
	float t = -vmathV3GetZ(&rayStart) / vmathV3GetZ(&rayDir);

	*pointerX = vmathV3GetX(&rayStart) + vmathV3GetX(&rayDir)*t;
	*pointerY = vmathV3GetY(&rayStart) + vmathV3GetY(&rayDir)*t;
}

/** @brief Calculates position of cursor in screen space.
 *
 * Takes the most recent position and orientation of the given controller and converts the pointed at location in the display plane to screen space.
 *
 * @param[in]	gem_num			index of controller we're calculating for
 * @param[in]	position		3D position of controller from most recently queried state
 * @param[in]	orientation		orientation of controller from most recently queried state
 * @param[out]	pointerX		normalized but unclamped X position of pointer in screen space
 * @param[out]	pointerY		normalized but unclamped Y position of pointer in screen space
 *
 * @post pointerX and pointerY contain normalized (but unclamped) screen positions based on the specified state
 *
 * @see MoveKitPointerCalcPointerNormalizedRawFromState
 */
void MoveKitPointerCalcPointerNormalizedRaw(int gem_num, VmathVector3 position, VmathQuat orientation, float* pointerX, float* pointerY)
{
	MoveKitPointerIntersectWithTrackerPlane(position, orientation, pointerX, pointerY);
	*pointerX = -1.0f + 2.0f*((*pointerX-s_tracker_plane_extents[gem_num].left)/(s_tracker_plane_extents[gem_num].right-s_tracker_plane_extents[gem_num].left));
	*pointerY = -1.0f + 2.0f*((*pointerY-s_tracker_plane_extents[gem_num].bottom)/(s_tracker_plane_extents[gem_num].top-s_tracker_plane_extents[gem_num].bottom));
}

//--------------------------------------------------------------------------------------------------
// ReadCamera
// Init camera if required and read image data for latest frame using cellCameraReadEx
// Returns the cellCameraReadEx return code
//--------------------------------------------------------------------------------------------------
static int32 ReadCamera(CellCameraReadEx *pCamReadEx)
{
	pCamReadEx->version=CELL_CAMERA_READ_VER;
	int32 camStatus = cellCameraReadEx(0,pCamReadEx);

	if (camStatus==CELL_CAMERA_ERROR_NOT_OPEN)
	{
		CellCameraType type;
		cellCameraGetType(0, &type);
		if (type == CELL_CAMERA_EYETOY2)
		{
			sys_memory_container_create(&s_camContainer, 0x100000);
			CellCameraInfoEx camera_info;
			camera_info.format=CELL_CAMERA_RAW8;
			camera_info.framerate=60;
			camera_info.resolution=CELL_CAMERA_VGA;
			camera_info.container=s_camContainer;
			camera_info.info_ver=CELL_CAMERA_INFO_VER_101;
			cellCameraOpenEx(0, &camera_info);
			camStatus = cellCameraReadEx(0,pCamReadEx);
		}
	}

	if (camStatus==CELL_CAMERA_ERROR_NOT_STARTED)
	{
		cellCameraReset(0);
		cellCameraStart(0);

		// Prepare camera using exposure and image quality settings 
		// for best tracking performance (see PS3 docs)
		cellGemPrepareCamera(CELL_GEM_MIN_CAMERA_EXPOSURE,0);

		camStatus = cellCameraReadEx(0,pCamReadEx);
	}

	return camStatus;
}

static void StartUpdateGemThread(GemThreadState *pState)
{
	s_bGemThreadExit = false;

	sys_ppu_thread_create( &s_gemThread, UpdateGemThread, (uint64)pState, 
		1001, // PRIORITY 0-3071, 0 highest 
		16*1024, // Stack size, multiple of 4 KB
		SYS_PPU_THREAD_CREATE_JOINABLE, "UpdateMoveController" );

}

static inline void FlashSphere(const int controllerNum)
{
	const int speed = (1<<18)-1;
	float t  = M_PI * (float)((int)clock()%speed) / (speed-1); // t in range [0,PI]
	float ft = exp(-t*t); // f(t) = 1 / e^(t^2) in range [1,0)
	cellGemForceRGB(controllerNum,ft,ft,ft);
}

static bool CheckForSharpshooterConnected( int controllerId )
{
	unsigned int ext_id;
	unsigned char ext_info[CELL_GEM_EXTERNAL_PORT_DEVICE_INFO_SIZE];
	cellGemReadExternalPortDeviceInfo( controllerId, &ext_id, ext_info);
	bool sharpshooterConnected = g_pInputSystem->IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER );

	if (ext_id == SHARP_SHOOTER_DEVICE_ID && 
		!sharpshooterConnected )
	{
		Msg("Sharp Shooter is connected and ready!\n");					
		g_pInputSystem->SetInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER, true );
		return true;
	}
	else if( !sharpshooterConnected )
	{
		g_pInputSystem->SetInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER, false );
		Msg("Sharp Shooter is disconnected2!\n");					

	}
	return false;
}

bool CheckForMoveConnected( int controllerId )
{
	CellGemState gemState;
	int32 gemStatus = cellGemGetState(controllerId, CELL_GEM_STATE_FLAG_LATEST_IMAGE_TIME, 0, &gemState);
	if(s_gemThreadState.m_CellGemInfo.status[controllerId]==CELL_GEM_STATUS_READY)
	{
		return true;
	}

	return false;
}


/** @brief Calculates position of cursor in screen space.
 *
 * Takes the state of the given controller and converts the pointed at location in the display plane to screen space.
 *
 * @param[in]	gem_num		index of controller corresponding to specified state
 * @param[in]	state		the most recently queried gem state for the specified controller
 * @param[out]	pointerX	normalized but unclamped X position of pointer in screen space
 * @param[out]	pointerY	normalized but unclamped Y position of pointer in screen space
 *
 * @post pointerX and pointerY contain normalized (but unclamped) screen positions based on the specified state
 *
 * @see MoveKitPointerCalcPointerNormalizedRaw
 */
void MoveKitPointerCalcPointerNormalizedRawFromState(int gem_num, CellGemState* state, float* pointerX, float* pointerY)
{
	VmathQuat orientation;
	vmathQMakeFrom128(&orientation, state->quat);
	VmathVector3 position;
	vmathV3MakeFrom128(&position, state->pos);
	return MoveKitPointerCalcPointerNormalizedRaw(gem_num, position, orientation, pointerX, pointerY);
}

/** @brief Implementation of simple low-pass filter
 *
 * Blend the current pointer position with previous samples.
 * The blend weight controls the amount of lag and smoothness.
 * Higher blend weight (alpha) favors the new position while
 * lower favors the older (smoother).
 *
 * @note
 * The canonical "sluggish feeling" filter.
 *
 * @param[out]	filteredX		X component of filtered pointer position
 * @param[out]	filteredY		Y component of filtered pointer position
 * @param		screenWidth		currently unused
 * @param		screenHeight	currently unused
 * @param[in]	whichGem		index of controller we're filtering [0-3]
 * @param[in]	numSamples		number of consecutive samples to include in filtering calculation, including current sample
 * @param[in]	alpha			blend weight [0.0-1.0]
 *
 * @returns return code from last \a cellGemGetState call
 */
int32_t calcFilteredPointerPos_LowPass(float &filteredX, float &filteredY,
									   const int whichGem, const int numSamples, const float alpha)
{
	assert(numSamples<=MC_MAX_NUM_SAMPLES);

	// grab the current gem state time stamp
	CellGemState gemState;
	int32_t retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_CURRENT_TIME,CELL_GEM_LATENCY_OFFSET,&gemState);
	system_time_t startTimeStamp = gemState.timestamp;

	// blend numSamples consecutive samples
	retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_TIMESTAMP,startTimeStamp-GYRO_SAMPLE_SPACING*(numSamples-1),&gemState);
	assert(retVal!=CELL_GEM_TIME_OUT_OF_RANGE);
	
	MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &filteredX, &filteredY);
	//laserPointerCalcPos(filteredX,filteredY,gemState,screenWidth,screenHeight);
	for (int i=numSamples-2; i>=0; i--)
	{
		float x, y;
		retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_TIMESTAMP,startTimeStamp-GYRO_SAMPLE_SPACING*i,&gemState);
		MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &x, &y);
		//laserPointerCalcPos(x,y,gemState,screenWidth,screenHeight);

		filteredX = (x * alpha) + (filteredX * (1-alpha));
		filteredY = (y * alpha) + (filteredY * (1-alpha));
	}

	return(retVal);
}

/** @brief Implementation of moving average filter
 *
 * Collect a history of N samples and average them.
 *
 * @note
 * Increasing the number of samples increases the smoothness as well as the latency.
 *
 * @note
 * More responsive than the simple low pass and nearly as stable.
 *
 * @note
 * Optimal at removing "white noise" while preserving sharp transitions. http://www.dspguide.com/ch15/2.htm
 *
 * @param[out]	filteredX		X component of filtered pointer position
 * @param[out]	filteredY		Y component of filtered pointer position
 * @param		screenWidth		currently unused
 * @param		screenHeight	currently unused
 * @param[in]	whichGem		index of controller we're filtering [0-3]
 * @param[in]	numSamples		number of consecutive samples to include in filtering calculation, including current sample
 *
 * @returns return code from last \a cellGemGetState call
 */
int32_t calcFilteredPointerPos_MovingAvg(float &filteredX, float &filteredY, const int whichGem, const int numSamples)
{
	assert(numSamples<=MC_MAX_NUM_SAMPLES);

	// grab the current gem state time stamp
	CellGemState gemState;
	int32_t retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_CURRENT_TIME,CELL_GEM_LATENCY_OFFSET,&gemState);
	system_time_t startTimeStamp = gemState.timestamp;

	// average numSamples consecutive samples
	filteredX=0.0f;
	filteredY=0.0f;
	int numActualSamples = 0;
	for (int i=0; i<numSamples; i++)
	{
		retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_TIMESTAMP,startTimeStamp-GYRO_SAMPLE_SPACING*i,&gemState);
		if ( retVal == 0 )
		{
			float x=0.0f, y=0.0f;
			MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &x, &y);
			filteredX +=x;
			filteredY +=y;
			numActualSamples ++;
		}
		else
		{
			assert(retVal!=CELL_GEM_TIME_OUT_OF_RANGE);
		}
	}
	if ( numActualSamples > 1 )
	{
		filteredX /= (float)numActualSamples;
		filteredY /= (float)numActualSamples;
	}

	return(retVal);
}

/** @brief Implementation of exponential moving average filter
 *
 * Same as the moving average, but weights newer points in the average higher
 * than the older ones. The weights drop off exponentially, so for 4 samples:
 * 1, 0.5, 0.25, 0.125
 *
 * @note
 * More responsive than the moving average, but also less smooth
 *
 * @param[out]	filteredX			X component of filtered pointer position
 * @param[out]	filteredY			Y component of filtered pointer position
 * @param		screenWidth			currently unused
 * @param		screenHeight		currently unused
 * @param[in]	whichGem			index of controller we're filtering [0-3]
 * @param[in]	numSamples			number of consecutive samples to include in filtering calculation, including current sample
 * @param[in]	expMovAvgExponent	exponential drop-off for samples
 *
 * @returns return code from last \a cellGemGetState call
 */
int32_t calcFilteredPointerPos_ExpMovingAvg(float &filteredX, float &filteredY,
											const int whichGem, const int numSamples, const float expMovAvgExponent)
{
	assert(numSamples<=MC_MAX_NUM_SAMPLES);

	// grab the current gem state time stamp
	CellGemState gemState;
	int32_t retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_CURRENT_TIME,CELL_GEM_LATENCY_OFFSET,&gemState);
	system_time_t startTimeStamp = gemState.timestamp;

	// average numSamples consecutive samples
	filteredX=0;
	filteredY=0;
	float totalWeight=0;
	for (int i=0; i<numSamples; i++)
	{
		float x, y;
		retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_TIMESTAMP,startTimeStamp-GYRO_SAMPLE_SPACING*i,&gemState);
		assert(retVal!=CELL_GEM_TIME_OUT_OF_RANGE);
		MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &x, &y);
		//laserPointerCalcPos(x,y,gemState,screenWidth,screenHeight);
		float weight = 1.f / powf(expMovAvgExponent, i);
		filteredX += (x * weight);
		filteredY += (y * weight);
		totalWeight += weight;
	}
	filteredX /= totalWeight;
	filteredY /= totalWeight;

	return(retVal);
}

/** @brief Implementation of gyro attentuated low pass filter
 *
 * Uses the magnitude of the gyro signal to guide a simple low-pass filter.
 * Faster gyro motion allows the filter to be more responsive, slower
 * motion smooths the signal. The intended behavior is that when the
 * user is moving slowly, the response is smoother. While this introduces
 * latency, its not as noticeable at slow speeds, where smoothness and
 * stability are more important. During fast motions, the gyro magnitude
 * is high, which allows the filter to pass the signal through "raw" so
 * there is no latency or smooth. Fast motions tend to be imprecise,
 * so low latency is more important than smoothness. Slow motions are
 * assumed to be "precision" adjustments, which can have some latency
 * at the cost of smoothing.
 *
 * @note
 * I think this filter feels best for twitchy precise motions. If the
 * user does a lot of fine corrections, or a lot of "medium speed" motions
 * it will feel sluggish. But for a precise user, it acts fast when 
 * twitching and then stabilizes on the local area.
 *
 * @param[out]	filteredX		X component of filtered pointer position
 * @param[out]	filteredY		Y component of filtered pointer position
 * @param		screenWidth		currently unused
 * @param		screenHeight	currently unused
 * @param[in]	whichGem		index of controller we're filtering [0-3]
 * @param[in]	numSamples		number of consecutive samples to include in filtering calculation, including current sample
 * @param[in]	maxGyroAngVel	speed at which the blend param = 1, in radians
 *
 * @returns return code from last \a cellGemGetState call
 */
int32_t calcFilteredPointerPos_GyroAttenLowPass(float &filteredX, float &filteredY,
												const int whichGem, const int numSamples, const float maxGyroAngVel)
{
	assert(numSamples<=MC_MAX_NUM_SAMPLES);

	// grab the current gem state time stamp
	CellGemState gemState;
	int32_t retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_CURRENT_TIME,CELL_GEM_LATENCY_OFFSET,&gemState);
	system_time_t startTimeStamp = gemState.timestamp;

	// blend numSamples consecutive samples
	retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_TIMESTAMP,startTimeStamp-GYRO_SAMPLE_SPACING*(numSamples-1),&gemState);
	assert(retVal!=CELL_GEM_TIME_OUT_OF_RANGE);
	MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &filteredX, &filteredY);
	//laserPointerCalcPos(filteredX,filteredY,gemState,screenWidth,screenHeight);
	for (int i=numSamples-2; i>=0; i--)
	{
		float x, y;
		retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_TIMESTAMP,startTimeStamp-GYRO_SAMPLE_SPACING*i,&gemState);
		MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &x, &y);
		//laserPointerCalcPos(x,y,gemState,screenWidth,screenHeight);

		float angVel = sqrtf( gemState.angvel[0]*gemState.angvel[0] + gemState.angvel[1]*gemState.angvel[1] + gemState.angvel[2]*gemState.angvel[2] );
		angVel = (angVel>maxGyroAngVel) ? maxGyroAngVel : angVel;
		float alpha = angVel / maxGyroAngVel;
		filteredX = (x * alpha) + (filteredX * (1-alpha));
		filteredY = (y * alpha) + (filteredY * (1-alpha));
	}

	return(retVal);
}

static inline double rotationAngle(const Quat& u, const Quat& v)
{
	double udotv = dot(u, v);
	return (fabsf(udotv)>1.0f) ? 0.0f : 2.0f*acosf(udotv);
}

/** @brief Implementation of gyro-based predictor/corrector filter
 *
 *  This filter prevents spurious angular corrections (like from the
 *  magnetometer or the internal gem filtering system) that are inconsistent 
 *  with the current gyro measurements. Corrections should always be
 *  proportional to the magnitude of motions read from the gyros, and when
 *  it is motionless, stray corrections should not occur.
 *
 *  This check is performed as follows:
 *  1) Directly integrates the angular velocity from the gyros (the gemState angvel)
 *  2) Compare the gyro motion to the "correction" motion (between the current
 *     state and the integrated result)
 *  3) limit the amount of correction proportional to the amount of actual gyro motion.
 *
 * @note
 * This is technically not a motion filter, but rather a spurious motion check 
 * that could be combined with other filters.
 *
 * @param[out]	filteredX		X component of filtered pointer position
 * @param[out]	filteredY		Y component of filtered pointer position
 * @param[in]	whichGem		index of controller we're filtering [0-3]
 *
 * @returns return code from last \a cellGemGetState call
 */
int32_t calcFilteredPointerPos_GyroPredictorCorrector(float &filteredX, float &filteredY,
													  const int whichGem)
{
	static Quat laserQuat[4] = { Quat(0,0,0,1), Quat(0,0,0,1), Quat(0,0,0,1), Quat(0,0,0,1) }; // quat integration accumulators
	static system_time_t lastStateTimestamp[4] = { 0,0,0,0 };

	CellGemState gemState;
	int32_t retVal = cellGemGetState(whichGem,CELL_GEM_STATE_FLAG_CURRENT_TIME,CELL_GEM_LATENCY_OFFSET,&gemState);
	float dt = (gemState.timestamp - lastStateTimestamp[whichGem]) * 1e-6f; // in seconds
	lastStateTimestamp[whichGem] = gemState.timestamp;

	if (dt < 3.0f/60.0f) // update normally if time between last sampling is less than a few frames
	{
		// this crazy math integrates the world-space angular velocity (which happens to come from the gyros only), and adds it to laserQuat.
		Vector3 angVel(gemState.angvel);
		Quat angVelQuat(angVel, 0.0f);
		Quat quatDot = (0.5f * angVelQuat) * laserQuat[whichGem];
		Quat integrationResult = normalize(laserQuat[whichGem] + quatDot*dt);

		Quat stateQuat(gemState.quat);

		float integrationAngle = fabsf(rotationAngle(laserQuat[whichGem], integrationResult)); // magnitude of the motion-due to gyros
		float stateDiffAngle = fabsf(rotationAngle(integrationResult, stateQuat)); // magnitude of the correction that would be needed to match the gyro-based result to libgem's result

		// limit the amount of "correction" that occurs to 0.1 * amount of the gyro motion, so as to hide it
		// the big benefit of this is that no "correction" occurs if there is no gyro motion, so the pointer
		// only moves when the gyros say there is motion occurring
		float angleLimit = (stateDiffAngle > 0.1f*integrationAngle)? 0.1f*integrationAngle : stateDiffAngle;

		// rotate the integrationResult by the correction amount.  this is the new output quat
		if (stateDiffAngle > 1e-10f)
			laserQuat[whichGem] = slerp(angleLimit/stateDiffAngle, integrationResult, stateQuat);
		else
			laserQuat[whichGem] = integrationResult;

		gemState.quat = laserQuat[whichGem].get128();
	}
	else // if too long since last sample, set the integration accumulator quat to the current gem state quat
	{
		laserQuat[whichGem] = Quat(gemState.quat);
	}

	MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &filteredX, &filteredY);

	return(retVal);
}

// Weights samples based on their distance to the latest sample.
int32_t calcFilteredPointerPos_DistanceFalloff(float &filteredX, float &filteredY, const int whichGem)
{
	// The higher the sampling (more lag), the smaller we want fRangeScale to be since a smaller range will include more samples and give them higher weights.
	// So we use 1.0 - ps3_move_cursor_sampling.
	const float fRangeScale = (float)(0x1 << (int)( (1.0f - ps3_move_cursor_sampling.GetFloat()) * 10.0f));
	float latestSampleX=0.0f, latestSampleY=0.0f;

	// grab the current gem state time stamp
	CellGemState gemState;
	int32_t retVal = cellGemGetState( whichGem, CELL_GEM_STATE_FLAG_CURRENT_TIME, CELL_GEM_LATENCY_OFFSET, &gemState );
	MoveKitPointerCalcPointerNormalizedRawFromState(whichGem, &gemState, &latestSampleX, &latestSampleY);
	system_time_t startTimeStamp = gemState.timestamp;

	// average numSamples consecutive samples
	filteredX=0.0f;
	filteredY=0.0f;
	float fTotalWeight = 0.0f;
	for (int i=0; i<MC_MAX_NUM_SAMPLES; i++)
	{
		retVal = cellGemGetState( whichGem, CELL_GEM_STATE_FLAG_TIMESTAMP, startTimeStamp-GYRO_SAMPLE_SPACING*i, &gemState );
		if ( retVal == 0 )
		{
			float x=0.0f, y=0.0f;
			MoveKitPointerCalcPointerNormalizedRawFromState( whichGem, &gemState, &x, &y );
			float dx = x - latestSampleX;
			float dy = y - latestSampleY;
			float weight = 1.0f - ( sqrtf(dx*dx + dy*dy) * fRangeScale );
			weight = clamp(weight, 0.0f, 1.0f);

			filteredX += x*weight;
			filteredY += y*weight;
			fTotalWeight += weight;
		}
		else
		{
			assert(retVal!=CELL_GEM_TIME_OUT_OF_RANGE);
		}
	}
	if ( fTotalWeight > 0.0f )
	{
		filteredX /= (float)fTotalWeight;
		filteredY /= (float)fTotalWeight;
	}

	return(retVal);
}



static void GetFilteredPointerPosition( int gem_num, float *cursorX, float *cursorY )
{
	CellGemState gemState;
	float filter_lowpass_alpha = 0.1f;				//parameter for low pass filter
	float filter_exp_moving_avg_exponent = 0.1f;		//parameter for exponential moving average filter
	float filter_max_gyro_ang_vel = 0.1f;			//parameter for gyro attenuated low pass filter
	int retVal = -1;

	FilterModeType filter_type = (FilterModeType)ps3_move_filter_method.GetInt();
	// Higher sampling means use more samples (more smooth and laggy).
	int filter_numsamples = (int)( ps3_move_cursor_sampling.GetFloat() * (float)MC_MAX_NUM_SAMPLES);
	if (filter_numsamples < 1)
	{
		filter_numsamples = 1;
	}

	switch (filter_type)
	{
	case NONE:
		retVal = cellGemGetState(gem_num, CELL_GEM_STATE_FLAG_CURRENT_TIME, CELL_GEM_LATENCY_OFFSET, &gemState);
		MoveKitPointerCalcPointerNormalizedRawFromState(gem_num, &gemState, cursorX, cursorY);
		break;

	case LOW_PASS:
		retVal = calcFilteredPointerPos_LowPass(*cursorX, *cursorY, gem_num, filter_numsamples, filter_lowpass_alpha);
		break;

	case MOVING_AVG:
		retVal = calcFilteredPointerPos_MovingAvg(*cursorX, *cursorY, gem_num, filter_numsamples);
		break;

	case EXP_MOVING_AVG:
		retVal = calcFilteredPointerPos_ExpMovingAvg(*cursorX, *cursorY, gem_num, filter_numsamples, filter_exp_moving_avg_exponent);
		break;

	case GYRO_ATTEN_LOW_PASS:
		retVal = calcFilteredPointerPos_GyroAttenLowPass(*cursorX, *cursorY, gem_num, filter_numsamples, filter_max_gyro_ang_vel);
		break;

	case GYRO_PREDICTOR_CORRECTOR:
		retVal = calcFilteredPointerPos_GyroPredictorCorrector(*cursorX, *cursorY, gem_num);
		break;

	case DISTANCE_FALLOFF:
		retVal = calcFilteredPointerPos_DistanceFalloff(*cursorX, *cursorY, gem_num);
		break;

	default:
		*cursorX = 0.0f;
		*cursorY = 0.0f;
		
		break;
	}

	const float fMinCursorSensitivity = mc_min_cursor_sensitivity.GetFloat();
	const float fCursorSensitivity = fMinCursorSensitivity + (mc_max_cursor_sensitivity.GetFloat() - fMinCursorSensitivity) * mc_cursor_sensitivity.GetFloat();

	(*cursorX) *= fCursorSensitivity;
	(*cursorY) *= fCursorSensitivity;
}

// the main gem update function (this can be called in a thread or directly)
static void GemFrameUpdate()
{
	// Read move controller state if camera is ok
	if (s_gemThreadState.m_camStatus == CELL_OK)
	{
		bool moveConnected = g_pInputSystem->IsInputDeviceConnected( INPUT_DEVICE_PLAYSTATION_MOVE );
		bool sharpshooterConnected = g_pInputSystem->IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER );

		if ( !moveConnected && !sharpshooterConnected )
		{
			g_pInputSystem->SetMotionControllerDeviceStatus( INPUT_DEVICE_MC_STATE_CONTROLLER_NOT_CONNECTED );
		}

		cellGemGetInfo(&s_gemThreadState.m_CellGemInfo);
		for (int ii=0; ii<MAX_PS3_MOVE_CONTROLLERS; ++ii)
		{
			CellGemState gemState;
			int32 gemStatus = cellGemGetState( ii, CELL_GEM_STATE_FLAG_CURRENT_TIME, 0, &gemState );
			uint64 gemStatusFlags = 0;
			cellGemGetStatusFlags( ii, &gemStatusFlags );
			if(s_gemThreadState.m_CellGemInfo.status[ii]==CELL_GEM_STATUS_READY)
			{
				//  check for sharpshooter connection:
				if( !sharpshooterConnected && gemState.ext.status != 0 )
				{
					CheckForSharpshooterConnected( ii );
				}
				else if ( sharpshooterConnected && gemState.ext.status == 0 )
				{
					g_pInputSystem->SetInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER, false );					
					Msg("Sharp Shooter is disconnected!\n");					
				}

				if( !moveConnected )
					g_pInputSystem->SetInputDeviceConnected( INPUT_DEVICE_PLAYSTATION_MOVE, true );

				if (gemStatus == CELL_GEM_HUE_NOT_SET)
				{
					// if the gem needs a color and the sphere calibration is complete

					// pick a color (see http://en.wikipedia.org/wiki/Hue for 0-360 hue value description)
					#define HUE_BLUE 200
					unsigned int hues[] = {HUE_BLUE,HUE_BLUE,HUE_BLUE,HUE_BLUE};
					cellGemTrackHues(hues, NULL);
				}

				if (gemStatus == CELL_GEM_SPHERE_NOT_CALIBRATED )
				{
					if ( g_pInputSystem->GetMotionControllerDeviceStatus() != INPUT_DEVICE_MC_STATE_CONTROLLER_ERROR )
					{						
						g_pInputSystem->SetMotionControllerDeviceStatus( INPUT_DEVICE_MC_STATE_CONTROLLER_NOT_CALIBRATED );
					}
				}

				if (gemStatus == CELL_GEM_SPHERE_CALIBRATING) { // several frames are needed to finish calibration
					// several frames are needed to finish calibration
					FlashSphere( ii );
					g_pInputSystem->SetMotionControllerDeviceStatus( INPUT_DEVICE_MC_STATE_CONTROLLER_CALIBRATING );
				}

				if ( gemStatus == CELL_OK )
				{
					g_pInputSystem->SetMotionControllerDeviceStatus( INPUT_DEVICE_MC_STATE_OK );
				}



				//// handle buttons ////
				uint16 lastpadbuttons = s_gemThreadState.m_aCellGemState[ii].pad.digitalbuttons;
				uint16 pressedbuttons = gemState.pad.digitalbuttons & ~lastpadbuttons;

				if ( pressedbuttons & CELL_GEM_CTRL_MOVE &&
					 gemStatus != CELL_OK ) 
				{ // Calibrates when pointing at the camera if it's not already calibrated
					//DevMsg("gem calibration started...\n");
					// dkorus note:  we should handle failure conditions on the PS move for PSMove calibration
					cellGemCalibrate(0);
				}
			}
			else if( g_pInputSystem->IsInputDeviceConnected( INPUT_DEVICE_PLAYSTATION_MOVE ) )
			{
				g_pInputSystem->SetInputDeviceConnected( INPUT_DEVICE_PLAYSTATION_MOVE, false );

				// [dkorus] without the MOVE, we certainly can't have the sharpshooter connected
				g_pInputSystem->SetInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER, false ); 
			}

			if ( g_pInputSystem->GetMotionControllerDeviceStatus() == INPUT_DEVICE_MC_STATE_OK )
			{
				// Do not display these notices while calibrating
				if ( (s_gemThreadState.m_aCellGemState[ii].tracking_flags & CELL_GEM_TRACKING_FLAG_VISIBLE) !=
					( gemState.tracking_flags & CELL_GEM_TRACKING_FLAG_VISIBLE ) )
				{
					// if status changes, send over a message
					InputEvent_t event;
					memset( &event, 0, sizeof(event) );
					event.m_nTick = g_pInputSystem->GetPollTick();
					event.m_nType = IE_PS_Move_OutOfView;
					event.m_nData = gemState.tracking_flags & CELL_GEM_TRACKING_FLAG_VISIBLE;
					g_pInputSystem->PostUserEvent( event );

					// handle the red light if we're using the sharpshooter or move
					if ( g_pInputSystem->GetCurrentInputDevice() == INPUT_DEVICE_PLAYSTATION_MOVE ||
						g_pInputSystem->GetCurrentInputDevice() == INPUT_DEVICE_SHARPSHOOTER )
					{
						bool redLightOn = false;
						if ( !event.m_nData )
							redLightOn = true;

						// set m_nData to 1 (TRUE) if in view 
						cellCameraSetAttribute( 0, CELL_CAMERA_LED, ( int ) redLightOn, 0 );
					}
				}
			}


			if ( gemStatusFlags & CELL_GEM_FLAG_CALIBRATION_OCCURRED )
			{
				if ( gemStatusFlags & CELL_GEM_FLAG_CALIBRATION_FAILED_CANT_FIND_SPHERE ||
					 gemStatusFlags & CELL_GEM_FLAG_CALIBRATION_FAILED_MOTION_DETECTED ||
					 gemStatusFlags & CELL_GEM_FLAG_CALIBRATION_FAILED_BRIGHT_LIGHTING )
				{
					g_pInputSystem->SetMotionControllerDeviceStatus(  INPUT_DEVICE_MC_STATE_CONTROLLER_ERROR );
				}
			}

			s_gemThreadState.m_aStatus[ii] = gemStatus;
			s_gemThreadState.m_aStatusFlags[ii] = gemStatusFlags;
			s_gemThreadState.m_aCellGemState[ii] = gemState;

			if ( s_nCalibrationStep == 4 )
			{
				// We've calibrated the cursor, so now we can filter it.
				GetFilteredPointerPosition( ii, &s_gemThreadState.m_posX[ii], &s_gemThreadState.m_posY[ii] );
			}
			else
			{
				VmathVector3 position;
				VmathQuat orientation;
				vmathV3MakeFrom128(&position, s_gemThreadState.m_aCellGemState[ii].pos);
				vmathQMakeFrom128(&orientation, s_gemThreadState.m_aCellGemState[ii].quat);
				MoveKitPointerIntersectWithTrackerPlane( position, orientation, &s_gemThreadState.m_posX[ii], &s_gemThreadState.m_posY[ii] );
			}
		}

	}
	else
	{
		g_pInputSystem->SetMotionControllerDeviceStatus( INPUT_DEVICE_MC_STATE_CAMERA_NOT_CONNECTED );
	}

}

static void UpdateGemThread(uint64 args)
{
	sys_ipc_key_t ipckey = 0xabcdefab;
	sys_event_queue_t eventQueue;
	sys_event_queue_attribute_t evattr;
	sys_event_queue_attribute_initialize(evattr);
	sys_event_queue_create(&eventQueue, &evattr, ipckey, 10);
	cellCameraSetNotifyEventQueue2(ipckey, SYS_EVENT_PORT_NO_NAME, CELL_CAMERA_EFLAG_FRAME_UPDATE);

	while(!s_bGemThreadExit)
	{
		sys_event_t event;

		// timeout after 4 frames
		int receive_ret = sys_event_queue_receive(eventQueue, &event, 4*1000000/60);
		//Assert(receive_ret == CELL_OK);

// 		if(receive_ret == ETIMEDOUT) {
// 			Msg("ERROR: UpdateGemThread timeout!\n");
// 		} else if(receive_ret != CELL_OK) {
// 			Msg("ERROR: UpdateGemThread failed\n");
// 		}

		if (receive_ret == CELL_OK)
		{
			// read the camera
			CellCameraReadEx camReadEx;
			int32 camStatus = ReadCamera(&camReadEx);

			if ( s_gemThreadState.m_camStatus != camStatus )
			{
				// if status changes, send over a message
				InputEvent_t event;
				memset( &event, 0, sizeof(event) );
				event.m_nTick = g_pInputSystem->GetPollTick();
				event.m_nType = IE_PS_CameraUnplugged;
				event.m_nData = camStatus;
				g_pInputSystem->PostUserEvent( event );
			}

			// analyze the image / update gem (do this regardless of the pseye status)
			CellCameraInfoEx camera_info;
			camera_info.buffer=0; // set to NULL just in case the camera isn't ready
			cellCameraGetBufferInfoEx(0,&camera_info);
			cellGemUpdateStart(camera_info.buffer, camReadEx.timestamp);
			cellGemUpdateFinish();

			s_gemThreadState.m_camStatus = camStatus;
		}

	}

	cellCameraRemoveNotifyEventQueue2(ipckey);
	sys_event_queue_destroy(eventQueue, 0);

	sys_ppu_thread_exit(0);
}

static void EndUpdateGemThread()
{
	s_bGemThreadExit = true;
	uint64 threadRet;
	sys_ppu_thread_join(s_gemThread, &threadRet);
}

void CMoveController::Init()
{
	MEM_ALLOC_CREDIT();

	// Init PS Eye lib
	cellSysmoduleLoadModule( CELL_SYSMODULE_CAMERA );
	cellCameraInit();

	// Load Move controller lib and alloc memory for it 
	// (but can't complete init until SPURS instance is available)
	cellSysmoduleLoadModule( CELL_SYSMODULE_GEM );
	m_iSizeGemMem = cellGemGetMemorySize(MAX_PS3_MOVE_CONTROLLERS);
	m_pGemMem = malloc(m_iSizeGemMem);

	// Move controller lib requires a SPURS instance, so register with VJobs
	g_pVJobs->Register( this );
}

void CMoveController::Shutdown()
{
	g_pVJobs->Unregister( this ); 

	free(m_pGemMem);
	m_pGemMem = NULL;
	
	cellCameraEnd();
	cellSysmoduleUnloadModule(CELL_SYSMODULE_CAMERA);
}

// note:  rumbleVal should represent between 0 and 255 to match cellGemSetRumble
void CMoveController::Rumble( unsigned char rumbleVal )
{
	for ( int ii = 0; ii < MAX_PS3_MOVE_CONTROLLERS; ++ii )
	{
		int32_t result = cellGemSetRumble( ii, rumbleVal );
		if ( result == CELL_GEM_ERROR_INVALID_PARAMETER )
		{
			Warning( "CMoveController::Rumble invalid paramater for rumble \n" );
		}
	}
}

void CMoveController::OnVjobsInit()
{

	// Init move controller lib using VJobs SPURS instance
	CellGemAttribute gem_attr;

	cellGemAttributeInit(&gem_attr, 1, m_pGemMem, &m_pRoot->m_spurs, ( uint8_t* )&m_pRoot->m_nGemWorkloadPriority);
	int res = cellGemInit(&gem_attr);
	Assert(res == CELL_OK);
	if (res!= CELL_OK) Msg("Error on cellGemInit %d", res);

	// Start Gem update thread (must run at 60Hz for accurate tracking performance)
	memset(&s_gemThreadState,0,sizeof(s_gemThreadState));
	s_gemThreadState.m_camStatus=CELL_CAMERA_ERROR_NOT_OPEN;
	s_gemThread = 0;

	m_bEnabled = false;
	if(ps3_move_enabled.GetBool())
	{
		StartUpdateGemThread(&s_gemThreadState);
		m_bEnabled = true;
	}
}

void CMoveController::OnVjobsShutdown()
{
	EndUpdateGemThread();
	m_bEnabled = false;

	// End move controller lib
	cellGemEnd();
	cellSysmoduleUnloadModule(CELL_SYSMODULE_GEM);
}

void CMoveController::ReadState( MoveControllerState* pState )
{
	GemFrameUpdate();

	pState->m_CellGemInfo = s_gemThreadState.m_CellGemInfo;
	pState->m_camStatus = s_gemThreadState.m_camStatus;
	for(int ii=0; ii< MAX_PS3_MOVE_CONTROLLERS; ++ii)
	{
		pState->m_aCellGemState[ii] = s_gemThreadState.m_aCellGemState[ii];
		pState->m_aStatus[ii] = s_gemThreadState.m_aStatus[ii];
		pState->m_aStatusFlags[ii] = s_gemThreadState.m_aStatusFlags[ii];
		pState->m_pos[ii] = s_gemThreadState.m_pos[ii];
		pState->m_quat[ii] = s_gemThreadState.m_quat[ii];
		pState->m_posX[ii] = s_gemThreadState.m_posX[ii];
		pState->m_posY[ii] = s_gemThreadState.m_posY[ii];
	}

}

//--------------------------------------------------------------------------------------------------
// Disable (for debugging)
//--------------------------------------------------------------------------------------------------
void CMoveController::Disable()
{
	if(m_bEnabled)
	{
		EndUpdateGemThread();
		m_bEnabled = false;
	}
}

//--------------------------------------------------------------------------------------------------
// Enable (for debugging)
//--------------------------------------------------------------------------------------------------
void CMoveController::Enable()
{
	if(!m_bEnabled)
	{
		StartUpdateGemThread(&s_gemThreadState);
		m_bEnabled = true;
	}
}

void CMoveController::InvalidateCalibration( void )
{
	for ( int ii = 0; ii < MAX_PS3_MOVE_CONTROLLERS; ++ii )
	{
		cellGemClearStatusFlags( ii, CELL_GEM_ALL_FLAGS );
		cellGemInvalidateCalibration( ii );
		cellGemReset( ii );
		g_pInputSystem->SetMotionControllerDeviceStatus( INPUT_DEVICE_MC_STATE_CAMERA_NOT_CONNECTED );
	}

	ResetMotionControllerScreenCalibration();
}

void CMoveController::ResetMotionControllerScreenCalibration( void )
{
	s_nCalibrationStep = 0;
}

void CMoveController::StepMotionControllerCalibration( void )
{
	float pointerX = 0.0;
	float pointerY = 0.0; 

	VmathVector3 position;
	VmathQuat orientation;

	MoveControllerState currentState;

	ReadState( &currentState );

	switch ( s_nCalibrationStep )
	{
	case 0:	// left
		s_tracker_plane_extents[0].left = currentState.m_posX[0];
	break;

	case 1:	// right
		s_tracker_plane_extents[0].right = currentState.m_posX[0];
	break;

	case 2:	// bottom
		s_tracker_plane_extents[0].bottom = currentState.m_posY[0];
	break;

	case 3:	// top
		s_tracker_plane_extents[0].top = currentState.m_posY[0];
	break;

	default:
		AssertMsg( false, "Invalid step.\n" );
	break;
	}

	if ( s_nCalibrationStep <= 3 )
	{
		s_nCalibrationStep++;
	}
}
