#if !defined CS_PLAYER_SHARED_H
#define CS_PLAYER_SHARED_H

// 
// Configuration for using high priority entities by CS players
//
class CConfigurationForHighPriorityUseEntity_t
{
public:
	enum EPriority_t
	{	// Priority of use entities, higher number is higher priority for use
		k_EPriority_Default,
		k_EPriority_Hostage,
		k_EPriority_Bomb
	};
	enum EPlayerUseType_t
	{
		k_EPlayerUseType_Start,		// Player wants to initiate the use
		k_EPlayerUseType_Progress	// Player wants to make progress using the entity
	};
	enum EDistanceCheckType_t
	{
		k_EDistanceCheckType_3D,
		k_EDistanceCheckType_2D
	};

	CBaseEntity *m_pEntity;
	EPriority_t m_ePriority;
	EDistanceCheckType_t m_eDistanceCheckType;
	Vector m_pos;
	float m_flMaxUseDistance;
	float m_flLosCheckDistance;
	float m_flDotCheckAngle;
	float m_flDotCheckAngleMax;

public:
	// Check if this high priority use entity is better for use than the other one
	bool IsBetterForUseThan( CConfigurationForHighPriorityUseEntity_t const &other ) const;

	// Check if this entity can be used by the given player according to its use rules
	bool UseByPlayerNow( CCSPlayer *pPlayer, EPlayerUseType_t ePlayerUseType );
};


struct HalloweenMaskModelStruct
{
	char* model;
};

static const HalloweenMaskModelStruct s_HalloweenMaskModels[] =
{
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_hoxton.mdl" },
	{ "models/player/holiday/facemasks/porcelain_doll.mdl" },
	{ "models/player/holiday/facemasks/facemask_skull.mdl" },	
	{ "models/player/holiday/facemasks/facemask_samurai.mdl" },	
	{ "models/player/holiday/facemasks/evil_clown.mdl" },
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_wolf.mdl" },
	{ "models/player/holiday/facemasks/facemask_sheep_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_bunny_gold.mdl" },
	{ "models/player/holiday/facemasks/facemask_anaglyph.mdl" },
	{ "models/player/holiday/facemasks/facemask_porcelain_doll_kabuki.mdl" },
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_dallas.mdl" },
	{ "models/player/holiday/facemasks/facemask_pumpkin.mdl" },
	{ "models/player/holiday/facemasks/facemask_sheep_bloody.mdl" },
	{ "models/player/holiday/facemasks/facemask_devil_plastic.mdl" },	
	{ "models/player/holiday/facemasks/facemask_boar.mdl" },	
	{ "tf2" },
	{ "models/player/holiday/facemasks/facemask_chains.mdl" },	
	{ "models/player/holiday/facemasks/facemask_tiki.mdl" },
	{ "models/player/holiday/facemasks/facemask_bunny.mdl" },
	{ "models/player/holiday/facemasks/facemask_sheep_gold.mdl" },
	{ "models/player/holiday/facemasks/facemask_zombie_fortune_plastic.mdl" },
	{ "models/player/holiday/facemasks/facemask_chicken.mdl" },
	{ "models/player/holiday/facemasks/facemask_skull_gold.mdl" },	
};

static const HalloweenMaskModelStruct s_HalloweenMaskModelsCompetitive[] =
{
	{ "models/player/holiday/facemasks/facemask_samurai.mdl" },
	{ "models/player/holiday/facemasks/facemask_boar.mdl" },
	{ "models/player/holiday/facemasks/facemask_zombie_fortune_plastic.mdl" },
	{ "models/player/holiday/facemasks/facemask_porcelain_doll_kabuki.mdl" },
	{ "models/player/holiday/facemasks/facemask_pumpkin.mdl" },

	{ "models/player/holiday/facemasks/facemask_hoxton.mdl" },
	{ "models/player/holiday/facemasks/facemask_chains.mdl" },
	{ "models/player/holiday/facemasks/evil_clown.mdl" },
	{ "models/player/holiday/facemasks/facemask_dallas.mdl" },
	{ "models/player/holiday/facemasks/facemask_wolf.mdl" },
};

static const HalloweenMaskModelStruct s_HalloweenMaskModelsTF2[] =
{
	{ "models/player/holiday/facemasks/facemask_tf2_demo_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_engi_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_heavy_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_medic_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_pyro_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_scout_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_sniper_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_soldier_model.mdl" },
	{ "models/player/holiday/facemasks/facemask_tf2_spy_model.mdl" },
};

#endif // CS_PLAYER_SHARED_H
