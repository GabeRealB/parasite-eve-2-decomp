#ifndef ROOMS_MIST_PARKING_H
#define ROOMS_MIST_PARKING_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/ui.h"
#include "rooms/room_common.h"

/// Task descriptor tables the room spawns its tasks from.
extern TaskDesc D_mist_parking_801869B8;
extern TaskDesc D_mist_parking_8018D75C;
extern TaskDesc D_mist_parking_8018FC24;
extern TaskDesc D_mist_parking_80190824;

/// Tasks the room keeps a handle on while they run.
extern Task* D_mist_parking_80195318;
extern Task* D_mist_parking_80195320;
extern Task* D_mist_parking_80195324;
extern Task* D_mist_parking_8019532C;

/// The "%" suffix appended to the play-data percentages.
extern u8 D_mist_parking_80186718[];

/// The item id the shop list's cursor last rested on.
extern s32 D_mist_parking_8018644C;

/// Resets the caption state and, for 1 or 2, loads that caption file.
void func_mist_parking_80183708(s32 arg0);

/// Drop the handles of room tasks without killing them; the argument their
/// caller passes is unused.
void func_mist_parking_801837A4(s32 arg0);
void func_mist_parking_8018471C(s32 arg0);

#endif // ROOMS_MIST_PARKING_H
