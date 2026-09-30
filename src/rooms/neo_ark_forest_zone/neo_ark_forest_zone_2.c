#include "rooms/neo_ark_forest_zone.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/stdio.h>

#include "common.h"

#include "neo_ark_forest_zone_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/enemy_params.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/room_events.h"
#include "../../shared/roaming_enemies.h"

/// A placement for a spawned task: the x and z written into its coordinate
/// translation (y is always zero) and the Y rotation passed to
/// `gfxRotMatrixY`. The halfword after `x` is not read.
typedef struct NeoArkForestZoneSpawnPos {
    s16 x;
    s16 pad_2;
    s16 z;
    s16 rotY;
} NeoArkForestZoneSpawnPos;

/// Enemy parameters shared by the spawn-slot controllers.
extern EnemyParams  gRoamerParams;
extern DamageAttack D_neo_ark_forest_zone_80182D04[6];

/// How many spawns each session slot arms, indexed by
/// `gGameSession->location.loc.variant`, for the second and the first arming task
/// respectively; zero disables that task's work in the slot.
extern u8 gRoamerArmCountsB[];
extern u8 gRoamerArmCountsA[];

/// Frame countdown: counted down each frame by the arming ticks, bumped by
/// 0x5A when a spawn is handed out, and tested for zero before a new
/// placement request is accepted.
extern s16 gRoamerCooldown;

/// How many of the pending spawn slots are armed and scanned.
extern s16 gRoamerReserveCount;

/// Placement request, one-based (zero means none), taken from a 0x13EF
/// message. Cleared every frame by the arming ticks after it has been acted
/// on.
extern s16 gRoamerSpawnRequest;

/// The value the 0x13EF handlers last saw in their message's third byte.
extern s16 gRoamerLastRequest;

/// Set when a spawn slot was filled while the `Gp_StateF0` reference could
/// not yet be released; the first arming task's tick releases it later.
extern s16 gRoamerReleasePending;

/// Message tables the two arming tasks install in `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*, s32, TaskMessageArg, TaskMessageArg);
        s32  (*call1)(Task*, s32, u8*, TaskMessageArg);
        void (*call2)(Task*, s32, s32);
    } handler;
} NeoArkForestZone2MsgEntry;
STATIC_ASSERT_SIZEOF(NeoArkForestZone2MsgEntry, 8);

extern NeoArkForestZone2MsgEntry gRoamerMsgTableA[];
extern GpMsgEntry                gRoamerMsgTableB[];

/// Spawn placements of the first and the second arming task, indexed by the
/// placement request minus one.
extern NeoArkForestZoneSpawnPos gRoamerSpawnPointsA[];
extern NeoArkForestZoneSpawnPos D_neo_ark_forest_zone_80182DE8[5];

/// `Gp_StateF0.field_6` as seen on the previous frame.
extern s16 gRoamerPrevBattleRefs;

static void func_neo_ark_forest_zone_8018141C(Task* arg0);

/// State table of the first arming task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D5E8 = { {
    roamerArmPoolA,
    roamerTickPoolA,
    func_neo_ark_forest_zone_8018141C,
    taskKill,
} };

