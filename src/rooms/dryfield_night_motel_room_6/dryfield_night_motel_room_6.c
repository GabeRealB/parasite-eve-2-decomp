#include "rooms/dryfield_night_motel_room_6.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

extern UiObjectDesc D_800611E4;

/// `Mc_SaveData[0].state.companionType` (ally present), read through its own symbol.

/// `Mc_SaveData[0].state.at4.loc.view` as it was when the cutscene started, restored
/// when it ends.

/// Area-record patch list applied when the cutscene advances the story flags.

/// Labels of the four entries of the room's save menu: "Save", "Play Data",
/// "Weapon Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_night_motel_room_6_8017D6DC` draws.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// The suffix appended to that panel's count rows.
static u8 Telephone_Data_80181A70[];

/// The "%" suffix appended to the percentages the play-data panels print.
static u8 Telephone_Data_80181A78[];

/// Help texts of the statistics panel's nine rows, handed to the UI holder for
/// the selected row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// The play-data menu panel's list.
static UiList Telephone_Data_80181C44;

/// The usage panel's list.
static UiList Telephone_Data_80181C6C;

/// UI descriptor the play-data panels spawn when they first open.
static UiObjectDesc Telephone_Data_80181C90;

/// UI descriptors the "Play Data" entry and the two usage entries open.
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;

/// The telephone menu panel's list.
static UiList Telephone_Data_80181CF4;

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
static u8 Reflection_Data_8017FC8C[];

/// Task table of the room's cutscene: entry 0 is the cutscene task
/// `func_dryfield_night_motel_room_6_801811F0`, entry 1 the sound task
/// `func_dryfield_night_motel_room_6_80181A0C` it runs alongside the scene.
extern TaskDesc D_dryfield_night_motel_room_6_80182E8C[];

/// The room's message table, installed on the room entry task.
extern GpMsgEntry D_dryfield_night_motel_room_6_80182EB0[];

/// Task table whose entries run the story task
/// `func_dryfield_night_motel_room_6_8018189C`.
extern TaskDesc D_dryfield_night_motel_room_6_80182EE0;

/// World position the room's marker is drawn at.
extern SVECTOR D_dryfield_night_motel_room_6_80182EF8[];

/// Area-record patch lists the story task applies as it ends.
extern GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[];
extern GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[];

/// The sound task the cutscene task spawned, killed when the player skips the
/// scene.
extern Task* D_dryfield_night_motel_room_6_801862B4;

/// Script record the room's event handler fills in and hands to the cutscene
/// task as its `spawnArg2`.
extern RoomCutsceneRec D_dryfield_night_motel_room_6_801862B8;

#define TELEPHONE_TITLE_BYTES "Telephone\0\xDF\xDC"
#include "../../shared/telephone.h"

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
#include "../../shared/planar_reflection.h"

