#pragma once

/// Determines if we are in vanilla-compatibility mode.
class VanillaCompatibility
{
	bool m_bCheckedCompatThisMatch;
	bool m_bEnabled;
	int m_iMatchSeed;

public:
	bool GetVanillaCompatibility();
	void Enable() { m_iMatchSeed = rand() % 1000; }

	void Disable() { m_iMatchSeed = 0; m_bCheckedCompatThisMatch = false; m_bEnabled = false; }
};

extern VanillaCompatibility* g_pVanillaCompatibility;