s32 func_neo_ark_forest_zone_801813BC(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_neo_ark_forest_zone_80181494(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_neo_ark_forest_zone_801814B0(Task*, s32, u8*, TaskMessageArg);

extern GpGridParams   D_neo_ark_forest_zone_80182274[1];
extern GpObj4C        D_neo_ark_forest_zone_801826B4[6];
extern GpObj4C        D_neo_ark_forest_zone_801829D0[10];
extern GpRoomCoordSet D_neo_ark_forest_zone_8018269C[1];

void func_neo_ark_forest_zone_80181430(Task*);
void func_neo_ark_forest_zone_8018151C(Task*);

AnimationPackedPose D_neo_ark_forest_zone_80181580[6] = {
#include "assets/neo_ark_forest_zone_animation_04364_bank1.inc"
};

AnimationPackedRotation D_neo_ark_forest_zone_801815C8[64] = {
#include "assets/neo_ark_forest_zone_animation_04364_bank4.inc"
};

AnimationRecord D_neo_ark_forest_zone_801816C8[141] = {
#include "assets/neo_ark_forest_zone_animation_04364_records.inc"
};

u16 D_neo_ark_forest_zone_801818FC[20] = {
#include "assets/neo_ark_forest_zone_animation_04364_indices.inc"
};

AnimationSet D_neo_ark_forest_zone_80181924 = {
    D_neo_ark_forest_zone_801816C8,
    D_neo_ark_forest_zone_801818FC,
    { NULL, D_neo_ark_forest_zone_80181580, NULL, NULL, D_neo_ark_forest_zone_801815C8, NULL, NULL, NULL },
};

AnimationPackedPose D_neo_ark_forest_zone_8018194C[10] = {
#include "assets/neo_ark_forest_zone_animation_047D4_bank1.inc"
};

AnimationPackedRotation D_neo_ark_forest_zone_801819C4[95] = {
#include "assets/neo_ark_forest_zone_animation_047D4_bank4.inc"
};

AnimationRecord D_neo_ark_forest_zone_80181B40[139] = {
#include "assets/neo_ark_forest_zone_animation_047D4_records.inc"
};

u16 D_neo_ark_forest_zone_80181D6C[20] = {
#include "assets/neo_ark_forest_zone_animation_047D4_indices.inc"
};

AnimationSet D_neo_ark_forest_zone_80181D94 = {
    D_neo_ark_forest_zone_80181B40,
    D_neo_ark_forest_zone_80181D6C,
    { NULL, D_neo_ark_forest_zone_8018194C, NULL, NULL, D_neo_ark_forest_zone_801819C4, NULL, NULL, NULL },
};

TaskDesc D_neo_ark_forest_zone_80181DBC = { 0, 32, roomEventStagedTask, { .model = NULL } };

GpMsgEntry D_neo_ark_forest_zone_80181DC8[6] = {
    { 5102, func_neo_ark_forest_zone_8017D7E4 },
    { 5105, func_neo_ark_forest_zone_8017D7DC },
    { 5103, func_neo_ark_forest_zone_8017D958 },
    { 5104, func_neo_ark_forest_zone_8017D950 },
    { 5108, func_neo_ark_forest_zone_8017DA14 },
    { 0x7FFFFFFF, NULL },
};

AnimationSet* D_neo_ark_forest_zone_80181DF8[2] = {
    &D_neo_ark_forest_zone_80181D94,
    &D_neo_ark_forest_zone_80181924,
};

GpCopyArg D_neo_ark_forest_zone_80181E00 = { { .sets = D_neo_ark_forest_zone_80181DF8 }, 2 };

AnimationPlayRequest D_neo_ark_forest_zone_80181E08 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_neo_ark_forest_zone_80181E1C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

s32 D_neo_ark_forest_zone_80181E30 = 2821;

ActorCommand D_neo_ark_forest_zone_80181E34 = { { .loc = { 5, 11 } }, 1 };

s32 D_neo_ark_forest_zone_80181E38 = 0x20B05;

AnimationPlayRequest D_neo_ark_forest_zone_80181E3C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_neo_ark_forest_zone_80181E50 = { { 2888, 128, -95, 0 }, { 0, -1024, 0, 0 } };

Task* D_neo_ark_forest_zone_80181E68 = NULL;

GpEvsCmd D_neo_ark_forest_zone_80181E6C[23] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_neo_ark_forest_zone_80181E00 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_neo_ark_forest_zone_80181E50 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_forest_zone_80181E1C }, { .value = 0 } },
    { 41, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x550B000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_neo_ark_forest_zone_80181E34 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_forest_zone_80181E08 }, { .value = 0 } },
    { 19, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = -1 }, { .value = 2010 }, { .storage = &D_neo_ark_forest_zone_80181E38 }, { .value = 2011 } },
    { 13, { .callbackNoArg = func_neo_ark_forest_zone_8017DA48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_neo_ark_forest_zone_801820A4[1] = {
    { D_neo_ark_forest_zone_80182274, D_neo_ark_forest_zone_801826B4, D_neo_ark_forest_zone_801829D0, NULL },
};

GpRoomCoordRec D_neo_ark_forest_zone_801820B4[1] = {
    { D_neo_ark_forest_zone_8018269C, NULL },
};

u8* D_neo_ark_forest_zone_801820BC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_forest_zone_801820C0[1] = {
    { { .bytes = { 6, 0 } } },
};

GpWarpRec D_neo_ark_forest_zone_801820C4[3] = {
    { { .words = { 3072, 8995, 0, -128 } }, { 0, 0, 0, 0 }, { .words = { 3072, 8995, 0, -128 } }, { 0, 0, 0, 0 }, 0x550B0002, 0x550B0001, 0, 2, 0, 434 },
    { { .words = { 1024, -7000, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 1024, -7000, 0, 0 } }, { 0, 0, 0, 0 }, 0x550B000C, 0x550B000B, 0, 5, 0, 0 },
    { { .words = { 0, -3000, 0, -770 } }, { 0, 0, 0, 0 }, { .words = { 0, -3000, 0, -770 } }, { 0, 0, 0, 0 }, 0x550B0004, 0x550B0003, 0, 4, 0, 0 },
};

SVECTOR D_neo_ark_forest_zone_8018216C[6] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_normals.inc"
};

SVECTOR D_neo_ark_forest_zone_8018219C[8] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_verts.inc"
};

