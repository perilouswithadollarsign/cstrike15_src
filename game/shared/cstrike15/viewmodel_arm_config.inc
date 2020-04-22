//  Builds static array of structs for adding special cases for viewmodel addons depending on 
//	the character model needs (eg skintone, different sleeves when wearing econ gloves, etc)
//
//  ORDER IS IMPORTANT: Finds the first rule that has 'model substring' as part of the query model name and early outs.
//	More narrow matches should go before broader matches, so tm_professional_var4 before tm_professional.  

//	 model substring,			skin index,		default glove model,																sleeve,																	arms to use when wearing econ gloves 
CFG( tm_leet,					0,				models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl,				/*none*/,																/*none*/	)
CFG( tm_phoenix_varianta,		3,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				/*none*/,																/*none*/	)
CFG( tm_phoenix_variantb,		2,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				/*none*/,																/*none*/	)
CFG( tm_phoenix,				0,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				/*none*/,																/*none*/	)
CFG( tm_separatist,				0,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				models/weapons/v_models/arms/separatist/v_sleeve_separatist.mdl,		/*none*/	)
CFG( tm_balkan,					0,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				models/weapons/v_models/arms/balkan/v_sleeve_balkan.mdl,				/*none*/	)
CFG( tm_professional_var4,		1,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				models/weapons/v_models/arms/professional/v_sleeve_professional.mdl,	/*none*/	)
CFG( tm_professional,			0,				models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl,				models/weapons/v_models/arms/professional/v_sleeve_professional.mdl,	/*none*/	)
CFG( tm_anarchist,				0,				models/weapons/v_models/arms/anarchist/v_glove_anarchist.mdl,						/*none*/,																models/weapons/v_models/arms/anarchist/v_sleeve_anarchist.mdl )
CFG( tm_pirate,					1,				models/weapons/v_models/arms/bare/v_bare_hands.mdl,									models/weapons/v_models/arms/pirate/v_pirate_watch.mdl,					/*none*/	)
CFG( tm_heavy,					0,				models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl,				models/weapons/v_models/arms/balkan/v_sleeve_balkan.mdl,				/*none*/	)
CFG( ctm_st6,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle.mdl,				models/weapons/v_models/arms/st6/v_sleeve_st6.mdl,						/*none*/	)
CFG( ctm_idf,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle.mdl,				models/weapons/v_models/arms/idf/v_sleeve_idf.mdl,						/*none*/	)
CFG( ctm_gign,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_blue.mdl,		models/weapons/v_models/arms/gign/v_sleeve_gign.mdl,					/*none*/	)
CFG( ctm_swat_variantb,			1,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl,		models/weapons/v_models/arms/swat/v_sleeve_swat.mdl,					/*none*/	)
CFG( ctm_swat,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl,		models/weapons/v_models/arms/swat/v_sleeve_swat.mdl,					/*none*/	)
CFG( ctm_gsg9,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_blue.mdl,		models/weapons/v_models/arms/gsg9/v_sleeve_gsg9.mdl,					/*none*/	)
CFG( ctm_sas,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl,		models/weapons/v_models/arms/sas/v_sleeve_sas.mdl,						/*none*/	)
CFG( ctm_fbi,					0,				models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl,		models/weapons/v_models/arms/fbi/v_sleeve_fbi.mdl,						/*none*/	)