#include "rooms/acropolis_observatory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_observatory_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

/// Work block of the observatory's one-time scripted player scene, owned by
/// the scene task through `Task::work`.
///
/// The block is allocated zeroed and only `playerTask` is ever stored, so
/// `followUpIndex` keeps selecting entry 0 of the follow-up clip table, the
/// entry that means "nothing to play".
typedef struct {
    Task* playerTask;    // Player task the scene's animation and release messages are sent to (NULL when the player slot is empty)
    u16   field_4;       // Never accessed; role unproven
    u16   followUpIndex; // Entry of the follow-up clip table to start once the player's current animation has ended
} _AcropolisObservatorySceneWork;
STATIC_ASSERT_SIZEOF(_AcropolisObservatorySceneWork, 8);

/// `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two weapon-id bases that record uses, and
/// `gDisplayState.pendingMode` / `Gp_StateC08.mode` gate the scene's setup (the latter is 1 while the attachment wheel is open). `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room` is the field-actor mode byte the
/// scene switches to 1 when it hands control back.

extern s16 D_acropolis_observatory_8017FE68[];

/// Per-view spawn table for the observatory's ambient effect. Entry `i` of
/// `D_acropolis_observatory_8017FEB8` is the bitmask of camera views that want
/// effect `i`, tested against `1 << Gp_GetViewIndex()`; the matching entry of
/// `D_acropolis_observatory_8017FE78` is the offset the effect is spawned at.
extern SVECTOR D_acropolis_observatory_8017FE78[8];
extern u16     D_acropolis_observatory_8017FEB8[8];

extern WorldCollisionGrid    D_acropolis_observatory_80180A50[1];
extern WorldCollisionTrigger D_acropolis_observatory_80180A74[10];
extern WorldCollisionTrigger D_acropolis_observatory_80180D6C[9];
extern WorldCoordRoomLights  D_acropolis_observatory_8018177C[1];

void func_acropolis_observatory_8017E19C(Task*);

extern SpriteBatch  D_acropolis_observatory_80181794[2];
extern SpriteBatch  D_acropolis_observatory_80181B78[7];
extern SpriteBatch  D_acropolis_observatory_80182074[22];
extern SpriteSource D_acropolis_observatory_801817A4[49];
extern SpriteSource D_acropolis_observatory_80181BB0[61];

s16 D_acropolis_observatory_8017FE68[2] = {
    -1,
    1,
};

TaskDesc D_acropolis_observatory_8017FE6C = { { { TASK_BODY_NONE, 192 } }, func_acropolis_observatory_8017E19C, { .value = 0 } };

SVECTOR D_acropolis_observatory_8017FE78[8] = {
    { -6832, -6250, -1216, 0 },
    { -6832, -6250, -4656, 0 },
    { -6832, -6250, -8176, 0 },
    { -6832, -6250, -0x2D50, 0 },
    { -576, -6250, -1024, 0 },
    { -700, -6270, -4570, 0 },
    { -688, -6250, -8310, 0 },
    { -576, -6250, -0x2E00, 0 },
};

u16 D_acropolis_observatory_8017FEB8[8] = {
    268,
    264,
    16,
    16,
    364,
    296,
    144,
    144,
};

WorldCollisionRoomResources D_acropolis_observatory_8017FEC8[2] = {
    { D_acropolis_observatory_80180A50, D_acropolis_observatory_80180A74, D_acropolis_observatory_80180D6C, NULL },
    { D_acropolis_observatory_80180A50, D_acropolis_observatory_80180A74, D_acropolis_observatory_80180D6C, NULL },
};

u8 D_acropolis_observatory_8017FEE8[8] = {
    1,
    8,
    3,
    4,
    5,
    6,
    7,
    2,
};

u8* D_acropolis_observatory_8017FEF0[2] = {
    gViewIdentityMap,
    D_acropolis_observatory_8017FEE8,
};

ViewCount D_acropolis_observatory_8017FEF8[2] = { 8, 8 };

WorldCoordRoomLighting D_acropolis_observatory_8017FEFC[2] = {
    { D_acropolis_observatory_8018177C, NULL },
    { D_acropolis_observatory_8018177C, NULL },
};

