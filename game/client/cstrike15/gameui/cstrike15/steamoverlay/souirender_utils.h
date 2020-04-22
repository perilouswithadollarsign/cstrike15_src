//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#ifndef SOUIRENDER_UTILS_H
#define SOUIRENDER_UTILS_H

enum SOUIrenderFont_t
{
	kSOUIrenderFont_DefaultFixedOutline,
	kSOUIrenderFont_Count
};

#define SOUIrenderDeclarePanel( classname ) static classname s_souirenderpanel_##classname; ISOUIrenderInputHandler *g_psouirenderpanel_##classname = &s_souirenderpanel_##classname;
#define SOUIrenderReferencePanel( classname ) s_souirenderpanel_##classname
#define SOUIrenderDeclareExternPanel( classname ) extern ISOUIrenderInputHandler *g_psouirenderpanel_##classname;
#define SOUIrenderReferenceExternPanel( classname ) g_psouirenderpanel_##classname

extern SteamOverlayFontHandle_t *g_ISteamOverlayRenderHost_FontHandles;
inline SteamOverlayFontHandle_t SOUIrenderGetFontHandle( SOUIrenderFont_t eFont )
{
	return g_ISteamOverlayRenderHost_FontHandles[eFont];
}


#endif // SOUIRENDER_UTILS_H
