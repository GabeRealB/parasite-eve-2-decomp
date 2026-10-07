#include "rooms/mine_refuge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
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
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_shelter.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_mine_refuge_80182ADC[4];

extern UiObjectDesc D_800611E4;

extern TaskDesc D_actor_548100_801358D8;

/// The save's `companionType` byte under a symbol of its own; the cutscene's
/// end reads it through this name rather than through `gMcSaveData`.

/// View saved when the cutscene starts and restored when it ends.

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Captions of the telephone menu's rows ("Save", "Play Data", "Weapon Data",
/// "PE Data").
static u8 Telephone_Data_801819F8[];
static u8 Telephone_Data_80181A00[];
static u8 Telephone_Data_80181A0C[];
static u8 Telephone_Data_80181A18[];

/// Row captions of the play-data panel, one per row.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Suffix appended after a plain count on rows 1, 2, 3 and 6 of the play-data
/// panel.
static u8 Telephone_Data_80181A70[];

/// Suffix appended after a percentage.
static u8 Telephone_Data_80181A78[];

/// Help strings handed to the UI holder while the cursor rests on a row of
/// the play-data panel, one per row.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Lists of the play-data panel, the usage panel and the telephone menu, and
/// the descriptors of the panels they spawn.
static UiList       Telephone_Data_80181C44;
static UiList       Telephone_Data_80181C6C;
static UiObjectDesc Telephone_Data_80181C90;
static UiObjectDesc Telephone_Data_80181CAC;
static UiObjectDesc Telephone_Data_80181CC8;
static UiList       Telephone_Data_80181CF4;

/// Task table the cutscene and its sound task are spawned from.
extern TaskDesc gRoomCutsceneTaskDescs[];

/// Message table of the room's message task.
extern TaskMessageEntry D_mine_refuge_80181884[];

/// Task table of the room's two scripted sequences,
/// `func_mine_refuge_8017FA08` and `func_mine_refuge_8017FDBC`.
extern TaskDesc D_mine_refuge_801818B4[];

/// World-space anchors of the room's per-view glows: `D8` and `E0` are the two
/// drawn in view 2, `E0` again in view 6, and `E8` the one of views 3-5. `E0`
/// is reached both as `D8[1]` (view 2) and by its own name (view 6), and the
/// two forms are different code - indexing emits `D8+8`, naming emits its own
/// `lui` - so it keeps its own declaration.
extern SVECTOR D_mine_refuge_801818E8;

/// The cutscene's sound task, killed when the scene is skipped.
extern Task* gRoomCutsceneSoundTask;

/// Task `func_mine_refuge_8017FA08` spawns from `D_actor_548100_801358D8` and waits on;
/// message 0x13F1 is relayed to it while it exists.
extern Task* D_mine_refuge_80182AD8;

/// View saved when `func_mine_refuge_8017FC2C` forces view 6 for its scene,
/// restored when the scene ends.

/// Parameters of the cutscene `func_mine_refuge_8017FE78` starts.
extern RoomCutsceneRec D_mine_refuge_80182AE0;

#define TELEPHONE_TITLE_BYTES "Telephone\0\x1A\x1C"
#include "../../shared/telephone.h"

static void func_mine_refuge_8017FE78(s32 arg0);
static void func_mine_refuge_8017FF4C(Task* task);
static void _mineRefugeIdleRoomTask(Task* task);

