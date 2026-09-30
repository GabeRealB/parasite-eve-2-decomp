#include "rooms/shelter_b6_corridor.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#define D_shelter_b6_corridor_8017F844 (D_shelter_b6_corridor_8017F834 + 2)
#define D_shelter_b6_corridor_8017F874 (D_shelter_b6_corridor_8017F834 + 8)

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_shelter_b6_corridor_8017EF20;

/// The room task's message records.
extern GpMsgEntry D_shelter_b6_corridor_8017EF24[];

extern GpEvsCmd D_shelter_b6_corridor_8017F354[];
extern GpEvsCmd D_shelter_b6_corridor_8017F684[];

/// The context the wave task was spawned with: its ramp limit, peak, mode and
/// tint.
extern OverlayWaveCtx* D_shelter_b6_corridor_80180568;

/// Phase records of the wave's 9 column edges and 30 row edges.
extern OverlayWaveRec D_shelter_b6_corridor_8018056C[10];
extern OverlayWaveRec D_shelter_b6_corridor_801805BC[30];

/// The two 8 by 30 quad grids, one per frame-buffer half.
// The task starts at row 1 and draws rows -1 through 28.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    POLY_FT4 value[2][30][8];
    u8       retained[4];
} ShelterB6CorridorStorage06AC;
STATIC_ASSERT_SIZEOF(ShelterB6CorridorStorage06AC, 19204);

extern ShelterB6CorridorStorage06AC D_shelter_b6_corridor_801806AC;

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s16 value;
    u8  retained[6];
} ShelterB6CorridorStorage51B0;
STATIC_ASSERT_SIZEOF(ShelterB6CorridorStorage51B0, 8);

extern ShelterB6CorridorStorage51B0 D_shelter_b6_corridor_801851B0;
extern s32                          D_shelter_b6_corridor_801851B8;

static void func_shelter_b6_corridor_8017E360(SVECTOR* arg0, s32 arg1, s32 arg2);

// Indexed views below share one contiguous table.
extern AnimationPlayRequest D_shelter_b6_corridor_8017F27C;
extern ActorCommand         D_shelter_b6_corridor_8017F34C;
extern ActorCommand         D_shelter_b6_corridor_8017F350;
extern GpCopyArg            D_shelter_b6_corridor_8017F260;
void                        func_shelter_b6_corridor_8017E19C(s32);
void                        func_shelter_b6_corridor_8017E204(void);

void func_shelter_b6_corridor_8017E19C(s32);
void func_shelter_b6_corridor_8017E204(void);

