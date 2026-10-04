#include "rooms/shelter_b2_laboratory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

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
#include "main/stage_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
// Symbol is `RoomCutsceneSoundTaskStorage`; the runner reads `task`.
#define ROOM_CUTSCENE_SOUND_TASK gRoomCutsceneSoundTask.task
#include "../../shared/room_cutscene.h"

/// Scratch block this room's diamond glow drawer takes from the scratch stack
/// for one projected centre.
///
/// One perspective transform of a world point through `gGfxViewCoord.workm`
/// writes the screen position and the GTE flag word. A negative flag word
/// means the transform reported an error, and the drawer links nothing.
/// Otherwise it stores the ordering-table depth and the on-screen half-extent,
/// a size divided by that depth, and builds the diamonds around the centre.
/// `sx` and `sy` are written by one screen-XY store, so they stay adjacent.
///
/// `GlowCentreScratch` holds the same four values in 16 bytes. This record
/// keeps eight more bytes between the half-extent and the screen position;
/// the drawer reserves and releases them with the rest of the block and never
/// reads or writes them, so the records stay separate.
typedef struct {
    s32 otz;        // Ordering-table depth of the centre; also the divisor for the half-extent
    s32 flag;       // GTE flag word; negative means the transform reported an error
    s32 radius;     // On-screen half-extent of the diamonds around the centre
    u8  unknown[8]; // Reserved with the block and never accessed; role and grouping unproven
    u16 sx;         // Projected centre, x
    u16 sy;         // Projected centre, y
} _ShelterB2LaboratoryGlowDiamondScratch;
STATIC_ASSERT_SIZEOF(_ShelterB2LaboratoryGlowDiamondScratch, 0x18);

extern UiObjectDesc D_800611E4;

extern TaskDesc D_80134564;

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

/// Task tables the room spawns from: the cutscene, the flag-gated exit's
/// transition, and the console's tasks.
extern TaskDesc gRoomCutsceneTaskDescs[];
extern TaskDesc gRoomEventTaskDesc;
extern TaskDesc D_shelter_b2_laboratory_80182A6C[];

/// Message table of the room's message task.
extern TaskMessageEntry D_shelter_b2_laboratory_80182A38[];

/// Task spawned by `func_shelter_b2_laboratory_80180290` and polled until it
/// dies.
extern Task* D_shelter_b2_laboratory_80182A68;

/// Per-view attenuation, in percent, of the looping sound played by
/// `func_shelter_b2_laboratory_8017FEB8`.
extern s8 D_shelter_b2_laboratory_80182A90[];

/// Glow positions `func_shelter_b2_laboratory_80180548` draws per view:
/// `glowDrawCapsule` takes the pair `pt[n]`, `pt[n + 1]`;
/// `func_shelter_b2_laboratory_801812F8` and
/// `glowDrawPulsingDisc` a single point.
extern SVECTOR D_shelter_b2_laboratory_80182AA0[45];

/// This room's cutscene sound-task slot. The runner uses `task`.
extern RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask;

/// Copies of the message and request that started the pending exit
/// transition, read back by `roomEventTask`.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set when `roomEventGate` started a transition,
/// cleared on every other call.
extern u8 gRoomEventActive;

/// Non-zero while the looping sound of `func_shelter_b2_laboratory_8017FEB8`
/// should keep playing; set by `func_shelter_b2_laboratory_801804FC`.
extern s32 D_shelter_b2_laboratory_801864B8;

/// Parameters of the cutscene `func_shelter_b2_laboratory_8017FD18` starts.
extern RoomCutsceneRecStorage D_shelter_b2_laboratory_801864BC;

/// World position the looping sound is panned and attenuated from.
extern GfxCoord D_shelter_b2_laboratory_801864DC;

/// Non-zero makes the view glows of `func_shelter_b2_laboratory_80180548`
/// pulse faster. Written through `func_shelter_b2_laboratory_801820F4`; the
/// glow task clears it when it starts.
extern u16 D_shelter_b2_laboratory_80186540;

#define TELEPHONE_TITLE_BYTES "Telephone\0\xFA\xA9"
#include "../../shared/telephone.h"

static void func_shelter_b2_laboratory_80180450(Task* task);
static void func_shelter_b2_laboratory_80180494(Task* task);
static void func_shelter_b2_laboratory_801812F8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_laboratory_801820F4(s16 arg0);

s32  func_shelter_b2_laboratory_8017FD18(Task*, s32, s32, s32);
s32  func_shelter_b2_laboratory_801800F4(Task*, s32, s32, s32);
s32  func_shelter_b2_laboratory_801800FC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_laboratory_801801D0(Task* task, s32 msgId, const void* firstArg, s32);
s32  func_shelter_b2_laboratory_8018025C(Task*, s32, s32, s32);
void func_shelter_b2_laboratory_8017FEB8(Task*);
void func_shelter_b2_laboratory_80180290(Task*);
void func_shelter_b2_laboratory_80180350(Task*);

extern WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186450[1];
extern WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186458[1];
extern WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186460[1];

#include "../../shared/telephone_data.inc.c"

static TmdBone _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gShelterB2LaboratoryAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gShelterB2LaboratoryAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0PartVerts,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Verts,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Normals,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Skeleton,
    _gShelterB2LaboratoryAcropolisSanctuaryModel090F0Stream,
};

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_shelter_b2_laboratory_80182A38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_laboratory_801800FC },
    { 5105, func_shelter_b2_laboratory_801800F4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b2_laboratory_801801D0 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b2_laboratory_8017FD18 },
    { ROOM_MESSAGE_SOUND, func_shelter_b2_laboratory_8018025C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

Task* D_shelter_b2_laboratory_80182A68 = NULL;

TaskDesc D_shelter_b2_laboratory_80182A6C[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_laboratory_80180290, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_laboratory_8017FEB8, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_shelter_b2_laboratory_80180350, { .value = 0 } },
};