static s32  func_dryfield_night_motel_room_6_80181A9C(Task* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_dryfield_night_motel_room_6_80181C34(Task* task);
static void func_dryfield_night_motel_room_6_80181C78(Task* task);

void func_dryfield_night_motel_room_6_801811F0(Task*);
void func_dryfield_night_motel_room_6_80181A0C(Task*);

void func_dryfield_night_motel_room_6_801811F0(Task*);
void func_dryfield_night_motel_room_6_80181A0C(Task*);

extern GpGridParams   D_dryfield_night_motel_room_6_80183984[1];
extern GpObj4C        D_dryfield_night_motel_room_6_80185A48[10];
extern GpObj4C        D_dryfield_night_motel_room_6_80185D40[15];
extern GpRoomCoordSet D_dryfield_night_motel_room_6_80185A30[1];
s32                   func_dryfield_night_motel_room_6_8018175C(Task*, s32, s32, s32);
s32                   func_dryfield_night_motel_room_6_80181B74(Task*, s32, TaskMessageArg, TaskMessageArg);
s32                   func_dryfield_night_motel_room_6_80181B7C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_dryfield_night_motel_room_6_80181BF8(Task*, s32, TaskMessageArg, TaskMessageArg);
s32                   func_dryfield_night_motel_room_6_80181C00(Task*, s32, s32, TaskMessageArg);
void                  func_dryfield_night_motel_room_6_8018189C(Task*);

#include "../../shared/telephone_data.inc.c"

#include "../../shared/planar_reflection_data.inc.c"

TaskDesc D_dryfield_night_motel_room_6_80182E74[2] = {
    { 0, 112, func_dryfield_night_motel_room_6_801811A0, { .model = NULL } },
    { 0, 112, Reflection_HeldObjectTask, { .model = NULL } },
};

static inline TaskDesc* Reflection_GetTasks(void)
{
    return D_dryfield_night_motel_room_6_80182E74;
}

TaskDesc D_dryfield_night_motel_room_6_80182E8C[3] = {
    { 0, 32, func_dryfield_night_motel_room_6_801811F0, { .model = NULL } },
    { 0, 32, func_dryfield_night_motel_room_6_80181A0C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_night_motel_room_6_80182EB0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_motel_room_6_80181B7C },
    { 5105, func_dryfield_night_motel_room_6_80181B74 },
    { 5103, func_dryfield_night_motel_room_6_80181BF8 },
    { 5104, func_dryfield_night_motel_room_6_8018175C },
    { 5106, func_dryfield_night_motel_room_6_80181C00 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_night_motel_room_6_80182EE0 = { 0, 32, func_dryfield_night_motel_room_6_8018189C, { .model = NULL } };

TaskDesc D_dryfield_night_motel_room_6_80182EEC = { 0, 192, func_dryfield_night_motel_room_6_8018189C, { .model = NULL } };

SVECTOR D_dryfield_night_motel_room_6_80182EF8[1] = {
    { 550, -850, 5170, 0 },
};

GpRoomCoordRec D_dryfield_night_motel_room_6_80182F00[1] = {
    { D_dryfield_night_motel_room_6_80185A30, NULL },
};

GpRoomObjRec D_dryfield_night_motel_room_6_80182F08[1] = {
    { D_dryfield_night_motel_room_6_80183984, D_dryfield_night_motel_room_6_80185A48, D_dryfield_night_motel_room_6_80185D40, NULL },
};

u8* D_dryfield_night_motel_room_6_80182F18[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_motel_room_6_80182F1C[1] = {
    { { .bytes = { 12, 0 } } },
};

GpWarpRec D_dryfield_night_motel_room_6_80182F20[2] = {
    { { .words = { 3072, 4350, 0, 1500 } }, { 0, 0, 0, 0 }, { .words = { 3072, 4350, 0, 1500 } }, { 0, 0, 0, 0 }, 0x531E0002, 0x531E0001, 0, 2, 0, 465 },
    { { .words = { 1024, 715, 0, 6640 } }, { 0, 0, 0, 0 }, { .words = { 1024, 715, 0, 6640 } }, { 0, 0, 0, 0 }, 0, 0x531E0003, 0, 5, 2, 0 },
};

SVECTOR D_dryfield_night_motel_room_6_80182F90[24] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_normals.inc"
};

SVECTOR D_dryfield_night_motel_room_6_80183050[158] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_verts.inc"
};

GpGridFace D_dryfield_night_motel_room_6_80183540[56] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_faces.inc"
};

s16 D_dryfield_night_motel_room_6_801837E0[198] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_motel_room_6_801837E0[i])
s16* D_dryfield_night_motel_room_6_8018396C[6] = {
#include "assets/dryfield_night_motel_room_6_collision_063C4_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_motel_room_6_80183984[1] = {
    { NULL, D_dryfield_night_motel_room_6_80182F90, D_dryfield_night_motel_room_6_80183050, D_dryfield_night_motel_room_6_80183540, D_dryfield_night_motel_room_6_8018396C, 0, 0, 2, 3, 4000, 56 },
};

GpViewRec D_dryfield_night_motel_room_6_801839A8[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x3656, -4000 } }, 289 },
    { { { { -3958, 0, -1051 }, { -55, 4090, 209 }, { 1049, 216, -3953 } }, { -1150, 1360, -5850 } }, 221 },
    { { { { 3963, 0, -1033 }, { -53, 4090, -206 }, { 1032, 213, 3957 } }, { -1200, 1360, -200 } }, 216 },
    { { { { 3992, 0, -914 }, { -253, 3935, -1106 }, { 879, 1134, 3836 } }, { -1000, 1960, -2700 } }, 246 },
    { { { { -567, 0, -4056 }, { -3575, 1934, 500 }, { 1916, 3610, -268 } }, { 750, 4410, -7350 } }, 235 },
    { { { { 358, 0, -4080 }, { -98, 4094, -8 }, { 4079, 98, 358 } }, { -200, 1060, -5350 } }, 263 },
    { { { { -4088, 0, -255 }, { -145, 3367, 2327 }, { 210, 2331, -3360 } }, { -3350, 2540, -7800 } }, 257 },
    { { { { 4092, 0, -163 }, { -99, 3249, -2491 }, { 129, 2493, 3246 } }, { -3350, 2390, -5000 } }, 246 },
    { { { { -3961, 0, -1041 }, { -524, 3537, 1997 }, { 899, 2065, -3421 } }, { -2523, 1553, -1908 } }, 282 },
    { { { { 1777, 0, 3690 }, { -2798, 2670, 1347 }, { -2405, -3105, 1158 } }, { -1947, 0x2BC9, 3461 } }, 257 },
    { { { { 1823, 0, 3667 }, { 2, 4096, -1 }, { -3667, 2, 1823 } }, { -7945, 4655, 5950 } }, 230 },
    { { { { 887, 0, 3998 }, { 2647, 3069, -587 }, { -2996, 2711, 665 } }, { -1401, 1658, -4928 } }, 680 },
};

