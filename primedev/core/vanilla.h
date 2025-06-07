#pragma once

/// Determines if we are in vanilla-compatibility mode.
class VanillaCompatibility
{
	bool m_bEnabled;
	int m_iMatchSeed;

public:
	bool m_bCheckedCompatThisMatch;
	bool GetVanillaCompatibility();

	void Disable() { m_bCheckedCompatThisMatch = false; m_bEnabled = false; }
};

extern VanillaCompatibility* g_pVanillaCompatibility;
