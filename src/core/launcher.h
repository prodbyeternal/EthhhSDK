#pragma once
#include "precomp.h"

namespace eth::launcher {

// Returns 1 when the player pressed Start (continue into the game), 0 to exit.
int Run();

#if defined(ETH_TESTING)
// Test-only routing probes. Never create/show the launcher or run the game gate.
void TestReset();
int TestState();
void TestApplyRequests(bool start, bool exit);
void TestMessage(HWND hwnd, UINT msg, WPARAM wp);
#endif

} // namespace eth::launcher