GpGridFace D_neo_ark_forest_zone_801821DC[6] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_faces.inc"
};

s16 D_neo_ark_forest_zone_80182224[30] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_forest_zone_80182224[i])
s16* D_neo_ark_forest_zone_80182260[5] = {
#include "assets/neo_ark_forest_zone_collision_04CB4_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_forest_zone_80182274[1] = {
    { NULL, D_neo_ark_forest_zone_8018216C, D_neo_ark_forest_zone_8018219C, D_neo_ark_forest_zone_801821DC, D_neo_ark_forest_zone_80182260, 8000, 1300, 5, 1, 4000, 6 },
};

GpViewRec D_neo_ark_forest_zone_80182298[6] = {
    { { { { 4096, 0, 0 }, { 0, 78, -4095 }, { 0, 4095, 78 } }, { -1000, 0x7530, 575 } }, 380 },
    { { { { -1370, 0, -3859 }, { -488, 4063, 173 }, { 3828, 518, -1359 } }, { -3940, 1460, -810 } }, 230 },
    { { { { -1441, 0, -3833 }, { 76, 4095, -28 }, { 3833, -81, -1441 } }, { 1990, 870, -910 } }, 230 },
    { { { { -1589, 0, -3775 }, { 258, 4086, -108 }, { 3766, -280, -1585 } }, { 6420, 670, -1080 } }, 230 },
    { { { { -899, 0, 3996 }, { -547, 4057, -123 }, { -3958, -560, -890 } }, { 2120, 500, -690 } }, 230 },
    { { { { -905, 0, 3994 }, { -83, 4095, -19 }, { -3993, -86, -905 } }, { -1010, 1270, -860 } }, 329 },
};