extern WorldCollisionGrid         D_mine_refuge_80181BA4[1];
extern WorldCollisionTrigger      D_mine_refuge_80182778[2];
extern WorldCollisionTrigger      D_mine_refuge_80182810[6];
extern WorldCoordRoomAmbientEntry D_mine_refuge_80182A58[8];
extern WorldCoordRoomLights       D_mine_refuge_80182760[1];
s32                               func_mine_refuge_8017FBB4(Task*, s32, s32, s32);
s32                               func_mine_refuge_8017FBE8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                               func_mine_refuge_8017FC2C(Task*, s32, s32, s32);
s32                               func_mine_refuge_8017FCD0(Task* task, s32 msgId, const void* firstArg, s32 arg3);
s32                               func_mine_refuge_8017FD48(Task*, s32, s32, s32);
void                              func_mine_refuge_8017FA08(Task*);
void                              func_mine_refuge_8017FDBC(Task*);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_mine_refuge_80181884[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mine_refuge_8017FBE8 },
    { 5105, func_mine_refuge_8017FBB4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_mine_refuge_8017FCD0 },
    { ROOM_MESSAGE_COMMAND, func_mine_refuge_8017FC2C },
    { ROOM_MESSAGE_SOUND, func_mine_refuge_8017FD48 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_mine_refuge_801818B4[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_mine_refuge_8017FA08, { .value = 0 } },
    { { { TASK_BODY_NONE, 31 } }, func_mine_refuge_8017FDBC, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

SVECTOR D_mine_refuge_801818D8[2] = {
    { 1910, -1850, 990, 0 },
    { 769, -798, 2640, 0 },
};

SVECTOR D_mine_refuge_801818E8 = { 2731, -1261, 4325, 0 };

WorldCoordRoomLighting D_mine_refuge_801818F0[1] = {
    { D_mine_refuge_80182760, D_mine_refuge_80182A58 },
};

WorldCollisionRoomResources D_mine_refuge_801818F8[1] = {
    { D_mine_refuge_80181BA4, D_mine_refuge_80182778, D_mine_refuge_80182810, NULL },
};

u8* D_mine_refuge_80181908[1] = {
    gViewIdentityMap,
};

ViewCount D_mine_refuge_8018190C[1] = { 7 };

DirectionWarpEntry D_mine_refuge_80181910[1] = {
    { { { .word = 0 }, 1472, 0, 288 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1472, 0, 288 }, { 0, 0, 0, 0 }, 0x54060002, 0x54060001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMineRefugeCollision045E4Normals[12] = {
#include "assets/mine_refuge_collision_045E4_normals.inc"
};

static SVECTOR _gMineRefugeCollision045E4Verts[34] = {
#include "assets/mine_refuge_collision_045E4_verts.inc"
};

static WorldCollisionGridFace _gMineRefugeCollision045E4Faces[15] = {
#include "assets/mine_refuge_collision_045E4_faces.inc"
};

static s16 _gMineRefugeCollision045E4Cells[24] = {
#include "assets/mine_refuge_collision_045E4_cells.inc"
};

#define GRID_CELL(i) (&_gMineRefugeCollision045E4Cells[i])
static s16* _gMineRefugeCollision045E4Table[2] = {
#include "assets/mine_refuge_collision_045E4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_refuge_80181BA4[1] = {
    { NULL, _gMineRefugeCollision045E4Normals, _gMineRefugeCollision045E4Verts, _gMineRefugeCollision045E4Faces, _gMineRefugeCollision045E4Table, 200, 200, 1, 2, 4000, 15 },
};

ViewCamera D_mine_refuge_80181BC8[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -1500, 8300, -2500 } }, 289 },
    { { { { -3873, 0, -1331 }, { -400, 3906, 1165 }, { 1269, 1232, -3693 } }, { -550, 1610, -4900 } }, 269 },
    { { { { 3915, 0, -1201 }, { -777, 3121, -2535 }, { 915, 2651, 2984 } }, { -1005, 2661, -1831 } }, 246 },
    { { { { 0, 0, -4096 }, { -891, 3997, 0 }, { 3997, 891, 0 } }, { -725, 1640, -3952 } }, 680 },
    { { { { 0, 0, -4096 }, { -891, 3997, 0 }, { 3997, 891, 0 } }, { -725, 1640, -3952 } }, 680 },
    { { { { -1156, 0, 3929 }, { 2912, 2750, 856 }, { -2638, 3035, -776 } }, { -1291, 1474, -2830 } }, 629 },
    { { { { 2270, 0, 3408 }, { 2132, 3195, -1420 }, { -2659, 2562, 1771 } }, { -2075, 1908, -3789 } }, 598 },
};

SpriteBatch D_mine_refuge_80181CC4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_refuge_80181CD4[79] = {
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 40, 32, 487, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, 48, 524, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, 48, 503, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, 88, 549, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 88, 527, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 24, 541, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 24, 541, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 40, 16, 582, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 16, 582, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 8, 620, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 8, 630, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 48, 0, 675, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, 0, 686, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -16, 834, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 834, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -8, 753, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -8, 753, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -24, 891, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -24, 891, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -16, 703, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 719, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -16, 675, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 675, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 0, 626, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 0, 653, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 24, 751, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 48, 688, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 696, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -32, 754, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 32, 0, 771, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 761, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 16, 80, 484, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 88, 467, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 88, 442, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, 104, 426, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 96, 425, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 96, 449, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 112, 420, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 112, 420, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 8, 593, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -144, 56, 618, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 554, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 56, 564, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -40, 760, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -32, 809, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 971, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -24, 965, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -8, 812, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 1005, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 8, 988, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 875, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 32, 803, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -104, 669, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, -104, 661, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -88, 693, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -64, 716, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -152, -32, 894, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -56, 696, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -24, 711, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -104, 705, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -96, 783, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -96, 880, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, -64, 911, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -88, 916, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -64, 945, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 761, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -40, 997, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -40, 844, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, -40, 786, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -72, -32, 941, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 8, 984, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, -16, 900, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -8, 998, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 786, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -8, 812, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 916, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 24, 838, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 815, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 861, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_refuge_80182300[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 9, 0, 0, { 0, 0 } },
    { 28, 3, 0, 0, { 5, 0 } },
    { 31, 8, 0, 0, { 1, 0 } },
    { 39, 4, 0, 0, { 4, 0 } },
    { 43, 36, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_refuge_80182340[33] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 104, 482, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 80, 513, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, 88, 495, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, 96, 489, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 104, 447, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 753, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -16, 896, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 849, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 0, 850, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, 16, 773, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -80, 16, 777, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 32, 724, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 48, 675, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 64, 641, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -72, 80, 600, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 88, 600, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 88, 660, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 32, 725, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 48, 684, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 64, 642, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 80, 608, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -8, 883, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -88, -32, 833, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 817, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -104, -24, 798, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -16, 766, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -16, 795, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -16, 865, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -8, 839, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 772, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -8, 812, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, 0, 810, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 0, 807, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_refuge_801825D4[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 1, 0, 0, { 3, 0 } },
    { 6, 15, 0, 0, { 2, 0 } },
    { 21, 12, 0, 0, { 4, 0 } },
    { 33, 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_refuge_8018260C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_refuge_8018261C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_refuge_8018262C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_refuge_8018263C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mine_refuge_8018264C[7] = {
    { { .empty = D_mine_refuge_80181CC4 }, D_mine_refuge_80181CC4, NULL },
    { { .elements = D_mine_refuge_80181CD4 }, D_mine_refuge_80182300, NULL },
    { { .elements = D_mine_refuge_80182340 }, D_mine_refuge_801825D4, NULL },
    { { .empty = D_mine_refuge_8018260C }, D_mine_refuge_8018260C, NULL },
    { { .empty = D_mine_refuge_8018261C }, D_mine_refuge_8018261C, NULL },
    { { .empty = D_mine_refuge_8018262C }, D_mine_refuge_8018262C, NULL },
    { { .empty = D_mine_refuge_8018263C }, D_mine_refuge_8018263C, NULL },
};

WorldCoordPointLight D_mine_refuge_801826A0[2] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2128, -1869, 1212 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3342, 2588 }, { 0, 0 } }, 0, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1562, -2170, 3664 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 0, 3000 },
};