SpriteBatch D_dryfield_night_motel_room_6_80183B58[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_6_80183B68[56] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 0, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 72, 0, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 56, 0, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, -48, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -48, 0, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, 32, 487, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -136, -40, 474, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 32, 443, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 56, 490, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 72, 471, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 460, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 0, 497, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, -48, 406, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -64, 0, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 16, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -48, 112, 0, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 32 } }, -16, 0, 0, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, 0, 0, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 16, 0, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 0, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 48, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 0, 926, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 929, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 24, 722, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 686, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 24, 627, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 16, 627, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 40, 614, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 0, 784, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 16, 680, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, 8, 485, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 24, 605, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 128, 48, 531, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 112, 500, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 32, 594, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 88, 72, 496, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, 32, 649, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 72, 523, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 16, 32, 708, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 16, 64, 548, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 112, 536, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 32, 728, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 40, 602, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 48, 555, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 56, 567, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 72, 634, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 96, 634, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, 64, 559, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 32, 718, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, 56, 569, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -24, 64, 578, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 72, 583, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 64, 601, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 80, 588, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 16, 819, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -32, 40, 613, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_6_80183FC8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 43, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_6_80183FE8[47] = {
    { 142, 0x3FC0, { .fields = { 184, 40 } }, -120, -40, 0, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 176, 8 } }, -112, 0, 0, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 24, 8, 0, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 16, 0, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -56, 112, 0, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 104, 0, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 24 } }, -112, 8, 0, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 0, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 8, 905, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 601, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 589, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 593, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 80, 596, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 32, 619, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 24, 704, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 24, 739, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 16, 839, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 24, 888, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 0, 700, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 371, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -160, 0, 0xAF28, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 48, 373, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 371, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -120, 88, 490, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -120, 48, 516, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 32, 642, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -56, 32, 700, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -24, 32, 700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 64, 579, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 56, 566, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 48, 578, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 572, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 40, 613, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -24, 48, 591, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -56, 48, 594, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -80, 40, 636, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 522, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -80, 64, 517, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -40, 64, 537, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -8, 64, 559, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 24, 88, 563, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 80, 564, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 567, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 88, 561, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, 88, 536, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, 88, 516, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 509, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_6_80184394[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_6_801843AC[43] = {
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -144, 16, 809, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -144, 32, 693, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 48, 621, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 56, 622, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 64, 618, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, 72, 618, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 80, 600, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -144, 80, 375, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 160 } }, -120, -88, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, -88, 825, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -64, 825, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -48, 825, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, -32, 834, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, -16, 837, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 900, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 16, 900, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 904, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -56, 48, 900, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 48, 680, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -88, 64, 680, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -88, 72, 679, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 80, 715, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 232 } }, 128, -120, 475, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 72, -120, 693, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 144 } }, 80, -120, 612, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 144 } }, 88, -120, 587, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 144 } }, 96, -120, 587, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 104, -120, 582, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 112, -120, 583, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 120, -120, 583, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 96, 24, 587, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 88, 425, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 88, 662, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 662, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 24, 590, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 24, 592, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 24, 593, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 64, 425, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 64, 662, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 24, 615, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, 64, 631, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, 24, 615, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 64, 631, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_6_80184708[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 10, 0, 0, { 0, 0 } },
    { 18, 4, 0, 0, { 2, 0 } },
    { 22, 21, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_6_80184738[29] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, -120, 0, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -80, 0, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -56, 0, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 24, -112, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -88, 0, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, -56, 0, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 0, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -8, 0, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 64, -8, 500, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 56, -32, 500, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 48, -40, 500, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 48, -56, 500, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, -80, 500, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 40, -88, 500, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 24, -120, 500, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 32, -112, 500, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -48, -64, 1500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -24, 1500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -40, 930, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -64, 930, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, 48, 0, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 32, 450, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, 48, 500, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, 48, 500, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -56, 48, 500, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 32, 0, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -32, 40, 450, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 16, 32, 450, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 32, 450, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_room_6_8018497C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 4, 0, 0, { 2, 0 } },
    { 20, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_room_6_801849A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_6_801849B4[24] = {
    { 142, 0x3FC0, { .fields = { 32, 184 } }, 112, -64, 300, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 184 } }, 80, -64, 287, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 88 } }, 24, -64, 0x30D4, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 24, 512, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 24, 32, 553, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 40, 560, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 24, 48, 542, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 56, 539, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 64, 537, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 72, 534, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 80, 532, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 88, 366, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 24, 96, 325, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 24, 104, 312, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 24, 1250, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 40, 710, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 48, 705, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 56, 709, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 706, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 72, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, -128, 24, 1250, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 32, 710, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 240 } }, 88, -120, 310, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 120, 240 } }, 40, -120, 200, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_dryfield_night_motel_room_6_80184B94[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 8, 0, 0, { 0, 0 } },
    { 22, 1, 0, 0, { 2, 0 } },
    { 23, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_room_6_80184BC4[147] = {
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -136, 0, 0, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 0, 489, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 40, 0, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 24, 0, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 56, 0, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -136, 8, 360, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -136, 24, 368, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -136, 40, 367, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, 56, 358, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -64, -112, 0, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -56, -64, 0, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -48, -16, 0, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, 40, 0, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -64, 104, 0, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 0, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -160, 24, 376, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -48, 303, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -120, 308, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, -16, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -48, 8, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -48, 40, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, -120, 24, 406, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -120, -80, 412, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -120, -120, 429, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, -120, 0, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -120, 625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, -80, 663, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 32, 1250, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -88, 56, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -88, 80, 1250, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, 96, 1250, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, 104, 556, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -80, -8, 606, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -8, 827, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 0, 807, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 8, 834, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 8, 483, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 16, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, -24, 811, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, -32, 768, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -32, 612, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -32, 447, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -24, 472, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -120, 8, 353, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 16, 432, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 0, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, -32, 0, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, -32, 0, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, -16, 609, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 0, 497, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 8, 0, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -104, -24, 0, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -40, 781, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -48, 818, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -56, 743, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -64, 733, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -64, 702, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -56, 692, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -48, 732, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -40, 711, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -16, 777, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -16, 817, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 0, 0, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, 80, 0, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -56, 96, 0, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, 0, 0, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 8, 569, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 16, 584, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 564, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 545, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 528, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 48, 512, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 56, 498, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 484, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, 72, 451, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -72, 16, 584, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -72, 24, 573, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -72, 72, 431, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -96, 96, 454, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 72, 428, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -112, 72, 406, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 64, 400, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 56, 405, { .fields = { 104, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -112, 32, 423, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -96, 24, 465, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -96, 16, 483, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -64, 8, 534, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -96, 0, 507, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 515, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 511, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -80, 56, 468, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -80, 64, 452, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 452, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -112, 48, 419, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 48, 510, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 40, 541, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -112, 40, 433, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 32, 511, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 32, 550, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 96, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 120, 104, 375, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 72, 96, 375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -32, -40, 0, { .fields = { 40, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 48 } }, -104, 8, 0, { .fields = { 24, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 32 } }, -104, -24, 0, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, -104, -64, 0, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 32 } }, -104, -120, 429, { .fields = { 88, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 24 } }, -104, -88, 410, { .fields = { 56, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, -32, -120, 0, { .fields = { 88, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, -40, -120, 0, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 16 } }, -80, -8, 549, { .fields = { 56, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 16, 1250, { .fields = { 0, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 24, 1250, { .fields = { 0, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 32, 1250, { .fields = { 8, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 40, 1250, { .fields = { 8, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -72, 48, 1250, { .fields = { 8, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 8 } }, -72, 8, 530, { .fields = { 32, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 8 } }, -56, 8, 574, { .fields = { 32, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 8 } }, -40, 8, 530, { .fields = { 32, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 56, 8 } }, -88, -64, 463, { .fields = { 120, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 56, 8 } }, -88, -56, 488, { .fields = { 120, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, -48, 482, { .fields = { 8, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, -40, 493, { .fields = { 8, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, -32, 472, { .fields = { 8, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -80, -24, 469, { .fields = { 16, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -80, -16, 529, { .fields = { 16, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 16 } }, -144, 8, 0, { .fields = { 104, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 8 } }, -96, 16, 0, { .fields = { 96, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -120, 16, 388, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -120, 8, 407, { .fields = { 16, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, -8, 0, { .fields = { 16, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -128, 0, 374, { .fields = { 8, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -128, -8, 390, { .fields = { 16, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 72 } }, -88, -72, 0, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 40 } }, -96, -72, 0, { .fields = { 24, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, -104, -72, 0, { .fields = { 72, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -136, -16, 376, { .fields = { 40, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, -144, -32, 0, { .fields = { 48, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 8 } }, -136, -24, 377, { .fields = { 80, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -136, -32, 377, { .fields = { 24, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 8 } }, -144, -40, 351, { .fields = { 24, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -136, -48, 365, { .fields = { 24, 8 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 8 } }, -128, -56, 380, { .fields = { 32, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -128, -64, 366, { .fields = { 40, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -128, -72, 373, { .fields = { 96, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 24 } }, -136, -72, 0, { .fields = { 112, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -144, -72, 0, { .fields = { 80, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_dryfield_night_motel_room_6_80185740[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 3, 0 } },
    { 9, 53, 0, 0, { 0, 0 } },
    { 62, 37, 0, 0, { 5, 0 } },
    { 99, 3, 0, 0, { 1, 0 } },
    { 102, 24, 0, 0, { 4, 0 } },
    { 126, 21, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_room_6_80185780[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_room_6_80185790[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_room_6_801857A0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_room_6_801857B0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_motel_room_6_801857C0[12] = {
    { { .empty = D_dryfield_night_motel_room_6_80183B58 }, D_dryfield_night_motel_room_6_80183B58, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80183B68 }, D_dryfield_night_motel_room_6_80183FC8, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80183FE8 }, D_dryfield_night_motel_room_6_80184394, NULL },
    { { .elements = D_dryfield_night_motel_room_6_801843AC }, D_dryfield_night_motel_room_6_80184708, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80184738 }, D_dryfield_night_motel_room_6_8018497C, NULL },
    { { .empty = D_dryfield_night_motel_room_6_801849A4 }, D_dryfield_night_motel_room_6_801849A4, NULL },
    { { .elements = D_dryfield_night_motel_room_6_801849B4 }, D_dryfield_night_motel_room_6_80184B94, NULL },
    { { .elements = D_dryfield_night_motel_room_6_80184BC4 }, D_dryfield_night_motel_room_6_80185740, NULL },
    { { .empty = D_dryfield_night_motel_room_6_80185780 }, D_dryfield_night_motel_room_6_80185780, NULL },
    { { .empty = D_dryfield_night_motel_room_6_80185790 }, D_dryfield_night_motel_room_6_80185790, NULL },
    { { .empty = D_dryfield_night_motel_room_6_801857A0 }, D_dryfield_night_motel_room_6_801857A0, NULL },
    { { .empty = D_dryfield_night_motel_room_6_801857B0 }, D_dryfield_night_motel_room_6_801857B0, NULL },
};

GpPointLight D_dryfield_night_motel_room_6_80185850[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -882, -3651, 0x2A8A } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 528, 586, 603 }, { 0, 0 } }, 0x32C8, 0x4650 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2452, -1396, 2755 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3173, 3151, 3128 }, { 0, 0 } }, 2120, 4223 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4822, -1215, 4142 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0, 0, 0 }, { 0, 0 } }, 0, 478 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 429, -1248, 4815 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4094, 3618, 3115 }, { 0, 0 } }, 1800, 2981 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3295, -1382, 6280 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3340, 3277, 3227 }, { 0, 0 } }, 652, 2681 },
};

GpRoomCoordSet D_dryfield_night_motel_room_6_80185A30[1] = {
    { 0, NULL, 5, D_dryfield_night_motel_room_6_80185850, 0, NULL },
};

GpObj4C D_dryfield_night_motel_room_6_80185A48[10] = {
    { NULL, NULL, NULL, { 3590, -1168, 3040, 0 }, { { -1702, -2192, 0, 0 }, { 1690, -2192, 0, 0 }, { -1702, 2192, 0, 0 }, { 1716, 2192, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 3743, -1104, 2911, 0 }, { { 1727, -2128, 51, 0 }, { -1726, -2128, -50, 0 }, { 1727, 2128, 51, 0 }, { -1726, 2128, -50, 0 } }, { -120, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 1215, -1088, 4559, 0 }, { { 1311, -2112, 99, 0 }, { -1310, -2112, -98, 0 }, { 1311, 2112, 99, 0 }, { -1310, 2112, -98, 0 } }, { -309, 0, 4084, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 1247, -1200, 4735, 0 }, { { -1304, -2224, -163, 0 }, { 1304, -2224, 163, 0 }, { -1304, 2224, -163, 0 }, { 1304, 2224, 163, 0 } }, { 508, 0, -4070, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 1439, -1248, 6239, 0 }, { { -1267, -2272, -353, 0 }, { 1266, -2272, 352, 0 }, { -1267, 2272, -353, 0 }, { 1266, 2272, 352, 0 } }, { 1101, 0, -3960, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 1343, -1296, 6079, 0 }, { { 1266, -2320, 352, 0 }, { -1266, -2320, -352, 0 }, { 1266, 2320, 352, 0 }, { -1266, 2320, -352, 0 } }, { -1099, 0, 3949, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 2654, -1296, 5439, 0 }, { { -1, -2320, 639, 0 }, { 1, -2320, -639, 0 }, { -1, 2320, 639, 0 }, { 1, 2320, -639, 0 } }, { -4111, 0, -9, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 4, 7, 1, 0 },
    { NULL, NULL, NULL, { 2527, -1376, 5407, 0 }, { { 33, -2400, -638, 0 }, { -32, -2400, 639, 0 }, { 33, 2400, -638, 0 }, { -32, 2400, 639, 0 } }, { 4103, 0, 208, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 3455, -1248, 6365, 0 }, { { -627, -2272, 124, 0 }, { 627, -2272, -123, 0 }, { -627, 2272, 124, 0 }, { 627, 2272, -123, 0 } }, { -797, 0, -4032, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 3392, -1312, 6237, 0 }, { { 627, -2336, -123, 0 }, { -627, -2336, 124, 0 }, { 627, 2336, -123, 0 }, { -627, 2336, 124, 0 } }, { 791, 0, 4028, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 8, 7, 129, 0 },
};

GpObj4C D_dryfield_night_motel_room_6_80185D40[15] = {
    { NULL, NULL, NULL, { 4512, -48, 1328, 0 }, { { -320, 0, -784, 0 }, { 320, 0, -784, 0 }, { -320, 0, 784, 0 }, { 320, 0, 784, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 846, 0, 29, 19, 2, 0 },
    { NULL, NULL, NULL, { 896, -64, 1199, 0 }, { { -320, 0, -1088, 0 }, { 320, 0, -1088, 0 }, { -320, 0, 1089, 0 }, { 320, 0, 1089, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1130, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { 1304, -64, 7248, 0 }, { { -504, 0, -1088, 0 }, { -216, 0, -1088, 0 }, { -504, 0, 1088, 0 }, { 1224, 0, 1088, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1634, 0x8005, 0, 0, 3, 0 },
    { NULL, NULL, NULL, { 3328, -64, 4384, 0 }, { { -704, 0, -400, 0 }, { 704, 0, -400, 0 }, { -704, 0, 400, 0 }, { 704, 0, 400, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 809, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 1264, -64, 3024, 0 }, { { -1776, 0, -1280, 0 }, { 1776, 0, -1280, 0 }, { -1776, 0, 1280, 0 }, { 1776, 0, 1280, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2187, 2, 6, 255, 4, 0 },
    { NULL, NULL, NULL, { 352, -128, 5568, 0 }, { { -240, 0, -1024, 0 }, { 1392, 0, -1024, 0 }, { -240, 0, 448, 0 }, { 1392, 0, 448, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1726, 2, 22, 255, 4, 0 },
    { NULL, NULL, NULL, { 640, -48, 6656, 0 }, { { -320, 0, -432, 0 }, { 320, 0, -432, 0 }, { -320, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 535, 0, 20, 36, 2, 0 },
    { NULL, NULL, NULL, { 2920, -64, 208, 0 }, { { -1480, 0, -160, 0 }, { 1432, 0, -160, 0 }, { -936, 0, 896, 0 }, { 984, 0, 896, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1487, 2, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 1728, -64, 576, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 4256, -64, 608, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 3936, -64, 4000, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 561, 2, 11, 255, 2, 0 },
    { NULL, NULL, NULL, { 1663, -64, 7415, 0 }, { { 669, 0, -1319, 0 }, { 650, 0, 404, 0 }, { -713, 0, -355, 0 }, { -604, 0, 1272, 0 } }, { 0, 4095, 0, 0 }, { -2896, 0, -2896, 0 }, 1476, 2, 12, 0, 2, 0 },
    { NULL, NULL, NULL, { 4544, -64, 7104, 0 }, { { -400, 0, -512, 0 }, { 400, 0, -512, 0 }, { -400, 0, 512, 0 }, { 400, 0, 512, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 649, 2, 13, 255, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, 5568, 0 }, { { -1072, 0, -576, 0 }, { 400, 0, -576, 0 }, { -1072, 0, 1184, 0 }, { 400, 0, 1184, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1593, 2, 14, 0, 4, 0 },
    { NULL, NULL, NULL, { 2976, -64, 6528, 0 }, { { -176, 0, -1024, 0 }, { 816, 0, -1024, 0 }, { -176, 0, 960, 0 }, { 816, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1305, 2, 15, 0, 132, 0 },
};

GpAreaVariant D_dryfield_night_motel_room_6_801861B4[11] = { 0 };

s32 D_dryfield_night_motel_room_6_8018620C[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

s32 D_dryfield_night_motel_room_6_80186218[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_dryfield_night_motel_room_6_80186224[3] = {
    0x10000001,
    0x10000003,
    0x10000001,
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186230[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186238[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_room_6_8018620C },
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186240[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_room_6_80186218 },
};

GpRoomParamRec D_dryfield_night_motel_room_6_80186248[1] = {
    { 0, 0, 1, 0, D_dryfield_night_motel_room_6_80186224 },
};

GpRoomParamRec* D_dryfield_night_motel_room_6_80186250[8] = {
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186238,
    D_dryfield_night_motel_room_6_80186240,
    D_dryfield_night_motel_room_6_80186248,
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186230,
    D_dryfield_night_motel_room_6_80186230,
};

GpAreaApplyRec D_dryfield_night_motel_room_6_80186270[16] = {
    { 3, 2, 2, 0 },
    { 3, 3, 3, 1 },
    { 3, 5, 3, 1 },
    { 3, 9, 3, 1 },
    { 3, 11, 2, 1 },
    { 3, 12, 2, 1 },
    { 3, 13, 2, 1 },
    { 3, 14, 2, 1 },
    { 3, 15, 4, 1 },
    { 3, 18, 3, 1 },
    { 3, 24, 3, 0 },
    { 3, 26, 2, 1 },
    { 3, 28, 2, 1 },
    { 3, 29, 2, 1 },
    { 3, 31, 4, 1 },
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_dryfield_night_motel_room_6_801862B0[1] = {
    { 255, 0, 0, 0 },
};

Task* D_dryfield_night_motel_room_6_801862B4 = NULL;

RoomCutsceneRec D_dryfield_night_motel_room_6_801862B8;

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

static void func_dryfield_night_motel_room_6_80181CD8(SVECTOR* arg0, s32 arg1, s32 arg2);

#include "../../shared/telephone.inc.c"

void func_dryfield_night_motel_room_6_8017EA74(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/planar_reflection.inc.c"

void func_dryfield_night_motel_room_6_801811A0(Task* task)
{
    Reflection_PlayerTask(task);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// The room's cutscene task, driven by the script record at `spawnArg2`. It
/// hides the HUD and holds the player's (and any ally's) weapon, forces the
/// script's area id and loads its cap file, then starts the scene with its
/// sound task and waits for it to end or for the player to skip it. On the way
/// out it advances the story flags the scene belongs to, runs the follow-up
/// cap commands, and restores the view, weapons and HUD before killing itself.
void func_dryfield_night_motel_room_6_801811F0(Task* task)
{
    s32              poll;
    s32              a0;
    s32              a1;
    s32              flag;
    RoomCutsceneRec* script;
    McSaveData*      save;

    script = task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            D_dryfield_night_motel_room_6_801862B4 = NULL;
            Gp_MsgPlayerWeapon(0);
            save = &Mc_SaveData[0];
            if (save->state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
            }
            if (script->field_0 > 0) {
                D_80115694               = save->state.at4.loc.view;
                save->state.at4.loc.view = (u8)script->field_0;
            } else {
                D_80115694 = -script->field_0;
            }
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            Gp_StateF0.field_4       = 2;
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            if (script->field_4 != 0) {
                SndEvt_EnqueueType6(script->field_4, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (script->field_3 != 0) {
                Gp_CapFile = 0;
                Gp_LoadCapFile(script->field_3);
                a0 = script->field_14;
                a1 = 0;
                if (a0 == 0) {
                    a0 = 0x3C0;
                } else {
                    a1 = script->field_16;
                }
                func_800E6D4C(a0, a1);
            }
            if (script->field_2 != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            D_dryfield_night_motel_room_6_801862B4 =
                Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E8C, 1, 0, script->field_10);
            Gp_StartCapSlot(script->field_1, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->field_10, 1);
                taskKill(D_dryfield_night_motel_room_6_801862B4);
                task->state++;
            } else if (Task_PollKill(D_dryfield_night_motel_room_6_801862B4, &poll) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (script->field_2 == 0) {
                SndEvt_EnqueueType6(script->field_C, 0, 0);
            }
            flag = GameFlag_GetNibble(0x7A);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (GameFlag_GetNibble(0x111) != 0) {
                            if (GameFlag_GetNibble(0x112) == 0) {
                                GameFlag_SetNibble(3, 0);
                                GameFlag_SetNibble(0x155, 9);
                                GameFlag_SetNibble(0x112, 1);
                            }
                        }
                    }
                }
            }
            if (script->field_1 == 1) {
                Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            } else {
                Gp_RunCapCmd(script->field_1, 0);
            }
            if (GameFlag_GetNibble(0x7A) == 1) {
                if (GameFlag_GetNibble(0) == 2) {
                    GameFlag_SetNibble(0, 3);
                    GameFlag_SetNibble(0xE, 4);
                    if ((GAME_LOCATION_WORD(Mc_SaveData[0].state.at4.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((GameFlag_GetNibble(0x155) == 0xE) && (GameFlag_GetNibble(3) == 0)) {
                    GameFlag_SetNibble(3, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            Mc_SaveData[0].state.at4.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            SndEvt_EnqueueType6(script->field_8, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (Mc_SaveData[0].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud    = 0;
            gGameSession->eventState = 0;
            Gp_StateF0.field_4       = 0;
            if (script->field_3 != 0) {
                Gp_ResetCap();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            break;
        case 20:
            Gp_RunCapCmd(GameFlag_GetNibble(0x155) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    GameFlag_SetNibble(3, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}

/// State handlers of the room entry task `func_dryfield_night_motel_room_6_80181C80`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_night_motel_room_6_8017D6B4 = {
    {
        func_dryfield_night_motel_room_6_80181C34,
        func_dryfield_night_motel_room_6_80181C78,
        taskKill,
    },
};

/// Handler of message 0x13F0 in the room's message table. For event 0x16 it
/// fills in the cutscene script record - the cap file and fade chosen from
/// flag nibble 0x7A and the stage, and the scene's sound events - and spawns
/// the cutscene task on it. Any other event goes to
/// `func_dryfield_night_motel_room_6_80181A9C`.
s32 func_dryfield_night_motel_room_6_8018175C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 count;

    count = 0;
    if (arg2 == 0x16) {
        D_dryfield_night_motel_room_6_801862B8.field_0 = 0xC;
        D_dryfield_night_motel_room_6_801862B8.field_1 = 1;
        switch (GameFlag_GetNibble(0x7A)) {
            case 0 ... 3:
                if (gGameSession->location.loc.stage == 2) {
                    count                                           = 4;
                    D_dryfield_night_motel_room_6_801862B8.field_14 = 0x3C0;
                    D_dryfield_night_motel_room_6_801862B8.field_3  = 1;
                } else {
                    count                                           = 2;
                    D_dryfield_night_motel_room_6_801862B8.field_14 = 0x380;
                    D_dryfield_night_motel_room_6_801862B8.field_3  = 1;
                }
                break;
            case 4 ... 6:
                count                                           = 2;
                D_dryfield_night_motel_room_6_801862B8.field_14 = 0x3C0;
                D_dryfield_night_motel_room_6_801862B8.field_3  = count;
                break;
        }
        D_dryfield_night_motel_room_6_801862B8.field_2  = 0;
        D_dryfield_night_motel_room_6_801862B8.field_4  = Gp_PackStageSndId(0x521E0008);
        D_dryfield_night_motel_room_6_801862B8.field_8  = Gp_PackStageSndId(0x521E000B);
        D_dryfield_night_motel_room_6_801862B8.field_10 = Gp_PackStageSndId(0x521E0009);
        D_dryfield_night_motel_room_6_801862B8.field_C  = Gp_PackStageSndId(0x521E000A);
        Task_SpawnFromTable(D_dryfield_night_motel_room_6_80182E8C, 0, count, &D_dryfield_night_motel_room_6_801862B8);
    } else {
        func_dryfield_night_motel_room_6_80181A9C(arg0, arg1, arg2, arg3);
    }
    return 0;
}

/// The room's story task: holds the player's weapon and runs cap command 0x10.
/// If the scene then reports event key 0xB the task ends there, giving the
/// weapon back. Otherwise it sets flag nibble 0x70 to 2 and, once the scene is
/// over, refills the player's HP and MP, stops the sound, applies the story's
/// area records (the second list only while nibble 0xCE is set), updates the
/// story flags, moves the saved location to area 8, warp 1, room 1 and spawns
/// task 0x11.
void func_dryfield_night_motel_room_6_8018189C(Task* arg0)
{
    Task* task;

    task = arg0;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto advance;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    Gp_MsgPlayerWeapon(0);
    Gp_RunCapCmd1(0x10);
    goto advance;

L_case2:
    if (Gp_GetCapEventKey() == 0xB) {
        taskKill(task);
        Gp_MsgPlayerWeapon(1);
    }
    goto advance;

L_case3:
    GameFlag_SetNibble(0x70, 2);
    goto advance;

L_case4:
    if (Gp_CapBusy() != 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case5:
    Gp_FillPlayerHpMp();
    SndEvt_EnqueueType7(0x80000000, 0);
    Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_80186270);
    if (GameFlag_GetNibble(0xCE) != 0) {
        Gp_ApplyAreaRecs(D_dryfield_night_motel_room_6_801862B0);
    }
    GameFlag_SetNibble(0x59, 1);
    GameFlag_SetNibble(0x5A, 2);
    GameFlag_SetNibble(0x30, 0);
    Mc_SaveData[0].state.at4.loc.area = 8;
    Mc_SaveData[0].state.at4.loc.warp = 1;
    Mc_SaveData[0].state.at4.loc.room = 1;
    gDisplayState.spriteVariant       = 1;
    Task_Spawn(0, 0x11, 0, 0);
    taskKill(task);
}

/// Sound task: plays the sound event `spawnArg2` on its first tick and again
/// at tick 0x50, then kills itself at tick 0x78.
void func_dryfield_night_motel_room_6_80181A0C(Task* task)
{
    switch (task->state) {
        case 0x50:
        case 0x0:
            SndEvt_EnqueueType6(task->spawnArg2.value, 0, 0);
            task->state += 1;
            break;
        case 0x78:
            Task_RequestKill(task, 0);
            break;
        default:
            task->state += 1;
            break;
    }
}

/// Runs the cap command for events 6, 0xD and 0xB, picking an alternative
/// command while flag nibble 0x61 is set. Event 6 instead spawns the story
/// task once nibble 0x6C is positive and nibble 0x70 is below 2.
static s32 func_dryfield_night_motel_room_6_80181A9C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x14);
        } else if (GameFlag_GetNibble(0x6C) > 0 && GameFlag_GetNibble(0x70) < 2) {
            Task_SpawnFromTable(&D_dryfield_night_motel_room_6_80182EE0, 0, 0x11, 0);
        } else {
            Gp_RunCapCmd1(arg2);
        }
    }
    if (arg2 == 0xD) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x13);
        } else {
            Gp_RunCapCmd1(0xD);
        }
    }
    if (arg2 == 0xB) {
        if (GameFlag_GetNibble(0x61) != 0) {
            Gp_RunCapCmd1(0x15);
        } else {
            Gp_RunCapCmd1(0xB);
        }
    }
    return 0;
}

/// Handler of message 0x13F1 in the room's message table: does nothing.
s32 func_dryfield_night_motel_room_6_80181B74(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler of message 0x13EE in the room's message table: copies the incoming record onto the outgoing one and,
/// for message 0x1D with `queryOnly` clear, answers 1 or 3 in `room`
/// depending on whether flag nibble 0x61 is set. Always returns 1.
s32 func_dryfield_night_motel_room_6_80181B7C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->areaId == 0x1D && in->queryOnly == ROOM_EVENT_EXECUTE) {
        nib = GameFlag_GetNibble(0x61);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->room = nib;
    }
    return 1;
}

/// Handler of message 0x13EF in the room's message table: does nothing.
s32 func_dryfield_night_motel_room_6_80181BF8(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler of message 0x13F2 in the room's message table: plays sound event
/// 0x531E000C for event 0x63.
s32 func_dryfield_night_motel_room_6_80181C00(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(0x531E000C, 0, 0);
    }
    return 0;
}

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances the state.
static void func_dryfield_night_motel_room_6_80181C34(Task* task)
{
    task->msgTable = D_dryfield_night_motel_room_6_80182EB0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room entry task: nothing left to do but idle.
static void func_dryfield_night_motel_room_6_80181C78(Task* task)
{
}

/// Room entry task: runs the state handler `D_dryfield_night_motel_room_6_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_night_motel_room_6_80181C80(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_room_6_8017D6B4;
    sp.funcs[task->state](task);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 48 / otz`. `arg1` scales `gDisplayState.animFrame`
/// into `rsin` so the lit vertex pulses as `rsin(...) / 34 + 0x78` on green and
/// blue.
static void func_dryfield_night_motel_room_6_80181CD8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    LINE_G3*           line;
    s32                sine;
    s32                pulse;
    s32                radius;
    s32                i;
    s32                t1;
    s32                t2;
    s32                twice;
    u16                sx;
    u16                sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - 0x10);
        block   = (RoomDraw13Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 48) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        i             = 0;
        pulse         = sine / 34 + 0x78;
        block->radius = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->radius;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->radius;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->radius) + (block->radius * twice);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, 0, pulse, pulse);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->radius * t1);
            line->y0 = block->sy - (block->radius * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->radius * t1);
            line->y2 = block->sy + (block->radius * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

#include "../../shared/glow_draw_pulsing_disc.inc.c"

/// Draws the room's highlight for the current camera view: the diamond marker
/// in views 3 and 4, the glow disc in view 12, nothing otherwise.
void func_dryfield_night_motel_room_6_80182AE0(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 3:
        case 4:
            func_dryfield_night_motel_room_6_80181CD8(&D_dryfield_night_motel_room_6_80182EF8[0], 0x60, 0x60);
            break;
        case 12:
            glowDrawPulsingDisc(&D_dryfield_night_motel_room_6_80182EF8[0], 0x60, 0x80);
            break;
    }
}
