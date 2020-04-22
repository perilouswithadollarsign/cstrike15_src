#ifndef ENV_CASCADE_LIGHT_H
#define ENV_CASCADE_LIGHT_H

#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------
// Purpose : Sunlight shadow control entity
//------------------------------------------------------------------------------
class CCascadeLight : public CBaseEntity
{
public:
	DECLARE_CLASS( CCascadeLight, CBaseEntity );

	CCascadeLight();
	virtual ~CCascadeLight();

	void Spawn( void );
	void Release( void );
	void OnActivate();
	void OnDeactivate();

	bool KeyValue( const char *szKeyName, const char *szValue );
	virtual bool GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen );
	int  UpdateTransmitState();

	inline const Vector &GetShadowDirection() const { return m_shadowDirection; }
	inline const Vector &GetEnvLightShadowDirection() const { return m_envLightShadowDirection; }
	// Inputs
	void	InputSetAngles( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputSetLightColor( inputdata_t &inputdata );
	void	InputSetLightColorScale( inputdata_t &inputdata );

	virtual int	ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	static void SetEnvLightShadowPitch( float flPitch );
	static void SetEnvLightShadowAngles( const QAngle &angles );
	static void SetLightColor( int r, int g, int b, int a );
	void SetEnabled( bool bEnable );

private:
	CNetworkVector( m_shadowDirection );
	CNetworkVector( m_envLightShadowDirection );

	CNetworkVar( bool, m_bEnabled );
	bool m_bStartDisabled;
	CNetworkVar( bool, m_bUseLightEnvAngles );

	CNetworkColor32( m_LightColor );
	CNetworkVar( int, m_LightColorScale );
	CNetworkVar( float, m_flMaxShadowDist );

	void UpdateEnvLight();

	static float m_flEnvLightShadowPitch;
	static QAngle m_EnvLightShadowAngles;
	static bool m_bEnvLightShadowValid;
	static color32 m_EnvLightColor;
	static int m_EnvLightColorScale;
};

extern CCascadeLight *g_pCascadeLight;

#endif // #ifndef ENV_CASCADE_LIGHT_H
