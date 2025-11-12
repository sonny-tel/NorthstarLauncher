#include "surface.h"
#include "core/tier1.h"

vgui::ISurface* g_pVGuiSurface = nullptr;

ON_DLL_LOAD_CLIENT("client.dll", VGuiSurface, (CModule module))
{
	g_pVGuiSurface = Sys_GetFactoryPtr("vguimatsurface.dll", "VGUI_Surface031").RCast<vgui::ISurface*>();
}
