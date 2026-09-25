#ifndef ACTORS_SHARED_80132724_H
#define ACTORS_SHARED_80132724_H

#include "common.h"

/// Sends message 0x7DA to the slot-4 task, tagged with the current session's
/// two id bytes and the caller's selector.
void ActorsShared80132724(s16 arg0);

#endif
