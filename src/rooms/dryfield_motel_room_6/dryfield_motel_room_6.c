#include "rooms/dryfield_motel_room_6.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
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
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

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

#include "mapui/map_dryfield.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

extern UiObjectDesc D_800611E4;
extern TaskDesc     D_actor_120500_8013843C;

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present), read through its own symbol.

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` as it was when the cutscene started, restored
/// when it ends.

/// Area-record patch list applied when the cutscene advances the story flags.

/// Labels of the four entries of the room's save menu: "Save", "Play Data",
/// "Weapon Data" and "PE Data".
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row labels of the play-data statistics panel, one per row
/// `func_dryfield_motel_room_6_8017D6C0` draws.
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
/// `roomCutsceneTask`, entry 1 the sound task
/// `roomCutsceneSoundTask` it runs alongside the scene.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// The room's message table, installed on the room entry task.
extern TaskMessageEntry D_dryfield_motel_room_6_80182D48[];

/// Task table whose entry 0 runs the room's event task
/// `func_dryfield_motel_room_6_80181A08`.
extern TaskDesc D_dryfield_motel_room_6_80182D78[];

/// World position the room's marker is drawn at.

/// The event task `func_dryfield_motel_room_6_80181A08` spawned and waits on.
extern Task* D_dryfield_motel_room_6_80186828;

/// The sound task the cutscene task spawned, killed when the player skips the
/// scene.
extern Task* gRoomCutsceneSoundTask;

/// Script record the room's event handler fills in and hands to the cutscene
/// task as its `spawnArg2`. It is a common, placed in first-declaration order,
/// so it is declared here, after its neighbours and before the library header.
extern RoomCutsceneRec gMotelRoom6CutsceneRec;

/// Selects the daytime motel room 6 glow export and action-handler signature.
///
/// Keep this binding through all motel room 6 implementation fragments.
#define DRYFIELD_TIME DRYFIELD_DAY
#include "../../shared/motel_room_6.h"

/// Script record the room's event handler fills in and hands to the cutscene
/// task as its `spawnArg2`.

#define TELEPHONE_TITLE_BYTES "Telephone\0\xFF\x1F"
#include "../../shared/telephone.h"

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
#include "../../shared/planar_reflection.h"

static void func_dryfield_motel_room_6_80181AC4(Task* task);
static void func_dryfield_motel_room_6_80181B10(Task* task);

extern WorldCollisionGrid         D_dryfield_motel_room_6_8018381C[1];
extern WorldCollisionTrigger      D_dryfield_motel_room_6_8018575C[10];
extern WorldCollisionTrigger      D_dryfield_motel_room_6_80185A54[15];
extern WorldCoordRoomAmbientEntry D_dryfield_motel_room_6_801866D8[13];
extern WorldCoordRoomLights       D_dryfield_motel_room_6_801866C0[1];
s32                               func_dryfield_motel_room_6_80181918(Task*, s32, s32, s32);
s32                               func_dryfield_motel_room_6_80181920(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                               func_dryfield_motel_room_6_801819A8(Task* task, s32 msgId, const void* firstArg, s32 arg3);
s32                               func_dryfield_motel_room_6_80181A00(Task*, s32, s32, s32);
void                              func_dryfield_motel_room_6_80181A08(Task*);

#include "../../shared/telephone_data.inc.c"

#include "../../shared/planar_reflection_data.inc.c"

TaskDesc D_dryfield_motel_room_6_80182D0C[2] = {
    { { { TASK_BODY_NONE, 112 } }, func_dryfield_motel_room_6_80181184, { .value = 0 } },
    { { { TASK_BODY_NONE, 112 } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return D_dryfield_motel_room_6_80182D0C;
}

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_motel_room_6_80182D48[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_motel_room_6_80181920 },
    { 5105, func_dryfield_motel_room_6_80181918 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_motel_room_6_801819A8 },
    { ROOM_MESSAGE_SOUND, func_dryfield_motel_room_6_80181A00 },
    { ROOM_MESSAGE_COMMAND, motelRoom6CutsceneMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_motel_room_6_80182D78[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_motel_room_6_80181A08, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

SVECTOR gMotelRoom6GlowPos[1] = {
    { 550, -850, 5170, 0 },
};

WorldCollisionRoomResources D_dryfield_motel_room_6_80182D98[1] = {
    { D_dryfield_motel_room_6_8018381C, D_dryfield_motel_room_6_8018575C, D_dryfield_motel_room_6_80185A54, NULL },
};

u8* D_dryfield_motel_room_6_80182DA8[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_motel_room_6_80182DAC[1] = { 12 };

WorldCoordRoomLighting D_dryfield_motel_room_6_80182DB0[1] = {
    { D_dryfield_motel_room_6_801866C0, D_dryfield_motel_room_6_801866D8 },
};

DirectionWarpEntry D_dryfield_motel_room_6_80182DB8[2] = {
    { { { .word = 3072 }, 4350, 0, 1500 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4350, 0, 1500 }, { 0, 0, 0, 0 }, 0x521E0002, 0x521E0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 465 },
    { { { .word = 1024 }, 715, 0, 6640 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 715, 0, 6640 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, 0x521E0003, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldMotelRoom6Collision0625CNormals[24] = {
#include "assets/dryfield_motel_room_6_collision_0625C_normals.inc"
};

static SVECTOR _gDryfieldMotelRoom6Collision0625CVerts[158] = {
#include "assets/dryfield_motel_room_6_collision_0625C_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelRoom6Collision0625CFaces[56] = {
#include "assets/dryfield_motel_room_6_collision_0625C_faces.inc"
};

static s16 _gDryfieldMotelRoom6Collision0625CCells[198] = {
#include "assets/dryfield_motel_room_6_collision_0625C_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom6Collision0625CCells[i])
static s16* _gDryfieldMotelRoom6Collision0625CTable[6] = {
#include "assets/dryfield_motel_room_6_collision_0625C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_room_6_8018381C[1] = {
    { NULL, _gDryfieldMotelRoom6Collision0625CNormals, _gDryfieldMotelRoom6Collision0625CVerts, _gDryfieldMotelRoom6Collision0625CFaces, _gDryfieldMotelRoom6Collision0625CTable, 0, 0, 2, 3, 4000, 56 },
};

ViewCamera D_dryfield_motel_room_6_80183840[12] = {
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

SpriteBatch D_dryfield_motel_room_6_801839F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_80183A00[56] = {
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

SpriteBatch D_dryfield_motel_room_6_80183E60[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 1, 0 } },
    { 13, 43, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_80183E80[47] = {
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

SpriteBatch D_dryfield_motel_room_6_8018422C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_80184244[43] = {
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

SpriteBatch D_dryfield_motel_room_6_801845A0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 10, 0, 0, { 0, 0 } },
    { 18, 4, 0, 0, { 2, 0 } },
    { 22, 21, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_801845D0[6] = {
    { 142, 0x3FC0, { .fields = { 144, 56 } }, -56, 32, 909, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -48, -64, 1500, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -24, 1500, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -64, 1500, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -48, 1175, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 136 } }, 24, -120, 688, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_6_80184648[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 4, 0, 0, { 2, 0 } },
    { 5, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_6_80184670[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_80184680[24] = {
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

SpriteBatch D_dryfield_motel_room_6_80184860[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 8, 0, 0, { 0, 0 } },
    { 22, 1, 0, 0, { 2, 0 } },
    { 23, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_80184890[147] = {
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
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, -64, 717, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -64, 702, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -56, 692, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -48, 732, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, -40, 727, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, -16, 777, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -16, 785, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
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

SpriteBatch D_dryfield_motel_room_6_8018540C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 3, 0 } },
    { 9, 53, 0, 0, { 0, 0 } },
    { 62, 37, 0, 0, { 5, 0 } },
    { 99, 3, 0, 0, { 1, 0 } },
    { 102, 24, 0, 0, { 4, 0 } },
    { 126, 21, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_6_8018544C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_6_8018545C[28] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, 32, 250, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, 72, 250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, 16, 250, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 0, 72, 250, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, 0, 250, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, 56, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -8, 250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, 56, 250, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 48, -8, 250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 48, 56, 250, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, -16, 250, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, 56, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -16, 250, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 56, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, -24, 250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, 56, 250, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 56, 250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 112, -24, 250, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 16, 250, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, 0, 250, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -32, -16, 250, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, 48, 250, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, 24, 250, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -24, -48, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -8, -72, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 8, -88, 250, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -88, 250, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -56, 250, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_6_8018568C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 1, 0 } },
    { 18, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_6_801856AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_6_801856BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_room_6_801856CC[12] = {
    { { .empty = D_dryfield_motel_room_6_801839F0 }, D_dryfield_motel_room_6_801839F0, NULL },
    { { .elements = D_dryfield_motel_room_6_80183A00 }, D_dryfield_motel_room_6_80183E60, NULL },
    { { .elements = D_dryfield_motel_room_6_80183E80 }, D_dryfield_motel_room_6_8018422C, NULL },
    { { .elements = D_dryfield_motel_room_6_80184244 }, D_dryfield_motel_room_6_801845A0, NULL },
    { { .elements = D_dryfield_motel_room_6_801845D0 }, D_dryfield_motel_room_6_80184648, NULL },
    { { .empty = D_dryfield_motel_room_6_80184670 }, D_dryfield_motel_room_6_80184670, NULL },
    { { .elements = D_dryfield_motel_room_6_80184680 }, D_dryfield_motel_room_6_80184860, NULL },
    { { .elements = D_dryfield_motel_room_6_80184890 }, D_dryfield_motel_room_6_8018540C, NULL },
    { { .empty = D_dryfield_motel_room_6_8018544C }, D_dryfield_motel_room_6_8018544C, NULL },
    { { .elements = D_dryfield_motel_room_6_8018545C }, D_dryfield_motel_room_6_8018568C, NULL },
    { { .empty = D_dryfield_motel_room_6_801856AC }, D_dryfield_motel_room_6_801856AC, NULL },
    { { .empty = D_dryfield_motel_room_6_801856BC }, D_dryfield_motel_room_6_801856BC, NULL },
};

WorldCollisionTrigger D_dryfield_motel_room_6_8018575C[10] = {
    { NULL, NULL, NULL, { 3590, -1168, 3040, 0 }, { { -1702, -2192, 0, 0 }, { 1690, -2192, 0, 0 }, { -1702, 2192, 0, 0 }, { 1716, 2192, 0, 0 } }, { 0, 0, -4098, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3743, -1104, 2911, 0 }, { { 1727, -2128, 51, 0 }, { -1726, -2128, -50, 0 }, { 1727, 2128, 51, 0 }, { -1726, 2128, -50, 0 } }, { -120, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1215, -1088, 4559, 0 }, { { 1311, -2112, 99, 0 }, { -1310, -2112, -98, 0 }, { 1311, 2112, 99, 0 }, { -1310, 2112, -98, 0 } }, { -309, 0, 4084, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1247, -1200, 4735, 0 }, { { -1304, -2224, -163, 0 }, { 1304, -2224, 163, 0 }, { -1304, 2224, -163, 0 }, { 1304, 2224, 163, 0 } }, { 508, 0, -4070, 0 }, { 0, 0, 4096, 0 }, 2572, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1471, -1248, 6271, 0 }, { { -1248, -2272, -415, 0 }, { 1247, -2272, 414, 0 }, { -1248, 2272, -415, 0 }, { 1247, 2272, 414, 0 } }, { 1294, 0, -3900, 0 }, { 0, 0, 4096, 0 }, 2623, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1663, -1296, 6207, 0 }, { { 1247, -2320, 414, 0 }, { -1247, -2320, -414, 0 }, { 1247, 2320, 414, 0 }, { -1247, 2320, -414, 0 } }, { -1292, 0, 3889, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2654, -1296, 5439, 0 }, { { -1, -2320, 639, 0 }, { 1, -2320, -639, 0 }, { -1, 2320, 639, 0 }, { 1, 2320, -639, 0 } }, { -4111, 0, -9, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2527, -1376, 5407, 0 }, { { 33, -2400, -638, 0 }, { -32, -2400, 639, 0 }, { 33, 2400, -638, 0 }, { -32, 2400, 639, 0 } }, { 4103, 0, 208, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3455, -1248, 6365, 0 }, { { -627, -2272, 124, 0 }, { 627, -2272, -123, 0 }, { -627, 2272, 124, 0 }, { 627, 2272, -123, 0 } }, { -797, 0, -4032, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3392, -1312, 6237, 0 }, { { 627, -2336, -123, 0 }, { -627, -2336, 124, 0 }, { 627, 2336, -123, 0 }, { -627, 2336, 124, 0 } }, { 791, 0, 4028, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_room_6_80185A54[15] = {
    { NULL, NULL, NULL, { 4512, -48, 1328, 0 }, { { -320, 0, -784, 0 }, { 320, 0, -784, 0 }, { -320, 0, 784, 0 }, { 320, 0, 784, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 846, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 896, -64, 1199, 0 }, { { -320, 0, -1088, 0 }, { 320, 0, -1088, 0 }, { -320, 0, 1089, 0 }, { 320, 0, 1089, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1304, -64, 7248, 0 }, { { -504, 0, -1087, 0 }, { -216, 0, -1087, 0 }, { -504, 0, 1088, 0 }, { 1224, 0, 1088, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 1634, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 3328, -64, 4384, 0 }, { { -704, 0, -400, 0 }, { 704, 0, -400, 0 }, { -704, 0, 400, 0 }, { 704, 0, 400, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 809, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1264, -64, 3024, 0 }, { { -1776, 0, -1280, 0 }, { 1776, 0, -1280, 0 }, { -1776, 0, 1280, 0 }, { 1776, 0, 1280, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -128, 5664, 0 }, { { -336, 0, -1344, 0 }, { 1520, 0, -1344, 0 }, { -336, 0, 512, 0 }, { 1520, 0, 512, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 2027, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 672, -48, 6656, 0 }, { { -320, 0, -432, 0 }, { 320, 0, -432, 0 }, { -320, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 535, WORLD_COLLISION_TRIGGER_ACTION_WARP, 20, 36, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2920, -64, 240, 0 }, { { -1480, 0, -160, 0 }, { 1432, 0, -160, 0 }, { -936, 0, 992, 0 }, { 984, 0, 992, 0 } }, { 0, 4104, 0, 0 }, { -4096, 0, 0, 0 }, 1487, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 576, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4256, -64, 608, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3936, -64, 4000, 0 }, { { -320, 0, -464, 0 }, { 320, 0, -464, 0 }, { -320, 0, 464, 0 }, { 320, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 561, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1631, -64, 7391, 0 }, { { 581, 0, -1196, 0 }, { 667, 0, 502, 0 }, { -699, 0, -500, 0 }, { -548, 0, 1197, 0 } }, { 0, 4109, 0, 0 }, { -2896, 0, -2896, 0 }, 1324, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4544, -64, 7104, 0 }, { { -400, 0, -512, 0 }, { 400, 0, -512, 0 }, { -400, 0, 512, 0 }, { 400, 0, 512, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 649, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4384, -64, 5568, 0 }, { { -1072, 0, -576, 0 }, { 400, 0, -576, 0 }, { -1072, 0, 1184, 0 }, { 400, 0, 1184, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1593, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2976, -64, 6528, 0 }, { { -176, 0, -1024, 0 }, { 816, 0, -1024, 0 }, { -176, 0, 960, 0 }, { 816, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Authored point lights for daytime Dryfield motel room 6, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensity uses `ONE` as 1.0.
/// The room light collection borrows the full array while this overlay is loaded.
/// Entries remain writable for view parenting, transform composition and attenuation queries.
static WorldCoordPointLight _gDryfieldMotelRoom6PointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -960, -2980, 6704 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1417, 1342, 1305 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 2700,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 492, -1614, 7495 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1228, 1155, 1060 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 0,
        .outer = 1,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 323, -2069, 2730 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3648, 3632, 3614 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1219,
        .outer = 2336,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3645, 2488, 6984 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1256, 1242, 1246 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1260,
        .outer = 8000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -382, -2019, 5379 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 4069, 4089, 4068 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1155,
        .outer = 2458,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -158, -1405, 3957 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 4091, 4040, 4045 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 835,
        .outer = 2420,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -696, -719, 7670 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3320, 3315, 3292 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 0,
        .outer = 0x2D19,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4237, -1600, 3654 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 4086, 3968, 4024 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1024,
        .outer = 1204,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4568, -1365, 6576 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 3578, 3580, 3541 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 690,
        .outer = 2169,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 164, -1302, 8133 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 2546, 2546, 2579 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 701,
        .outer = 1538,
    },
};

/// Authored cone lights for daytime Dryfield motel room 6, available in every view.
///
/// Positions and falloff radii use integer world units; RGB intensity and axes use
/// `ONE` as 1.0, and full cone openings use 0x1000 angle units per turn.
/// The room light collection borrows both entries while this overlay is loaded.
/// Entries remain writable for aiming, view parenting, transform
/// composition and attenuation queries.
static WorldCoordSpotLight _gDryfieldMotelRoom6ConeLights[2] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { -4092, -2, 258 }, { 90, 3831, 1457 }, { -242, 1459, -3827 } }, { 1225, -2361, 7903 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 0, 0, 0 },
            .unknown_56 = { 0, 0 },
        },
        .axis  = { 258, 1455, -3819, 0 },
        .inner = 2000,
        .outer = 6500,
        .angle = 398,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { -3451, -2, -2222 }, { -1605, -2838, 2492 }, { -1537, 2965, 2384 } }, { 6876, -2510, -586 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 1465, 1443, 1440 },
            .unknown_56 = { 0, 0 },
        },
        .axis  = { -2217, 2488, 2380, 0 },
        .inner = 2184,
        .outer = 0x1822C,
        .angle = 682,
    },
};

/// Unreferenced image bytes following the room's cone lights; original purpose unproven.
static u8 _gDryfieldMotelRoom6UnreferencedData[] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0A,
    0x00,
    0x00,
    0x00,
    0x38,
    0x51,
    0x19,
    0x80,
    0x02,
    0x00,
    0x00,
    0x00,
    0xF8,
    0x54,
    0x00,
    0x00,
    0x53,
    0xE5,
    0x00,
    0x0C,
    0x21,
    0x38,
    0x80,
    0x00,
    0x0C,
    0x00,
    0x40,
    0x10,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x28,
    0x00,
    0x00,
    0x19,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC8,
    0x64,
    0xC6,
    0x24,
    0x21,
    0x20,
    0x00,
    0x00,
    0x01,
    0x00,
    0x05,
    0x24,
    0x10,
    0x00,
    0x00,
    0x00,
    0x53,
    0xE5,
    0x00,
    0x0C,
    0x21,
    0x38,
    0x80,
    0x00,
    0x1A,
    0x00,
    0x40,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x28,
    0x80,
    0x00,
    0x00,
    0x02,
    0x06,
    0x24,
    0x53,
    0xE5,
    0x00,
    0x0C,
    0x21,
    0x38,
    0x80,
    0x00,
    0x14,
    0x00,
    0x00,
    0x00,
    0x19,
    0x80,
    0x04,
    0x3C,
    0x0E,
    0x61,
    0x00,
    0x0C,
    0x9C,
    0x40,
    0x84,
    0x24,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0xA6,
    0x27,
    0x11,
    0xA7,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x19,
    0x80,
    0x04,
    0x3C,
    0xAC,
    0x40,
    0x00,
    0x00,
    0x18,
    0x00,
    0xA5,
    0x97,
    0x1A,
    0x00,
    0x00,
    0x00,
    0x0E,
    0x61,
    0x00,
    0x0C,
    0x21,
    0x38,
    0x40,
    0x00,
    0x20,
    0x00,
    0xBF,
    0x8F,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0xE0,
    0x03,
    0x28,
    0x00,
    0xBD,
    0x27,
    0xD8,
    0xFF,
    0xBD,
    0x27,
    0x18,
    0x00,
    0xB0,
    0xAF,
    0x21,
    0x80,
    0x00,
    0x00,
    0x20,
    0x00,
    0xB2,
    0xAF,
    0x21,
    0x90,
    0xE0,
    0x00,
    0x01,
    0x00,
    0x02,
    0x24,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x00,
    0x48,
    0xAA,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x96,
    0x09,
    0x00,
    0x00,
    0x00,
    0x19,
    0x00,
    0x62,
    0x14,
    0x02,
    0x00,
    0x00,
    0x00,
    0x19,
    0x80,
    0x02,
    0x3C,
    0xE0,
    0x62,
    0x42,
    0x8C,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x09,
    0x00,
    0x02,
    0x24,
    0x05,
    0x00,
    0x02,
    0x92,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x00,
    0x40,
    0x14,
    0x09,
    0x00,
    0x00,
    0x00,
    0x03,
    0x00,
    0x04,
    0x24,
    0x32,
    0xFD,
    0x03,
    0x0C,
    0x01,
    0x00,
    0x05,
    0x24,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x02,
    0x24,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x00,
    0x42,
    0xA2,
    0x1A,
    0x00,
    0x00,
    0x00,
    0x19,
    0x80,
    0x02,
    0x3C,
    0xE0,
    0x62,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x07,
    0x00,
    0x40,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x00,
    0x40,
    0x14,
    0x03,
    0x00,
    0x04,
    0x24,
    0x32,
    0xFD,
    0x03,
    0x0C,
    0x01,
    0x00,
    0x00,
    0x00,
    0x47,
    0xFD,
    0x03,
    0x0C,
    0x21,
    0x20,
    0x00,
    0x00,
    0x02,
    0x00,
    0x42,
    0x28,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x20,
    0x00,
    0x00,
    0x03,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x11,
    0x00,
    0x02,
    0x24,
    0x18,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x47,
    0xFD,
    0x03,
    0x0C,
    0x21,
    0x20,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x29,
    0x00,
    0x40,
    0x14,
    0x01,
    0x00,
    0x02,
    0x24,
    0x47,
    0xFD,
    0x03,
    0x0C,
    0x21,
    0x20,
    0x00,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x06,
    0x00,
    0x43,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x47,
    0xFD,
    0x03,
    0x0C,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x58,
    0xFD,
    0x03,
    0x0C,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x03,
    0x00,
    0x02,
    0x24,
    0x13,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x02,
    0x24,
    0x05,
    0x00,
    0x02,
    0x92,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x02,
    0x24,
    0x47,
    0xFD,
    0x03,
    0x0C,
    0x21,
    0x20,
    0x00,
    0x00,
    0x02,
    0x00,
    0x42,
    0x28,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x47,
    0xFD,
    0x03,
    0x0C,
    0x21,
    0x00,
    0x04,
    0x24,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x20,
    0x00,
    0xB2,
    0x8F,
    0x1C,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0xE0,
    0x03,
    0x28,
    0x00,
    0x00,
    0x00,
    0xE0,
    0xFF,
    0xBD,
    0x27,
    0x18,
    0x00,
    0x00,
    0x00,
    0x21,
    0x90,
    0x80,
    0x00,
    0x1C,
    0x00,
    0xBF,
    0xAF,
    0x14,
    0x00,
    0xB1,
    0xAF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x30,
    0x00,
    0x43,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x06,
    0x00,
    0x62,
    0x2C,
    0xB4,
    0x00,
    0x40,
    0x10,
    0x19,
    0x80,
    0x00,
    0x00,
    0xDC,
    0x40,
    0x42,
    0x24,
    0x80,
    0x18,
    0x03,
    0x00,
    0x21,
    0x18,
    0x62,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x28,
    0x00,
    0x00,
    0x19,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x19,
    0x80,
    0x03,
    0x3C,
    0x01,
    0x00,
    0x00,
    0x00,
    0xE0,
    0x62,
    0x62,
    0xAC,
    0x1A,
    0x80,
    0x00,
    0x00,
    0xC7,
    0x0C,
    0x04,
    0x0C,
    0x90,
    0xAF,
    0x40,
    0xAC,
    0x9B,
    0x56,
    0x06,
    0x08,
};

WorldCoordRoomLights D_dryfield_motel_room_6_801866C0[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelRoom6PointLights), _gDryfieldMotelRoom6PointLights, ARRAY_SIZE(_gDryfieldMotelRoom6ConeLights), _gDryfieldMotelRoom6ConeLights },
};

WorldCoordRoomAmbientEntry D_dryfield_motel_room_6_801866D8[13] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_motel_room_6_801866D8) - 1 },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
    { .color = { 16, 0, 0, 6 } },
};

AreaResource D_dryfield_motel_room_6_80186740[2] = {
    { 101, 205, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_120500_8013843C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_room_6_80186758[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_motel_room_6_80186764[12] = {
    { NULL, NULL },
    { D_map_dryfield_8017BB54, D_dryfield_motel_room_6_80186740 },
    { D_map_dryfield_8017BB74, D_dryfield_motel_room_6_80186758 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_dryfield_motel_room_6_801867C4 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionFootstepSounds D_dryfield_motel_room_6_801867D0 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_dryfield_motel_room_6_801867DC = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_6_801867E8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_6_801867F0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_6_801867C4 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_6_801867F8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_6_801867D0 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_6_80186800[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_6_801867DC },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_room_6_80186808[8] = {
    D_dryfield_motel_room_6_801867E8,
    D_dryfield_motel_room_6_801867F0,
    D_dryfield_motel_room_6_801867F8,
    D_dryfield_motel_room_6_80186800,
    D_dryfield_motel_room_6_801867E8,
    D_dryfield_motel_room_6_801867E8,
    D_dryfield_motel_room_6_801867E8,
    D_dryfield_motel_room_6_801867E8,
};

Task* D_dryfield_motel_room_6_80186828;

Task* gRoomCutsceneSoundTask;

RoomCutsceneRec gMotelRoom6CutsceneRec;

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

#include "../../shared/telephone.inc.c"

void func_dryfield_motel_room_6_8017EA58(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/planar_reflection.inc.c"

void func_dryfield_motel_room_6_80181184(Task* task)
{
    _planarReflectionPlayerTask(task);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

#include "../../shared/room_cutscene_task.inc.c"

/// State handlers of the room entry task `func_dryfield_motel_room_6_80181B18`,
/// indexed by `Task::state`: the set-up tick, the idle tick, and `taskKill`.
static const TaskFuncTable3 D_dryfield_motel_room_6_8017D6B4 = {
    {
        func_dryfield_motel_room_6_80181AC4,
        func_dryfield_motel_room_6_80181B10,
        taskKill,
    },
};

#include "../../shared/motel_room_6_cutscene_msg.inc.c"

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Fallback of the room's message-0x13F0 handler for every event other than
/// the cutscene's; this room does nothing with them.
void motelRoom6ActionMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
}

s32 func_dryfield_motel_room_6_80181918(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Handler for a message `0x14` request: copies the incoming record to the
/// outgoing one, then answers 1 while the request is not the one this room
/// waits for or the 0x54 nibble is already latched. Otherwise, with no
/// sub-state pending, it latches nibble 0x54 and runs cap command 7, and
/// answers 0 either way.
/// Same gate as `func_neo_ark_shrine_8017D6AC` and
/// `func_shelter_b3_incinerator_control_room_8017FA8C`, which also latch a
/// nibble and run a cap command.
s32 func_dryfield_motel_room_6_80181920(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    if (in->areaId != GAME_AREA_DRYFIELD_WATER_TOWER) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_MOTEL_ROOM_6_WATER_TOWER_EXIT_SEEN) != 0) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 0;
    }
    gameFlagSetNibble(GAME_FLAG_MOTEL_ROOM_6_WATER_TOWER_EXIT_SEEN, 1);
    Gp_RunCapCmd1(7);
    return 0;
}

/// Handler for a slot-7 msg `0x13EF` request (`DirectionActionRequest`) whose sub-id
/// (`actionId`) is clear: the first time it runs it latches nibble 0x31 and
/// arms the room's script task from `D_dryfield_motel_room_6_80182D78`.
/// Where the sibling gates of this shape (`func_acropolis_security_room_8017D740`,
/// `func_acropolis_sanctuary_8017D848`) answer 0, this one answers 1.
s32 func_dryfield_motel_room_6_801819A8(Task* arg0, s32 arg1, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0 && gameFlagGetNibble(GAME_FLAG_DRYFIELD_MOTEL_ROOM_6_031) == 0) {
        gameFlagSetNibble(GAME_FLAG_DRYFIELD_MOTEL_ROOM_6_031, 1);
        taskSpawnFromTable(D_dryfield_motel_room_6_80182D78, 0, 0, 0);
    }
    return 1;
}

s32 func_dryfield_motel_room_6_80181A00(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Spawns this room's event task from entry 1 of `D_actor_120500_8013843C`, keeps it in
/// `D_dryfield_motel_room_6_80186828`, waits for it to be killed and then kills
/// this task. Same shape as `func_dryfield_gas_station_8017FE20`.
void func_dryfield_motel_room_6_80181A08(Task* arg0)
{
    s32 out;

    switch (arg0->state) {
        case 0:
            D_dryfield_motel_room_6_80186828 = taskSpawnFromTable(&D_actor_120500_8013843C, 1, 0, 0);
            arg0->state++;
            break;
        case 1:
            if (Task_PollKill(D_dryfield_motel_room_6_80186828, &out) != 0) {
                arg0->state++;
            }
            break;
        case 2:
            taskKill(arg0);
            break;
    }
}

/// First state of the room entry task: installs the room's message table,
/// publishes the task in pointer slot 7, advances the state and sets the
/// gameplay byte `D_80115598`.
static void func_dryfield_motel_room_6_80181AC4(Task* arg0)
{
    arg0->msgTable = D_dryfield_motel_room_6_80182D48;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// Idle state of the room entry task: does nothing, so the task stays alive
/// holding the room's message table.
static void func_dryfield_motel_room_6_80181B10(Task* task)
{
}

/// Room entry task: runs the state handler `D_dryfield_motel_room_6_8017D6B4`
/// names for `Task::state`, through a copy of the table taken onto the stack.
void func_dryfield_motel_room_6_80181B18(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_motel_room_6_8017D6B4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_wide_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"

#include "../../shared/motel_room_6_draw_glow.inc.c"