s8 D_shelter_b2_laboratory_80182A90[16] = {
    0,
    0,
    60,
    30,
    45,
    100,
    75,
    40,
    30,
    0,
    0,
    0,
    0,
    0,
    0,
    100,
};

SVECTOR D_shelter_b2_laboratory_80182AA0[45] = {
    { 182, -1966, 5669, 0 },
    { 823, -1966, 5669, 0 },
    { 3202, -1966, 5669, 0 },
    { 3843, -1966, 5669, 0 },
    { 1642, -2807, 4491, 0 },
    { 2462, -2807, 4491, 0 },
    { 1642, -2807, 4351, 0 },
    { 2462, -2807, 4351, 0 },
    { 1709, -2807, 1442, 0 },
    { 2529, -2807, 1442, 0 },
    { 1709, -2807, 1302, 0 },
    { 2529, -2807, 1302, 0 },
    { 1709, -2807, -1369, 0 },
    { 2529, -2807, -1369, 0 },
    { 1709, -2807, -1511, 0 },
    { 2529, -2807, -1511, 0 },
    { 8935, -3150, 1448, 0 },
    { 9762, -3150, 1448, 0 },
    { 8935, -3150, 1306, 0 },
    { 9762, -3150, 1306, 0 },
    { 8902, -3150, -1343, 0 },
    { 9721, -3150, -1343, 0 },
    { 8902, -3150, -1483, 0 },
    { 9721, -3150, -1483, 0 },
    { 12886, -2946, -1615, 0 },
    { 12886, -2946, -2278, 0 },
    { -339, -2195, 2398, 0 },
    { -339, -2195, 1547, 0 },
    { 6018, -2111, 3328, 0 },
    { 6806, -2111, 3328, 0 },
    { -278, -2407, -1452, 0 },
    { -278, -2407, -1091, 0 },
    { -278, -2407, -780, 0 },
    { -278, -2407, -418, 0 },
    { -278, -2407, -97, 0 },
    { -278, -2407, 262, 0 },
    { 11312, -3085, -362, 0 },
    { 11312, -3085, -1, 0 },
    { 11312, -3085, 1054, 0 },
    { 11312, -3085, 1418, 0 },
    { 11312, -3085, 2483, 0 },
    { 11312, -3085, 2846, 0 },
    { 3100, -1163, -966, 0 },
    { 3472, -1163, -966, 0 },
    { 3181, -1161, -3312, 0 },
};

u8* D_shelter_b2_laboratory_80182C08[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b2_laboratory_80182C0C[1] = { 15 };

DirectionWarpEntry D_shelter_b2_laboratory_80182C10[2] = {
    { { { .word = 1024 }, 100, 0, 1800 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 100, 0, 1800 }, { 0, 0, 0, 0 }, 0x541F0004, 0x541F0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 451 },
    { { { .word = 2048 }, 6350, 0, 2900 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 6350, 0, 2900 }, { 0, 0, 0, 0 }, 0x541F0002, 0x541F0001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB2LaboratoryCollision05F9CNormals[23] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_normals.inc"
};

static SVECTOR _gShelterB2LaboratoryCollision05F9CVerts[123] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_verts.inc"
};

static WorldCollisionGridFace _gShelterB2LaboratoryCollision05F9CFaces[51] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_faces.inc"
};

static s16 _gShelterB2LaboratoryCollision05F9CCells[220] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB2LaboratoryCollision05F9CCells[i])
static s16* _gShelterB2LaboratoryCollision05F9CTable[12] = {
#include "assets/shelter_b2_laboratory_collision_05F9C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b2_laboratory_8018355C = { NULL, _gShelterB2LaboratoryCollision05F9CNormals, _gShelterB2LaboratoryCollision05F9CVerts, _gShelterB2LaboratoryCollision05F9CFaces, _gShelterB2LaboratoryCollision05F9CTable, 1411, 4736, 4, 3, 4000, 51 };

ViewCamera D_shelter_b2_laboratory_80183580[15] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -6200, 0x7530, -1480 } }, 564 },
    { { { { 1318, 0, 3878 }, { 1296, 3860, -440 }, { -3655, 1369, 1242 } }, { -7992, 1967, -470 } }, 230 },
    { { { { 3662, 0, 1834 }, { 408, 3992, -816 }, { -1788, 912, 3570 } }, { -4133, 1597, -776 } }, 230 },
    { { { { 1559, 0, -3787 }, { -761, 4012, -313 }, { 3709, 823, 1527 } }, { -3665, 1388, -448 } }, 230 },
    { { { { 922, 0, 3990 }, { 799, 4012, -184 }, { -3909, 820, 904 } }, { -8742, 1568, 3074 } }, 230 },
    { { { { 988, 0, -3974 }, { -1089, 3939, -270 }, { 3822, 1122, 950 } }, { -4027, 2154, 3224 } }, 230 },
    { { { { 1535, 0, -3797 }, { -1082, 3926, -437 }, { 3639, 1167, 1471 } }, { -8309, 3056, 3056 } }, 230 },
    { { { { 3861, 0, -1367 }, { -277, 4010, -783 }, { 1338, 830, 3780 } }, { -9759, 2782, 2019 } }, 230 },
    { { { { -3525, 0, -2084 }, { 56, 4094, -95 }, { 2083, -110, -3524 } }, { -2270, 1220, -572 } }, 380 },
    { { { { 3982, 0, -958 }, { 27, 4094, 112 }, { 958, -115, 3980 } }, { -2860, 1220, 3450 } }, 541 },
    { { { { 4095, 0, 0 }, { 0, 4091, -197 }, { 0, 197, 4091 } }, { 0x2E72, 1215, 1870 } }, 541 },
    { { { { -794, 0, 4018 }, { -178, 4091, -35 }, { -4014, -182, -793 } }, { -6219, 895, 1568 } }, 269 },
    { { { { -4036, 0, 695 }, { 136, 4016, 793 }, { -682, 804, -3957 } }, { -3499, 1404, 2437 } }, 269 },
    { { { { 4095, 0, 0 }, { 0, 4091, -197 }, { 0, 197, 4091 } }, { 0x2E72, 1215, 1870 } }, 541 },
    { { { { 862, 0, 4004 }, { 1204, 3906, -259 }, { -3818, 1231, 822 } }, { -6593, 1887, 2833 } }, 246 },
};