WorldCoordRoomLights D_mine_refuge_80182760[1] = {
    { 0, NULL, ARRAY_SIZE(D_mine_refuge_801826A0), D_mine_refuge_801826A0, 0, NULL },
};

WorldCollisionTrigger D_mine_refuge_80182778[2] = {
    { NULL, NULL, NULL, { 2048, -1600, 2790, 0 }, { { -1888, -1904, 176, 0 }, { 1888, -1904, -176, 0 }, { -1888, 1904, 176, 0 }, { 1888, 1904, -176, 0 } }, { -382, 0, -4086, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2017, -1568, 2734, 0 }, { { 1792, -1904, -176, 0 }, { -1792, -1904, 176, 0 }, { 1792, 1904, -176, 0 }, { -1792, 1904, 176, 0 } }, { 399, 0, 4075, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mine_refuge_80182810[6] = {
    { NULL, NULL, NULL, { 1504, -48, 208, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2368, -64, 4257, 0 }, { { -448, 0, -880, 0 }, { 448, 0, -880, 0 }, { -448, 0, 880, 0 }, { 448, 0, 880, 0 } }, { 0, 4105, 0, 0 }, { -4091, 0, -201, 0 }, 987, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, 4544, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, -201, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, 2416, 0 }, { { -576, 0, -576, 0 }, { 576, 0, -576, 0 }, { -576, 0, 576, 0 }, { 576, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 4095, 0, 0, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2592, -64, 1808, 0 }, { { -576, 0, -704, 0 }, { 576, 0, -704, 0 }, { -576, 0, 704, 0 }, { 576, 0, 704, 0 } }, { 0, 4097, 0, 0 }, { -4095, 0, 0, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2464, -64, 2944, 0 }, { { -576, 0, -400, 0 }, { 576, 0, -400, 0 }, { -576, 0, 400, 0 }, { 576, 0, 400, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 701, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_mine_refuge_801829D8[2] = {
    { 101, 481, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, &D_actor_548100_801358D8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_mine_refuge_801829F0[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_mine_refuge_80182A00[11] = {
    { NULL, NULL },
    { D_mine_refuge_801829F0, D_mine_refuge_801829D8 },
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

WorldCoordRoomAmbientEntry D_mine_refuge_80182A58[8] = {
    { .viewCount = ARRAY_SIZE(D_mine_refuge_80182A58) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 520, 520, 520, 520 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_mine_refuge_80182A98 = {
    0x10000041,
    0x10000043,
    0x10000041,
};

WorldCollisionSurfaceProperties D_mine_refuge_80182AA4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_refuge_80182AAC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_refuge_80182A98 },
};

WorldCollisionSurfaceProperties* D_mine_refuge_80182AB4[8] = {
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AAC,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
    D_mine_refuge_80182AA4,
};

Task* gRoomCutsceneSoundTask = NULL;

Task* D_mine_refuge_80182AD8 = NULL;

u8 D_mine_refuge_80182ADC[4] = {
    0,
    47,
    36,
    50,
};

RoomCutsceneRec D_mine_refuge_80182AE0;

static void _mineRefugeDrawPulsingCyanStar(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _mineRefugeDrawPulsingCyanBurst(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _mineRefugeDrawLayeredGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);

#include "../../shared/telephone.inc.c"

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

void func_mine_refuge_8017EA78(Task* task)
{
    _telephoneMenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// States of the room's message task, run by `func_mine_refuge_8017FFBC`:
/// install the message table, idle, die.
static const TaskFuncTable3 D_mine_refuge_8017D6A4 = {
    {
        func_mine_refuge_8017FF4C,
        _mineRefugeIdleRoomTask,
        taskKill,
    },
};

void func_mine_refuge_8017FA08(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            if (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE) == 1) {
                gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_SCENE_STATE, 2);
                capRunCommandWithTransition(0xF);
            }
            task->state = task->state + 1;
            return;
        case 2:
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_PANEL_OPEN, 0, 0);
            D_mine_refuge_80182AD8 = taskSpawnFromTable(&D_actor_548100_801358D8, 0, 0, 0);
            task->state            = task->state + 1;
            return;
        case 3:
            if (taskPollKill(D_mine_refuge_80182AD8, &sp10) != 0) {
                D_mine_refuge_80182AD8 = NULL;
                task->state            = task->state + 1;
            }
            return;
        case 1:
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            sndEvtRequestScriptStart(SOUND_MINE_REFUGE_CIRCUIT_PANEL_CLOSE, 0, 0);
            taskKill(task);
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// The `0x13F1` message handler of `D_mine_refuge_80181884`: relays the
/// message unchanged to `D_mine_refuge_80182AD8` and returns its answer, or 0
/// while that task does not exist.
s32 func_mine_refuge_8017FBB4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 ret;

    if (D_mine_refuge_80182AD8 == NULL) {
        ret = 0;
    } else {
        ret = taskMessageDispatch(D_mine_refuge_80182AD8, msgId, arg2, arg3);
    }
    return ret;
}

/// A handler of the room's message table: copies the incoming record onto the
/// outgoing one, hands both to `mapShelterRoomVariantResolve` and returns 1.
s32 func_mine_refuge_8017FBE8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapShelterRoomVariantResolve(in, out);
    return 1;
}

s32 func_mine_refuge_8017FC2C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8 temp_a3;

    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_PROMPT_ACCEPTED) != 0) {
            func_mine_refuge_8017FE78(0U);
        } else {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            temp_a3                                                    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6U;
            D_mine_refuge_80182ADC[0]                                  = temp_a3;
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 3), 0, 0);
            capRunCommand(0xD, CAP_PLAYBACK_IN_PLACE);
            taskSpawnFromTable(D_mine_refuge_801818B4, 1, 0, 0);
        }
    }
    return 0;
}

s32 func_mine_refuge_8017FCD0(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    u8 actionId = request->actionId;

    if (actionId == 1) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_SECRET_PASSAGE_STATE) != actionId) {
            gameFlagSetNibble(GAME_FLAG_0C4, 0);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_mine_refuge_801818B4, 0, 0, 0);
        } else {
            capRunCommandWithTransition(0xA);
        }
    }
    return 0;
}

