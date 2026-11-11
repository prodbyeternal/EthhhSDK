#pragma once
#include "glass.h"

namespace eth::ui {
enum class Sound {Click, Welcome, Start, Off, On};
void InitUiAudio();
void PlayUiSound(Sound sound);
void ShutdownUiAudio();
int UiAudioReadyCount();
}
