#include "rooms/shelter_b6_nursery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
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
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/acropolis_square.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_cutscene.h"

/// Cues the room's cutscenes leave for its view-effect task.
///
/// The task clears both when it starts, and again once it has fired the spark
/// shower, so the shower plays once per cue and takes the pulse cue with it.
typedef struct {
    u16 fastGlintPulse;   // non-zero: the glints of views 3, 6, 8 and 10 pulse at four times their idle rate
    u16 sparkShowerScale; // non-zero: view 13 throws sixteen spark-shower shards, their spread and size multiplied by this
} _ShelterB6NurseryEffectCues;
STATIC_ASSERT_SIZEOF(_ShelterB6NurseryEffectCues, 0x4);

/// Scratch-stack block one spark-shower shard is drawn from.
///
/// Each corner is staged in `corners` as a point of the shard's own plane and
/// replaced in place by its world position, narrowed to signed 16-bit
/// coordinate units. One RTPT then projects the three together; the screen
/// positions go straight into the packet, so the block keeps none of them.
///
/// Reserve the whole block and release it before the drawer returns.
typedef struct {
    SVECTOR corners[3];      // Local corner workspace, then the world positions supplied to the projection
    s32     otz;             // SZ3 / 4 of the projection, a quarter of the last corner's depth; ordering-table and blend depth
    s32     projectionFlags; // GTE FLAG word of the projection; bit 31 set drops the triangle
} _ShelterB6NurseryTriScratch;
STATIC_ASSERT_SIZEOF(_ShelterB6NurseryTriScratch, 0x20);

s32 rsin(s32);
s32 rcos(s32);

extern void func_actor_450800_80131E2C(void);
extern void func_actor_450800_80132000(void);
extern void func_actor_450800_80132028(void);

extern UiObjectDesc D_800611E4;

extern EvsCommand D_actor_450800_80139964[];
extern EvsCommand D_actor_450800_8013A33C[];
extern EvsCommand D_actor_450800_8013A84C[];
// Script in the companion actor slot; this address also holds a task table
// when a different actor package is loaded.
extern EvsCommand D_actor_450800_8013A8DC[];
extern EvsCommand D_actor_450800_8013AF8C[];
extern EvsCommand D_actor_450800_8013BA84[];