/// The `0x13F2` message handler of `D_mine_refuge_80181884`: arguments 0xC, 0x63
/// and 0x67 each cue a sound (ids 0x5406000C, 0x5406000F and 0x5406000D),
/// centred and at zero depth. Any other argument is ignored.
s32 func_mine_refuge_8017FD48(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0xC:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0C), 0, 0);
            break;
        case 0x63:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0F), 0, 0);
            break;
        case 0x67:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0D), 0, 0);
            break;
    }
    return 0;
}

void func_mine_refuge_8017FDBC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == 5) {
                arg0->state = arg0->state + 1;
                return;
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_mine_refuge_80182ADC[0];
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            break;
        case 1:
            gameFlagSetNibble(GAME_FLAG_MINE_REFUGE_PROMPT_ACCEPTED, 1);
            func_mine_refuge_8017FE78(D_mine_refuge_80182ADC[0]);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// Fills `D_mine_refuge_80182AE0` and spawns the cutscene task with it. A
/// non-zero `arg0` is the view restored when the scene ends, with no opening
/// sound; zero forces view 6 for the scene and opens with sound 0x54060003.
/// Progress nibble 0x155 picks the scene: when it is 0xF, CAP slot 0xE with no
/// file and CAP command 1 afterwards; otherwise CAP slot 1 from file 1 and
/// command 5 afterwards.
static void func_mine_refuge_8017FE78(s32 arg0)
{
    s32 slot;

    if (arg0 != 0) {
        D_mine_refuge_80182AE0.startSound = 0;
        D_mine_refuge_80182AE0.view       = -arg0;
    } else {
        D_mine_refuge_80182AE0.view       = 6;
        D_mine_refuge_80182AE0.startSound = 0x54060003;
    }
    if (gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) == 0xF) {
        slot                           = 1;
        D_mine_refuge_80182AE0.capSlot = 0xE;
        D_mine_refuge_80182AE0.capFile = 0;
    } else {
        slot                           = 5;
        D_mine_refuge_80182AE0.capSlot = 1;
        D_mine_refuge_80182AE0.capFile = 1;
    }
    D_mine_refuge_80182AE0.skipScene       = 0;
    D_mine_refuge_80182AE0.endSound        = 0x54060006;
    D_mine_refuge_80182AE0.sceneSound      = 0x54060004;
    D_mine_refuge_80182AE0.afterSceneSound = 0x54060005;
    taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, slot, &D_mine_refuge_80182AE0);
}

