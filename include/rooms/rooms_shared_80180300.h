#ifndef ROOMS_SHARED_80180300_H
#define ROOMS_SHARED_80180300_H

#include "common.h"

/// The garage's script-event hook: cues the garage's stage sound 6 for event 9
/// and asks the cap system for its event key on 0x6C, reporting "handled
/// nothing" for every other event. The day and night garage both carry this
/// body.
s32 RoomsShared80180300(s32 arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHARED_80180300_H
