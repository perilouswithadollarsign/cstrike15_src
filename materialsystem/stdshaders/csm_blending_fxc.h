//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================


HALF3 BlendDiffuseLightmapWithCSM( HALF3 diffuseLighting, HALF a, float3 worldPos,
								   out float flShadow, out float flSunAmount )
{
	HALF3 blendedDiffuseLighting = diffuseLighting;

	flShadow = 1.0f;
	flSunAmount = 0.0f;

#if ( ( CSM_BLENDING == 1 ) && ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
	{
#if !defined( _X360 ) && !defined( _PS3 )  && !defined( SHADER_MODEL_PS_2_B )
		if ( g_bCSMEnabled )
		{
#endif						
			flSunAmount = a;

			// Can't enable dynamic jumps around the Fetch4 shader, because it can't use tex2dlod()
#if ( CSM_MODE != CSM_MODE_ATI_FETCH4 ) && !defined( SHADER_MODEL_PS_2_B )
			[branch]
#endif		
			if ( flSunAmount > 0.0f )
			{
				flShadow = CSMComputeShadowing( worldPos );

				float3 direct = a * g_vCSMLightColor.rgb;
				float3 indirect = diffuseLighting.rgb - direct;

				// Apply csm shadows
				blendedDiffuseLighting.rgb = ( direct * flShadow ) + indirect;
			}
#if !defined( _X360 ) && !defined( _PS3 ) && !defined( SHADER_MODEL_PS_2_B )
		}
#endif							
	}
#endif

	return blendedDiffuseLighting;
}

HALF3 BlendBumpDiffuseLightmapWithCSM( HALF3 diffuseLighting, HALF a1, HALF a2, HALF a3, HALF3 dp, float3 worldPos,
									   out float flShadow, out float flSunAmount )
{
	HALF3 blendedDiffuseLighting = diffuseLighting;

	flShadow = 1.0f;
	flSunAmount = 0.0f;

	// modify/correct diffuse lighting using CSM
#if ( ( CSM_BLENDING == 1 ) && ( CASCADED_SHADOW_MAPPING ) && ( CASCADE_SIZE > 0 ) )
	{
#if !defined( _X360 ) && !defined( _PS3 )  && !defined( SHADER_MODEL_PS_2_B )
		if ( g_bCSMEnabled )
		{
#endif	
			flSunAmount = ( a1 + a2 + a3 );

			// Can't enable dynamic jumps around the Fetch4 shader, because it can't use tex2dlod()
#if ( CSM_MODE != CSM_MODE_ATI_FETCH4 ) && !defined( SHADER_MODEL_PS_2_B )
				[branch]
#endif	
			if ( flSunAmount > 0.0f )
			{
				flShadow = CSMComputeShadowing( worldPos );

				// reconstruct direct lighting from CSM light in the bump basis
				float3 directCSM = g_vCSMLightColor.rgb * ( dp.x * a1 + dp.y * a2 + dp.z * a3 );

				// blend CSM shadows with static lighting by removing the direct CSM light where there is CSM shadow
				blendedDiffuseLighting -= directCSM * ( 1.0f - flShadow );
			}

#if !defined( _X360 ) && !defined( _PS3 ) && !defined( SHADER_MODEL_PS_2_B )
		}
#endif	
	}
#endif

	return blendedDiffuseLighting;
}

