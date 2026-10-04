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
static void func_mine_refuge_8017FFAC(Task* task);

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

static void func_mine_refuge_8018029C(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2);

#include "../../shared/telephone.inc.c"

void func_mine_refuge_8017EA78(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// States of the room's message task, run by `func_mine_refuge_8017FFBC`:
/// install the message table, idle, die.
static const TaskFuncTable3 D_mine_refuge_8017D6A4 = {
    {
        func_mine_refuge_8017FF4C,
        func_mine_refuge_8017FFAC,
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
                Gp_RunCapCmd1(0xF);
            }
            task->state = task->state + 1;
            return;
        case 2:
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_CIRCUIT_PANEL_OPEN, 0, 0);
            D_mine_refuge_80182AD8 = Task_SpawnFromTable(&D_actor_548100_801358D8, 0, 0, 0);
            task->state            = task->state + 1;
            return;
        case 3:
            if (Task_PollKill(D_mine_refuge_80182AD8, &sp10) != 0) {
                D_mine_refuge_80182AD8 = NULL;
                task->state            = task->state + 1;
            }
            return;
        case 1:
        case 4:
            task->state = task->state + 1;
            return;
        case 5:
            SndEvt_EnqueueType6(SOUND_MINE_REFUGE_CIRCUIT_PANEL_CLOSE, 0, 0);
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
/// outgoing one, hands both to `func_map_shelter_80179A04` and returns 1.
s32 func_mine_refuge_8017FBE8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_mine_refuge_8017FC2C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8 temp_a3;

    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_REFUGE_PROMPT_ACCEPTED) != 0) {
            func_mine_refuge_8017FE78(0U);
        } else {
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            temp_a3                                                    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6U;
            D_mine_refuge_80182ADC[0]                                  = temp_a3;
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 3), 0, 0);
            Gp_RunCapCmd(0xD, 0);
            Task_SpawnFromTable(D_mine_refuge_801818B4, 1, 0, 0);
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
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(D_mine_refuge_801818B4, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(0xA);
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
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0C), 0, 0);
            break;
        case 0x63:
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0F), 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_MINE_REFUGE, 0x0D), 0, 0);
            break;
    }
    return 0;
}

void func_mine_refuge_8017FDBC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                return;
            }
            if (Gp_GetCapEventKey() == 5) {
                arg0->state = arg0->state + 1;
                return;
            }
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_mine_refuge_80182ADC[0];
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
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
    Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, slot, &D_mine_refuge_80182AE0);
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