DirectionWarpEntry D_acropolis_observatory_8017FF0C[4] = {
    { { { .word = 1024 }, -2296, -2923, -0x2C02 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2296, -2923, -0x2C02 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -2408, -2926, -1748 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2408, -2926, -1748 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SECURITY_ROOM },
    { { { .word = 1024 }, -2296, -2923, -0x2C02 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -2296, -2923, -0x2C02 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -2408, -2926, -1748 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0, 0, 0 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SECURITY_ROOM },
};

static SVECTOR _gAcropolisObservatoryCollision03490Normals[22] = {
#include "assets/acropolis_observatory_collision_03490_normals.inc"
};

static SVECTOR _gAcropolisObservatoryCollision03490Verts[155] = {
#include "assets/acropolis_observatory_collision_03490_verts.inc"
};

static WorldCollisionGridFace _gAcropolisObservatoryCollision03490Faces[62] = {
#include "assets/acropolis_observatory_collision_03490_faces.inc"
};

static s16 _gAcropolisObservatoryCollision03490Cells[232] = {
#include "assets/acropolis_observatory_collision_03490_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisObservatoryCollision03490Cells[i])
static s16* _gAcropolisObservatoryCollision03490Table[9] = {
#include "assets/acropolis_observatory_collision_03490_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_observatory_80180A50[1] = {
    { NULL, _gAcropolisObservatoryCollision03490Normals, _gAcropolisObservatoryCollision03490Verts, _gAcropolisObservatoryCollision03490Faces, _gAcropolisObservatoryCollision03490Table, 8340, 0x2D57, 3, 3, 4000, 62 },
};

WorldCollisionTrigger D_acropolis_observatory_80180A74[10] = {
    { NULL, NULL, NULL, { -1089, -3648, -4219, 0 }, { { -2229, -2048, 147, 0 }, { 2229, -2048, -147, 0 }, { -2229, 2048, 147, 0 }, { 2229, 2048, -147, 0 } }, { -270, 0, -4091, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4724, -3648, -3323, 0 }, { { -1441, -2048, 746, 0 }, { 1442, -2048, -746, 0 }, { -1441, 2048, 746, 0 }, { 1442, 2048, -746, 0 } }, { -1887, 0, -3646, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1217, -3616, -4753, 0 }, { { 2271, -2048, -109, 0 }, { -2270, -2048, 110, 0 }, { 2271, 2048, -109, 0 }, { -2270, 2048, 110, 0 } }, { 198, 0, 4113, 0 }, { 0, 0, 4096, 0 }, 3050, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5121, -3776, -3777, 0 }, { { 1739, -2048, -978, 0 }, { -1738, -2048, 979, 0 }, { 1739, 2048, -978, 0 }, { -1738, 2048, 979, 0 } }, { 2012, 0, 3575, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6433, -3776, -6722, 0 }, { { -794, -2048, 859, 0 }, { 778, -2048, -871, 0 }, { -794, 2048, 859, 0 }, { 778, 2048, -871, 0 } }, { -3038, 0, -2761, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3970, -3744, -7225, 0 }, { { -1571, -2048, -332, 0 }, { 1572, -2048, 333, 0 }, { -1571, 2048, -332, 0 }, { 1572, 2048, 333, 0 } }, { 849, 0, -4014, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3890, -3712, -7703, 0 }, { { 1509, -2048, 332, 0 }, { -1508, -2048, -332, 0 }, { 1509, 2048, 332, 0 }, { -1508, 2048, -332, 0 } }, { -883, 0, 4008, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6688, -3712, -7296, 0 }, { { 823, -2048, -829, 0 }, { -832, -2048, 823, 0 }, { 823, 2048, -829, 0 }, { -832, 2048, 823, 0 } }, { 2900, 0, 2905, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1088, -3744, -7841, 0 }, { { 1456, -2048, -502, 0 }, { -1473, -2048, 479, 0 }, { 1456, 2048, -502, 0 }, { -1473, 2048, 479, 0 } }, { 1303, 0, 3891, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -992, -3680, -7298, 0 }, { { -1473, -2048, 481, 0 }, { 1456, -2048, -501, 0 }, { -1473, 2048, 481, 0 }, { 1456, 2048, -501, 0 } }, { -1305, 0, -3892, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_observatory_80180D6C[9] = {
    { NULL, NULL, NULL, { -2352, -3016, -1664, 0 }, { { -752, 0, -480, 0 }, { 752, 0, -480, 0 }, { -752, 0, 480, 0 }, { 752, 0, 480, 0 } }, { 0, 4094, 0, 0 }, { 4096, 0, 0, 0 }, 891, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2192, -3024, -0x2AB0, 0 }, { { -752, 0, -512, 0 }, { 752, 0, -512, 0 }, { -752, 0, 512, 0 }, { 752, 0, 512, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 909, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 20, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -576, -3072, -3520, 0 }, { { -784, 0, -672, 0 }, { 304, 0, -672, 0 }, { -784, 0, 672, 0 }, { 304, 0, 672, 0 } }, { 0, 4102, 0, 0 }, { -4076, 0, 401, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -512, -3072, -7536, 0 }, { { -913, 0, -816, 0 }, { 208, 0, -816, 0 }, { -913, 0, 816, 0 }, { 208, 0, 816, 0 } }, { 0, 4097, 0, 0 }, { -4076, 0, 401, 0 }, 1221, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5856, -3072, -2496, 0 }, { { -752, 0, -960, 0 }, { 752, 0, -960, 0 }, { -752, 0, -160, 0 }, { 752, 0, -160, 0 } }, { 0, 4115, 0, 0 }, { 201, 0, -4092, 0 }, 1214, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7328, -3072, -6208, 0 }, { { -752, 0, -1216, 0 }, { 1424, 0, -1216, 0 }, { -752, 0, 1312, 0 }, { 1424, 0, 1312, 0 } }, { 0, 4097, 0, 0 }, { -4076, 0, 401, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5712, -3072, -7776, 0 }, { { -448, 0, -2016, 0 }, { 1280, 0, -2016, 0 }, { -448, 0, 1248, 0 }, { 1280, 0, 1248, 0 } }, { 0, 4108, 0, 0 }, { -4076, 0, 401, 0 }, 2387, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1488, -3072, -4576, 0 }, { { -1888, 0, -32, 0 }, { 1888, 0, -416, 0 }, { -1888, 0, 352, 0 }, { 1888, 0, 96, 0 } }, { 0, 4101, 0, 0 }, { 4096, 0, 0, 0 }, 1932, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -4864, -3104, -3584, 0 }, { { -1664, 0, 560, 0 }, { 1664, 0, -1072, 0 }, { -1664, 0, 1072, 0 }, { 1664, 0, -560, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1978, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_observatory_80181018[2] = {
    { 19, 19, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_80179120 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_observatory_80181030[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_observatory_80181048[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_observatory_80181060[2] = {
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_observatory_80181078[2] = {
    { 13, 13, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401300_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_observatory_80181090[2] = {
    { 10, 10, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401000_80155004 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_acropolis_observatory_801810A8[2] = {
    { 10, 0, 4, -4930, -2990, -5330, 820, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_acropolis_observatory_801810C8[2] = {
    { 12, 12, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80138E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_acropolis_observatory_801810E0[2] = {
    { 12, 0, 0, -1529, -2990, -6500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_acropolis_observatory_80181100[7] = {
    { 12, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 12, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 12, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 12, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 12, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 12, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_acropolis_observatory_80181170[2] = {
    { 40, 40, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_8013E500 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_acropolis_observatory_80181188[3] = {
    { 40, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 40, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_acropolis_observatory_801811B8[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_acropolis_observatory_801811C4[3] = {
    { 9, 0, 0, -1529, -2990, -8500, 0, 0, 0, 2, 0 },
    { 18, 0, 0, -3838, -2990, -4000, 1500, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_acropolis_observatory_801811F4[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_acropolis_observatory_8018120C[2] = {
    { 18, 0, 0, -4930, -2990, -5330, 820, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaResource D_acropolis_observatory_8018122C[2] = {
    { 19, 19, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_80149120 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_acropolis_observatory_80181244[2] = {
    { 19, 0, 0, -4930, -2990, -5330, 820, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_acropolis_observatory_80181264[19] = {
    { NULL, NULL },
    { D_map_akropolis_8017B6DC, D_acropolis_observatory_80181018 },
    { D_map_akropolis_8017B70C, D_acropolis_observatory_80181030 },
    { D_map_akropolis_8017B73C, D_acropolis_observatory_80181048 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B7DC, D_acropolis_observatory_80181060 },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B80C, D_acropolis_observatory_80181078 },
    { D_acropolis_observatory_801810A8, D_acropolis_observatory_80181090 },
    { D_acropolis_observatory_801810E0, D_acropolis_observatory_801810C8 },
    { D_acropolis_observatory_80181100, D_acropolis_observatory_801810C8 },
    { D_acropolis_observatory_80181188, D_acropolis_observatory_80181170 },
    { D_acropolis_observatory_801811C4, D_acropolis_observatory_801811B8 },
    { D_acropolis_observatory_8018120C, D_acropolis_observatory_801811F4 },
    { D_acropolis_observatory_80181244, D_acropolis_observatory_8018122C },
    { NULL, NULL },
};

/// Point lights shared by both observatory room entries in every view.
///
/// Overlay-owned writable records: room updates parent and compose their
/// transforms, and shading queries overwrite attenuation. Positions and
/// falloff radii use integer world units; RGB uses 12 fractional bits
/// (`ONE` is full intensity). Storage remains live while this overlay is loaded.
static WorldCoordPointLight _gAcropolisObservatoryPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5386, -4300, -10820 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6650, -4300, -6290 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1000,
        .outer = 4500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -5457, -610 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -5457, -12320 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6377, -5457, -8500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1306, -5457, -8400 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6423, -5457, -4020 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1306, -5457, -4560 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3788, -5457, -6160 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 1638, 1638, 1638 },
            .unknown_56 = { 0, 0 },
        },
        // Equal radii keep this neutral contribution at full strength through 20000 world units.
        .inner = 20000,
        .outer = 20000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2873, -5000, -10740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2873, -5000, -1770 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5386, -4300, -1770 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 100,
        .outer = 6000,
    },
};

WorldCoordRoomLights D_acropolis_observatory_8018177C[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisObservatoryPointLights), _gAcropolisObservatoryPointLights, 0, NULL },
};

SpriteBatch D_acropolis_observatory_80181794[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_observatory_801817A4[49] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 48, 1167, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, 104, -40, 742, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 104, 48, 729, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -160, -48, 1538, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -88, -16, 1450, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -72, -24, 1325, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -56, -40, 1200, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 104 } }, -40, -56, 1180, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -8, -16, 1167, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, -128, -48, 1475, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, -48, 1389, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -128, 0, 1521, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 0, 1449, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -96, -16, 1408, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -24, 1550, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -32, 1384, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -64, -48, 1384, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -32, -80, 1157, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -8, -64, 1132, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 24, -88, 1132, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 56 } }, -160, -8, 1216, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -88, 32, 1350, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -120, 32, 1450, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -144, 32, 1475, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -160, 32, 1500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -40, 40, 1175, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -64, 40, 1250, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 32, -88, 1171, { .fields = { 88, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 56, 40 } }, -24, -88, 1206, { .fields = { 48, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -32, -72, 1200, { .fields = { 96, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -40, -64, 1250, { .fields = { 112, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 24 } }, -48, -56, 1316, { .fields = { 0, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -56, -56, 1352, { .fields = { 64, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -64, -48, 1374, { .fields = { 64, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -72, -40, 1396, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -80, -32, 1419, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -88, -24, 1388, { .fields = { 104, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -96, -16, 1472, { .fields = { 64, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -104, -8, 1501, { .fields = { 56, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 32 } }, -112, 0, 1490, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 24 } }, -120, 8, 1534, { .fields = { 112, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 24 } }, -128, 16, 1601, { .fields = { 120, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 48 } }, -32, -8, 1180, { .fields = { 72, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 16 } }, 16, 8, 1141, { .fields = { 96, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 48 } }, -40, 0, 1245, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 48 } }, -48, 8, 1250, { .fields = { 112, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 48 } }, -56, 8, 1250, { .fields = { 120, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 48 } }, -64, 16, 1257, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 48 } }, -72, 24, 1350, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_observatory_80181B78[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { 10, 10, 0, 0, { 3, 0 } },
    { 20, 1, 0, 0, { 2, 0 } },
    { 21, 6, 0, 0, { 4, 0 } },
    { 27, 22, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_observatory_80181BB0[61] = {
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -64, 1996, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 32, -48, 2050, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -48, 2175, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, -64, 2133, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 8, -48, 2208, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -8, -40, 2275, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -32, 2375, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -16, 2450, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -24, 2075, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -24, 2075, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 24, -48, 2170, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -24, 2269, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 8, -24, 2211, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -24, -40, 2362, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, -40, 2425, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -56, -48, 2433, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 48, -16, 2075, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 32, -16, 2212, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -16, 2277, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 0, -8, 2275, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -16, 0, 2300, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 2275, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -96, -24, 1776, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, -40, 1776, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, -40, 1737, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, -40, 1715, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -136, -40, 1715, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, -40, 1675, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -8, 2075, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -16, 2050, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, -16, 1885, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -16, 2436, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 16, 1275, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 8, 1210, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -8, 1131, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 8, 1962, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -24, 1669, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 120 } }, 120, -16, 825, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 64, 825, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -16, 725, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 136, 16, 725, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 64, 725, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -120, 1625, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 128, -120, 1625, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 128, -56, 1625, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 120, -56, 1650, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -16, 1650, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, -120, 1650, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, 32, -64, 2070, { .fields = { 80, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, 16, -64, 2177, { .fields = { 56, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, 0, -48, 2256, { .fields = { 104, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, -16, -40, 2362, { .fields = { 104, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, -32, -24, 2432, { .fields = { 120, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, -48, -16, 2433, { .fields = { 120, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 24 } }, -64, 0, 2431, { .fields = { 88, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -32, 16, 2175, { .fields = { 56, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 24 } }, -16, 0, 2181, { .fields = { 96, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, 0, -8, 2273, { .fields = { 120, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 32 } }, 16, -16, 2271, { .fields = { 120, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, -16, 2200, { .fields = { 24, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 24 } }, 48, -16, 2077, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_observatory_80182074[22] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 16, 0 } },
    { 8, 8, 0, 0, { 3, 0 } },
    { 16, 6, 0, 0, { 14, 0 } },
    { 22, 6, 0, 0, { 19, 0 } },
    { 28, 7, 0, 0, { 8, 0 } },
    { 35, 2, 0, 0, { 2, 0 } },
    { 37, 2, 0, 0, { 11, 0 } },
    { 39, 3, 0, 0, { 0, 0 } },
    { 42, 6, 0, 0, { 15, 0 } },
    { 48, 0, 0, 0, { 1, 0 } },
    { 48, 13, 0, 0, { 12, 0 } },
    { 61, 0, 0, 0, { 6, 0 } },
    { 61, 0, 0, 0, { 10, 0 } },
    { 61, 0, 0, 0, { 7, 0 } },
    { 61, 0, 0, 0, { 9, 0 } },
    { 61, 0, 0, 0, { 5, 0 } },
    { 61, 0, 0, 0, { 13, 0 } },
    { 61, 0, 0, 0, { 18, 0 } },
    { 61, 0, 0, 0, { 17, 0 } },
    { 61, 0, 0, 0, { 4, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

u16 D_acropolis_observatory_80182124[4] = {
    256,
    0,
    0,
    0,
};

SpriteSource D_acropolis_observatory_8018212C[67] = {
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 144, -48, 1287, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -8, 1675, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -16, 1565, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -16, 1434, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -16, 1412, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -16, 1412, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, -16, 1000, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -24, 1000, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 40, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -152, -24, 1000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 40, 1000, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -32, 16, 1850, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 8, 1962, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 40, 8, 2000, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, -24, 1962, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -24, 2050, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -104, 2050, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 128 } }, 80, -112, 2050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, -56, 2050, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, -112, 2050, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -56, -24, 1798, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, -24, 1900, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 2221, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -56, 1925, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -48, 1925, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, -32, 1995, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -16, 2000, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -80, -80, 1797, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, -56, 1797, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -72, 1797, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -32, -56, 1797, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -48, 1800, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -40, 1925, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 1925, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -24, 1925, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -32, 1945, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1945, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -8, 2000, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -8, 2000, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, -16, 2000, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -16, 1797, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -48, 8, 1872, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -48, -16, 1797, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, -8, 1925, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 0, 1942, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 8, 8, 1942, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 16, 1925, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 32 } }, -56, -16, 1850, { .fields = { 16, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 40 } }, -16, -16, 1965, { .fields = { 40, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 0, -8, 1966, { .fields = { 24, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, 16, 8, 1965, { .fields = { 72, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 48, 32 } }, -72, -72, 2275, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 48 } }, -24, -72, 2275, { .fields = { 80, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 40 } }, -8, -56, 2275, { .fields = { 56, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 48 } }, 8, -48, 2275, { .fields = { 72, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 40 } }, 24, -32, 2275, { .fields = { 56, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 40 } }, 40, -24, 2275, { .fields = { 56, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 32 } }, 56, -8, 2275, { .fields = { 24, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4080, { .fields = { 56, 32 } }, -72, -72, 1697, { .fields = { 88, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 16, 32 } }, -16, -64, 1807, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 16, 40 } }, 0, -56, 1912, { .fields = { 56, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 16, 32 } }, 16, -40, 1940, { .fields = { 8, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 16, 32 } }, 48, -16, 2050, { .fields = { 8, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 16, 32 } }, 64, -8, 2050, { .fields = { 0, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4080, { .fields = { 16, 16 } }, 80, 8, 2075, { .fields = { 120, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 8, 32 } }, 32, -32, 1945, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4080, { .fields = { 8, 32 } }, 40, -24, 2013, { .fields = { 96, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_observatory_80182668[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 6, 0 } },
    { 1, 5, 0, 0, { 1, 0 } },
    { 6, 5, 0, 0, { 8, 0 } },
    { 11, 3, 0, 0, { 0, 0 } },
    { 14, 8, 0, 0, { 5, 0 } },
    { 22, 18, 0, 0, { 2, 0 } },
    { 40, 7, 0, 0, { 9, 0 } },
    { 47, 4, 0, 0, { 4, 0 } },
    { 51, 7, 0, 0, { 7, 0 } },
    { 58, 9, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_observatory_801826C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_observatory_801826D8[29] = {
    { 141, 0x3FC0, { .fields = { 88, 16 } }, -8, -120, 700, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -24, -104, 700, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 64, -104, 700, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, 32, 375, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -88, 8, 375, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 32 } }, -64, -8, 375, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 32, 0, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 56, 16, 375, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 88, 48, 375, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -120, 756, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 72, -120, 865, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -56, -64, 544, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -32, -80, 562, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, -8, -88, 564, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 48, -80, 540, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 72, -64, 553, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 0, -120, 1500, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 32, -120, 1500, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 56, -120, 1500, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 120, 40 } }, -24, -120, 375, { .fields = { 72, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 104, 56 } }, -88, -24, 250, { .fields = { 112, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 88, 56 } }, 16, -24, 250, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 112, 48 } }, -112, 32, 250, { .fields = { 16, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 104, 48 } }, 0, 32, 250, { .fields = { 8, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 88, 40 } }, -136, 80, 250, { .fields = { 40, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 80, 40 } }, -48, 80, 250, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 80, 40 } }, 32, 80, 250, { .fields = { 88, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 80, 56 } }, -56, -80, 375, { .fields = { 120, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 72, 56 } }, 24, -80, 375, { .fields = { 32, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_observatory_8018291C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 3, 0 } },
    { 9, 7, 0, 0, { 0, 0 } },
    { 16, 3, 0, 0, { 2, 0 } },
    { 19, 10, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_observatory_8018294C[36] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -104, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, -112, 750, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, -120, 750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -32, -112, 750, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -96, 750, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -32, 475, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -80, -56, 470, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 24 } }, -48, -64, 475, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 16, -56, 475, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -32, 475, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -8, 96, 255, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 32, 88, 247, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, 56, 80, 246, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, 88, 251, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -96, -88, 750, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 0, -80, 750, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, -72, -96, 751, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -96, -120, 800, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -80, -120, 800, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -32, -120, 800, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, -104, 1500, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, -112, 1500, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -40, -104, 1500, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, -96, -120, 1250, { .fields = { 72, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 40, 24 } }, -80, -120, 1250, { .fields = { 120, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 48 } }, -40, -120, 1250, { .fields = { 104, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 96, 40 } }, -96, -120, 630, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 136, 48 } }, -96, -80, 450, { .fields = { 0, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 88, 64 } }, -96, -32, 388, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 96, 64 } }, -8, -32, 394, { .fields = { 32, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 104, 64 } }, -96, 32, 280, { .fields = { 24, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 96, 64 } }, 8, 32, 175, { .fields = { 8, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 40 } }, 104, 56, 175, { .fields = { 64, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 80, 24 } }, -104, 96, 249, { .fields = { 56, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 104, 24 } }, -24, 96, 197, { .fields = { 64, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 80, 24 } }, 80, 96, 203, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_observatory_80182C1C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 3, 0, 0, { 0, 0 } },
    { 17, 3, 0, 0, { 5, 0 } },
    { 20, 3, 0, 0, { 1, 0 } },
    { 23, 3, 0, 0, { 4, 0 } },
    { 26, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_observatory_80182C5C[81] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -56, 1547, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -40, 1467, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 32, -16, 1544, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -24, 1609, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -40, 1557, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -64, 1612, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, -48, 1797, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -32, 1761, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, -16, 1770, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -40, 1971, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -16, 1938, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -8, 1842, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 2139, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -24, 1800, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 8, -8, 1785, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 24, 0, 1775, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 8, 16, 1750, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 0, 0, 1775, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 0, 1775, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1984, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 16, 2019, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 16, 1746, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 8, 1703, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 1349, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, -8, 1862, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 0, 0, 1800, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 8, 1712, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, 0, 1717, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -8, 1900, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -40, -24, 1900, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -32, 1987, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -72, -40, 2000, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -88, -48, 2037, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 875, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 72, 993, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 72, 1075, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 88, 1198, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 96, 1271, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 32, 875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -96, 0, 2157, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -120, 32, 1950, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -40, 32, 1927, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -112, 24, 1987, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -40, 24, 1988, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -104, 16, 2020, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -56, 16, 2020, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 56, 1575, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 0, 1762, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 32, 1761, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, 16, 1637, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 32, 1591, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 48, 1575, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 56, 1575, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 32, 1587, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, 48, 1475, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 96, 64, 1475, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 72, 1475, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 80, 1475, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 88, 1475, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 16, 1787, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 16, 1775, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 32, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, 56, 1500, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 56, 1500, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 72, 1554, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 88, 1549, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -48, -32, 1754, { .fields = { 88, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 16 } }, 16, -40, 1524, { .fields = { 96, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 64, 16 } }, 16, -24, 1540, { .fields = { 88, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 40, 8 } }, 32, -8, 1554, { .fields = { 96, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, -40, 1690, { .fields = { 120, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 24 } }, -16, -24, 1731, { .fields = { 32, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -40, -40, 1898, { .fields = { 48, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -40, -16, 1879, { .fields = { 0, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, -32, 8, 2052, { .fields = { 72, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 8, 1750, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, 16, 0, 1750, { .fields = { 88, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 32 } }, -16, 0, 1800, { .fields = { 88, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 24 } }, 0, 0, 1800, { .fields = { 64, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 16 } }, -16, -56, 1841, { .fields = { 0, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 72, 24 } }, 16, -64, 1607, { .fields = { 16, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_acropolis_observatory_801832B0[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 6, 0 } },
    { 13, 2, 0, 0, { 1, 0 } },
    { 15, 7, 0, 0, { 4, 0 } },
    { 22, 17, 0, 0, { 0, 0 } },
    { 39, 7, 0, 0, { 5, 0 } },
    { 46, 15, 0, 0, { 3, 0 } },
    { 61, 5, 0, 0, { 7, 0 } },
    { 66, 15, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_observatory_80183300[8] = {
    { { .empty = D_acropolis_observatory_80181794 }, D_acropolis_observatory_80181794, NULL },
    { { .elements = D_acropolis_observatory_801817A4 }, D_acropolis_observatory_80181B78, NULL },
    { { .elements = D_acropolis_observatory_80181BB0 }, D_acropolis_observatory_80182074, NULL },
    { { .elements = D_acropolis_observatory_8018212C }, D_acropolis_observatory_80182668, NULL },
    { { .empty = D_acropolis_observatory_801826C8 }, D_acropolis_observatory_801826C8, NULL },
    { { .elements = D_acropolis_observatory_801826D8 }, D_acropolis_observatory_8018291C, NULL },
    { { .elements = D_acropolis_observatory_8018294C }, D_acropolis_observatory_80182C1C, NULL },
    { { .elements = D_acropolis_observatory_80182C5C }, D_acropolis_observatory_801832B0, NULL },
};

ViewCamera D_acropolis_observatory_80183360[8] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 4000, 0x7530, 6417 } }, 447 },
    { { { { 3720, 0, 1712 }, { 16, 4095, -35 }, { -1712, 39, 3720 } }, { 805, 3995, 6905 } }, 230 },
    { { { { 3737, 0, 1674 }, { 228, 4057, -510 }, { -1659, 559, 3702 } }, { 988, 4680, 0x2AC5 } }, 230 },
    { { { { -3839, 0, 1427 }, { 215, 4048, 580 }, { -1411, 618, -3795 } }, { 1561, 4666, 3006 } }, 230 },
    { { { { -2731, 0, 3052 }, { 2171, 2879, 1942 }, { -2145, 2913, -1919 } }, { 5020, 4510, 7270 } }, 230 },
    { { { { 1023, 0, -3966 }, { -830, 4005, -214 }, { 3878, 857, 1000 } }, { 8017, 4142, 2203 } }, 230 },
    { { { { -1356, 0, -3864 }, { -277, 4085, 97 }, { 3854, 293, -1353 } }, { 8918, 3365, 0x274F } }, 230 },
    { { { { 3757, 0, 1630 }, { 1140, 2928, -2626 }, { -1165, 2863, 2686 } }, { 1914, 9275, 7324 } }, 230 },
};

PadScriptCmd D_acropolis_observatory_80183480[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 19), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_observatory_80183498[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

PadScriptCmd D_acropolis_observatory_801834A0[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 19), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_observatory_801834B8[2] = {
    { 0, 0, 2, 0 },
    { 60, 60, 1, 0 },
};

WorldCollisionFootstepSounds D_acropolis_observatory_801834C0 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_observatory_801834CC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_observatory_801834D4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_observatory_801834C0 },
};

WorldCollisionSurfaceProperties* D_acropolis_observatory_801834DC[8] = {
    D_acropolis_observatory_801834CC,
    D_acropolis_observatory_801834D4,
    D_acropolis_observatory_801834CC,
    D_acropolis_observatory_801834CC,
    D_acropolis_observatory_801834CC,
    D_acropolis_observatory_801834CC,
    D_acropolis_observatory_801834CC,
    D_acropolis_observatory_801834CC,
};

/// The observatory's scene task. State 0 allocates the `_AcropolisObservatorySceneWork` block,
/// captures slot 3 in it and cues the scene with the 0x3F4 record at
/// `gAcropolisObservatoryPlayerAnimationSets`; it does nothing at all while the
/// attachment wheel is open (`Gp_StateC08.mode`) or `gDisplayState.pendingMode` is set. States 1, 2 and 4 just
/// tick, state 3 waits for the shared field-actor byte to reach 2 and arms
/// `Gp_ArmStateF0`, state 5 republishes the player's weapon to slot 3 and puts
/// the session back into field mode, and state 6 releases slot 3 (msg 0x3F1)
/// and kills the task.
///
/// Every state then falls into the same tail: while slot 3 is idle (msg 0x3ED
/// returns 0) the `followUpIndex`th entry of `D_acropolis_observatory_8017FE68` is sent
/// as a second 0x3F4 record, unless that entry is negative.
void func_acropolis_observatory_8017E19C(Task* task)
{
    AnimationPlayRequest            rec;
    AnimationPlayRequest            arg;
    AnimationPlayRequest*           msg;
    _AcropolisObservatorySceneWork* work;
    _AcropolisObservatorySceneWork* tail;
    _AcropolisObservatorySceneWork* dest;
    _AcropolisObservatorySceneWork* blk;
    s16*                            p;
    u16                             entry;
    s32                             temp;
    s32                             weaponId;
    s32                             id;

    work = task->work;
    switch (task->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            blk        = memCalloc(sizeof(*blk), 0);
            temp       = (blk == NULL);
            task->work = blk;
            if (temp) {
                taskKill(task);
            } else {
                memFillBytes(blk, 0, sizeof(*blk));
                blk->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            }
            work = task->work;
            if (work->playerTask != NULL) {
                rec.source.sets          = gAcropolisObservatoryPlayerAnimationSets;
                rec.animationId          = 1;
                rec.blend                = ANIMATION_BLEND_RESET;
                rec.blendFrames          = 0;
                rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &rec, 0);
            }
            gSceneCombatState.actor03700Wave = 0;
            /* fallthrough */
        case 1:
        case 2:
        case 4:
            task->state = task->state + 1;
            break;
        case 3:
            if (gSceneCombatState.actor03700Wave == 2) {
                Gp_ArmStateF0(1);
                task->state = task->state + 1;
            }
            break;
        case 5:
            weaponId                 = gPlayerStatus.weapon;
            id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.source.index         = id;
            rec.animationId          = 1;
            rec.blend                = ANIMATION_BLEND_RESET;
            rec.blendFrames          = 0;
            rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            gGameSession->location.loc.room                            = 1;
            gGameSession->roomObjsDirty                                = 1;
            gGameSession->viewDirty                                    = 1;
            task->state                                                = task->state + 1;
            break;
        case 6:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            taskKill(task);
            break;
    }

    tail = task->work;
    msg  = &arg;
    if (tail->playerTask != NULL && taskMessageDispatch(tail->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        p     = &D_acropolis_observatory_8017FE68[tail->followUpIndex];
        temp  = *p;
        entry = *p;
        if (temp >= 0) {
            dest = task->work;
            if (dest->playerTask != NULL) {
                arg.source.sets           = gAcropolisObservatoryPlayerAnimationSets;
                arg.animationId           = entry;
                msg->blend                = ANIMATION_BLEND_INTERPOLATE;
                msg->blendFrames          = 0xA;
                msg->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(dest->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, msg, 0);
            }
        }
    }
}

/// Draws the observatory's lens flare: the model's world position is projected
/// through `GsWSMATRIX` into an `EffectCentreScratch` block off the scratch stack,
/// and, when the `rtps` reports no error, the projected point becomes the
/// centre of a semi-transparent `POLY_FT4` on tpage 0x2B. The depth used for
/// both the size and the ordering-table slot is the raw `depth` pulled 0x40
/// towards the camera and clamped to 0x10, so the flare stops growing once it
/// is very close. The CLUT alternates between two palettes on odd and even
/// frames, which is what makes the flare flicker.
void func_acropolis_observatory_8017E424(Task* arg0)
{
    void**               scratch;
    u8*                  head;
    EffectCentreScratch* blk;
    POLY_FT4*            prim;
    GfxCoord*            coord;
    void*                mem;
    u16                  vz;
    s16                  x;
    s16                  y;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    Gp_UpdateCoord(coord);

    scratch            = SCRATCH_STACK_CURSOR_SLOT;
    head               = *scratch;
    blk                = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    blk->worldPoint.vx = (u16)coord->workm.t[0];
    blk->worldPoint.vy = (u16)coord->workm.t[1];
    vz                 = (u16)coord->workm.t[2];
    *scratch           = blk;
    blk->worldPoint.vz = vz;

    {
        SVECTOR* v = &blk->worldPoint;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(v);
    }
    gte_rtps();
    gte_stsxy(&blk->screenX);
    gte_stflg(&blk->projectionFlags);
    if (blk->projectionFlags >= 0) {
        gte_stszotz(&blk->depth);
        blk->depth -= 0x40;
        if (blk->depth < 0x10) {
            blk->depth = 0x10;
        }
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage       = 0x2B;
        prim->clut        = getClut(0xE0 + (u32)(gDisplayState.animFrame & 1) * 0x10, 0x10F);
        prim->u0          = 0;
        prim->v0          = 0xA0;
        prim->u1          = 0x1F;
        prim->v1          = 0xA0;
        prim->u2          = 0;
        prim->v2          = 0xBF;
        prim->u3          = 0x1F;
        prim->v3          = 0xBF;
        blk->screenExtent = 0x5D00 / blk->depth;
        x                 = blk->screenX - (u16)blk->screenExtent;
        prim->x2          = x;
        prim->x0          = x;
        x                 = blk->screenX + (u16)blk->screenExtent;
        prim->x3          = x;
        prim->x1          = x;
        y                 = blk->screenY - (u16)blk->screenExtent;
        prim->y1          = y;
        prim->y0          = y;
        y                 = blk->screenY + (u16)blk->screenExtent;
        prim->y3          = y;
        prim->y2          = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
    effectKillTask(mem, arg0);
}

/// Re-spawns the observatory's ambient effects for the current camera view,
/// one per entry whose view mask contains the active view. Skipped entirely
/// once `gRoomEffectState->effectControl` reaches the cancellation threshold of 4.
void func_acropolis_observatory_8017E6F8(Task* task)
{
    GfxCoord* coord;
    s32       mask;
    s32       i;
    SVECTOR*  vec;
    u16*      flags;

    coord = task->extra.coordBody->coord;
    mask  = 1 << Gp_GetViewIndex();
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        i     = 0;
        vec   = D_acropolis_observatory_8017FE78;
        flags = D_acropolis_observatory_8017FEB8;
        do {
            if (*flags & mask) {
                Gp_SpawnEff(EFFECT_ACROPOLIS_OBSERVATORY_LENS_FLARE, coord, 0, vec);
            }
            vec++;
            i++;
            flags++;
        } while (i < 8);
    }
}