SpriteBatch D_shelter_b2_laboratory_8018379C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801837AC[51] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 40, 0, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, 16, 0, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 24, 0, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -136, 32, 0, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 16, 535, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 24, 521, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 519, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 48, 474, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 48, 504, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 64, 471, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 64, 423, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 80, 402, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 80, 510, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 48, 0, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 88, 507, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 104, 501, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 80, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 112, 504, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 96, 351, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -8, 0, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 128, -48, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 0, 0, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 0, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 16, 0, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, -72, 743, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, -72, 783, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -32, 826, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -72, 856, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -40, 923, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -72, 965, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, -24, 994, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -72, 1011, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, -32, 1042, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -32, 1096, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -72, 1065, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -72, 1110, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 1159, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 16, 1140, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 8, 1212, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -8, 1194, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 1147, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 0, 1036, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -72, 1065, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -72, 1100, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, -40, 1144, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -24, 1162, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -72, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -104, -24, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -16, 1447, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, -24, 1456, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, -24, 1480, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80183BA8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 28, 0, 0, { 2, 0 } },
    { 47, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80183BD0[20] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, -80, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -24, 0, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -24, 0, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, 16, 0, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -80, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -80, 375, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -56, 375, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, -32, 375, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 0, 375, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 104, 24, 375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 120, 56, 0, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 64, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 0, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 96, 375, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 88, 375, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 80, 375, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 96, 56, 375, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 64, 375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 375, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 104, 0, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80183D60[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80183D78[30] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -40, 1397, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -40, 1311, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 80, -40, 1267, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -40, 1048, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 16, 0, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 40, 0, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 72, 609, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 48, 564, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 136, 16, 555, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 0, 560, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -88, 0, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, -64, 0, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, 0, 0, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 24, 0, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 56, 678, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 64, 0, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 56, 709, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 730, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 736, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 32, 738, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 16, 727, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 0, 690, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 0, 643, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -88, 638, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -64, 636, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, -40, 630, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, -16, 637, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -64, 636, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -40, 642, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 648, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80183FD0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 6, 0, 0, { 2, 0 } },
    { 10, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80183FF8[23] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -32, 0, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -24, 0, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 0, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 48, 0, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 104, 72, 0, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, 64, 0, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 48, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, -32, 698, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 523, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, -32, 767, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -32, 0, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -8, 903, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 0, 778, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 0, 717, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 0, 711, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 606, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 0, 529, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 120, 48, 536, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 32, 574, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 16, 727, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, 48, 813, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 48, 578, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 16, 619, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_801841C4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_801841DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801841EC[99] = {
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 64, 0, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 64, 0, { .fields = { 80, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 72, 0, { .fields = { 32, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, 80, 0, { .fields = { 104, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, 88, 0, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 0, { .fields = { 56, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 96, 0, { .fields = { 48, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 88, 0, { .fields = { 56, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 72, 0, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 56, 0, { .fields = { 64, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 0, { .fields = { 80, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -16, -8, 0, { .fields = { 32, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -160, -8, 0, { .fields = { 104, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -136, -8, 0, { .fields = { 88, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -96, -8, 0, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, 0, 0, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 8, 0, { .fields = { 56, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 16, 0, { .fields = { 56, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 0, { .fields = { 64, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 32, 0, { .fields = { 80, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, -8, 899, { .fields = { 120, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 899, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 0, 826, { .fields = { 56, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 859, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 8, 707, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 40, 734, { .fields = { 96, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 16, 627, { .fields = { 80, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 56, 665, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 32, 558, { .fields = { 72, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 64, 570, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 40, 507, { .fields = { 56, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 72, 514, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 32, 503, { .fields = { 64, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, 64, 527, { .fields = { 80, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 32, 520, { .fields = { 64, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 64, 545, { .fields = { 64, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -40, -8, 0, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -8, 0, { .fields = { 32, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 24, 536, { .fields = { 32, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, 56, 563, { .fields = { 8, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 24, 570, { .fields = { 0, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -96, 56, 597, { .fields = { 96, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 16, 602, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 40, 632, { .fields = { 32, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 653, { .fields = { 0, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 40, 683, { .fields = { 0, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, 8, 647, { .fields = { 72, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, 0, 811, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 32, 856, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, 16, 827, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 8, 0, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, 32, 56, 787, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, 32, 64, 0, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 104 } }, 24, 16, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 16, 24, 0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -32, -16, 0, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -96, -16, 0, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -160, -16, 0, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -24, -8, 0, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -24, 0, 0, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -160, 8, 0, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -160, 16, 0, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -112, 16, 0, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -72, 24, 0, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -112, 24, 0, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -160, 24, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 24 } }, -160, 32, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -128, 56, 639, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 64 } }, -160, 56, 0, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -128, 72, 0, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, -112, 32, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -72, 32, 0, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 56, 0, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 88, 0, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 64 } }, -64, 56, 0, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -40, 32, 0, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -40, 40, 0, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -40, 72, 0, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -8, 40, 490, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -8, 64, 508, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -8, 88, 526, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, 0, 24, 491, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, 32, -16, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 24, -8, 707, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 16, 0, 713, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, 8, 688, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 0, 16, 480, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -8, 16, 467, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -24, 8, 0, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -16, 16, 474, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -24, 16, 481, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, 8, 487, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, -40, 8, 503, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 16 } }, -80, 8, 516, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -80, -8, 0, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -80, 0, 0, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, -128, 0, 562, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -128, -8, 0, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -160, -8, 619, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_801849A8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 47, 0, 0, { 1, 0 } },
    { 47, 52, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801849C8[78] = {
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -64, -32, 0, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -56, -16, 0, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -48, 0, 0, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -40, 8, 0, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -32, 24, 0, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 40, 0, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -16, 48, 0, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 64, 0, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 80, 0, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 0, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 64, 0, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 0, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 40, 0, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -8, 0, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -24, 104, 225, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 104, 315, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 96, 238, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 80, 253, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 96, 309, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 80, 324, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 64, 278, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -48, 48, 311, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 40, 343, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 24, 385, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 48, 661, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 16, 644, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 16, 442, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 8, 476, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, 16, 1014, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 960, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 541, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, -8, 631, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -80, -16, 779, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -32, 1112, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -80, 0, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -64, 0, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -56, 0, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 0, 0, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 16, 1879, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -80, 1718, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 32, -64, 0, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -80, 1714, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 8, 1869, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -8, 1862, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -24, 1795, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -64, 1750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -40, 1750, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 24, 0, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 64, -56, 0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -80, 713, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -56, 713, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -32, 725, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -8, 750, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 8, 776, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 32, 803, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, -64, -24, 0, { .fields = { 48, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 40, 16 } }, -56, 0, 0, { .fields = { 16, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, -48, 16, 0, { .fields = { 56, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, 40, 0, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, -32, 64, 0, { .fields = { 64, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, -24, 80, 0, { .fields = { 96, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -80, 112, 0, { .fields = { 0, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 40 } }, -80, 72, 0, { .fields = { 88, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, -80, 40, 0, { .fields = { 88, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 32 } }, -80, 8, 0, { .fields = { 104, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -48, 112, 269, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, -56, 104, 294, { .fields = { 112, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 16 } }, -56, 88, 322, { .fields = { 24, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, 80, 313, { .fields = { 0, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -56, 72, 348, { .fields = { 8, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 8 } }, -64, 64, 392, { .fields = { 0, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -64, 48, 442, { .fields = { 24, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -64, 40, 427, { .fields = { 0, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -72, 24, 596, { .fields = { 24, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -72, 16, 568, { .fields = { 0, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, 8, 694, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 8 } }, -80, 0, 890, { .fields = { 0, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, -80, -24, 1164, { .fields = { 72, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_shelter_b2_laboratory_80184FE0[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 3, 0 } },
    { 34, 13, 0, 0, { 0, 0 } },
    { 47, 8, 0, 0, { 2, 0 } },
    { 55, 23, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_80185010[16] = {
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -56, -16, 375, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -16, -16, 375, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 56, 403, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 56, 375, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 375, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 375, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 56, 375, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, 56, 438, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 48, 375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 48, 375, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 375, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 375, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 64, 393, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, 64, 393, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 375, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 96, 375, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_80185150[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185168[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185178[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185188[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_80185198[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_laboratory_801851A8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b2_laboratory_801851B8[37] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 48, 825, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -24, 787, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, -88, 825, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -72, 800, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 32, 928, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 32, 920, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -64, 844, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, -88, 825, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, -88, 772, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, -72, 804, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -56, 787, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -48, 825, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, -32, 825, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 24, 825, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -16, 825, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, -8, 864, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 0, 864, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 24, 925, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -48, 825, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -32, 825, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -16, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 0, 825, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 8, 941, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 929, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 24, 787, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -16, 0, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 24, 0, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 0, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 32, 0, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 864, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 8, 890, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 24, 949, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 32, 944, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 40, 919, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -16, 786, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 857, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 0, 864, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_laboratory_8018549C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 25, 0, 0, { 1, 0 } },
    { 25, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_shelter_b2_laboratory_801854BC[2] = {
    { { 60, 3, 257, 235 }, 745 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView D_shelter_b2_laboratory_801854D0[15] = {
    { { .empty = D_shelter_b2_laboratory_8018379C }, D_shelter_b2_laboratory_8018379C, NULL },
    { { .elements = D_shelter_b2_laboratory_801837AC }, D_shelter_b2_laboratory_80183BA8, NULL },
    { { .elements = D_shelter_b2_laboratory_80183BD0 }, D_shelter_b2_laboratory_80183D60, NULL },
    { { .elements = D_shelter_b2_laboratory_80183D78 }, D_shelter_b2_laboratory_80183FD0, NULL },
    { { .elements = D_shelter_b2_laboratory_80183FF8 }, D_shelter_b2_laboratory_801841C4, NULL },
    { { .empty = D_shelter_b2_laboratory_801841DC }, D_shelter_b2_laboratory_801841DC, NULL },
    { { .elements = D_shelter_b2_laboratory_801841EC }, D_shelter_b2_laboratory_801849A8, NULL },
    { { .elements = D_shelter_b2_laboratory_801849C8 }, D_shelter_b2_laboratory_80184FE0, NULL },
    { { .elements = D_shelter_b2_laboratory_80185010 }, D_shelter_b2_laboratory_80185150, NULL },
    { { .empty = D_shelter_b2_laboratory_80185168 }, D_shelter_b2_laboratory_80185168, NULL },
    { { .empty = D_shelter_b2_laboratory_80185178 }, D_shelter_b2_laboratory_80185178, NULL },
    { { .empty = D_shelter_b2_laboratory_80185188 }, D_shelter_b2_laboratory_80185188, NULL },
    { { .empty = D_shelter_b2_laboratory_80185198 }, D_shelter_b2_laboratory_80185198, NULL },
    { { .empty = D_shelter_b2_laboratory_801851A8 }, D_shelter_b2_laboratory_801851A8, NULL },
    { { .elements = D_shelter_b2_laboratory_801851B8 }, D_shelter_b2_laboratory_8018549C, D_shelter_b2_laboratory_801854BC },
};

WorldCoordPointLight D_shelter_b2_laboratory_80185584[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5547, -1204, 1082 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2071, 2222, 2201 }, { 0, 0 } }, 4000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6419, -1756, 3004 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 890, 1978, 1175 }, { 0, 0 } }, 1261, 1721 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2237, -2188, 1564 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1010, 2438, 694 }, { 0, 0 } }, 500, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2783, -1669, 5243 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3331, 3396, 3526 }, { 0, 0 } }, 1640, 2940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9198, -3405, -364 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1826, 2022, 2062 }, { 0, 0 } }, 2739, 7039 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2D99, -2950, -1960 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1848, 2009, 2285 }, { 0, 0 } }, 1201, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 457, -3405, -852 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1569, 1642, 1699 }, { 0, 0 } }, 1979, 4319 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3290, -989, -1161 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1474, 1940, 1859 }, { 0, 0 } }, 700, 1199 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2B0A, -2989, -198 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 562, 1720, 1805 }, { 0, 0 } }, 500, 1500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2B0A, -3109, 2300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 562, 1280, 1485 }, { 0, 0 } }, 500, 1500 },
};

WorldCoordRoomLights D_shelter_b2_laboratory_80185944 = { 0, NULL, ARRAY_SIZE(D_shelter_b2_laboratory_80185584), D_shelter_b2_laboratory_80185584, 0, NULL };

WorldCollisionTrigger D_shelter_b2_laboratory_8018595C[14] = {
    { NULL, NULL, NULL, { 5680, -1616, 1944, 0 }, { { 28, -3232, -2247, 0 }, { -47, -3232, 2228, 0 }, { 28, 3232, -2247, 0 }, { -47, 3232, 2228, 0 } }, { 4098, 0, 68, 0 }, { 0, 0, 4096, 0 }, 3924, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5824, -1568, 2000, 0 }, { { -66, -3216, 2306, 0 }, { 23, -3216, -2349, 0 }, { -66, 3216, 2306, 0 }, { 23, 3216, -2349, 0 } }, { -4103, 0, -79, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -1601, 3552, 0 }, { { -2912, -3216, -176, 0 }, { 2912, -3216, 176, 0 }, { -2912, 3216, -176, 0 }, { 2912, 3216, 176, 0 } }, { 246, 0, -4090, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1824, -1568, 3393, 0 }, { { 2976, -3216, 208, 0 }, { -2976, -3216, -208, 0 }, { 2976, 3216, 208, 0 }, { -2976, 3216, -208, 0 } }, { -288, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6526, -1504, -2436, 0 }, { { 89, -3216, 1462, 0 }, { -89, -3216, -1462, 0 }, { 89, 3216, 1462, 0 }, { -89, 3216, -1462, 0 } }, { -4107, 0, 249, 0 }, { 0, 0, 4096, 0 }, 3528, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6432, -1600, -2386, 0 }, { { -55, -3216, -1476, 0 }, { 56, -3216, 1477, 0 }, { -55, 3216, -1476, 0 }, { 56, 3216, 1477, 0 } }, { 4095, 0, -155, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4720, -1568, -80, 0 }, { { -1059, -3216, -27, 0 }, { 1015, -3216, -27, 0 }, { -1059, 3216, -27, 0 }, { 1015, 3216, -27, 0 } }, { 0, 0, -4107, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4786, -1729, -209, 0 }, { { 1239, -3216, -12, 0 }, { -1257, -3216, -11, 0 }, { 1239, 3216, -12, 0 }, { -1257, 3216, -11, 0 } }, { 1, 0, 4098, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A20, -1600, -416, 0 }, { { -1268, -3216, -168, 0 }, { 1240, -3216, 147, 0 }, { -1268, 3216, -168, 0 }, { 1240, 3216, 147, 0 } }, { 510, 0, -4068, 0 }, { 0, 0, 4096, 0 }, 3453, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2A51, -1536, -544, 0 }, { { 1495, -3216, 197, 0 }, { -1521, -3216, -215, 0 }, { 1495, 3216, 197, 0 }, { -1521, 3216, -215, 0 } }, { -555, 0, 4061, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2930, -1600, -3297, 0 }, { { 0, -3232, -832, 0 }, { 0, -3232, 832, 0 }, { 0, 3232, -832, 0 }, { 0, 3232, 832, 0 } }, { 4099, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x2981, -1472, -3312, 0 }, { { 0, -3232, 912, 0 }, { 0, -3232, -912, 0 }, { 0, 3232, 912, 0 }, { 0, 3232, -912, 0 } }, { -4104, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4431, -1536, -2401, 0 }, { { -685, -3216, 1727, 0 }, { 685, -3216, -1727, 0 }, { -685, 3216, 1727, 0 }, { 685, 3216, -1727, 0 } }, { -3821, 0, -1516, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 15, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4320, -1504, -2400, 0 }, { { 685, -3216, -1727, 0 }, { -685, -3216, 1727, 0 }, { 685, 3216, -1727, 0 }, { -685, 3216, 1727, 0 } }, { 3819, 0, 1515, 0 }, { 0, 0, 4096, 0 }, 3709, 0, 5, 15, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_shelter_b2_laboratory_80185D84[19] = {
    { NULL, NULL, NULL, { 0, -48, 1920, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_WARP, 33, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6384, -48, 2976, 0 }, { { 560, 0, -544, 0 }, { 560, 0, 544, 0 }, { -560, 0, -544, 0 }, { -560, 0, 544, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_WARP, 29, 35, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3489, -64, -3680, 0 }, { { -1056, 0, -224, 0 }, { 960, 0, -224, 0 }, { -1056, 0, 1024, 0 }, { 960, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1470, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3296, -64, -816, 0 }, { { -1120, 0, -1808, 0 }, { 1120, 0, -1808, 0 }, { -1120, 0, 176, 0 }, { 1120, 0, 176, 0 } }, { 0, 4119, 0, 0 }, { 4096, 0, 0, 0 }, 2126, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 432, -64, 4112, 0 }, { { -656, 0, -1104, 0 }, { 432, 0, -1104, 0 }, { -432, 0, 1104, 0 }, { 656, 0, 1104, 0 } }, { 0, 4115, 0, 0 }, { 4096, 0, 0, 0 }, 1280, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2288, -64, 4800, 0 }, { { -2544, 0, -464, 0 }, { 2544, 0, -464, 0 }, { -2544, 0, 464, 0 }, { 2544, 0, 464, 0 } }, { 0, 4095, 0, 0 }, { 200, 0, -4091, 0 }, 2585, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8464, -64, 2336, 0 }, { { -1344, 0, -336, 0 }, { 1344, 0, -336, 0 }, { -1344, 0, 336, 0 }, { 1344, 0, 336, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, -4091, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7024, -64, 320, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { -201, 0, 4090, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2848, -64, 320, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { -402, 0, 4076, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7392, -64, -1568, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x29A0, -1491, 3072, 0 }, { { -736, 0, -336, 0 }, { 736, 0, -336, 0 }, { -736, 0, 336, 0 }, { 736, 0, 336, 0 } }, { 0, 4102, 0, 0 }, { -201, 0, -4091, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1491, 2592, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B40, -1491, 1152, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B20, -1504, -224, 0 }, { { -416, 0, -464, 0 }, { 416, 0, -464, 0 }, { -416, 0, 464, 0 }, { 416, 0, 464, 0 } }, { 0, 4096, 0, 0 }, { -4091, 0, 201, 0 }, 620, WORLD_COLLISION_TRIGGER_ACTION_CAP, 13, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3120, -1491, -3136, 0 }, { { -416, 0, -1072, 0 }, { 416, 0, -1072, 0 }, { -416, 0, 1072, 0 }, { 416, 0, 1072, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3423, -64, 4543, 0 }, { { -308, 0, 1294, 0 }, { -1474, 0, -102, 0 }, { 1475, 0, 103, 0 }, { 309, 0, -1293, 0 } }, { 0, 4099, 0, 0 }, { 4091, 0, -202, 0 }, 1476, WORLD_COLLISION_TRIGGER_ACTION_CAP, 27, 1, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -64, 896, 0 }, { { -1104, 0, -336, 0 }, { 1104, 0, -336, 0 }, { -1104, 0, 336, 0 }, { 1104, 0, 336, 0 } }, { 0, 4110, 0, 0 }, { 798, 0, 4017, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4800, -64, 2784, 0 }, { { -1008, 0, -112, 0 }, { 1008, 0, -560, 0 }, { -1008, 0, 560, 0 }, { 1008, 0, 112, 0 } }, { 0, 4101, 0, 0 }, { -798, 0, -4017, 0 }, 1152, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3248, -64, -1952, 0 }, { { -768, 0, -672, 0 }, { 768, 0, -672, 0 }, { -768, 0, 672, 0 }, { 768, 0, 672, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1019, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b2_laboratory_80186328[2] = {
    { 101, 430, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_801350B0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_laboratory_80186340[2] = {
    { 101, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b2_laboratory_80186360[11] = {
    { NULL, NULL },
    { D_shelter_b2_laboratory_80186340, D_shelter_b2_laboratory_80186328 },
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

WorldCoordRoomAmbientEntry D_shelter_b2_laboratory_801863B8[16] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b2_laboratory_801863B8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 952, 944, 999, 953 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b2_laboratory_80186438 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionFootstepSounds D_shelter_b2_laboratory_80186444 = {
    0x10000061,
    0x10000063,
    0x10000061,
};

WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186450[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186458[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_laboratory_80186438 },
};

WorldCollisionSurfaceProperties D_shelter_b2_laboratory_80186460[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_laboratory_80186444 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_laboratory_80186468[8] = {
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186458,
    D_shelter_b2_laboratory_80186460,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
    D_shelter_b2_laboratory_80186450,
};

AreaApplyRec D_shelter_b2_laboratory_80186488[5] = {
    { 4, 8, 3, 1 },
    { 4, 12, 3, 1 },
    { 4, 27, 3, 1 },
    { 3, 21, 12, 0 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_shelter_b2_laboratory_8018649C[2] = {
    { 4, 47, 2, 0 },
    { 255, 0, 0, 0 },
};

RoomCutsceneSoundTaskStorage gRoomCutsceneSoundTask = { NULL, { 0 } };

RoomEventMsg gRoomEventMsg = { 0, 0, 0, 0, 0, 0 };

u8 gRoomEventActive = 0;

s32 D_shelter_b2_laboratory_801864B8 = 0;

RoomCutsceneRecStorage D_shelter_b2_laboratory_801864BC = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GfxCoord D_shelter_b2_laboratory_801864DC = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

RoomEventReq gRoomEventReq = { 0, 0, 0, 0, 0, 0 };

u16 D_shelter_b2_laboratory_80186540;

#include "../../shared/telephone.inc.c"

void func_shelter_b2_laboratory_8017EAB4(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

s32 func_shelter_b2_laboratory_8017FD18(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 4) {
        D_shelter_b2_laboratory_801864B8 = 0;
        if (GameFlag_GetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == 2) {
            func_shelter_b2_laboratory_801820F4(0);
            GameFlag_SetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, 3);
            GameFlag_SetNibble(GAME_FLAG_B1_POD_TUNNEL_R47_DOOR_UNLOCKED, 1);
            GameFlag_SetNibble(GAME_FLAG_MAP_MARK_B2_LABORATORY, 0);
            GameFlag_SetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_017, 1);
            GameFlag_SetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_018, 1);
            if (GameFlag_GetNibble(GAME_FLAG_083) != 0) {
                func_800E3FAC(0xA2, 0x28);
            } else {
                func_800E3FAC(0xA2, 0x29);
            }
            D_shelter_b2_laboratory_801864BC.rec.view            = 0xD;
            D_shelter_b2_laboratory_801864BC.rec.capSlot         = 4;
            D_shelter_b2_laboratory_801864BC.rec.capFile         = 3;
            D_shelter_b2_laboratory_801864BC.rec.skipScene       = 1;
            D_shelter_b2_laboratory_801864BC.rec.capTPageX       = 0x180;
            D_shelter_b2_laboratory_801864BC.rec.capTPageY       = 0x100;
            D_shelter_b2_laboratory_801864BC.rec.startSound      = 0x541F0005;
            D_shelter_b2_laboratory_801864BC.rec.endSound        = 0x541F0008;
            D_shelter_b2_laboratory_801864BC.rec.sceneSound      = 0x541F0006;
            D_shelter_b2_laboratory_801864BC.rec.afterSceneSound = 0x541F0007;
            Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 1, &D_shelter_b2_laboratory_801864BC.rec);
            Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 2, 0, 0);
        } else {
            D_shelter_b2_laboratory_801864BC.rec.view            = 0xD;
            D_shelter_b2_laboratory_801864BC.rec.capSlot         = 1;
            D_shelter_b2_laboratory_801864BC.rec.capFile         = 2;
            D_shelter_b2_laboratory_801864BC.rec.skipScene       = 0;
            D_shelter_b2_laboratory_801864BC.rec.capTPageX       = 0;
            D_shelter_b2_laboratory_801864BC.rec.startSound      = 0x541F0005;
            D_shelter_b2_laboratory_801864BC.rec.endSound        = 0x541F0008;
            D_shelter_b2_laboratory_801864BC.rec.sceneSound      = 0x541F0006;
            D_shelter_b2_laboratory_801864BC.rec.afterSceneSound = 0x541F0007;
            Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, 8, &D_shelter_b2_laboratory_801864BC.rec);
        }
    }
    return 0;
}

/// States of the room's message task, run by
/// `func_shelter_b2_laboratory_801804A4`: install the message table, idle, die.
static const TaskFuncTable3 D_shelter_b2_laboratory_8017D6BC = {
    {
        func_shelter_b2_laboratory_80180450,
        func_shelter_b2_laboratory_80180494,
        taskKill,
    },
};

void func_shelter_b2_laboratory_8017FEB8(Task* arg0)
{
    s8  pan;
    s8  depth;
    s32 vol;

    D_shelter_b2_laboratory_801864DC.coord.t[0]   = 0xC1C;
    D_shelter_b2_laboratory_801864DC.coord.t[1]   = -0x5DC;
    D_shelter_b2_laboratory_801864DC.coord.t[2]   = -0xC80;
    D_shelter_b2_laboratory_801864DC.parent       = &gGfxViewCoord;
    D_shelter_b2_laboratory_801864DC.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&D_shelter_b2_laboratory_801864DC);
    pan   = worldCoordGetOriginAudioPan(&D_shelter_b2_laboratory_801864DC);
    depth = worldCoordGetOriginAudioDepth(&D_shelter_b2_laboratory_801864DC);
    switch (arg0->state) {
        case 0:
            SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x0E), pan, depth);
            arg0->state++;
            break;
        case 1:
            if (D_shelter_b2_laboratory_801864B8 == 0) {
                SndEvt_EnqueueType7(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x0E), 1);
                taskKill(arg0);
                return;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != gGameSession->location.loc.view) {
                arg0->state++;
            }
            break;
        case 2:
        case 3:
        case 4:
            arg0->state++;
            break;
        case 5:
            vol = 0x7F - D_shelter_b2_laboratory_80182A90[gGameSession->location.loc.view] * 0x7F / 100;
            if (vol >= 0x80) {
                vol = 0x7F;
            }
            SndEvt_EnqueueTypeA(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x0E), pan, (s8)vol);
            arg0->state = 1;
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

s32 func_shelter_b2_laboratory_801800F4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_shelter_b2_laboratory_801800FC(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (GameFlag_GetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) == 2) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(5);
        }
        return 2;
    }
    if (in->areaId != GAME_AREA_SHELTER_B2_MAIN_CORRIDOR) {
        return 1;
    }
    req.capCmd        = 1;
    req.missingCapCmd = 1;
    req.firstSnd      = 0x541F0014;
    req.secondSnd     = 0x541F0003;
    req.flagId        = GAME_FLAG_B2_LABORATORY_DOOR_UNLOCKED;
    req.collectedBit  = 0;
    return roomEventGate(&req, out);
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b2_laboratory_80182A38`: the
/// directed action on the laboratory console (`actionId` 1). Runs the scripted
/// scene once, then replays cap script `6` on later visits.
s32 func_shelter_b2_laboratory_801801D0(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 1) {
        if (GameFlag_GetNibble(GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE) != 0) {
            if (GameFlag_GetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS) < 2) {
                Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 0, 0, 0);
            } else {
                Gp_RunCapCmd1(6);
            }
        } else {
            Gp_RunCapCmd1(0x1E);
            GameFlag_SetNibble(GAME_FLAG_LABORATORY_CONSOLE_FIRST_USE, 1);
        }
    }
    return 0;
}

s32 func_shelter_b2_laboratory_8018025C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 0x63) {
        SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B2_LABORATORY, 0x17), 0, 0);
    }
    return 0;
}

void func_shelter_b2_laboratory_80180290(Task* task)
{
    s32 result;

    switch (task->state) {
        case 0:
            D_shelter_b2_laboratory_80182A68 = Task_SpawnFromTable(&D_80134564, 0, 0, 0);
            task->state                     += 1;
            return;
        case 1:
            if (Task_PollKill(D_shelter_b2_laboratory_80182A68, &result) != 0) {
                D_shelter_b2_laboratory_80182A68 = NULL;
                if (result != 0) {
                    GameFlag_SetNibble(GAME_FLAG_SHELTER_B2_LABORATORY_PROGRESS, 1);
                } else {
                    Gp_MsgPlayerWeapon(1);
                }
                taskKill(task);
            }
            return;
    }
}

void func_shelter_b2_laboratory_80180350(Task* task)
{
    switch (task->state) {
        case 0:
            if (gGameSession->location.loc.view == 0xD) {
                task->state = 1;
            }
            return;
        case 1:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0xE;
            gStageMusicParams.fadeOutTicks                      = 0;
            gStageMusicParams.field_2                           = 0;
            Task_SpawnFromTable(&Stage_MusicTaskDesc, 0, 0, 0);
            task->state++;
            return;
        case 2:
            if (gGameSession->eventState == 0) {
                Gp_RunCapCmd1(GameFlag_GetNibble(GAME_FLAG_083) != 0 ? 0x24 : 0x23);
                taskKill(task);
            }
            return;
    }
}

/// Installs `D_shelter_b2_laboratory_80182A38` as the task's message table,
/// registers the task in pointer slot 7 and steps it on one state.
static void func_shelter_b2_laboratory_80180450(Task* task)
{
    task->msgTable = D_shelter_b2_laboratory_80182A38;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room's message task: does nothing. The unused local
/// reproduces the original's stack frame.
static void func_shelter_b2_laboratory_80180494(Task* task)
{
    char pad[0x10];
}

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b2_laboratory_8017D6BC`.
void func_shelter_b2_laboratory_801804A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_laboratory_8017D6BC;
    sp.funcs[task->state](task);
}

void func_shelter_b2_laboratory_801804FC(void)
{
    if (D_shelter_b2_laboratory_801864B8 == 0) {
        D_shelter_b2_laboratory_801864B8 = 1;
        func_shelter_b2_laboratory_801820F4(1);
        Task_SpawnFromTable(D_shelter_b2_laboratory_80182A6C, 1, 0, 0);
    }
}

void func_shelter_b2_laboratory_80180548(Task* task)
{
    if (task->state == 0) {
        D_shelter_b2_laboratory_80186540 = 0;
        task->state                      = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            break;
        case 3:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x444);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[2], 0x180, 0x444);
            break;
        case 4:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[24], 0x200, 0x444);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[28], 0x200, 0x241);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], 0x200, 0x124);
            break;
        case 5:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[0], 0x180, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[8], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[10], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[12], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[14], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x60, 0x80);
            }
            break;
        case 6:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[16], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[18], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[20], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[22], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[24], 0x180, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], 0x200, 0x124);
            break;
        case 7:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[24], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], 0x200, 0x124);
            break;
        case 8:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[16], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[18], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[36], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[38], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[40], 0x200, 0x124);
            break;
        case 9:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            break;
        case 10:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[2], 0x180, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[4], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[6], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[28], 0x200, 0x241);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x60, 0x80);
            }
            break;
        case 12:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[12], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[14], 0x180, 0x333);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x60, 0x80);
            }
            break;
        case 13:
            if (D_shelter_b2_laboratory_80186540 != 0) {
                glowDrawPulsingDisc(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                glowDrawPulsingDisc(&D_shelter_b2_laboratory_80182AA0[44], 0x60, 0x80);
            }
            break;
        case 15:
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[26], 0x200, 0x241);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[30], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[32], 0x200, 0x222);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[34], 0x200, 0x124);
            glowDrawCapsule(&D_shelter_b2_laboratory_80182AA0[42], 0x180, 0x124);
            if (D_shelter_b2_laboratory_80186540 != 0) {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x180, 0x80);
            } else {
                func_shelter_b2_laboratory_801812F8(&D_shelter_b2_laboratory_80182AA0[44], 0x60, 0x80);
            }
            break;
    }
}

#include "../../shared/glow_draw_capsule.inc.c"

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues two gouraud `POLY_G4` diamonds and two
/// gouraud `LINE_G3` diagonals around the projected centre, with an on-screen
/// radius of `(s16)arg2 * 32 / otz`. The lit vertex pulses on green and blue at
/// `rsin(animFrame * (s16)arg1) / 34 + 0x78`.
static void func_shelter_b2_laboratory_801812F8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                                     head;
    _ShelterB2LaboratoryGlowDiamondScratch* block;
    POLY_G4*                                prim;
    LINE_G3*                                line;
    s32                                     sine;
    s32                                     pulse;
    s32                                     radius;
    s32                                     i;
    s32                                     t1;
    s32                                     t2;
    s32                                     twice;
    u16                                     sx;
    u16                                     sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(_ShelterB2LaboratoryGlowDiamondScratch));
        block   = (_ShelterB2LaboratoryGlowDiamondScratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((_ShelterB2LaboratoryGlowDiamondScratch*)head)[-1].sx);
    gte_stflg(&((_ShelterB2LaboratoryGlowDiamondScratch*)head)[-1].flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        sine          = rsin(gDisplayState.animFrame * (s16)arg1);
        radius        = ((s16)arg2 * 32) / ((_ShelterB2LaboratoryGlowDiamondScratch*)head)[-1].otz;
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
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
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
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB2LaboratoryGlowDiamondScratch);
}

#include "../../shared/glow_draw_pulsing_disc.inc.c"

static void func_shelter_b2_laboratory_801820F4(s16 arg0)
{
    D_shelter_b2_laboratory_80186540 = arg0;
}