s32  func_shelter_b6_corridor_8017DEA8(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b6_corridor_8017DEB0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b6_corridor_8017DF48(Task*, s32, s32, GpMessageArg);
s32  func_shelter_b6_corridor_8017E020(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b6_corridor_8017E028(Task*, s32, GpMessageArg, GpMessageArg);
void func_shelter_b6_corridor_8017D5D0(Task*);

TaskDesc D_shelter_b6_corridor_8017EF08[2] = {
    { 0, 192, func_shelter_b6_corridor_8017D5D0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_shelter_b6_corridor_8017EF20 = 256;

GpMsgEntry D_shelter_b6_corridor_8017EF24[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b6_corridor_8017DEB0 },
    { 5105, func_shelter_b6_corridor_8017DEA8 },
    { 5103, func_shelter_b6_corridor_8017E020 },
    { 5104, func_shelter_b6_corridor_8017DF48 },
    { 5108, func_shelter_b6_corridor_8017E028 },
    { 0x7FFFFFFF, NULL },
};

AnimationPackedPose D_shelter_b6_corridor_8017EF54[6] = {
#include "assets/shelter_b6_corridor_animation_01C70_bank1.inc"
};

AnimationPackedRotation D_shelter_b6_corridor_8017EF9C[46] = {
#include "assets/shelter_b6_corridor_animation_01C70_bank4.inc"
};

AnimationRecord D_shelter_b6_corridor_8017F054[109] = {
#include "assets/shelter_b6_corridor_animation_01C70_records.inc"
};

u16 D_shelter_b6_corridor_8017F208[20] = {
#include "assets/shelter_b6_corridor_animation_01C70_indices.inc"
};

AnimationSet D_shelter_b6_corridor_8017F230 = {
    D_shelter_b6_corridor_8017F054,
    D_shelter_b6_corridor_8017F208,
    { NULL, D_shelter_b6_corridor_8017EF54, NULL, NULL, D_shelter_b6_corridor_8017EF9C, NULL, NULL, NULL },
};

AnimationSet* D_shelter_b6_corridor_8017F258[2] = {
    NULL,
    &D_shelter_b6_corridor_8017F230,
};

GpCopyArg D_shelter_b6_corridor_8017F260 = { { .sets = D_shelter_b6_corridor_8017F258 }, 2 };

AnimationPlayRequest D_shelter_b6_corridor_8017F268 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_corridor_8017F27C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_shelter_b6_corridor_8017F290[5] = {
    { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_shelter_b6_corridor_8017F2F4 = { { 7000, 0, 0, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_shelter_b6_corridor_8017F30C = { { 7800, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_shelter_b6_corridor_8017F324 = { { 0x4E20, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorCommand D_shelter_b6_corridor_8017F33C = { { .loc = { 5, 24 } }, 1 };

ActorCommand D_shelter_b6_corridor_8017F340 = { { .loc = { 5, 24 } }, 2 };

GpSpawnAnimArg D_shelter_b6_corridor_8017F344 = { 3, 4 };

ActorCommand D_shelter_b6_corridor_8017F34C = { { .loc = { 5, 24 } }, 1 };

ActorCommand D_shelter_b6_corridor_8017F350 = { { .loc = { 5, 24 } }, 1 };

GpEvsCmd D_shelter_b6_corridor_8017F354[34] = {
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b6_corridor_8017F260 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_corridor_8017F27C }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b6_corridor_8017E204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F34C } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F350 } }, { .value = 0 } },
    { 3, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F33C } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_shelter_b6_corridor_8017F2F4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_shelter_b6_corridor_8017F30C }, { .storage = &D_shelter_b6_corridor_8017F344 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 15, { .value = 0x55180004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F340 } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2013 }, { .storage = &D_shelter_b6_corridor_8017F324 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b6_corridor_8017E19C }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_shelter_b6_corridor_8017F684[18] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_shelter_b6_corridor_8017E204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_shelter_b6_corridor_8017F260 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_shelter_b6_corridor_8017F27C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F34C } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_shelter_b6_corridor_8017F350 } }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 11, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_shelter_b6_corridor_8017E19C }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_shelter_b6_corridor_8017F834[16] = {
    { 2000, -3250, 1450, 0 },
    { 2000, -400, 1450, 0 },
    { 4000, -3250, 1450, 0 },
    { 4000, -400, 1450, 0 },
    { 6000, -3250, 1450, 0 },
    { 6000, -400, 1450, 0 },
    { 8000, -3250, 1450, 0 },
    { 8000, -400, 1450, 0 },
    { 2000, -3250, -1450, 0 },
    { 2000, -400, -1450, 0 },
    { 4000, -3250, -1450, 0 },
    { 4000, -400, -1450, 0 },
    { 6000, -3250, -1450, 0 },
    { 6000, -400, -1450, 0 },
    { 8000, -3250, -1450, 0 },
    { 8000, -400, -1450, 0 },
};

u8* D_shelter_b6_corridor_8017F8B4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b6_corridor_8017F8B8[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_shelter_b6_corridor_8017F8BC[2] = {
    { { .words = { 1024, 430, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 1024, 430, 0, 0 } }, { 0, 0, 0, 0 }, 0x55180001, 0, 0, 4, 0, 0 },
    { { .words = { 3072, 8400, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 3072, 8400, 0, 0 } }, { 0, 0, 0, 0 }, 0, 0x55180003, 0, 3, 0, 0 },
};

SVECTOR D_shelter_b6_corridor_8017F92C[6] = {
#include "assets/shelter_b6_corridor_collision_024D0_normals.inc"
};

SVECTOR D_shelter_b6_corridor_8017F95C[12] = {
#include "assets/shelter_b6_corridor_collision_024D0_verts.inc"
};

GpGridFace D_shelter_b6_corridor_8017F9BC[12] = {
#include "assets/shelter_b6_corridor_collision_024D0_faces.inc"
};

s16 D_shelter_b6_corridor_8017FA4C[28] = {
#include "assets/shelter_b6_corridor_collision_024D0_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b6_corridor_8017FA4C[i])
s16* D_shelter_b6_corridor_8017FA84[3] = {
#include "assets/shelter_b6_corridor_collision_024D0_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b6_corridor_8017FA90 = { NULL, D_shelter_b6_corridor_8017F92C, D_shelter_b6_corridor_8017F95C, D_shelter_b6_corridor_8017F9BC, D_shelter_b6_corridor_8017FA84, 0, 1350, 3, 1, 4000, 12 };

GpViewRec D_shelter_b6_corridor_8017FAB4[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -4500, 0x61A8, 0 } }, 680 },
    { { { { 715, 0, 4033 }, { 264, 4087, -46 }, { -4024, 268, 713 } }, { -7040, 1470, 560 } }, 230 },
    { { { { 682, 0, -4038 }, { -252, 4087, -42 }, { 4030, 256, 681 } }, { -1740, 1470, 560 } }, 230 },
    { { { { 955, 0, 3982 }, { -1522, 3784, 365 }, { -3680, -1565, 883 } }, { -3190, 170, 810 } }, 230 },
    { { { { 0, 0, -4096 }, { -860, 4004, 0 }, { 4004, 860, 0 } }, { -6235, 1350, 0 } }, 329 },
};

SpriteBatch D_shelter_b6_corridor_8017FB68[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_corridor_8017FB78[35] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 32, 1246, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -120, 906, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -88, 1012, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -56, 1009, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -24, 1100, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, 8, 1037, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 1007, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -112, 1025, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -80, 1170, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -48, 1167, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -16, 1162, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 16, 1162, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -96, 1174, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -72, 1278, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -48, 1287, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -24, 1296, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 0, 1303, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 24, 1288, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -88, 1256, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 615, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 586, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 591, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 605, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 40, 614, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 556, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -120, 649, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -80, 656, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -40, 662, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 0, 661, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 40, 660, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 598, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 16, 672, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 40, 677, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -136, 64, 672, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 72, 643, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_corridor_8017FE34[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b6_corridor_8017FE54[22] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -104, 1199, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -72, 1396, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -40, 1450, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -8, 1382, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 24, 1363, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -96, 1422, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -80, 1503, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -56, 1564, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 1581, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -8, 1590, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 16, 1563, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -120, 745, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 745, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -40, 702, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -120, 830, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -80, 926, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 104, -40, 895, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 0, 822, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 112, 40, 799, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 0, 951, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 32, 944, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, -120, 927, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b6_corridor_8018000C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_corridor_8018002C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b6_corridor_8018003C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b6_corridor_8018004C[5] = {
    { { .empty = D_shelter_b6_corridor_8017FB68 }, D_shelter_b6_corridor_8017FB68, NULL },
    { { .elements = D_shelter_b6_corridor_8017FB78 }, D_shelter_b6_corridor_8017FE34, NULL },
    { { .elements = D_shelter_b6_corridor_8017FE54 }, D_shelter_b6_corridor_8018000C, NULL },
    { { .empty = D_shelter_b6_corridor_8018002C }, D_shelter_b6_corridor_8018002C, NULL },
    { { .empty = D_shelter_b6_corridor_8018003C }, D_shelter_b6_corridor_8018003C, NULL },
};

GpPointLight D_shelter_b6_corridor_80180088[1] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4600, -2750, -50 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 5000, 0x2710 },
};

GpRoomCoordSet D_shelter_b6_corridor_801800E8 = { 0, NULL, 1, D_shelter_b6_corridor_80180088, 0, NULL };

GpObj4C D_shelter_b6_corridor_80180100[6] = {
    { NULL, NULL, NULL, { 4576, -2065, 96, 0 }, { { 0, -2528, 2144, 0 }, { 0, -2528, -2144, 0 }, { 0, 2528, 2144, 0 }, { 0, 2528, -2144, 0 } }, { -4112, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 4384, -2177, 128, 0 }, { { 0, -2480, -2256, 0 }, { 0, -2480, 2256, 0 }, { 0, 2480, -2256, 0 }, { 0, 2480, 2256, 0 } }, { 4110, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 1245, -1952, 924, 0 }, { { -571, -2528, 1161, 0 }, { 572, -2528, -1161, 0 }, { -571, 2528, 1161, 0 }, { 572, 2528, -1161, 0 } }, { -3684, 0, -1813, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 1048, -1952, 1047, 0 }, { { 611, -2480, -1198, 0 }, { -612, -2480, 1198, 0 }, { 611, 2480, -1198, 0 }, { -612, 2480, 1198, 0 } }, { 3657, 0, 1865, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 4, 1, 0 },
    { NULL, NULL, NULL, { 1567, -2080, -1409, 0 }, { { 233, -2528, 1273, 0 }, { -232, -2528, -1273, 0 }, { 233, 2528, 1273, 0 }, { -232, 2528, -1273, 0 } }, { -4038, 0, 736, 0 }, { 0, 0, 4096, 0 }, 2839, 0, 4, 2, 1, 0 },
    { NULL, NULL, NULL, { 1439, -2016, -1409, 0 }, { { -222, -2480, -1327, 0 }, { 223, -2480, 1327, 0 }, { -222, 2480, -1327, 0 }, { 223, 2480, 1327, 0 } }, { 4050, 0, -680, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 4, 129, 0 },
};

GpAreaTmdRec D_shelter_b6_corridor_801802C8[5] = {
    { 131, 505, 2, 0, { 0, 0 }, D_80168EA4 },
    { 49, 49, 0, 0, { 0, 0 }, D_80147400 },
    { 52, 52, 1, 0, { 0, 0 }, D_8014CA60 },
    { 60, 60, 1, 0, { 0, 0 }, D_801567C4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_b6_corridor_80180304[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C0F0, D_shelter_b6_corridor_801802C8 },
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

GpObj4C D_shelter_b6_corridor_8018036C[5] = {
    { NULL, NULL, NULL, { 512, -48, 0, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1144, 0, 9, 17, 2, 0 },
    { NULL, NULL, NULL, { 8512, -48, 0, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1144, 0, 25, 33, 2, 0 },
    { NULL, NULL, NULL, { 2752, -64, 704, 0 }, { { -1056, 0, -608, 0 }, { 1056, 0, -608, 0 }, { -1056, 0, 608, 0 }, { 1056, 0, 608, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, -4096, 0 }, 1214, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, -688, 0 }, { { -1056, 0, -688, 0 }, { 1056, 0, -688, 0 }, { -1056, 0, 688, 0 }, { 1056, 0, 688, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, 4096, 0 }, 1254, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { 6720, -64, 688, 0 }, { { -1056, 0, -656, 0 }, { 1056, 0, -656, 0 }, { -1056, 0, 656, 0 }, { 1056, 0, 656, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 1241, 2, 4, 255, 130, 0 },
};

GpRoomBoundVec D_shelter_b6_corridor_801804E8[6] = {
    { 5, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 392, 437, 278, 400 },
    { 393, 435, 277, 399 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_shelter_b6_corridor_80180518[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

s32 D_shelter_b6_corridor_80180524[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b6_corridor_80180530[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b6_corridor_80180538[1] = {
    { 0, 0, 1, 0, D_shelter_b6_corridor_80180518 },
};

GpRoomParamRec D_shelter_b6_corridor_80180540[1] = {
    { 0, 0, 1, 0, D_shelter_b6_corridor_80180524 },
};

GpRoomParamRec* D_shelter_b6_corridor_80180548[8] = {
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180538,
    D_shelter_b6_corridor_80180540,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
    D_shelter_b6_corridor_80180530,
};

OverlayWaveCtx* D_shelter_b6_corridor_80180568 = NULL;

// Nine active columns and one retained zero entry.
OverlayWaveRec D_shelter_b6_corridor_8018056C[10] = { 0 };

OverlayWaveRec D_shelter_b6_corridor_801805BC[30] = { 0 };

ShelterB6CorridorStorage06AC D_shelter_b6_corridor_801806AC = { { 0 }, { 0 } };

ShelterB6CorridorStorage51B0 D_shelter_b6_corridor_801851B0;

s32 D_shelter_b6_corridor_801851B8;

static void func_shelter_b6_corridor_8017E064(Task* arg0);
static void func_shelter_b6_corridor_8017E12C(Task* task);

/// Task that ripples the whole screen. On its first frame it gives each of the
/// 9 column and 30 row edges a random phase offset and speed, takes its
/// context from `spawnArg2`, passes -8 to `displaySetShakeY` and builds
/// two 8 by 30 grids of `POLY_FT4`s sampling the two frame-buffer halves,
/// tinted with the context's colour when its tint flag is set. Afterwards the
/// context's mode ramps the strength up to its limit (mode 0), back down to
/// zero and on to mode 2 (mode 1), or ends the task and passes 0 back
/// (mode 2); the displacement is the ramp's share of the context's peak. The
/// edges advance while `Gp_StateF0.field_4` is clear, and the current
/// buffer's grid is drawn with every corner pushed by sine waves of that
/// displacement, bracketed by draw-mode packets that turn mask-bit setting on
/// at the back of the order table and off again at the front.
void func_shelter_b6_corridor_8017D5D0(Task* arg0)
{
    OverlayWaveScratch* scratch;
    OverlayWaveScratch* head;
    OverlayWaveCtx*     ctx;
    OverlayWaveRec*     cols;
    POLY_FT4*           p;
    DR_STP*             stp;
    s32                 i;
    s32                 j;
    s32                 k;
    s32                 rowIndex;
    s32                 rowBack;
    s32                 u0;
    s32                 u1;
    s32                 v0;
    s32                 v1;
    s32                 waveX0;
    s32                 waveY0;
    s32                 waveX1;
    s32                 waveY1;
    s32                 waveX2;
    s32                 waveY2;
    s32                 waveX3;
    s32                 waveY3;
    OverlayWaveRec*     row;
    POLY_FT4(*grid)
    [8];
    s32 tpage0;
    s32 tpage1;

    head                             = SCRATCH_HEAD(OverlayWaveScratch);
    CdCmd_Queue.imageMdecMode        = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    SCRATCH_HEAD(OverlayWaveScratch) = head - 1;
    cols                             = head[-1].cols;
    scratch                          = head - 1;
    switch (arg0->state) {
        case 0:
            for (i = 0; i < 9; i++) {
                D_shelter_b6_corridor_8018056C[i].phase  = 0;
                D_shelter_b6_corridor_8018056C[i].offset = (u32)rand() >> 3;
                D_shelter_b6_corridor_8018056C[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            for (i = 0; i < 30; i++) {
                D_shelter_b6_corridor_801805BC[i].phase  = 0;
                D_shelter_b6_corridor_801805BC[i].offset = (u32)rand() >> 3;
                D_shelter_b6_corridor_801805BC[i].speed  = ((rand() * 100) >> 15) + 20;
            }
            D_shelter_b6_corridor_8017EF20        = 0;
            D_shelter_b6_corridor_80180568        = arg0->spawnArg2.pointer;
            D_shelter_b6_corridor_80180568->frame = 0;
            D_shelter_b6_corridor_80180568->state = 0;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            for (i = 0; i < 2; i++) {
                tpage0 = getTPage(2, 0, 0, i << 8);
                tpage1 = getTPage(2, 0, 128, i << 8);
                grid   = &D_shelter_b6_corridor_801806AC.value[i][1];
                for (j = -1; j < 29; j++) {
                    p = grid[j];
                    for (k = 0; k < 8; p++, k++) {
                        setPolyFT4(p);
                        if (D_shelter_b6_corridor_80180568->blend == ANIMATION_BLEND_RESET) {
                            setShadeTex(p, 1);
                        } else {
                            setShadeTex(p, 0);
                            p->r0 = D_shelter_b6_corridor_80180568->r;
                            p->g0 = D_shelter_b6_corridor_80180568->g;
                            p->b0 = D_shelter_b6_corridor_80180568->b;
                        }
                        u0 = k * 40;
                        u1 = (k + 1) * 40;
                        if (u1 == 320) {
                            u1 = 319;
                        }
                        if (u0 < 128) {
                            p->tpage = tpage0;
                        } else {
                            p->tpage = tpage1;
                            u0      -= 128;
                            u1      -= 128;
                        }
                        v1 = (j + 1) * 8 + i * 16;
                        if (j != -1) {
                            v0 = j * 8 + i * 16;
                        } else {
                            v0 = i * 16 + 8;
                            v1 = i * 16;
                        }
                        p->u0 = u0;
                        p->v0 = v0;
                        p->u1 = u1;
                        p->v1 = v0;
                        do {
                            p->u2 = u0;
                            p->v2 = v1;
                            p->u3 = u1;
                        } while (0);
                        p->v3 = v1;
                    }
                }
            }
            arg0->state++;
            break;
        case 1:
            ctx = D_shelter_b6_corridor_80180568;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        if (Gp_StateF0.field_4 == 0) {
                            ctx->frame--;
                        }
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    displaySetShakeY(0);
                    break;
            }
            D_shelter_b6_corridor_8017EF20 = D_shelter_b6_corridor_80180568->frame * D_shelter_b6_corridor_80180568->scale / D_shelter_b6_corridor_80180568->span;
            for (i = 0; i < 9; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b6_corridor_8018056C[i].phase += D_shelter_b6_corridor_8018056C[i].speed;
                }
                *(s32*)&cols[i] = *(s32*)&D_shelter_b6_corridor_8018056C[i];
            }
            for (i = 0; i < 30; i++) {
                if (Gp_StateF0.field_4 == 0) {
                    D_shelter_b6_corridor_801805BC[i].phase += D_shelter_b6_corridor_801805BC[i].speed;
                }
                *(s32*)&scratch->rows[i] = *(s32*)&D_shelter_b6_corridor_801805BC[i];
            }
            rowIndex = -1;
            for (j = -1; j < 29; rowIndex += 2, j++, rowIndex--) {
                rowBack = -rowIndex;
                row     = scratch->rows - rowBack;
                grid    = &D_shelter_b6_corridor_801806AC.value[gDisplayState.drawBuffer][1];
                p       = grid[j];
                for (k = 0; k < 8; k++, p++) {
                    if (j != -1) {
                        waveX0 = D_shelter_b6_corridor_8017EF20 * (rsin((j << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x0  = k * 40 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_shelter_b6_corridor_8017EF20 * (rsin((k << 10) + row->phase + row->offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_shelter_b6_corridor_8017EF20 * (rsin((j << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 40 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_shelter_b6_corridor_8017EF20 * (rsin(((k + 1) << 10) + row->phase + row->offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        p->x0 = k * 40 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 40 - 160;
                        p->y1 = -112;
                    }
                    {
                        OverlayWaveRec* next = row + 1;
                        waveX2               = D_shelter_b6_corridor_8017EF20 * (rsin(((j + 1) << 9) + cols[k].phase + cols[k].offset) << 3);
                        p->x2                = k * 40 + (s16)((waveX2 >> 20) - 160);
                        waveY2               = D_shelter_b6_corridor_8017EF20 * (rsin((k << 10) + row[1].phase + next->offset) << 3);
                        p->y2                = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3               = D_shelter_b6_corridor_8017EF20 * (rsin(((j + 1) << 9) + cols[k + 1].phase + cols[k + 1].offset) << 3);
                        p->x3                = (k + 1) * 40 + (s16)((waveX3 >> 20) - 160);
                        waveY3               = D_shelter_b6_corridor_8017EF20 * (rsin(((k + 1) << 10) + row[1].phase + next->offset) << 3);
                        p->y3                = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    addPrim(&gGpuCurrentOt[3], p);
                }
                SOFT_USE_REG(p);
            }
            break;
    }
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[1023], stp);
    stp            = gGpuPrimCursor;
    gGpuPrimCursor = stp + 1;
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[0], stp);
    SCRATCH_POP(OverlayWaveScratch);
}

s32 func_shelter_b6_corridor_8017DEA8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017DEB0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u16 id;
    s32 k;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    k  = in->areaId;
    id = k;
    k  = 0x19;
    if (id == 9) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(1);
        }
        return 0;
    }
    if (id == k) {
        return Gp_StateF0.prefix.bytes.field_0 != 1;
    }
    return 1;
}

s32 func_shelter_b6_corridor_8017DF48(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 2:
            if (GameFlag_GetNibble(0x144) != 0) {
                Gp_RunCapCmd1(5);
            } else if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                Gp_RunCapCmd1(2);
            } else {
                Gp_RunCapCmd1(8);
            }
            break;
        case 3:
            if (GameFlag_GetNibble(0x145) != 0) {
                Gp_RunCapCmd1(6);
            } else if (Gp_StateF0.prefix.bytes.field_0 == 1) {
                Gp_RunCapCmd1(3);
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
        case 4:
            if (GameFlag_GetNibble(0x146) != 0) {
                Gp_RunCapCmd1(7);
            } else if (Gp_StateF0.prefix.bytes.field_0 != 1) {
                Gp_RunCapCmd1(0xA);
            } else {
                Gp_RunCapCmd1(4);
            }
            break;
    }
    return 0;
}

s32 func_shelter_b6_corridor_8017E020(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b6_corridor_8017E028(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    func_800E8634(D_shelter_b6_corridor_8017F354, 0, D_shelter_b6_corridor_8017F684);
    func_800E3FAC(0xA2, 0x2F);
    return 0;
}

static void func_shelter_b6_corridor_8017E064(Task* arg0)
{
    u16* ptr;
    s32  i;

    arg0->msgTable = D_shelter_b6_corridor_8017EF24;
    Game_SetPtrSlot(arg0, 7);
    ptr = (u16*)Fs_ImgBuffers;
    i   = 0;
    do {
        *ptr = (u16)(*ptr | 0x8000);
        i   += 1;
        ptr += 1;
    } while (i <= 0x12BFF);
    D_shelter_b6_corridor_801851B0.value = 2;
    if (gGameSession->at4.loc.variant == 1) {
        gStageSceneMusicEntry    = 2;
        gGameSession->flowFlags |= 1;
        gGameSession->flowFlags |= 2;
    }
    arg0->state = (s32)(arg0->state + 1);
}

static void func_shelter_b6_corridor_8017E12C(Task* task)
{
    char pad[0x10];

    CdCmd_Queue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
}

/// The room task's three states: set the room up, the per-frame state, end.
static const TaskFuncTable3 D_shelter_b6_corridor_8017D5C4 = {
    { func_shelter_b6_corridor_8017E064, func_shelter_b6_corridor_8017E12C, taskKill },
};

/// Runs the room task's current state from its three-entry table, which it
/// copies onto the stack before the call.
void func_shelter_b6_corridor_8017E144(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b6_corridor_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_b6_corridor_8017E19C(s32 arg0)
{
    if (!(gGameSession->flowFlags & 0x80)) {
        gGameSession->flowFlags        |= 0x80;
        Gp_StateF0.prefix.bytes.field_1 = arg0;
        Gp_ReleaseStateF0Add(Gp_LookupSlot4(1), 0x31);
        Task_CallExit(Gp_LookupSlot4(1));
    }
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_shelter_b6_corridor_8017E204(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_shelter_b6_corridor_8017E238(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b6_corridor_801851B8 = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[0], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[2], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[8], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F834[10], 0x140, 0x442);
            break;
        case 3:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[0], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[2], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[4], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[8], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[10], 0x140, 0x442);
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F844[12], 0x140, 0x442);
            break;
        case 4:
            func_shelter_b6_corridor_8017E360(&D_shelter_b6_corridor_8017F874[0], 0x140, 0x442);
            break;
    }
}

/// Draws a glowing bar between the world points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn when either
/// projection flags an error. Each end gets a half-disc of gouraud wedges of
/// radius `(s16)arg1 * 64` over its depth, and a strip of quads joins them
/// across the bar. The lit vertices take the colour packed in `arg2`, one
/// nibble per channel shifted into the high nibble, with bit 3 following the
/// animation frame.
static void func_shelter_b6_corridor_8017E360(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
    s32                      conn;
    u8                       r;
    u8                       g;
    u8                       b;

    p1       = arg0 + 1;
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x1C;
    block    = (OverlayPointPairScratch*)(head - 0x1C);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx0);
    gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((OverlayPointPairScratch*)(head - 0x1C))->sx1);
        gte_stflg(&((OverlayPointPairScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((OverlayPointPairScratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((OverlayPointPairScratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)ang;
            blend     = ((u8)ds->animFrame & 1) * 8;
            packed    = arg2 << 16;
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend | tr;
            g         = blend | tg;
            b         = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

void func_shelter_b6_corridor_8017EBA4(Task* task)
{
    GfxCoord* coord;
    u8        rgb[3];
    u32       shade;

    coord = task->extra.tmd->coords + 1;
    if (Gp_State1C->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        shade  = ((gDisplayState.animFrame & 1) << 4) + 0x40;
        rgb[0] = shade;
        rgb[1] = shade;
        rgb[2] = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0, task->extra.tmd->coords + (((Gp_LcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

void func_shelter_b6_corridor_8017ECA8(Task* task)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        effectControl;
    u8         rgb[3];

    mem           = task->spawnArg2.pointer;
    effectControl = Gp_State1C->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_corridor_801851B8++;
        task->state           = 1;
        task->spawnArg1.value = D_shelter_b6_corridor_801851B8;
    }
    if (task->spawnArg1.value != D_shelter_b6_corridor_801851B8) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        Gp_ReleaseState1CMem(mem, task);
    }
}

void func_shelter_b6_corridor_8017EE08(s32 arg0, s32 arg1)
{
    GameLocationKey* sess = &gGameSession->at4.loc;
    GpSprtRec*       rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    SpriteBatch*     batches;
    s32              run = arg0 & 0xFF;
    s32              flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[1].field_4;
            batches[1].hidden = 1;
            return;
        }
        if (flag == 1) {
            batches           = rec[1].field_4;
            batches[1].hidden = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[1].field_4;
            batches[2].hidden = run;
            batches           = rec[2].field_4;
            batches[2].hidden = run;
            return;
        }
        if (flag == run) {
            batches           = rec[1].field_4;
            batches[2].hidden = 0;
            batches           = rec[2].field_4;
            batches[2].hidden = 0;
            return;
        }
    } else if (run == 2) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            batches           = rec[2].field_4;
            batches[1].hidden = 1;
            return;
        }
        if (flag == 1) {
            batches           = rec[2].field_4;
            batches[1].hidden = 0;
        }
    }
}
