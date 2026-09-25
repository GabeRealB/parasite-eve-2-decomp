#ifndef ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
#define ROOMS_SHELTER_B1_STERILIZATION_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

#include "rooms/room.h"
#include "main/task.h"
#include "main/ui.h"

/// 0x14-byte scratch block `func_shelter_b1_sterilization_room_80183B8C` takes
/// from `G_SCRATCH_HEAD`. `otz`, `flag` and `sx`/`sy` receive the projection of
/// the glow's centre; `rOuter` and `rInner` are its two on-screen radii,
/// derived from that `otz`.
typedef struct {
    s32 otz;
    s32 rOuter;
    s32 rInner;
    s32 flag;
    u16 sx;
    u16 sy;
} ShelterB1SterilizationRoomGlowScratch;

STATIC_ASSERT_SIZEOF(ShelterB1SterilizationRoomGlowScratch, 0x14);

/// Panel titles drawn by the room's menu tasks.
extern const char D_shelter_b1_sterilization_room_8017D610[];
extern const char D_shelter_b1_sterilization_room_8017D624[];
extern const char D_shelter_b1_sterilization_room_8017D630[];
extern const char D_shelter_b1_sterilization_room_8017D638[];

/// The "100.0%" string drawn for a full-share usage row.
extern const u8 D_shelter_b1_sterilization_room_8017D61C[];

/// State handlers of the tasks run by `func_shelter_b1_sterilization_room_80180518`
/// and `func_shelter_b1_sterilization_room_801811E0`.
extern const TaskFuncTable3 D_shelter_b1_sterilization_room_8017D6A4;
extern const TaskFuncTable4 D_shelter_b1_sterilization_room_8017D700;

/// The "%" suffix appended to a formatted percentage.
extern u8 D_shelter_b1_sterilization_room_80184594[];

/// UI descriptor of the help-line box the "Play Data" panels open beside their
/// lists.
extern UiObjectDesc D_shelter_b1_sterilization_room_801847AC;

/// Task descriptor table used by the cutscene runner
/// `func_shelter_b1_sterilization_room_8017F550`, which spawns entry 1 and
/// waits on it while the cutscene plays. The room's event handler spawns
/// entry 0 with a `RoomCutsceneRec` as its argument.
extern TaskDesc D_shelter_b1_sterilization_room_80184E1C;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_shelter_b1_sterilization_room_80188504[];

/// One bit per entry of `D_shelter_b1_sterilization_room_80188504` already
/// spawned, so each is spawned only once until the mask is cleared.
extern s32 D_shelter_b1_sterilization_room_8018C340;

/// Exit callback of the help-line box task: releases `Wip_UiHolder` if the
/// task owns it, then frees the task's UI object and kills it.
void func_shelter_b1_sterilization_room_8017F514(Task* task);

/// Draws the room's two backdrop halves as opaque sprites tinted by `shade`.
void func_shelter_b1_sterilization_room_80180A2C(s32 shade);

/// Draws the room's two backdrop halves as semi-transparent sprites tinted by
/// `shade`.
void func_shelter_b1_sterilization_room_80180BF0(s32 shade);

/// Appends a semi-transparent 15-bit `DR_TPAGE` for VRAM origin (`x`, `y`)
/// to OT slot 8.
void func_shelter_b1_sterilization_room_80181308(s32 x, s16 y);

/// Draws a two-point gouraud glow between the projections of `pos[0]` and
/// `pos[1]`; `size` is a signed half-extent scaled by depth and `rgb` a
/// 4-bit-per-channel colour.
void func_shelter_b1_sterilization_room_80182B34(SVECTOR* pos, s32 size, s32 rgb);

/// Draws a gouraud disc of four wedges around the projection of `pos`; `size`
/// is a signed half-extent scaled by depth and `rgb` a 4-bit-per-channel
/// colour.
void func_shelter_b1_sterilization_room_80183378(SVECTOR* pos, s32 size, s32 rgb);

/// Draws a pulsing diamond glow around the projection of `pos`; `speed`
/// scales the frame counter driving the pulse and `size` is a signed
/// half-extent scaled by depth.
void func_shelter_b1_sterilization_room_80183718(SVECTOR* pos, s32 speed, s32 size);

void func_shelter_b1_sterilization_room_80183B8C(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