/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` (ally present). A distinct symbol so the restore
/// path does not share the `gMcSaveData` address with case 0.

/// View saved when the cutscene starts and restored when it ends.

/// Row labels of the play-data statistics list, one per row.
static u8 Telephone_Data_80181A20[];
static u8 Telephone_Data_80181A50[];
static u8 Telephone_Data_80181A28[];
static u8 Telephone_Data_80181A2C[];
static u8 Telephone_Data_80181A34[];
static u8 Telephone_Data_80181A40[];
static u8 Telephone_Data_80181A58[];
static u8 Telephone_Data_80181A60[];
static u8 Telephone_Data_80181A68[];

/// Suffix appended to the plain counts.
static u8 Telephone_Data_80181A70[];

/// Suffix appended to the percentages.
static u8 Telephone_Data_80181A78[];

/// Help lines shown while the matching row is selected.
static u8 Telephone_Data_80181A7C[];
static u8 Telephone_Data_80181AA8[];
static u8 Telephone_Data_80181ACC[];
static u8 Telephone_Data_80181AFC[];
static u8 Telephone_Data_80181B30[];
static u8 Telephone_Data_80181B64[];
static u8 Telephone_Data_80181B9C[];
static u8 Telephone_Data_80181BD0[];
static u8 Telephone_Data_80181C08[];

/// Lines of the four confirm prompts, and the panels two of them open.
static u8           Telephone_Data_801819F8[];
static u8           Telephone_Data_80181A00[];
static UiObjectDesc Telephone_Data_80181CAC;
static u8           Telephone_Data_80181A0C[];
static u8           Telephone_Data_80181A18[];
static UiObjectDesc Telephone_Data_80181CC8;

/// The play-data menu's list, the usage list, the child-panel descriptor both
/// spawn, and the telephone menu's list.
static UiList       Telephone_Data_80181C44;
static UiList       Telephone_Data_80181C6C;
static UiObjectDesc Telephone_Data_80181C90;
static UiList       Telephone_Data_80181CF4;

/// Telephone menu title, including retained bytes after its terminator.
static const char Telephone_Data_8017D638[];

/// Task tables: the cutscene and its sound task; the ambient sound task.
extern TaskDesc gRoomCutsceneTaskDescs[];
extern TaskDesc D_shelter_b6_nursery_80185000;

/// The room's message table.
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_shelter_b6_nursery_8018500C[5];

/// Per-view depth override for the ambient sound (-1 keeps the computed one).
extern s8 D_shelter_b6_nursery_80185034[];

/// Anchor points of the room's glints and effects.

/// The two ends of the trail effect, relative to its parent coordinate.

/// The cutscene's running sound task.
extern Task* gRoomCutsceneSoundTask;

/// Non-zero while the ambient sound task runs.
extern s32 D_shelter_b6_nursery_8018797C;

extern RoomCutsceneRecStorage D_shelter_b6_nursery_80187980;

/// Position the ambient sound is panned and attenuated from.
extern GfxCoord D_shelter_b6_nursery_801879A0;

extern _ShelterB6NurseryEffectCues D_shelter_b6_nursery_801879F0;

#define TELEPHONE_TITLE_BYTES "Telephone\0\x1FQ"
#include "../../shared/telephone.h"
#include "../../shared/sprite_quad.h"

static void func_shelter_b6_nursery_8017FEC4(Task* task);
static void _shelterB6NurseryMessageIdle(Task* task);
static void _shelterB6NurseryDrawParticleFrame(const GfxCoord* coord, u16 animationFrame, s16 halfDiagonal, s16 rotation);
static void _shelterB6NurseryDrawSparkShowerShard(const GfxCoord* coord, s16 radius, s16 shade);

void func_shelter_b6_nursery_8017FBC0(Task*);

#include "../../shared/telephone_data.inc.c"

TaskDesc gRoomCutsceneTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, roomCutsceneSoundTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc D_shelter_b6_nursery_80185000 = { { { TASK_BODY_NONE, 32 } }, func_shelter_b6_nursery_8017FBC0, { .value = 0 } };

s32 func_shelter_b6_nursery_8017FA54(Task*, s32, s32, s32);
/// Room message carrying an inventory key-item ID in its first argument word.
enum { SHELTER_B6_NURSERY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

static s32 _shelterB6NurseryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
s32        func_shelter_b6_nursery_8017FDD4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_shelter_b6_nursery_8017FE3C(Task* task, s32 msgId, DirectionActionRequest* msg, s32);

TaskMessageEntry D_shelter_b6_nursery_8018500C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b6_nursery_8017FDD4 },
    { SHELTER_B6_NURSERY_MESSAGE_USE_KEY_ITEM, _shelterB6NurseryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b6_nursery_8017FE3C },
    { ROOM_MESSAGE_COMMAND, func_shelter_b6_nursery_8017FA54 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s8 D_shelter_b6_nursery_80185034[24] = {
    -1,
    -1,
    25,
    0,
    25,
    64,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    0,
    0,
};

SVECTOR D_shelter_b6_nursery_8018504C[7] = {
    { 6170, -80, -860, 0 },
    { 3000, -1000, 0, 0 },
    { 5500, 0, 2500, 0 },
    { 6000, 0, 0, 0 },
    { 4000, 0, -1000, 0 },
    { 2000, 0, 0, 0 },
    { 7000, -1500, 2000, 0 },
};

static TmdBone _gShelterB6NurseryModel07BACSkeleton[1] = {
#include "assets/shelter_b6_nursery_model_07BAC_skeleton.inc"
};

static u32 _gShelterB6NurseryModel07BACPartVerts[1] = {
#include "assets/shelter_b6_nursery_model_07BAC_partVerts.inc"
};

static SVECTOR _gShelterB6NurseryModel07BACVerts[12] = {
#include "assets/shelter_b6_nursery_model_07BAC_verts.inc"
};

static SVECTOR _gShelterB6NurseryModel07BACNormals[12] = {
#include "assets/shelter_b6_nursery_model_07BAC_normals.inc"
};

static u32 _gShelterB6NurseryModel07BACStream[89] = {
#include "assets/shelter_b6_nursery_model_07BAC_stream.inc"
};

TmdSource gShelterB6NurseryModel07BAC = {
    0,
    576,
    0,
    1,
    _gShelterB6NurseryModel07BACPartVerts,
    _gShelterB6NurseryModel07BACVerts,
    _gShelterB6NurseryModel07BACNormals,
    _gShelterB6NurseryModel07BACSkeleton,
    _gShelterB6NurseryModel07BACStream,
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b6_nursery_80185304[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b6_nursery_80185308[1] = { 19 };

DirectionWarpEntry D_shelter_b6_nursery_8018530C[2] = {
    { { { .word = 1024 }, 680, 0, 0 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 680, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 0x4CF4, 0, 3600 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 0x4CF4, 0, 3600 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gShelterB6NurseryCollision082E0Normals[14] = {
#include "assets/shelter_b6_nursery_collision_082E0_normals.inc"
};

static SVECTOR _gShelterB6NurseryCollision082E0Verts[54] = {
#include "assets/shelter_b6_nursery_collision_082E0_verts.inc"
};

static WorldCollisionGridFace _gShelterB6NurseryCollision082E0Faces[43] = {
#include "assets/shelter_b6_nursery_collision_082E0_faces.inc"
};

static s16 _gShelterB6NurseryCollision082E0Cells[120] = {
#include "assets/shelter_b6_nursery_collision_082E0_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB6NurseryCollision082E0Cells[i])
static s16* _gShelterB6NurseryCollision082E0Table[4] = {
#include "assets/shelter_b6_nursery_collision_082E0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b6_nursery_801858A0 = { NULL, _gShelterB6NurseryCollision082E0Normals, _gShelterB6NurseryCollision082E0Verts, _gShelterB6NurseryCollision082E0Faces, _gShelterB6NurseryCollision082E0Table, -50, 1450, 2, 2, 4000, 43 };

ViewCamera D_shelter_b6_nursery_801858C4[19] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3500, 0x61A8, -2500 } }, 447 },
    { { { { 540, 0, 4060 }, { -792, 4017, 105 }, { -3982, -799, 530 } }, { -6040, 500, 440 } }, 230 },
    { { { { 668, 0, -4041 }, { -249, 4088, -41 }, { 4033, 252, 667 } }, { -940, 1450, 570 } }, 257 },
    { { { { 3940, 0, 1118 }, { 170, 4048, -601 }, { -1105, 624, 3894 } }, { -6140, 1650, 90 } }, 230 },
    { { { { 4042, 0, 659 }, { 298, 3654, -1825 }, { -588, 1850, 3606 } }, { -6090, 2850, -2510 } }, 230 },
    { { { { -364, 0, -4079 }, { -2502, 3234, 223 }, { 3222, 2512, -287 } }, { -5469, 711, 782 } }, 680 },
    { { { { 3615, 0, -1925 }, { 0, 4096, 0 }, { 1925, 0, 3615 } }, { -4740, 870, -450 } }, 329 },
    { { { { -3171, 0, -2592 }, { -979, 3792, 1197 }, { 2400, 1547, -2936 } }, { -5280, 1300, -2310 } }, 329 },
    { { { { 3044, 0, -2740 }, { -2083, 2660, -2314 }, { 1779, 3114, 1977 } }, { -6030, 1220, -1570 } }, 447 },
    { { { { 4020, 0, -781 }, { 149, 4019, 771 }, { 766, -785, 3946 } }, { -6050, 100, 1490 } }, 329 },
    { { { { 1075, 0, 3952 }, { -297, 4084, 80 }, { -3941, -308, 1072 } }, { -7400, 780, 60 } }, 289 },
    { { { { 3641, 0, 1875 }, { -8, 4095, 16 }, { -1875, -18, 3641 } }, { -5890, 1100, 1170 } }, 289 },
    { { { { 4074, 0, 422 }, { -44, 4072, 433 }, { -419, -435, 4051 } }, { -6360, 760, 1430 } }, 257 },
    { { { { 4073, 0, -430 }, { -20, 4091, -198 }, { 429, 199, 4068 } }, { -5560, 1280, 1550 } }, 230 },
    { { { { -182, 0, 4091 }, { 676, 4039, 30 }, { -4035, 677, -179 } }, { -3100, 1350, -80 } }, 257 },
    { { { { 3990, 0, 924 }, { 25, 4094, -109 }, { -923, 112, 3988 } }, { -6560, 1210, 780 } }, 257 },
    { { { { -3964, 0, 1029 }, { 55, 4090, 211 }, { -1027, 219, -3958 } }, { -5890, 1130, -2310 } }, 257 },
    { { { { -3641, 0, 1875 }, { 0, 4096, 0 }, { -1875, 0, -3641 } }, { -5860, 1460, -980 } }, 329 },
    { { { { 2356, 0, 3349 }, { -350, 4073, 246 }, { -3331, -428, 2344 } }, { -6700, 780, 110 } }, 289 },
};

SpriteBatch D_shelter_b6_nursery_80185B70[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80185B80[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_80185B90[24] = {
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -120, 760, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -88, 758, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, -48, 750, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -16, 775, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, -16, 762, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 24, 775, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -136, 24, 757, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, 64, 787, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 64, 758, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 96, 759, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -120, 717, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -88, 721, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -48, 720, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -16, 721, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 24, 723, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, 64, 724, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 96, 726, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -120, 673, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 693, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 676, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -16, 677, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 24, 679, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 64, 680, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 96, 682, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_80185D70[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_80185D88[44] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 16, 836, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 16, 787, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 16, 806, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 16, 793, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 8, 806, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 16, 777, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 40, 800, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 48, 688, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 56, 656, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 64, 637, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 64, 637, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 72, 547, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 72, 554, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 80, 535, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 80, 510, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 88, 476, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 32, 88, 428, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 96, 434, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 104, 438, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 32, 800, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 32, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 48, 675, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 64, 514, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 80, 493, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 96, 424, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 427, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 96, 415, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 32, 803, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 48, 664, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 64, 491, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 80, 492, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 96, 403, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 32, 785, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 24, 773, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 48, 642, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 849, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, -48, 917, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, -48, 913, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -32, 915, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -32, 846, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -24, 884, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, -8, 897, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 8, 792, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 24, 799, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_801860F8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 35, 0, 0, { 1, 0 } },
    { 35, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_80186118[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 48, 381, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 326, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 475, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 48, 396, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 64, 399, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 72, 432, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 80, 404, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 56, 374, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 72, 377, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 392, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 400, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 96, 476, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 64, 354, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 80, 360, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 96, 363, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 80, 332, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 96, 346, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_8018626C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80186284[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80186294[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_801862A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_801862B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_801862C4[38] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 32, 129, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 154, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 142, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 24, 135, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 24, 132, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 40, 150, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, 40, 122, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, 40, 150, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 48, 151, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, 48, 153, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 64, 150, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 8, 64, 150, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, 64, 150, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 64, 150, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 64, 121, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 64, 139, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -24, 88, 150, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, 88, 150, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 40, 88, 137, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 72, 88, 122, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 104, 88, 128, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 0, 739, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -8, 745, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, -8, 752, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -8, 759, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -8, 766, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 8, 732, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 8, 742, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 8, 752, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 8, 763, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -8, 774, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 32, 722, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 56, 713, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 32, 732, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 32, 742, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -24, 32, 752, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 56, 700, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 56, 1163, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_801865BC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 1, 0 } },
    { 21, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_801865DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_801865EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_801865FC[12] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -16, 511, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -120, 536, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -96, 681, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -72, 861, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -48, 1000, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, -24, 1000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -80, 766, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, -64, 905, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, -48, 1123, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, -24, 1130, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 0, 977, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 0, 696, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_801866EC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80186704[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_80186714[51] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 664, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 667, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 668, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 670, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -120, 739, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 762, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 785, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 809, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -120, 737, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 758, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 784, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 24, 809, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -120, 731, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -96, 740, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -72, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -48, 762, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -24, 773, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 0, 786, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 24, 798, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 48, 812, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, -16, 787, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 0, 789, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 24, 812, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 48, 804, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -120, 724, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -104, 726, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -120, 722, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, -104, 724, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 720, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -104, 723, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -120, 718, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -104, 721, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 56, 810, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 56, 808, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 56, 805, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 718, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -96, 741, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -72, 752, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -48, 763, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, -24, 775, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 0, 787, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 24, 800, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 48, 794, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, -120, 717, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 48, -96, 729, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -72, 737, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -48, 750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -24, 761, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 0, 773, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 24, 784, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 48, 799, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_80186B10[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 51, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b6_nursery_80186B28[56] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -88, 318, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -72, 319, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -56, 320, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -40, 319, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, -24, 320, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 0, 490, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 0, 525, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -8, 320, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -96, 307, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -72, 308, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -48, 309, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -24, 310, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 0, 318, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -120, 269, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -88, 268, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -56, 268, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -24, 271, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 8, 269, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 40, 268, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 72, 268, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 144, 24, 299, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 56, 295, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 16, 360, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 40, 327, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 16, 400, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 40, 358, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 16, 475, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 40, 475, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -120, 307, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, -120, 479, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, -120, 436, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, -104, 517, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -120, 454, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, -96, 573, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -120, 462, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -96, 606, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -72, 645, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -48, 658, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, -24, 651, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 0, 500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 16, 475, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 32, 475, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -120, 543, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -96, 597, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -72, 725, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -48, 722, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -24, 720, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 0, 550, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 24, 539, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -120, 725, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -96, 729, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -72, 873, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -48, 962, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, -24, 957, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 720, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 24, 684, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_nursery_80186F88[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 56, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80186FA0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80186FB0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_nursery_80186FC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b6_nursery_80186FD0[19] = {
    { { .empty = D_shelter_b6_nursery_80185B70 }, D_shelter_b6_nursery_80185B70, NULL },
    { { .empty = D_shelter_b6_nursery_80185B80 }, D_shelter_b6_nursery_80185B80, NULL },
    { { .elements = D_shelter_b6_nursery_80185B90 }, D_shelter_b6_nursery_80185D70, NULL },
    { { .elements = D_shelter_b6_nursery_80185D88 }, D_shelter_b6_nursery_801860F8, NULL },
    { { .elements = D_shelter_b6_nursery_80186118 }, D_shelter_b6_nursery_8018626C, NULL },
    { { .empty = D_shelter_b6_nursery_80186284 }, D_shelter_b6_nursery_80186284, NULL },
    { { .empty = D_shelter_b6_nursery_80186294 }, D_shelter_b6_nursery_80186294, NULL },
    { { .empty = D_shelter_b6_nursery_801862A4 }, D_shelter_b6_nursery_801862A4, NULL },
    { { .empty = D_shelter_b6_nursery_801862B4 }, D_shelter_b6_nursery_801862B4, NULL },
    { { .elements = D_shelter_b6_nursery_801862C4 }, D_shelter_b6_nursery_801865BC, NULL },
    { { .empty = D_shelter_b6_nursery_801865DC }, D_shelter_b6_nursery_801865DC, NULL },
    { { .empty = D_shelter_b6_nursery_801865EC }, D_shelter_b6_nursery_801865EC, NULL },
    { { .elements = D_shelter_b6_nursery_801865FC }, D_shelter_b6_nursery_801866EC, NULL },
    { { .empty = D_shelter_b6_nursery_80186704 }, D_shelter_b6_nursery_80186704, NULL },
    { { .elements = D_shelter_b6_nursery_80186714 }, D_shelter_b6_nursery_80186B10, NULL },
    { { .elements = D_shelter_b6_nursery_80186B28 }, D_shelter_b6_nursery_80186F88, NULL },
    { { .empty = D_shelter_b6_nursery_80186FA0 }, D_shelter_b6_nursery_80186FA0, NULL },
    { { .empty = D_shelter_b6_nursery_80186FB0 }, D_shelter_b6_nursery_80186FB0, NULL },
    { { .empty = D_shelter_b6_nursery_80186FC0 }, D_shelter_b6_nursery_80186FC0, NULL },
};

WorldCoordPointLight D_shelter_b6_nursery_801870B4[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5491, -2000, 5000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5491, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5491, -2000, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3491, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1491, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3112, 3031, 2949 }, { 0, 0 } }, 2000, 2750 },
};

WorldCoordRoomLights D_shelter_b6_nursery_80187294 = { 0, NULL, ARRAY_SIZE(D_shelter_b6_nursery_801870B4), D_shelter_b6_nursery_801870B4, 0, NULL };

WorldCollisionTrigger D_shelter_b6_nursery_801872AC[6] = {
    { NULL, NULL, NULL, { 5616, -1504, 4352, 0 }, { { 2032, -1904, 0, 0 }, { -2032, -1904, 0, 0 }, { 2032, 1904, 0, 0 }, { -2032, 1904, 0, 0 } }, { 0, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2780, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5488, -1505, 4416, 0 }, { { -2000, -1904, 0, 0 }, { 2000, -1904, 0, 0 }, { -2000, 1904, 0, 0 }, { 2000, 1904, 0, 0 } }, { 0, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3936, -1441, 96, 0 }, { { 0, -1904, 2144, 0 }, { 0, -1904, -2144, 0 }, { 0, 1904, 2144, 0 }, { 0, 1904, -2144, 0 } }, { -4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3808, -1601, 128, 0 }, { { 0, -1904, -2256, 0 }, { 0, -1904, 2256, 0 }, { 0, 1904, -2256, 0 }, { 0, 1904, 2256, 0 } }, { 4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2941, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6193, -1440, 2032, 0 }, { { 2959, -1904, 437, 0 }, { -2961, -1904, -440, 0 }, { 2959, 1904, 437, 0 }, { -2961, 1904, -440, 0 } }, { -601, 0, 4052, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6017, -1504, 2081, 0 }, { { -2694, -1904, -404, 0 }, { 2686, -1904, 395, 0 }, { -2694, 1904, -404, 0 }, { 2686, 1904, 395, 0 } }, { 602, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 3318, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_shelter_b6_nursery_80187474[4] = {
    { 101, 508, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_450800_8014AC88 },
    { 20, 358, AREA_RESOURCE_FILE_GROUP_BASE_50, 0, { 0, 0 }, gActor450800PairWalkTasks },
    { 140, 508, AREA_RESOURCE_FILE_GROUP_BASE_60, 3, { 0, 0 }, D_actor_450800_8014AC88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_shelter_b6_nursery_801874A4[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C090, D_shelter_b6_nursery_80187474 },
    { NULL, NULL },
    { NULL, NULL },
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

WorldCollisionTrigger D_shelter_b6_nursery_8018750C[12] = {
    { NULL, NULL, NULL, { 5968, -64, 2416, 0 }, { { -1232, 0, -720, 0 }, { 368, 0, -720, 0 }, { -1232, 0, 688, 0 }, { -240, 0, 1040, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, 4096, 0 }, 1425, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6816, -64, 320, 0 }, { { -1024, 0, -864, 0 }, { 288, 0, -864, 0 }, { -1024, 0, 896, 0 }, { 288, 0, 896, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 1360, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6597, -64, -1143, 0 }, { { -1382, 0, -1023, 0 }, { 1103, 0, 1080, 0 }, { -1359, 0, 584, 0 }, { -730, 0, 1151, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1717, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 512, -48, 16, 0 }, { { -352, 0, -880, 0 }, { 352, 0, -880, 0 }, { -352, 0, 880, 0 }, { 352, 0, 880, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 947, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5392, -64, 1488, 0 }, { { -1872, 0, -528, 0 }, { 1872, 0, -16, 0 }, { -1872, 0, 16, 0 }, { 1872, 0, 528, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1941, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6287, -64, 4127, 0 }, { { -64, 0, -1188, 0 }, { 1154, 0, 296, 0 }, { -1153, 0, -295, 0 }, { 65, 0, 1189, 0 } }, { 0, 4101, 0, 0 }, { -3513, 0, 2106, 0 }, 1187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, -1153, 0 }, { { -1584, 0, -352, 0 }, { 1584, 0, -352, 0 }, { -1584, 0, 352, 0 }, { 1584, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3888, -64, -1632, 0 }, { { -864, 0, 48, 0 }, { 928, 0, 48, 0 }, { -864, 0, 976, 0 }, { 928, 0, 976, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6592, -64, 1728, 0 }, { { -1472, 0, -832, 0 }, { 448, 0, -832, 0 }, { -1472, 0, 256, 0 }, { 448, 0, 256, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1688, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6335, -64, 5535, 0 }, { { -991, 0, 349, 0 }, { 493, 0, -869, 0 }, { -492, 0, 870, 0 }, { 992, 0, -348, 0 } }, { 0, 4112, 0, 0 }, { -2598, 0, -3166, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_CAP, 19, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, 5728, 0 }, { { -960, 0, -1152, 0 }, { 864, 0, -1152, 0 }, { -960, 0, 544, 0 }, { 1792, 0, 544, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 1872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5536, -64, 5440, 0 }, { { -1584, 0, -704, 0 }, { 1584, 0, -704, 0 }, { -1584, 0, 704, 0 }, { 1584, 0, 704, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 1731, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b6_nursery_8018789C[20] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b6_nursery_8018789C) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 1098, 1116, 1256, 1126 } },
    { .color = { 1237, 1258, 1320, 1257 } },
    { .color = { 1601, 1619, 1653, 1616 } },
    { .color = { 1175, 1200, 1260, 1198 } },
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
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b6_nursery_8018793C = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_shelter_b6_nursery_80187948[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b6_nursery_80187950[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b6_nursery_8018793C },
};

WorldCollisionSurfaceProperties* D_shelter_b6_nursery_80187958[8] = {
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187950,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
    D_shelter_b6_nursery_80187948,
};

Task* gRoomCutsceneSoundTask;

s32 D_shelter_b6_nursery_8018797C;

RoomCutsceneRecStorage D_shelter_b6_nursery_80187980;

GfxCoord D_shelter_b6_nursery_801879A0;

_ShelterB6NurseryEffectCues D_shelter_b6_nursery_801879F0;

#include "../../shared/telephone.inc.c"

static void _glowDrawDiamond(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);
static void _glowDrawPulsingDisc(const SVECTOR* worldPoint, s32 pulseRate, s32 radiusScale);

void func_shelter_b6_nursery_8017EAC4(Task* task)
{
    Telephone_MenuTask(task);
}

#include "../../shared/telephone_panels.inc.c"

#undef TELEPHONE_TITLE_BYTES

#include "../../shared/room_cutscene_task.inc.c"

/// States of the room's message task, run by
/// `func_shelter_b6_nursery_8017FF9C`: install the message table, idle, die.
static const TaskFuncTable3 D_shelter_b6_nursery_8017D6A4 = {
    {
        func_shelter_b6_nursery_8017FEC4,
        _shelterB6NurseryMessageIdle,
        taskKill,
    },
};

s32 func_shelter_b6_nursery_8017FA54(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 flag;

    if (arg2 == 0xA) {
        D_shelter_b6_nursery_8018797C                     = 0;
        D_shelter_b6_nursery_80187980.rec.startSound      = 0x55160002;
        D_shelter_b6_nursery_80187980.rec.endSound        = 0x55160005;
        D_shelter_b6_nursery_80187980.rec.sceneSound      = 0x55160003;
        D_shelter_b6_nursery_80187980.rec.afterSceneSound = 0x55160004;
        flag                                              = gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS);
        if (flag == 1) {
            if (gameFlagGetNibble(GAME_FLAG_083) != 0) {
                Gp_SetBit2Flag(0x22, 1, 4);
            }
            func_800E3FAC(0xA2, 0x31);
            gameFlagSetNibble(GAME_FLAG_B6_NURSERY_PROGRESS, 2);
            D_shelter_b6_nursery_80187980.rec.view      = 6;
            D_shelter_b6_nursery_80187980.rec.capSlot   = 0xB;
            D_shelter_b6_nursery_80187980.rec.capFile   = 0;
            D_shelter_b6_nursery_80187980.rec.skipScene = flag;
            taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 0x19,
                               &D_shelter_b6_nursery_80187980.rec);
            func_actor_450800_80132028();
            func_shelter_b6_nursery_80182D14(0, 0);
            return 0;
        }
        if (gameFlagGetNibble(GAME_FLAG_NURSERY_SCENE_SEEN) == 0) {
            Gp_SpawnIfCapIdle(0x17, 0);
            gameFlagSetNibble(GAME_FLAG_NURSERY_SCENE_SEEN, 1);
            return 0;
        }
        D_shelter_b6_nursery_80187980.rec.view      = 6;
        D_shelter_b6_nursery_80187980.rec.capSlot   = 0x16;
        D_shelter_b6_nursery_80187980.rec.capFile   = 0;
        D_shelter_b6_nursery_80187980.rec.skipScene = 0;
        taskSpawnFromTable(gRoomCutsceneTaskDescs, 0, 0xA,
                           &D_shelter_b6_nursery_80187980.rec);
    }
    return 0;
}

void func_shelter_b6_nursery_8017FBC0(Task* arg0)
{
    s32 pan;
    s32 depth;
    s32 viewDepth;

    D_shelter_b6_nursery_801879A0.coord.t[0]   = 0x1770;
    D_shelter_b6_nursery_801879A0.coord.t[1]   = 0;
    D_shelter_b6_nursery_801879A0.coord.t[2]   = -0x33E;
    D_shelter_b6_nursery_801879A0.parent       = &gGfxViewCoord;
    D_shelter_b6_nursery_801879A0.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&D_shelter_b6_nursery_801879A0);
    pan   = worldCoordGetOriginAudioPan(&D_shelter_b6_nursery_801879A0);
    depth = worldCoordGetOriginAudioDepth(&D_shelter_b6_nursery_801879A0);
    switch (arg0->state) {
        case 0:
            sndEvtRequestScriptStart(SOUND_SHELTER_B6_NURSERY_AMBIENCE, (s8)pan, (s8)depth);
            arg0->state++;
            break;
        case 1:
            if (D_shelter_b6_nursery_8018797C == 0) {
                sndEvtRequestScriptStop(SOUND_SHELTER_B6_NURSERY_AMBIENCE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
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
            viewDepth = D_shelter_b6_nursery_80185034[gGameSession->location.loc.view];
            if (viewDepth != -1) {
                depth = viewDepth;
            }
            sndEvtRequestScriptMix(SOUND_SHELTER_B6_NURSERY_AMBIENCE, (s8)pan, (s8)depth);
            arg0->state = 1;
            break;
    }
}

#include "../../shared/room_cutscene_sound_task.inc.c"

/// Refuses every key-item-use request to this room, returning 0 without consuming the item.
static s32 _shelterB6NurseryRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    enum { KEY_ITEM_USE_REFUSED = 0 };

    return KEY_ITEM_USE_REFUSED;
}

s32 func_shelter_b6_nursery_8017FDD4(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    mapNeoArkResolveRoomVariant(src, dst);
    if (src->queryOnly == ROOM_EVENT_EXECUTE) {
        Gp_RunCapCmd1(0xC);
    }
    return 0;
}

s32 func_shelter_b6_nursery_8017FE3C(Task* task, s32 msgId, DirectionActionRequest* msg, s32 arg3)
{
    if (msg->actionId == 1) {
        func_actor_450800_80131E2C();
    }
    if (msg->actionId == 2) {
        func_actor_450800_80132000();
    }
    if (msg->actionId == 3 && gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) != 0) {
        evsStartScriptWithSkip(D_actor_450800_8013AF8C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_450800_8013BA84);
    }
    return 0;
}

static void func_shelter_b6_nursery_8017FEC4(Task* arg0)
{
    arg0->msgTable = D_shelter_b6_nursery_8018500C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    Gp_FillAllyHp();
    if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == 0) {
        gameFlagSetNibble(GAME_FLAG_B6_NURSERY_PROGRESS, 1);
        evsStartScriptWithSkip(D_actor_450800_80139964, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_450800_8013A33C);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        func_800E3FAC(0xA2, 0x30);
    } else if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == 1) {
        evsStartScript(D_actor_450800_8013A84C, EVENT_SCRIPT_HUD_KEEP);
    } else {
        evsStartScript(D_actor_450800_8013A8DC, EVENT_SCRIPT_HUD_KEEP);
    }
    arg0->state++;
}

/// Keeps the room message task alive while it waits for messages.
static void _shelterB6NurseryMessageIdle(Task* task)
{
    // Retain the original idle state's otherwise unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b6_nursery_8017FF9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_nursery_8017D6A4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_nursery_8017FFF4(void)
{
    if (D_shelter_b6_nursery_8018797C == 0) {
        D_shelter_b6_nursery_8018797C = 1;
        taskSpawnFromTable(&D_shelter_b6_nursery_80185000, 0, 0, 0);
    }
}

void shelterB6NurserySetView13SpriteHidden(u8 hidden)
{
    enum { VIEW_INDEX     = 12,
           BATCH_INDEX    = 1,
           SPRITE_VISIBLE = 0,
           SPRITE_HIDDEN  = 1 };

    const GameLocationKey* location = &gGameSession->location.loc;
    SpriteBatch*           view13Batches;

    view13Batches = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1][VIEW_INDEX].batches;
    if (hidden == SPRITE_VISIBLE) {
        view13Batches[BATCH_INDEX].hidden = SPRITE_VISIBLE;
    } else if (hidden == SPRITE_HIDDEN) {
        view13Batches[BATCH_INDEX].hidden = SPRITE_HIDDEN;
    }
}

void func_shelter_b6_nursery_801800A0(Task* task)
{
    SVECTOR* pos;
    u32      a;
    u32      b;
    s32      angle;
    s32      r;
    s32      i;

    if (task->state == 0) {
        gRoomEffectFlashId                             = EFFECT_SHELTER_B6_NURSERY_FLASH;
        gRoomEffectTwinTrailId                         = EFFECT_SHELTER_B6_NURSERY_TWIN_TRAIL;
        gRoomEffectSparkBurstId                        = EFFECT_SHELTER_B6_NURSERY_SPARK_BURST;
        D_shelter_b6_nursery_801879F0.fastGlintPulse   = 0;
        D_shelter_b6_nursery_801879F0.sparkShowerScale = 0;
        task->state                                    = 1;
    }
    switch (viewGetMappedIndex() & 0xFF) {
        case 3:
        case 8:
            if (D_shelter_b6_nursery_801879F0.fastGlintPulse != 0) {
                _glowDrawDiamond(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                _glowDrawDiamond(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 6:
        case 10:
            if (D_shelter_b6_nursery_801879F0.fastGlintPulse != 0) {
                _glowDrawPulsingDisc(D_shelter_b6_nursery_8018504C, 0x180, 0x80);
            } else {
                _glowDrawPulsingDisc(D_shelter_b6_nursery_8018504C, 0x60, 0x80);
            }
            break;
        case 12:
            if (task->state == 1) {
                effectSpawn(EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK, NULL, task->spawnArg1.value, &D_shelter_b6_nursery_8018504C[1]);
                effectSpawn(EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK, NULL, task->spawnArg1.value, &D_shelter_b6_nursery_8018504C[1]);
                effectSpawn(EFFECT_SHELTER_B6_NURSERY_DEBRIS_CHUNK, NULL, task->spawnArg1.value, &D_shelter_b6_nursery_8018504C[1]);
                task->state = 2;
            }
            break;
        case 13:
            task->state = 3;
            if (D_shelter_b6_nursery_801879F0.sparkShowerScale != 0) {
                for (i = 0; i < 16; i++) {
                    a                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    b                                   = a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    angle                               = (a >> 16) & 0xFFF;
                    gRandomLcgState                     = b;
                    r                                   = ((b >> 16) & 0xFF) * D_shelter_b6_nursery_801879F0.sparkShowerScale;
                    D_shelter_b6_nursery_8018504C[6].vx = 0x1C20;
                    D_shelter_b6_nursery_8018504C[6].vy = ((r * rsin(angle)) >> 12) - 0x6D6;
                    D_shelter_b6_nursery_8018504C[6].vz = ((r * rsin(angle)) >> 12) + 0x7D0;
                    gRandomLcgState                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_SHELTER_B6_NURSERY_SPARK_SHOWER, NULL,
                                (((gRandomLcgState >> 16) & 0x1F) + 8) * D_shelter_b6_nursery_801879F0.sparkShowerScale,
                                &D_shelter_b6_nursery_8018504C[6]);
                }
                D_shelter_b6_nursery_801879F0.fastGlintPulse   = 0;
                D_shelter_b6_nursery_801879F0.sparkShowerScale = 0;
            }
            break;
        case 15:
            if (!(gDisplayState.animFrame & 1)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_1A4, NULL, ((gRandomLcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[5]);
            }
            break;
        case 17:
            pos = D_shelter_b6_nursery_8018504C;
            _glowDrawDiamond(pos, 0x60, 0x80);
            if (!(gDisplayState.animFrame & 1)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_1A4, NULL, ((gRandomLcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
        case 18:
            if (!(gDisplayState.animFrame & 1)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_1A4, NULL, ((gRandomLcgState >> 16) & 0x11FF) + 0x2303300, &D_shelter_b6_nursery_8018504C[4]);
            }
            break;
    }
    if (task->state == 3 && !(gDisplayState.animFrame & 1)) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effectSpawn(EFFECT_1A4, NULL, ((gRandomLcgState >> 16) & 0x11FF) + 0x2303300, &(D_shelter_b6_nursery_8018504C + 2)[0]);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effectSpawn(EFFECT_1A4, NULL, ((gRandomLcgState >> 16) & 0x11FF) + 0x2303300, &(D_shelter_b6_nursery_8018504C + 2)[1]);
    }
}

#include "../../shared/glow_draw_diamond.inc.c"

#include "../../shared/glow_draw_pulsing_disc.inc.c"

void func_shelter_b6_nursery_80181314(Task* task)
{
    SVECTOR     step;
    SVECTOR     pos;
    SVECTOR     base;
    TmdObject*  obj;
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    obj   = task->extra.tmd;
    work  = task->spawnArg2.pointer;
    coord = obj->coords;
    if ((viewGetMappedIndex() & 0xFF) != 0xC) {
        effectKillTask(work, task);
        return;
    }
    {
        effectControl = gRoomEffectState->effectControl;
        if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
            if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                effectKillTask(work, task);
            }
        } else {
            actorRenderComposeCoord(coord);
            if (task->state == 0) {
                obj->flags     &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                work->move.vy   = 0;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = ((gRandomLcgState >> 16) & 0x3F) + 0x20;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                VectorNormalSS(&work->move, &work->move);
                work->pos.vy        = 0;
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->pos.vx        = -((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->pos.vz        = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                task->state++;
                return;
            }
            gfxRotMatrixXYZ(&coord->coord, &work->pos, GRAPHICS_ROTATION_COMPOSE);
            MatrixNormal(&coord->coord, &coord->coord);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&step);
            coord->coord.t[0]  += step.vx;
            coord->coord.t[1]  += step.vy;
            coord->coord.t[2]  += step.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&step);
            gte_rtv0();
            gte_stsv(&pos);
            base.vx = coord->workm.t[0];
            base.vy = coord->workm.t[1];
            base.vz = coord->workm.t[2];
            pos.vx += base.vx;
            pos.vy += base.vy;
            pos.vz += base.vz;
            if (worldCollisionProbeGridSegment(&pos, &base, &pos, &base) == 1) {
                coord->coord.t[0] -= step.vx;
                coord->coord.t[1] -= step.vy;
                coord->coord.t[2] -= step.vz;
                work->move.vx      = (base.vx >> 1) + (work->move.vx >> 1);
                work->move.vy      = base.vy + (work->move.vy >> 1);
                work->move.vz      = (base.vz >> 1) + (work->move.vz >> 1);
                VectorNormalSS(&work->move, &work->move);
                work->scale = work->scale * 2 / 3;
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&step);
                coord->coord.t[0] += step.vx;
                coord->coord.t[1] += step.vy;
                coord->coord.t[2] += step.vz;
            } else {
                work->move.vy += 0x180;
            }
            if (work->age & 1) {
                if (work->age > 0x40) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_1A4, coord, ((gRandomLcgState >> 16) & 0x10FF) + 0x02183300, NULL);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    effectSpawn(EFFECT_1A4, coord, ((gRandomLcgState >> 16) & 0x1000) + 0x82101300, NULL);
                }
            }
            work->age++;
        }
    }
}

/// Advances a moving particle in its parent's coordinate frame and updates its Y velocity.
///
/// A zero `work->step` disables both motion and acceleration. `work->move`
/// holds signed coordinate units per frame; movement precedes the velocity
/// update and invalidates the composed matrix. Motion mode 7 adds signed
/// `work->age / 10` to Y velocity; other modes subtract
/// `negativeYAcceleration` (2 for the ten-frame strip, 1 for the eight-frame
/// strip), in coordinate units per frame squared. Velocity updates narrow to
/// s16. All pointers borrow live objects for this call; the task is read-only.
static inline void _shelterB6NurseryAdvanceParticle(const Task* task, EffectWork* work, GfxCoord* coord, s32 negativeYAcceleration)
{
    enum { PARTICLE_MOTION_PARENT_JET      = 7,
           PARTICLE_MOTION_SHIFT           = 24,
           PARTICLE_MOTION_MASK            = 0xF,
           PARTICLE_PARENT_JET_AGE_DIVISOR = 10 };

    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (((task->spawnArg1.value >> PARTICLE_MOTION_SHIFT) & PARTICLE_MOTION_MASK) == PARTICLE_MOTION_PARENT_JET) {
            work->move.vy += work->age / PARTICLE_PARENT_JET_AGE_DIVISOR;
        } else {
            work->move.vy -= negativeYAcceleration;
        }
    }
}

void shelterB6NurseryAnimatedParticleTask(Task* task)
{
    enum { PARTICLE_INITIALIZE,
           PARTICLE_LARGE_STRIP,
           PARTICLE_SMALL_STRIP,
           PARTICLE_LARGE_FRAME_COUNT     = 10,
           PARTICLE_SMALL_FRAME_COUNT     = 8,
           PARTICLE_FRAME_PERIOD_DEFAULT  = 1,
           PARTICLE_SPEED_DEFAULT         = 0x40,
           PARTICLE_MOTION_STATIONARY     = 0,
           PARTICLE_MOTION_NEGATIVE_Y_FAN = 1,
           PARTICLE_MOTION_SCATTER        = 2,
           PARTICLE_MOTION_NEGATIVE_Y_JET = 3,
           PARTICLE_MOTION_SPAWN_VECTOR   = 5,
           PARTICLE_MOTION_XZ_SCATTER     = 6,
           PARTICLE_MOTION_PARENT_JET     = 7,
           PARTICLE_HALF_DIAGONAL_MASK    = 0xFFF,
           PARTICLE_PERIOD_SELECT_MASK    = 0xF000,
           PARTICLE_PERIOD_SHIFT          = 12,
           PARTICLE_PERIOD_MASK           = 7,
           PARTICLE_SPEED_SELECT_MASK     = 0xFF0000,
           PARTICLE_SPEED_SHIFT           = 16,
           PARTICLE_SPEED_MASK            = 0xFF,
           PARTICLE_MOTION_SHIFT          = 24,
           PARTICLE_MOTION_MASK           = 0xF };

    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    velocity;
    s32         framePeriod;
    s32         initialSpeed;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    work->age++;
    switch (task->state) {
        case PARTICLE_INITIALIZE:
            // Decode the two texture strips, frame cadence and initial motion from the spawn word.
            work->scale     = task->spawnArg1.value & PARTICLE_HALF_DIAGONAL_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            if (task->spawnArg1.value & PARTICLE_PERIOD_SELECT_MASK) {
                framePeriod = (task->spawnArg1.value >> PARTICLE_PERIOD_SHIFT) & PARTICLE_PERIOD_MASK;
            } else {
                framePeriod = PARTICLE_FRAME_PERIOD_DEFAULT;
            }
            work->period = framePeriod;
            work->age    = 0;
            task->state  = task->spawnArg1.value < 0 ? PARTICLE_SMALL_STRIP : PARTICLE_LARGE_STRIP;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & PARTICLE_SPEED_SELECT_MASK) {
                    initialSpeed = (task->spawnArg1.value >> PARTICLE_SPEED_SHIFT) & PARTICLE_SPEED_MASK;
                } else {
                    initialSpeed = PARTICLE_SPEED_DEFAULT;
                }
                work->step = initialSpeed;
                switch ((task->spawnArg1.value >> PARTICLE_MOTION_SHIFT) & PARTICLE_MOTION_MASK) {
                    case PARTICLE_MOTION_STATIONARY:
                        work->step = 0;
                        break;
                    case PARTICLE_MOTION_NEGATIVE_Y_FAN:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0xFFC0 - ((gRandomLcgState >> 16) & 0x7F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case PARTICLE_MOTION_SCATTER:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case PARTICLE_MOTION_NEGATIVE_Y_JET:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        break;
                    case PARTICLE_MOTION_SPAWN_VECTOR:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case PARTICLE_MOTION_XZ_SCATTER:
                        work->move.vy   = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                        break;
                    case PARTICLE_MOTION_PARENT_JET:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = (gRandomLcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
                        break;
                }
                velocity = &work->move;
                VectorNormalSS(velocity, velocity);
                gte_lddp(work->step);
                gte_ldsv(velocity);
                gte_gpf12();
                gte_stsv(velocity);
            } else {
                work->step = PARTICLE_SPEED_DEFAULT;
            }
            break;
        case PARTICLE_LARGE_STRIP:
            _shelterB6NurseryDrawParticleFrame(coord, work->index, work->scale, work->angle);
            _shelterB6NurseryAdvanceParticle(task, work, coord, 2);
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= PARTICLE_LARGE_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
        case PARTICLE_SMALL_STRIP:
            spriteQuadDraw(coord, work->index, work->scale, work->angle);
            _shelterB6NurseryAdvanceParticle(task, work, coord, 1);
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= PARTICLE_SMALL_FRAME_COUNT) {
                    effectKillTask(work, task);
                }
            }
            break;
    }
}

/// Writes one rotated pixel offset from the ten-frame particle's projected centre.
///
/// `projection` borrows a live scratch block with `depth` initialized to SZ3 / 4;
/// its caller admits depths of at least 65. Only `extent.corner` is replaced.
/// The signed `sizeFactor * 47 / depth` is the screen-space half-diagonal in
/// pixels, truncated toward zero before rotation; 47 is the cell's UV span.
/// `cornerAngle` uses 4096 units per turn, with zero pointing up and a quarter
/// turn pointing right. Signed Q12 products shift to pixels, rounding negative
/// values down. The caller applies the Y offset with the opposite sign for
/// screen coordinates. No scratch pointer is retained.
static inline void _shelterB6NurserySetParticleCornerOffset(EffectShapeScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    enum { PARTICLE_PROJECTION_SCALE   = 47,
           PARTICLE_TRIG_FRACTION_BITS = 12 };

    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                  = rsin(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * PARTICLE_PROJECTION_SCALE) / projection->depth;
    projection->extent.corner.x = (halfDiagonalPixels * trigSample) >> PARTICLE_TRIG_FRACTION_BITS;
    trigSample                  = rcos(cornerAngle);
    halfDiagonalPixels          = (sizeFactor * PARTICLE_PROJECTION_SCALE) / projection->depth;
    projection->extent.corner.y = (halfDiagonalPixels * trigSample) >> PARTICLE_TRIG_FRACTION_BITS;
}

/// Draws one raw additive frame of the nursery's ten-cell particle animation.
///
/// `animationFrame` is 0..9. The cached centre must be current; `halfDiagonal`
/// is scaled by 47 / (SZ3 / 4) to pixels, then rotated by `rotation` in
/// 4096 units per turn. Uses 28 scratch-stack bytes and allocates a textured
/// quad before rejecting negative projection flags or depths below 65.
/// Packets borrow the frame arena until GPU drawing completes.
static void _shelterB6NurseryDrawParticleFrame(const GfxCoord* coord, u16 animationFrame, s16 halfDiagonal, s16 rotation)
{
    enum { PARTICLE_CELLS_PER_ROW = 5,
           PARTICLE_CELL_TEXELS   = 48,
           PARTICLE_TOP_V         = 0x28,
           PARTICLE_MIN_DEPTH     = 0x41,
           PARTICLE_QUARTER_TURN  = ACTOR_TRANSFORM_ANGLE_TURN / 4 };

    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    s32                 u0;
    s32                 v0;
    s32                 cornerAngle;
    s32                 nextCornerAngle;
    u16                 textureColumn;
    u16                 textureRow;

    // Stage the cached centre's low 16 bits, preserving packet allocation before culling.
    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    projection->worldPoint.vx = (u16)coord->workm.t[0];
    projection->worldPoint.vy = (u16)coord->workm.t[1];
    projection->worldPoint.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        if (projection->depth >= PARTICLE_MIN_DEPTH) {
            quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
            quad->clut  = getClut(64, 270);
            setSemiTrans(quad, 1);
            setShadeTex(quad, 1);
            textureColumn = animationFrame % PARTICLE_CELLS_PER_ROW;
            textureRow    = animationFrame / PARTICLE_CELLS_PER_ROW;
            u0            = textureColumn * PARTICLE_CELL_TEXELS;
            v0            = textureRow * PARTICLE_CELL_TEXELS;
            setUV4(quad, u0, v0 + PARTICLE_TOP_V, u0 + PARTICLE_CELL_TEXELS - 1, v0 + PARTICLE_TOP_V, u0, v0 + PARTICLE_TOP_V + PARTICLE_CELL_TEXELS - 1, u0 + PARTICLE_CELL_TEXELS - 1, v0 + PARTICLE_TOP_V + PARTICLE_CELL_TEXELS - 1);
            cornerAngle = rotation;
            _shelterB6NurserySetParticleCornerOffset(projection, halfDiagonal, cornerAngle);
            quad->x0        = projection->screenX + (u16)projection->extent.corner.x;
            quad->x3        = projection->screenX - (u16)projection->extent.corner.x;
            quad->y0        = projection->screenY - (u16)projection->extent.corner.y;
            nextCornerAngle = cornerAngle + PARTICLE_QUARTER_TURN;
            quad->y3        = projection->screenY + (u16)projection->extent.corner.y;
            _shelterB6NurserySetParticleCornerOffset(projection, halfDiagonal, nextCornerAngle);
            quad->x1 = projection->screenX + (u16)projection->extent.corner.x;
            quad->x2 = projection->screenX - (u16)projection->extent.corner.x;
            quad->y1 = projection->screenY - (u16)projection->extent.corner.y;
            quad->y2 = projection->screenY + (u16)projection->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Packed additive particle texture page: 4-bit indexed texels at VRAM X=704 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 704, 0)
/// Eight-frame particle palette: VRAM X=80 words, Y=270 scanlines.
#define SPRITE_QUAD_CLUT getClut(80, 270)
/// Texel width and horizontal stride of each cell in the eight-frame particle strip.
#define SPRITE_QUAD_CELL_WIDTH 32
#define SPRITE_QUAD_CELL_MASK  7
/// Inclusive top texel row of the particle strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x88
#define SPRITE_QUAD_V1    0xA7
/// Perspective-sizing multiplier for the nursery particle sprite.
///
/// Uses the cell's inclusive 31-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE    (SPRITE_QUAD_CELL_WIDTH - 1)
#define SPRITE_QUAD_OTZ_BIAS 0
#define SPRITE_QUAD_MIN_OTZ  0x41
#include "../../shared/sprite_quad_draw.inc.c"

void shelterB6NurserySparkShowerShardTask(Task* task)
{
    /// Spins one shard and advances it by its scaled Q12 direction.
    ///
    /// Arguments must be side-effect-free pointers, evaluated repeatedly; the
    /// output vector receives coordinate units. Requires distinct live coordinate,
    /// work and output storage. The macro is undefined after this callback.
#define SHELTER_B6_NURSERY_ADVANCE_SHARD(coordNode, effectWork, displacementOut)             \
    do {                                                                                     \
        gfxRotMatrixXYZ(&(coordNode)->coord, &(effectWork)->pos, GRAPHICS_ROTATION_COMPOSE); \
        MatrixNormal(&(coordNode)->coord, &(coordNode)->coord);                              \
        gte_lddp((effectWork)->scale);                                                       \
        gte_ldsv(&(effectWork)->move);                                                       \
        gte_gpf12();                                                                         \
        gte_stsv(displacementOut);                                                           \
        (coordNode)->coord.t[0]  += (displacementOut)->vx;                                   \
        (coordNode)->coord.t[1]  += (displacementOut)->vy;                                   \
        (coordNode)->coord.t[2]  += (displacementOut)->vz;                                   \
        (coordNode)->composeStamp = GRAPHICS_COORD_DIRTY;                                    \
    } while (0)

    enum { SHARD_INITIALIZE  = 0,
           SHARD_RADIUS_MASK = 0xFFF,
           SHARD_GRAVITY_Q12 = 0x180 };

    SVECTOR     displacement;
    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        if (task->state == SHARD_INITIALIZE) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
            work->angle     = task->spawnArg1.value & SHARD_RADIUS_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 0x7F) + 0x40;
            VectorNormalSS(&work->move, &work->move);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vx        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz        = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            return;
        }
        SHELTER_B6_NURSERY_ADVANCE_SHARD(coord, work, &displacement);
        _shelterB6NurseryDrawSparkShowerShard(coord, work->angle, work->period);
        // The release plane is Y=0 in the coordinate's parent frame.
        if (coord->coord.t[1] > 0) {
            effectKillTask(work, task);
        } else {
            work->move.vy += SHARD_GRAVITY_Q12;
        }
    }
}

#undef SHELTER_B6_NURSERY_ADVANCE_SHARD

/// Scales a shard's Q12 local corner and transforms it in place for projection.
///
/// `corner` is a word-aligned SVECTOR; its XYZ components start as a Q12 unit
/// direction and finish as signed coordinate units in `composedCoord->workm`'s
/// destination frame (view space for the shard task). `radius` is a signed
/// local-coordinate distance (0..4095 in that task). The coordinate is borrowed
/// read-only; its cached Q12 rotation and integer translation are used as
/// supplied, without recomposition. The corner must not overlap it.
/// GTE scaling and rotation each saturate through signed IR
/// results before the translation is added modulo 65536; the vector's fourth
/// halfword is untouched. GTE rotation and result registers change, while its
/// translation registers are preserved. Neither pointer is retained.
static inline void _shelterB6NurseryTransformShardCorner(SVECTOR* corner, const GfxCoord* composedCoord, s16 radius)
{
    // Keep scaling and rotation as separate GTE operations with s16 results.
    gte_lddp(radius);
    gte_ldsv(corner);
    gte_gpf12();
    gte_stsv(corner);
    gte_SetRotMatrix(&composedCoord->workm);
    gte_ldv0(corner);
    gte_rtv0();
    gte_stsv(corner);
    // The final translation preserves only the low 16 bits for the next RTPT.
    corner->vx = (u16)corner->vx + (u16)composedCoord->workm.t[0];
    corner->vy = (u16)corner->vy + (u16)composedCoord->workm.t[1];
    corner->vz = (u16)corner->vz + (u16)composedCoord->workm.t[2];
}

/// Draws a rotating spark-shower shard as a grey triangle in the coordinate's YZ plane.
///
/// `radius` is in local coordinate units; `shade` supplies the low byte of each
/// RGB channel. Requires a current cached matrix. Transforms three corners
/// through it, narrows their positions to s16, and projects with `GsWSMATRIX`.
/// Reserves 32 scratch-stack bytes and one `POLY_F3` before culling; visible
/// triangles also consume a blend packet, selecting average or additive blending
/// at random. Packets remain in the frame arena until GPU drawing completes.
static void _shelterB6NurseryDrawSparkShowerShard(const GfxCoord* coord, s16 radius, s16 shade)
{
    enum { SHARD_CORNER_ANGLE_STEP = 0x555 }; // One third of a 4096-unit turn, truncated.

    _ShelterB6NurseryTriScratch* projection;
    SVECTOR*                     corner;
    POLY_F3*                     triangle;
    s32                          cornerIndex;

    projection = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB6NurseryTriScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Scale the local corners, then transform and narrow them for one RTPT.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(projection->corners); cornerIndex++) {
        corner     = &projection->corners[cornerIndex];
        corner->vx = 0;
        corner->vy = rsin(cornerIndex * SHARD_CORNER_ANGLE_STEP);
        corner->vz = rcos(cornerIndex * SHARD_CORNER_ANGLE_STEP);
        _shelterB6NurseryTransformShardCorner(corner, coord, radius);
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&projection->corners[0], &projection->corners[1], &projection->corners[2]);
    gte_rtpt();
    triangle       = gGpuPrimCursor;
    gGpuPrimCursor = triangle + 1;
    setPolyF3(triangle);
    gte_stsxy3(&triangle->x0, &triangle->x1, &triangle->x2);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->otz);
        setRGB0(triangle, shade, shade, shade);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(projection->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                triangle);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gpuSetPrimitiveBlendMode(triangle, (gRandomLcgState >> 16) & 1, projection->otz);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB6NurseryTriScratch);
}

void func_shelter_b6_nursery_80182D14(s32 arg0, s32 arg1)
{
    D_shelter_b6_nursery_801879F0.fastGlintPulse   = arg0;
    D_shelter_b6_nursery_801879F0.sparkShowerScale = arg1;
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void shelterB6NurseryRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void shelterB6NurseryRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b6_nursery_80184074(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
