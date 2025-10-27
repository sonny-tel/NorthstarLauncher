#pragma once

enum LevelLoadingProgress_e
{
	PROGRESS_NONE,
	PROGRESS_CHANGELEVEL,
	PROGRESS_SPAWNSERVER,
	PROGRESS_LOADWORLDMODEL,
	PROGRESS_CRCMAP,
	PROGRESS_CRCCLIENTDLL,
	PROGRESS_CREATENETWORKSTRINGTABLES,
	PROGRESS_PRECACHEWORLD,
	PROGRESS_CLEARWORLD,
	PROGRESS_LEVELINIT,
	PROGRESS_PRECACHE,
	PROGRESS_ACTIVATESERVER,
	PROGRESS_BEGINCONNECT,
	PROGRESS_SIGNONCHALLENGE,
	PROGRESS_SIGNONCONNECT,
	PROGRESS_SIGNONCONNECTED,
	PROGRESS_PROCESSSERVERINFO,
	PROGRESS_PROCESSSTRINGTABLE,
	PROGRESS_SIGNONNEW,
	PROGRESS_SENDCLIENTINFO,
	PROGRESS_SENDSIGNONDATA,
	PROGRESS_SIGNONSPAWN,
	PROGRESS_CREATEENTITIES,
	PROGRESS_FULLYCONNECTED,
	PROGRESS_PRECACHELIGHTING,
	PROGRESS_READYTOPLAY,
	PROGRESS_HIGHESTITEM,
	PROGRESS_INVALID = -2,
	PROGRESS_DEFAULT = -1,
};

struct LoadingProgressDescription_t
{
	LevelLoadingProgress_e eProgress;
	int nPercent;
	int nRepeat;
	const char* pszDesc;
};

namespace vgui
{
	class ContinuousProgressBar
	{
	public:
		M_VMETHOD(__int64, SetProgress, 232, (float progress), (this, progress));
		M_VMETHOD(__int64, Paint, 132, (), (this));
	};

	class Panel;

	class Label;
} // namespace vgui

extern vgui::Panel* (*vgui_Panel_FindChildByName)(vgui::Panel* thisptr, const char* childName, bool recurseDown);
extern void (*vgui_Label_SetText)(vgui::Label* thisptr, const char* text);
