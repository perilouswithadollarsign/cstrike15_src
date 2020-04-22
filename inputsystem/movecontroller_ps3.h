//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================
#ifndef INCLUDED_MOVECONTROLLER_PS3_H
#define INCLUDED_MOVECONTROLLER_PS3_H

#include <vjobs_interface.h>
#include <cell/gem.h> // PS3 move controller lib
#include <mathlib/vector.h>

// Max number of supported move controllers
// Increasing this value increases the memory required by the GEM lib
#define MAX_PS3_MOVE_CONTROLLERS 1

class QAngle;

struct MoveControllerState
{
	CellGemInfo  m_CellGemInfo;
	CellGemState m_aCellGemState[MAX_PS3_MOVE_CONTROLLERS];
	int32		 m_aStatus[MAX_PS3_MOVE_CONTROLLERS];
	uint64		 m_aStatusFlags[MAX_PS3_MOVE_CONTROLLERS];
	Vector	     m_pos[MAX_PS3_MOVE_CONTROLLERS];
	Quaternion   m_quat[MAX_PS3_MOVE_CONTROLLERS];
	float		 m_posX[MAX_PS3_MOVE_CONTROLLERS];
	float		 m_posY[MAX_PS3_MOVE_CONTROLLERS];
	int32		 m_camStatus;
};

//--------------------------------------------------------------------------------------------------
// CMoveController
// Manages PS3 Move Controller
// Use of VJobInstance ensures init gets called after Vjobs SPURS init
//--------------------------------------------------------------------------------------------------
class CMoveController: public VJobInstance
{
public:
	void	Init();
	void	Shutdown();

	void	ReadState(MoveControllerState* pState);

	void	OnVjobsInit(); // gets called after m_pRoot was created and assigned
	void	OnVjobsShutdown(); // gets called before m_pRoot is about to be destructed and NULL'ed

	// Disable/Enable for use debugging
	// Stops/starts update thread
	void	Disable();
	void	Enable();
	void	InvalidateCalibration( void );
	void	StepMotionControllerCalibration( void );
	void	ResetMotionControllerScreenCalibration( void );
	void	Rumble( unsigned char rumbleVal );

	bool	m_bEnabled;

	// Memory required by GEM lib
	void*	m_pGemMem;
	int32	m_iSizeGemMem;
};

extern CMoveController* g_pMoveController;

#endif // INCLUDED_MOVECONTROLLER_PS3_H