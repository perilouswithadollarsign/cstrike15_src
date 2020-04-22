//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#ifndef PAINT_CLEANSER_MANAGER_H
#define PAINT_CLEANSER_MANAGER_H

#ifdef CLIENT_DLL
class C_TriggerPaintCleanser;
#else
class CTriggerPaintCleanser;
#endif

//The paint cleanser on client and server
#ifdef CLIENT_DLL
typedef CUtlVector<C_TriggerPaintCleanser*> PaintCleanserVector_t;
#else
typedef CUtlVector<CTriggerPaintCleanser*> PaintCleanserVector_t;
typedef CTriggerPaintCleanser C_TriggerPaintCleanser;
#endif

#ifdef CLIENT_DLL
class CPaintCleanserManager : public CAutoGameSystemPerFrame
#else
class CPaintCleanserManager : public CAutoGameSystem
#endif
{
public:
	CPaintCleanserManager( char const *name );
	~CPaintCleanserManager();

	//CAutoGameSystem members
	virtual char const *Name() { return "PaintCleanserManager"; }
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();

	void AddPaintCleanser( C_TriggerPaintCleanser *pCleanser );
	void RemovePaintCleanser( C_TriggerPaintCleanser *pCleanser );

	void GetPaintCleansers( PaintCleanserVector_t& paintCleansers );

#ifdef CLIENT_DLL
	//CAutoGameSystemPerFrame members
	virtual void Update( float frametime );
#endif

private:

#ifdef CLIENT_DLL
	void UpdatePaintCleanserVisibility( void );

	C_TriggerPaintCleanser *m_ppVisibleCleanser[ MAX_SPLITSCREEN_PLAYERS ];
	float m_flNextPollTime;
#endif

	PaintCleanserVector_t m_PaintCleansers;
};

extern CPaintCleanserManager PaintCleanserManager;

#endif //PAINT_CLEANSER_MANAGER_H