static void func_mine_refuge_8017FF4C(Task* arg0)
{
    arg0->msgTable = D_mine_refuge_80181884;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    D_mine_refuge_80182AD8 = NULL;
    gStageSceneMusicEntry  = 1;
    arg0->state            = arg0->state + 1;
    D_80115598             = 1;
}

/// Keeps the room message task idle while its installed handlers remain available.
static void _mineRefugeIdleRoomTask(Task* task)
{
    // Retain the idle callback's 16-byte stack frame.
    char stackFrame[0x10];
}

/// Runs the handler for the task's current state, from a local copy of
/// `D_mine_refuge_8017D6A4`.
void func_mine_refuge_8017FFBC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_refuge_8017D6A4;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_flare.inc.c"

/// Initializes the packet header and centre-to-rim colors of a Gouraud glow quad.
///
/// Vertex 2 receives the low byte of each intensity without clamping; vertices
/// 0, 1 and 3 are black. The untextured polygon command starts opaque, clearing
/// any previous semitransparency setting.
///
/// Borrows one word-aligned, writable `POLY_G4` without allocating it. Coordinates
/// and the DMA link remain untouched. The caller places vertex 2 at the centre
/// and the other vertices on the rim, links the packet and selects blending
/// before drawing; queued storage must remain live until GPU drawing completes.
static inline void _mineRefugeInitGlowQuad(POLY_G4* quad, s32 redIntensity, s32 greenIntensity, s32 blueIntensity)
{
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, redIntensity, greenIntensity, blueIntensity);
    setRGB3(quad, 0, 0, 0);
}