SpriteBatch D_neo_ark_forest_zone_80182370[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_80182380[4] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -104, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 88 } }, -160, -120, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, -32, 1000, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 80 } }, -160, 40, 1000, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_801823D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_801823E8[5] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 72, 1604, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 32 } }, -160, -120, 1550, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, -160, -88, 1571, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 80 } }, -160, -40, 1611, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -160, 40, 1502, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_8018244C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_80182464[5] = {
    { 143, 0x3FC0, { .fields = { 96, 96 } }, 64, -120, 1050, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, 96, -24, 1050, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, 80, 24, 1050, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, 96, 32, 1050, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 64, 1050, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_801824C8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_forest_zone_801824E0[7] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -40, 1342, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 48 } }, 64, -40, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 48 } }, 56, 8, 1125, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 64 } }, 80, 56, 1125, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 80 } }, 24, -120, 1314, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 128, -120, 1125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -120, 1125, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_forest_zone_8018256C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_forest_zone_80182584[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_forest_zone_80182594[6] = {
    { { .empty = D_neo_ark_forest_zone_80182370 }, D_neo_ark_forest_zone_80182370, NULL },
    { { .elements = D_neo_ark_forest_zone_80182380 }, D_neo_ark_forest_zone_801823D0, NULL },
    { { .elements = D_neo_ark_forest_zone_801823E8 }, D_neo_ark_forest_zone_8018244C, NULL },
    { { .elements = D_neo_ark_forest_zone_80182464 }, D_neo_ark_forest_zone_801824C8, NULL },
    { { .elements = D_neo_ark_forest_zone_801824E0 }, D_neo_ark_forest_zone_8018256C, NULL },
    { { .empty = D_neo_ark_forest_zone_80182584 }, D_neo_ark_forest_zone_80182584, NULL },
};

WorldCoordPointLight D_neo_ark_forest_zone_801825DC[2] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4220, -1742, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3641, 3828, 3683 }, { 0, 0 } }, 6362, 0x3E20 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -252, -969, 623 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, 6, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4100, 4096 }, { 0, 0 } }, 1601, 2781 },
};

GpRoomCoordSet D_neo_ark_forest_zone_8018269C[1] = {
    { 0, NULL, 2, D_neo_ark_forest_zone_801825DC, 0, NULL },
};

GpObj4C D_neo_ark_forest_zone_801826B4[6] = {
    { NULL, NULL, NULL, { 6141, -2640, 12, 0 }, { { -242, -2976, -3011, 0 }, { 231, -2976, 3006, 0 }, { -242, 2976, -3011, 0 }, { 231, 2976, 3006, 0 } }, { 4092, 0, -323, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 6407, -2672, -38, 0 }, { { 217, -2976, 2875, 0 }, { -219, -2976, -2877, 0 }, { 217, 2976, 2875, 0 }, { -219, 2976, -2877, 0 } }, { -4085, 0, 309, 0 }, { 0, 0, 4096, 0 }, 4127, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 303, -2625, -17, 0 }, { { -927, -3088, -2918, 0 }, { 922, -3088, 2912, 0 }, { -927, 3088, -2918, 0 }, { 922, 3088, 2912, 0 } }, { 3905, 0, -1239, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 511, -2674, -49, 0 }, { { 900, -2976, 2902, 0 }, { -907, -2976, -2908, 0 }, { 900, 2977, 2902, 0 }, { -907, 2977, -2908, 0 } }, { -3927, 0, 1221, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -4322, -2641, 94, 0 }, { { -289, -3104, -3782, 0 }, { 264, -3104, 3755, 0 }, { -289, 3105, -3782, 0 }, { 264, 3105, 3755, 0 } }, { 4086, 0, -301, 0 }, { 0, 0, 4096, 0 }, 4884, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -4176, -2657, 110, 0 }, { { 192, -3056, 3755, 0 }, { -200, -3056, -3764, 0 }, { 192, 3057, 3755, 0 }, { -200, 3057, -3764, 0 } }, { -4098, 0, 213, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 5, 4, 129, 0 },
};