/// Idle state of the room's message task: does nothing.
static void func_mine_refuge_8017FFAC(Task* task)
{
    char pad[0x10];
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

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 32 / depth`. The lit vertex pulses on green and blue at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78`. A 0x18-byte scratch block in the
/// `EffectCentreScratch` layout is taken from the scratch stack and returned.
static void func_mine_refuge_8018029C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_G4*             prim;
    LINE_G3*             line;
    s32                  sine;
    s32                  pulse;
    s32                  radius;
    s32                  i;
    s32                  t1;
    s32                  t2;
    s32                  twice;
    u16                  sx;
    u16                  sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(EffectCentreScratch));
        block   = (EffectCentreScratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        sine                = rsin(gDisplayState.animFrame * (s16)arg1);
        radius              = ((s16)arg2 * 32) / block->depth;
        i                   = 0;
        pulse               = sine / 34 + 0x78;
        block->screenExtent = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, pulse, pulse);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX - (u16)block->screenExtent;
            sx       = block->screenX;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->screenX + (u16)block->screenExtent;
            sy       = block->screenY;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->screenY - (u16)block->screenExtent) + (block->screenExtent * twice);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
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
            line->x0 = block->screenX + (block->screenExtent * t1);
            line->y0 = block->screenY - (block->screenExtent * t2);
            line->x1 = block->screenX;
            line->y1 = block->screenY;
            line->x2 = block->screenX - (block->screenExtent * t1);
            line->y2 = block->screenY + (block->screenExtent * t2);
            addPrim((&gGpuCurrentOt[((u32)block->depth << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->depth);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues a glow of gouraud `POLY_G4` wedges
/// around the projected centre: an eight-wedge disc of radius
/// `(s16)arg2 * 64 / otz`, each wedge paired with a half-radius copy, then four
/// wedges reaching between that radius and an inner one of `(s16)arg2 * 8 /
/// otz`. Only the centre vertex is lit, on green and blue, with a level of
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78` so the glow pulses; the half-radius
/// copies take that level and every other wedge half of it. The scratch block is returned to
/// the scratch stack on exit.
static void func_mine_refuge_80180710(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**              scratch;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_G4*            prim;
    s32                 pulse;
    s32                 color;
    s32                 half;
    s32                 size;
    s32                 ang;
    s32                 t;
    s32                 t2;
    s32                 u;

    scratch = SCRATCH_STACK_CURSOR_SLOT;
    head    = *scratch;
    block   = (*scratch = head - 1);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        pulse                     = rsin(gDisplayState.animFrame * (s16)arg1);
        ang                       = 0;
        size                      = (s16)arg2;
        block->extent.burst.outer = (size * 64) / block->depth;
        block->extent.burst.inner = (size * 8) / block->depth;
        color                     = pulse / 34 + 0x78;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, half, half);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->extent.burst.outer * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->screenY + ((block->extent.burst.outer * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(t)) >> 12);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.outer * rsin(t2)) >> 12);
            prim->y3 = block->screenY + ((block->extent.burst.outer * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->extent.burst.outer * rsin(ang)) >> 13);
            prim->y0 = block->screenY + ((block->extent.burst.outer * rcos(ang)) >> 13);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(t)) >> 13);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(t)) >> 13);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.outer * rsin(t2)) >> 13);
            prim->y3 = block->screenY + ((block->extent.burst.outer * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);

        color = half;
        ang   = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->screenX + ((block->extent.burst.inner * rsin(u)) >> 13);
            prim->y0 = block->screenY + ((block->extent.burst.inner * rcos(u)) >> 13);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(ang)) >> 12);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.inner * rsin(u)) >> 13);
            prim->y3 = block->screenY + ((block->extent.burst.inner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->extent.burst.inner * rsin(ang)) >> 12);
            prim->y0 = block->screenY + ((block->extent.burst.inner * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(u)) >> 11);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.inner * rsin(u)) >> 12);
            prim->y3 = block->screenY + ((block->extent.burst.inner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues three concentric rings of eight
/// gouraud `POLY_G4` wedges around the projected centre. The first ring's
/// radius is `(s16)arg1 * 64 / otz`; each later ring doubles it and halves the
/// centre colour. `arg2` packs three RGB nibbles for the centre vertex, each
/// offset by `(animFrame & 1) << 5` so the glow flickers on alternate frames.
/// Unlike the room's other draws it never returns its 0x10-byte scratch block
/// to the scratch stack.
static void func_mine_refuge_80181094(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    GlowCentreScratch* block;
    POLY_G4*           prim;
    s32                ring;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;

    scratch = SCRATCH_STACK_CURSOR_SLOT;
    head    = *scratch;
    block   = (GlowCentreScratch*)(*scratch = head - 0x10);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((GlowCentreScratch*)(head - 0x10))->sx);
    gte_stflg(&((GlowCentreScratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / ((GlowCentreScratch*)(head - 0x10))->otz;
        ring          = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) << 5;
        packed        = arg2 << 16;
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            ang = 0;
            do {
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, r, g, b);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
                t        = ang + 0x100;
                prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
                prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
                prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
                t2       = ang + 0x200;
                prim->x2 = block->sx;
                prim->y2 = block->sy;
                prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
                prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
                ang      = t2;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            } while (ang < 0x1000);
            r              = (u8)r >> 1;
            g              = (u8)g >> 1;
            b              = (u8)b >> 1;
            block->radius *= 2;
            ring++;
        } while (ring < 3);
    }
}

/// Draws the room's glows for whichever view is current. View 2 draws a
/// sprite and a diamond; views 3 and 4/5 draw rings at one anchor, but only
/// while progress nibble 0xC3 is 1; view 6 draws a pulsing disc.
void func_mine_refuge_80181454(Task* unused)
{
    u8 view;

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            glowDrawFlare(&D_mine_refuge_801818D8[0], 1, 0x300);
            func_mine_refuge_8018029C(&D_mine_refuge_801818D8[1], 0x60, 0x40);
            break;
        case 3:
            if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x30, 0xF0);
            }
            break;
        case 4:
        case 5:
            if (gameFlagGetNibble(GAME_FLAG_MINE_POWER_PANEL_SWITCHED_ON) == 1) {
                func_mine_refuge_80181094(&D_mine_refuge_801818E8, 0x60, 0xD0);
            }
            break;
        case 6:
            func_mine_refuge_80180710(&D_mine_refuge_801818D8[1], 0x60, 0x80);
            break;
    }
}