/// Draws a pulsing cyan diamond and two crossing diagonals at a world point.
///
/// The signed low halfword of `pulseRate` is in 4096 angle units per animation
/// frame. Intensity is `rsin(animFrame * pulseRate) / 34 + 120` (0..240).
/// The signed low halfword of `radiusScale` gives a pixel half-extent of
/// `radiusScale * 32 / depth`, where depth is camera Z / 4. The second diagonal
/// extends twice as far as the diamond. Rejects negative GTE flags and requires
/// nonzero depth. Borrows the point during this call, reserves and releases one
/// `EffectCentreScratch`, and queues two additive quads and two three-point
/// lines plus their blend commands in the current frame.
static void _mineRefugeDrawPulsingCyanStar(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    EffectCentreScratch* scratchEnd;
    EffectCentreScratch* scratchBlock;
    POLY_G4*             quad;
    LINE_G3*             diagonal;
    s32                  pulseSine;
    s32                  cyanIntensity;
    s32                  screenRadius;
    s32                  partIndex;
    s32                  xRadiusMultiple;
    s32                  yRadiusMultiple;
    s32                  verticalSide;
    u16                  screenX;
    u16                  screenY;

    scratchEnd   = *SCRATCH_STACK_CURSOR_SLOT;
    scratchBlock = (*SCRATCH_STACK_CURSOR_SLOT = scratchEnd - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratchBlock->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        pulseSine                  = rsin(gDisplayState.animFrame * (s16)pulseRate);
        screenRadius               = ((s16)radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / scratchBlock->depth;
        partIndex                  = 0;
        cyanIntensity              = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        scratchBlock->screenExtent = screenRadius;
        // Fill the diamond, then add diagonals; the second diagonal has twice the extent.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            quad->x0     = scratchBlock->screenX - scratchBlock->screenExtent;
            screenX      = scratchBlock->screenX;
            quad->x2     = screenX;
            quad->x1     = screenX;
            quad->x3     = scratchBlock->screenX + scratchBlock->screenExtent;
            screenY      = scratchBlock->screenY;
            quad->y3     = screenY;
            quad->y2     = screenY;
            quad->y0     = screenY;
            verticalSide = partIndex * 2;
            quad->y1     = (scratchBlock->screenY - scratchBlock->screenExtent) + (scratchBlock->screenExtent * verticalSide);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);
            partIndex++;
        } while (partIndex < 2);

        partIndex = 0;
        do {
            diagonal       = gGpuPrimCursor;
            gGpuPrimCursor = diagonal + 1;
            setLineG3(diagonal);
            setRGB0(diagonal, 0, 0, 0);
            setRGB1(diagonal, 0, cyanIntensity, cyanIntensity);
            setRGB2(diagonal, 0, 0, 0);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            diagonal->x0    = scratchBlock->screenX + (scratchBlock->screenExtent * xRadiusMultiple);
            diagonal->y0    = scratchBlock->screenY - (scratchBlock->screenExtent * yRadiusMultiple);
            diagonal->x1    = scratchBlock->screenX;
            diagonal->y1    = scratchBlock->screenY;
            diagonal->x2    = scratchBlock->screenX - (scratchBlock->screenExtent * xRadiusMultiple);
            diagonal->y2    = scratchBlock->screenY + (scratchBlock->screenExtent * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    diagonal);
            gpuSetPrimitiveBlendMode(diagonal, GPU_BLEND_ADD, scratchBlock->depth);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Draws a pulsing cyan disc with four extended blades at a world point.
///
/// The signed low halfword of `pulseRate` is in 4096 angle units per animation
/// frame. Intensity is `rsin(animFrame * pulseRate) / 34 + 120` (0..240).
/// The signed low halfword of `radiusScale` gives outer and inner pixel radii
/// of `radiusScale * 64 / depth` and `radiusScale * 8 / depth`, where depth is
/// camera Z / 4. Eight half-bright outer wedges have full-bright copies at
/// half radius; four half-bright blades alternate tips at one and two outer
/// radii. Rejects negative GTE flags and requires nonzero depth. Borrows the
/// point during this call, reserves and releases one `EffectShapeScratch`,
/// and queues twenty additive quads plus blend commands in the current frame.
static void _mineRefugeDrawPulsingCyanBurst(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale)
{
    void**              scratchCursor;
    EffectShapeScratch* scratchEnd;
    EffectShapeScratch* scratchBlock;
    POLY_G4*            quad;
    s32                 pulseSine;
    s32                 cyanIntensity;
    s32                 halfIntensity;
    s32                 radiusNumerator;
    s32                 angle;
    s32                 middleAngle;
    s32                 endAngle;
    s32                 rimAngle;

    scratchCursor = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd    = *scratchCursor;
    scratchBlock  = (*scratchCursor = scratchEnd - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);
    if (scratchBlock->projectionFlags >= 0) {
        gte_stszotz(&(scratchEnd - 1)->depth);
        pulseSine                        = rsin(gDisplayState.animFrame * (s16)pulseRate);
        angle                            = 0;
        radiusNumerator                  = (s16)radiusScale;
        scratchBlock->extent.burst.outer = (radiusNumerator * GLOW_RADIUS_SCALE) / scratchBlock->depth;
        scratchBlock->extent.burst.inner = (radiusNumerator * GLOW_INNER_RADIUS_SCALE) / scratchBlock->depth;
        cyanIntensity                    = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        // Pair eight dim outer wedges with brighter wedges at half the radius.
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            halfIntensity  = (s16)cyanIntensity >> 1;
            _mineRefugeInitGlowQuad(quad, 0, halfIntensity, halfIntensity);
            quad->x0    = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(angle)) >> GLOW_TRIG_SHIFT);
            middleAngle = angle + GLOW_SIXTEENTH_TURN;
            quad->y0    = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(angle)) >> GLOW_TRIG_SHIFT);
            quad->x1    = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(middleAngle)) >> GLOW_TRIG_SHIFT);
            quad->y1    = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(middleAngle)) >> GLOW_TRIG_SHIFT);
            endAngle    = angle + GLOW_EIGHTH_TURN;
            quad->x2    = scratchBlock->screenX;
            quad->y2    = scratchBlock->screenY;
            quad->x3    = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(endAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3    = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(endAngle)) >> GLOW_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            quad->x0 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(angle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y0 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(angle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->x1 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(middleAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y1 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(middleAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->x2 = scratchBlock->screenX;
            quad->y2 = scratchBlock->screenY;
            quad->x3 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(endAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y3 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(endAngle)) >> (GLOW_TRIG_SHIFT + 1));
            angle    = endAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);
        } while (angle < GLOW_FULL_TURN);

        // Add four blades, alternating tips at one and two outer radii.
        cyanIntensity = halfIntensity;
        angle         = GLOW_EIGHTH_TURN;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            rimAngle = angle - GLOW_QUARTER_TURN;
            quad->x0 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y0 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->x1 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y1 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(angle)) >> GLOW_TRIG_SHIFT);
            rimAngle = angle + GLOW_QUARTER_TURN;
            quad->x2 = scratchBlock->screenX;
            quad->y2 = scratchBlock->screenY;
            quad->x3 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            quad->y3 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(rimAngle)) >> (GLOW_TRIG_SHIFT + 1));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _mineRefugeInitGlowQuad(quad, 0, cyanIntensity, cyanIntensity);
            quad->x0 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(angle)) >> GLOW_TRIG_SHIFT);
            quad->y0 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(angle)) >> GLOW_TRIG_SHIFT);
            quad->x1 = scratchBlock->screenX + ((scratchBlock->extent.burst.outer * rsin(rimAngle)) >> (GLOW_TRIG_SHIFT - 1));
            quad->y1 = scratchBlock->screenY + ((scratchBlock->extent.burst.outer * rcos(rimAngle)) >> (GLOW_TRIG_SHIFT - 1));
            rimAngle = angle + GLOW_HALF_TURN;
            quad->x2 = scratchBlock->screenX;
            quad->y2 = scratchBlock->screenY;
            quad->x3 = scratchBlock->screenX + ((scratchBlock->extent.burst.inner * rsin(rimAngle)) >> GLOW_TRIG_SHIFT);
            quad->y3 = scratchBlock->screenY + ((scratchBlock->extent.burst.inner * rcos(rimAngle)) >> GLOW_TRIG_SHIFT);
            angle    = rimAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchBlock->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->depth);
        } while (angle < GLOW_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws three overlapping, flickering discs with a fading centre at a world point.
///
/// The signed low halfword of `radiusScale` gives the first pixel radius as
/// `radiusScale * 64 / depth`, where depth is camera Z / 4. Later discs double
/// that radius and halve each centre color byte; each disc has eight wedges.
/// `packedColor` is RGB444 (0..0xFFF); each nibble is scaled by 16 and odd
/// animation frames add 32 before byte narrowing. Rims are black.
///
/// Rejects negative GTE flags and requires nonzero depth. Borrows the point
/// during this call and queues twenty-four additive quads plus blend commands.
/// Reserves one `GlowCentreScratch` even for a rejected projection and leaves
/// that reservation active until the enclosing scratch-stack reset.
static void _mineRefugeDrawLayeredGlow(const SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        MINE_REFUGE_GLOW_DISC_COUNT         = 3,
        MINE_REFUGE_GLOW_FLICKER_SHIFT      = 5,
        MINE_REFUGE_GLOW_COLOR_NIBBLE_SHIFT = 4,
        MINE_REFUGE_GLOW_COLOR_NIBBLE_MASK  = 0xF,
        MINE_REFUGE_GLOW_COLOR_BYTE_MASK    = 0xF0,
    };

    void**             scratchCursor;
    GlowCentreScratch* scratchEnd;
    GlowCentreScratch* scratchBlock;
    POLY_G4*           quad;
    s32                screenRadius;
    s32                discIndex;
    s32                angle;
    s32                middleAngle;
    s32                endAngle;
    s32                shiftedColor;
    s32                flickerIntensity;
    s32                redIntensity;
    s32                greenIntensity;
    s32                blueIntensity;

    scratchCursor = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd    = *scratchCursor;
    scratchBlock  = (*scratchCursor = scratchEnd - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&(scratchEnd - 1)->sx);
    gte_stflg(&(scratchEnd - 1)->flag);
    if (scratchBlock->flag >= 0) {
        gte_stszotz(&scratchBlock->otz);
        screenRadius         = ((s16)radiusScale * GLOW_RADIUS_SCALE) / (scratchEnd - 1)->otz;
        discIndex            = 0;
        flickerIntensity     = ((u8)gDisplayState.animFrame & 1) << MINE_REFUGE_GLOW_FLICKER_SHIFT;
        shiftedColor         = packedColor << 16;
        redIntensity         = flickerIntensity + ((shiftedColor >> (16 + MINE_REFUGE_GLOW_COLOR_NIBBLE_SHIFT)) & MINE_REFUGE_GLOW_COLOR_BYTE_MASK);
        greenIntensity       = flickerIntensity + ((shiftedColor >> 16) & MINE_REFUGE_GLOW_COLOR_BYTE_MASK);
        blueIntensity        = flickerIntensity + ((packedColor & MINE_REFUGE_GLOW_COLOR_NIBBLE_MASK) << MINE_REFUGE_GLOW_COLOR_NIBBLE_SHIFT);
        scratchBlock->radius = screenRadius;
        // Each filled disc doubles the radius and halves the centre color.
        do {
            angle = 0;
            do {
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                _mineRefugeInitGlowQuad(quad, redIntensity, greenIntensity, blueIntensity);
                quad->x0    = scratchBlock->sx + ((scratchBlock->radius * rsin(angle)) >> GLOW_TRIG_SHIFT);
                middleAngle = angle + GLOW_SIXTEENTH_TURN;
                quad->y0    = scratchBlock->sy + ((scratchBlock->radius * rcos(angle)) >> GLOW_TRIG_SHIFT);
                quad->x1    = scratchBlock->sx + ((scratchBlock->radius * rsin(middleAngle)) >> GLOW_TRIG_SHIFT);
                quad->y1    = scratchBlock->sy + ((scratchBlock->radius * rcos(middleAngle)) >> GLOW_TRIG_SHIFT);
                endAngle    = angle + GLOW_EIGHTH_TURN;
                quad->x2    = scratchBlock->sx;
                quad->y2    = scratchBlock->sy;
                quad->x3    = scratchBlock->sx + ((scratchBlock->radius * rsin(endAngle)) >> GLOW_TRIG_SHIFT);
                quad->y3    = scratchBlock->sy + ((scratchBlock->radius * rcos(endAngle)) >> GLOW_TRIG_SHIFT);
                angle       = endAngle;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratchBlock->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        quad);
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratchBlock->otz);
            } while (angle < GLOW_FULL_TURN);
            redIntensity          = (u8)redIntensity >> 1;
            greenIntensity        = (u8)greenIntensity >> 1;
            blueIntensity         = (u8)blueIntensity >> 1;
            scratchBlock->radius *= 2;
            discIndex++;
        } while (discIndex < MINE_REFUGE_GLOW_DISC_COUNT);
    }
}

void mineRefugeDrawGlowsTask(Task* task)
{
    enum { MINE_REFUGE_POWER_PANEL_ON  = 1,
           MINE_REFUGE_CYAN_PULSE_RATE = 0x60 };
    u8 viewIndex;

    viewIndex = viewGetMappedIndex();
    switch (viewIndex) {
        case 2:
            _glowDrawFlare(&D_mine_refuge_801818D8[0], 1, 0x300);
            _mineRefugeDrawPulsingCyanStar(&D_mine_refuge_801818D8[1], MINE_REFUGE_CYAN_PULSE_RATE, 0x40);
            break;
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == MINE_REFUGE_POWER_PANEL_ON) {
                _mineRefugeDrawLayeredGlow(&D_mine_refuge_801818E8, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == MINE_REFUGE_POWER_PANEL_ON) {
                _mineRefugeDrawLayeredGlow(&D_mine_refuge_801818E8, 0x60, 0xD0);
            }
            break;
        case 6:
            _mineRefugeDrawPulsingCyanBurst(&D_mine_refuge_801818D8[1], MINE_REFUGE_CYAN_PULSE_RATE, 0x80);
            break;
    }
}