GpAreaTmdRec D_neo_ark_forest_zone_8018287C[3] = {
    { 13, 13, 3, 0, { 0, 0 }, D_80158A18 },
    { 10, 561, 2, 0, { 0, 0 }, D_80173294 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_forest_zone_801828A0[2] = {
    { 13, 13, 3, 0, { 0, 0 }, D_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_forest_zone_801828B8[2] = {
    { 13, 13, 3, 0, { 0, 0 }, D_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_forest_zone_801828D0[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_forest_zone_801828E8[3] = {
    { 13, 13, 3, 0, { 0, 0 }, D_80158A18 },
    { 20, 20, 2, 0, { 0, 0 }, D_80177DF0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_forest_zone_8018290C[3] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 56, 56, 1, 0, { 0, 0 }, D_801602C0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_forest_zone_80182930[2] = {
    { 13, 13, 3, 0, { 0, 0 }, D_80158A18 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_neo_ark_forest_zone_80182948[2] = {
    { 13, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_neo_ark_forest_zone_80182968[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B190, D_neo_ark_forest_zone_8018287C },
    { D_map_neo_ark_8017B1C0, D_neo_ark_forest_zone_801828A0 },
    { D_map_neo_ark_8017B1F0, D_neo_ark_forest_zone_801828B8 },
    { D_map_neo_ark_8017B210, D_neo_ark_forest_zone_801828D0 },
    { D_map_neo_ark_8017B2C0, D_neo_ark_forest_zone_801828E8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_neo_ark_forest_zone_80182948, D_neo_ark_forest_zone_80182930 },
    { D_map_neo_ark_8017B300, D_neo_ark_forest_zone_8018290C },
    { NULL, NULL },
};

GpObj4C D_neo_ark_forest_zone_801829D0[10] = {
    { NULL, NULL, NULL, { 9488, -48, 0, 0 }, { { -688, 0, -1024, 0 }, { 688, 0, -1024, 0 }, { -688, 0, 1024, 0 }, { 688, 0, 1024, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1227, 0, 10, 18, 2, 0 },
    { NULL, NULL, NULL, { -7680, -48, -16, 0 }, { { -592, 0, -880, 0 }, { 592, 0, -880, 0 }, { -592, 0, 880, 0 }, { 592, 0, 880, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1055, 0, 12, 33, 2, 0 },
    { NULL, NULL, NULL, { -2480, -48, -928, 0 }, { { 1040, 0, -400, 0 }, { 1008, 0, 400, 0 }, { -1008, 0, -400, 0 }, { -1040, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1108, 0, 29, 49, 2, 0 },
    { NULL, NULL, NULL, { 6768, -64, -96, 0 }, { { -480, 0, -1792, 0 }, { 224, 0, -1792, 0 }, { -224, 0, 1792, 0 }, { 480, 0, 1792, 0 } }, { 0, 4109, 0, 0 }, { -4096, 0, 0, 0 }, 1854, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 4399, -64, -33, 0 }, { { -1851, 0, -1772, 0 }, { 1541, 0, -1796, 0 }, { -1541, 0, 1796, 0 }, { 1851, 0, 1772, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 2560, 0x8005, 2, 0, 3, 0 },
    { NULL, NULL, NULL, { -626, -64, 46, 0 }, { { -1476, 0, -1706, 0 }, { 371, 0, -1800, 0 }, { -370, 0, 1801, 0 }, { 1477, 0, 1707, 0 } }, { 0, 4101, 0, 0 }, { -4096, 0, 0, 0 }, 2246, 0x8005, 3, 0, 3, 0 },
    { NULL, NULL, NULL, { -6624, -64, -16, 0 }, { { -528, 0, -1744, 0 }, { 528, 0, -1744, 0 }, { -528, 0, 1744, 0 }, { 528, 0, 1744, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1819, 0x8005, 4, 0, 3, 0 },
    { NULL, NULL, NULL, { -4959, -64, 0, 0 }, { { -720, 0, -1696, 0 }, { 464, 0, -1696, 0 }, { -464, 0, 1696, 0 }, { 720, 0, 1696, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1841, 0x8005, 5, 0, 3, 0 },
    { NULL, NULL, NULL, { 992, -64, 1184, 0 }, { { 9328, 0, -400, 0 }, { 9296, 0, 400, 0 }, { -9296, 0, -400, 0 }, { -9328, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4097, 0 }, 9328, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 1024, -64, -1120, 0 }, { { 9328, 0, -400, 0 }, { 9296, 0, 400, 0 }, { -9296, 0, -400, 0 }, { -9328, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4095, 0 }, 9328, 2, 4, 0, 130, 0 },
};

s32 D_neo_ark_forest_zone_80182CC8[3] = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

GpRoomParamRec D_neo_ark_forest_zone_80182CD4[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_forest_zone_80182CDC[1] = {
    { 0, 0, 1, 0, D_neo_ark_forest_zone_80182CC8 },
};

GpRoomParamRec* D_neo_ark_forest_zone_80182CE4[8] = {
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CDC,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
    D_neo_ark_forest_zone_80182CD4,
};

DamageAttack D_neo_ark_forest_zone_80182D04[6] = {
    { 30, 7 },
    { 30, 7 },
    { 50, 7 },
    { 50, 7 },
    { 40, 0 },
    { 40, 0 },
};

EnemyParams gRoamerParams = { D_neo_ark_forest_zone_80182D04, 420, 115, 200, 5, 100, 10, 100, 10 };

// Retained numeric records following the enemy parameters.
u16 D_neo_ark_forest_zone_80182D2C[3][4] = {
    { 0, 900, 3, 0 },
    { 0, 800, 5, 0 },
    { 0, 500, 7, 0 },
};

u8 gRoamerArmCountsB[16] = {
    0,
    1,
    3,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u8 gRoamerArmCountsA[14] = {
    0,
    3,
    2,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

s16 gRoamerCooldown = 30;

s16 gRoamerReserveCount = 0;

s16 gRoamerSpawnRequest = 0;

s16 gRoamerLastRequest = 0;

s16 gRoamerReleasePending = 0;

NeoArkForestZone2MsgEntry gRoamerMsgTableA[4] = {
    { 5103, { .call0 = roamerLatchRequest } },
    { 5108, { .call2 = roamerBankRetreat } },
    { 2011, { .call0 = func_neo_ark_forest_zone_801813BC } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

NeoArkForestZoneSpawnPos gRoamerSpawnPointsA[7] = {
    { -2000, 0, 8977, -1024 },
    { 379, 0, 7700, 2048 },
    { 7950, 0, 4650, -1024 },
    { 7950, 0, -4650, -1024 },
    { -633, 0, -1000, 2048 },
    { -2280, 0, -6378, -1024 },
    { 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
};

s16 gRoamerPrevBattleRefs = 0;

GpMsgEntry gRoamerMsgTableB[4] = {
    { 5103, func_neo_ark_forest_zone_801814B0 },
    { 5108, func_neo_ark_forest_zone_80181494 },
    { ACTOR_COMMAND_MESSAGE_APPLY, roamerAmbushMsg },
    { 0x7FFFFFFF, NULL },
};

NeoArkForestZoneSpawnPos D_neo_ark_forest_zone_80182DE8[5] = {
    { 8884, 0, 2200, 2048 },
    { 0, 0, -2000, 200 },
    { -4200, 0, -3000, 0 },
    { -4100, 0, 2000, 1900 },
    { -6500, 0, 2000, 2200 },
};

s16 D_neo_ark_forest_zone_80182E10[4] = {
    0x7FFF,
    0x7FFF,
    0x7FFF,
    0x7FFF,
};

TaskDesc D_neo_ark_forest_zone_80182E18 = { 0, 32, func_neo_ark_forest_zone_8018151C, { .model = NULL } };

TaskDesc D_neo_ark_forest_zone_80182E24 = { 0, 32, func_neo_ark_forest_zone_80181430, { .model = NULL } };

static void func_neo_ark_forest_zone_80180D24(Task* arg0);

static void func_neo_ark_forest_zone_80181508(Task* arg0);

#include "../../shared/roaming_enemies_bank_retreat.inc.c"

#include "../../shared/roaming_enemies_arm_pool_a.inc.c"

#include "../../shared/roaming_enemies_tick_pool_a.inc.c"

#include "../../shared/roaming_enemies_ambush_msg.inc.c"

#include "../../shared/roaming_enemies_arm_pool_b.inc.c"

static void func_neo_ark_forest_zone_80180D24(Task* arg0)
{
    s16    i;
    s16    count;
    s32    a;
    s32    b;
    Enemy* obj;
    s16    j;
    s16    k;

    gameGetPtrSlot(3);
    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (gRoamerCooldown > 0) {
        gRoamerCooldown--;
    }
    if (Gp_StateF0.field_6 == 0 && gRoamerPrevBattleRefs > 0) {
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", b - count);
        a     = GameFlag_GetNibble(0x167);
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x167, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10A, count);
        areaSyncLocationVariant(&gGameSession->location.loc);
        gRoamerCooldown = 0x96;
    }
    gRoamerPrevBattleRefs = Gp_StateF0.field_6;
    if (gGameSession->battleResetPending == 1 && gRoamerCooldown == 0) {
        Gp_StateF0.prefix.bytes.field_0  = 0;
        Gp_StateF0.field_5               = 0;
        Gp_StateF0.field_6               = 0;
        Gp_StateF0.field_8               = 0;
        Gp_StateF0.field_C               = 0;
        Gp_StateF0.field_10              = 0;
        gGameSession->battleResetPending = 0;
    }
    if (Gp_StateF0.prefix.bytes.field_0 != 2 && gRoamerSpawnRequest != 0) {
        gRoamerCommand.context.loc.stage = 5;
        gRoamerCommand.context.loc.area  = 0xB;
        gRoamerCommand.command           = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = Gp_LookupSlot4(i)->spawnArg2.pointer;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < gRoamerReserveCount; j++) {
                    if (((s16*)gRoamerReserveHp)[j] > 0) {
                        obj->hp             = gRoamerReserveHp[j];
                        obj->reactionFlags  = 0;
                        gRoamerReserveHp[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    Gp_IncStateF0Ref(0);
                    gRoamerCooldown += 0x5A;
                    Gp_DispatchMsgPtr(Gp_LookupSlot4(i), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    switch ((s16)(gRoamerSpawnRequest - 1)) {
                        case 0:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_forest_zone_80182DE8[0].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1]   = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_forest_zone_80182DE8[0].z;
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            gfxRotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[0].rotY, 1);
                            break;
                        case 1:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0]   = D_neo_ark_forest_zone_80182DE8[1].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1]   = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2]   = D_neo_ark_forest_zone_80182DE8[1].z;
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            gfxRotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[1].rotY, 1);
                            break;
                        case 2:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[2].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1] = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[2].z;
                            gfxRotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[2].rotY, 1);
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 3:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[3].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1] = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[3].z;
                            gfxRotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[3].rotY, 1);
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                        case 4:
                        default:
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[4].x;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1] = 0;
                            Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[4].z;
                            gfxRotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                          D_neo_ark_forest_zone_80182DE8[4].rotY, 1);
                            Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                            break;
                    }
                }
                break;
            }
        }
    }
    gRoamerSpawnRequest = 0;
}

s32 func_neo_ark_forest_zone_801813BC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/* Publishes the byte at 0x2 of `arg2` as `gRoamerSpawnRequest` only
 * while the counter is idle, and remembers the byte in `D_...80182D68` either
 * way; a change arriving while the counter runs is suppressed to zero. */
#include "../../shared/roaming_enemies_latch_request.inc.c"

static void func_neo_ark_forest_zone_8018141C(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// The first arming task: runs the state handler its state selects, through a
/// copy of the state table on the stack.
void func_neo_ark_forest_zone_80181430(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D5E8;
    sp.funcs[task->state](task);
}

s32 func_neo_ark_forest_zone_80181494(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    gRoamerCooldown += 0x5A;
    return 1;
}

/* The same latch as roamerLatchRequest directly above, emitted a
 * second time at 0x801814B0 - two objects in the overlay, so shared code
 * cannot cover it. */
/// A further copy, under this file's own name.
#define roamerLatchRequest func_neo_ark_forest_zone_801814B0
#include "../../shared/roaming_enemies_latch_request.inc.c"
#undef roamerLatchRequest

static void func_neo_ark_forest_zone_80181508(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// State table of the second arming task, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_forest_zone_8017D634 = { {
    roamerArmPoolB,
    func_neo_ark_forest_zone_80180D24,
    func_neo_ark_forest_zone_80181508,
    taskKill,
} };

/// The second arming task: runs the state handler its state selects, through
/// a copy of the state table on the stack.
void func_neo_ark_forest_zone_8018151C(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D634;
    sp.funcs[task->state](task);
}
