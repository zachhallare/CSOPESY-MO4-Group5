// Volume and network controls, shared by the taskbar's VOL / NET flyouts and
// the Settings app so both always show the same state.
#pragma once

class Shell;

void VolumeControls(Shell& shell);
void NetworkControls(Shell& shell);
