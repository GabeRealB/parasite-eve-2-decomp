#include "rooms/dryfield_dilapidated_house.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_521100.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/ending.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
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
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room_common.h"

extern SVECTOR D_dryfield_dilapidated_house_80186944[2];

// Retained exporter slots follow the active spotlights. Their contents
// include stale/incomplete addresses; preserve them as bytes pending review.
typedef struct {
    GpSpotLight active[1];
    u8          retained[756];
} DryfieldDilapidatedHouseSpotLightStorage;
STATIC_ASSERT_SIZEOF(DryfieldDilapidatedHouseSpotLightStorage, 864);

extern DryfieldDilapidatedHouseSpotLightStorage D_dryfield_dilapidated_house_8018959C;

extern AnimationSet* D_dryfield_dilapidated_house_80183F00[16];

/// Work block of the task family whose state-0 init is
/// `func_dryfield_dilapidated_house_80180B84`, which allocates it with
/// `Mem_Malloc(0x6C, 0)` and parks it in the `Task::work` slot (0x1C) -- that
/// slot is *not* a `TaskIdMap` here. Reach it with
/// `(DdhCoordWork*)task->work`.
///
/// `func_dryfield_dilapidated_house_80180F5C` writes the same ramp value (from
/// `func_dryfield_dilapidated_house_80180FD8`, 0..0x1000) into all three of
/// `field_0` / `field_4` / `field_8`; `func_dryfield_dilapidated_house_80181028`
/// rebuilds `mtx` as the identity and then composes it against the parent's
/// `GfxCoord` chain, and `func_dryfield_dilapidated_house_80180B84` copies
/// `mtx` verbatim into a spawned child's `GfxCoord::coord`.
typedef struct DdhCoordWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ MATRIX mtx;
    /* 0x2C */ byte   pad_2C[0x40];
} DdhCoordWork;
STATIC_ASSERT_SIZEOF(DdhCoordWork, 0x6C);

/// Vertex-morph record for the model at `D_dryfield_dilapidated_house_8018669C`.
/// Setup snapshots the model's vertices into `field_8` (`field_10` of them) and,
/// when `field_4` is set, its normals into `field_C` (`field_12` of them). The
/// morph restores that snapshot into the model from part index `field_14` and
/// blends `field_16` vertices toward `field_0` and the normals toward
/// `field_4`, which is null when the model has no normal pass.
typedef struct DdhRoomRec {
    /* 0x00 */ SVECTOR* field_0;
    /* 0x04 */ SVECTOR* field_4;
    /* 0x08 */ SVECTOR* field_8;
    /* 0x0C */ SVECTOR* field_C;
    /* 0x10 */ s16      field_10;
    /* 0x12 */ s16      field_12;
    /* 0x14 */ s16      field_14;
    /* 0x16 */ s16      field_16;
} DdhRoomRec;
STATIC_ASSERT_SIZEOF(DdhRoomRec, 0x18);

/// Work block of the handler table at `D_dryfield_dilapidated_house_8017D61C`,
/// whose state 0 is `func_dryfield_dilapidated_house_8018118C`: allocated with
/// `Mem_Malloc(0x24, 0)` and parked in the `Task::work` slot. It holds a
/// snapshot of the placed model coordinate's matrix (`mtx`, copied from
/// `GfxCoord::coord`) plus one 0x1000 word.
typedef struct DdhModelWork {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ s32    field_20;
} DdhModelWork;
STATIC_ASSERT_SIZEOF(DdhModelWork, 0x24);

/// Work block of the state family at `D_dryfield_dilapidated_house_8017D634`,
/// whose state 0 is `func_dryfield_dilapidated_house_801814B4`: allocated with
/// `Mem_Malloc(0x40, 0)` and parked in the `Task::work` slot. One angle step per
/// model part, each the matching entry of `D_dryfield_dilapidated_house_80186804`
/// scaled by the task's spawn arg 1 and wrapped into the 0x4000 angle period.
/// `func_dryfield_dilapidated_house_80180738` advances the same table against a
/// running per-part angle.
typedef struct DdhAngleStep {
    /* 0x00 */ s32 step[16];
} DdhAngleStep;
STATIC_ASSERT_SIZEOF(DdhAngleStep, 0x40);

/// Work block of the three effect handlers `func_dryfield_dilapidated_house_80182744`,
/// `func_dryfield_dilapidated_house_80183C8C` and
/// `func_dryfield_dilapidated_house_80183D5C`, reached as `task->spawnArg2.pointer`
/// and handed to `Gp_ReleaseState1CMem` when their ramp runs out. `field_24` is a
/// scale and `field_26` an angle in the 0x100-step rotation space: the pair starts
/// at 0x80 / 0x100, steps by -8 and +0x80 per frame and drives one
/// `Gfx_RotMatrixZ` + `Gp_UpdateCoord` + draw call per frame. `field_22` is the
/// per-frame tick the task rolls back while the `Gp_State1C` fade is armed;
/// `field_20` and `field_28` are a third ramp value the two `80182744` states
/// seed from one `Gp_LcgState` draw and hand to the same draw routine.
typedef struct DdhEffWork {
    /* 0x00 */ byte pad_00[0x20];
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
    /* 0x24 */ s16  field_24;
    /* 0x26 */ s16  field_26;
    /* 0x28 */ s16  field_28;
} DdhEffWork;
STATIC_ASSERT_SIZEOF(DdhEffWork, 0x2A);

/// One `gte_rtps` result kept on the stack: the screen position, and the slot
/// the depth-cue value is written to. Only the first entry's slot is ever
/// written.
typedef struct DdhScreenPoint {
    DVECTOR sxy;
    s32     depthCue;
} DdhScreenPoint;

extern void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

/// Current displacement of the screen wave, recomputed every frame from the
/// context's ramp.
extern s32 D_dryfield_dilapidated_house_80183E60;

/// The ramp and tint the wave task was spawned with.
extern OverlayWaveCtx* D_dryfield_dilapidated_house_80189B74;

/// Phase records of the wave's 11 column edges and 30 row edges.
extern OverlayWaveRec6 D_dryfield_dilapidated_house_80189B84[13];
extern OverlayWaveRec6 D_dryfield_dilapidated_house_80189BD4[32];

extern RECT D_dryfield_dilapidated_house_80183E7C;
extern RECT D_dryfield_dilapidated_house_80183E84;

extern s32            D_dryfield_dilapidated_house_80189B70;
extern s32            D_dryfield_dilapidated_house_80189B6C;
extern s32            D_dryfield_dilapidated_house_80183EFC;
extern GpEvsCmd       D_dryfield_dilapidated_house_80184408[];
extern GpEvsCmd       D_dryfield_dilapidated_house_80184C60[];
extern TaskDesc       D_dryfield_dilapidated_house_80183EB4[];
extern GpEvsCmd       D_dryfield_dilapidated_house_80184EA0[];
extern GpEvsCmd       D_dryfield_dilapidated_house_801855F0[];
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189AA0[];
extern GpAreaApplyRec D_dryfield_dilapidated_house_80189B24[];

/// The room's cutscene task, spawned from entry 0 of
/// `D_dryfield_dilapidated_house_80183EB4` when `Gp_LookupSlot4(1)` is non-zero
/// as the room starts, and NULL otherwise.
extern Task* D_dryfield_dilapidated_house_80189B78;

extern TaskDesc D_dryfield_dilapidated_house_80183E64[];
extern Task*    D_dryfield_dilapidated_house_801857E8;
extern TaskDesc D_dryfield_dilapidated_house_80186854[];
extern Task*    D_dryfield_dilapidated_house_80189B7C;
/// Spawn argument the spawned task reads back; its address is also the
/// `Task_SpawnFromTable` arg, so the store and the call must stay ordered.
typedef struct {
    s16 spawnArg;
    s16 active;
} DryfieldDilapidatedHouseSpawnState;
extern DryfieldDilapidatedHouseSpawnState D_dryfield_dilapidated_house_80189B80;

/// Shared in source with actor 136300: the ramp context the message handler
/// seeds and hands to the screen-wave task it starts, and that task's entry.
extern OverlayWaveCtx D_dryfield_dilapidated_house_80189C94;
extern TaskDesc       D_dryfield_dilapidated_house_80183E48[];

extern GpMsgEntry D_dryfield_dilapidated_house_80183E8C[];
extern s32        D_dryfield_dilapidated_house_80186804[16];
extern SVECTOR    D_dryfield_dilapidated_house_80186844[2];
extern DdhRoomRec D_dryfield_dilapidated_house_8018669C;
extern SVECTOR    D_dryfield_dilapidated_house_801866B4[];
extern s8         D_dryfield_dilapidated_house_801866F4[16][4];
extern u8         D_dryfield_dilapidated_house_80186734[24][4];
extern SVECTOR    D_dryfield_dilapidated_house_80186794[2];
extern SVECTOR    D_dryfield_dilapidated_house_801867A4[6];
extern SVECTOR    D_dryfield_dilapidated_house_801867D4[6];

static void func_dryfield_dilapidated_house_8017E9A4(s32 arg0);
static void func_dryfield_dilapidated_house_8017EBB8(Task* task);
static void func_dryfield_dilapidated_house_8017EE58(Task* task);
static void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2);
static void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3);
static void func_dryfield_dilapidated_house_80180A0C(Task* task, DdhRoomRec* rec, s32 arg2);
static void func_dryfield_dilapidated_house_80180FB8(Task* task);
static s32  func_dryfield_dilapidated_house_80180FD8(Task* task);
static void func_dryfield_dilapidated_house_80181028(Task* task);
static void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src);
static void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);

static void func_dryfield_dilapidated_house_8017E48C(void);
static void func_dryfield_dilapidated_house_8017EAB4(Task* arg0);
static void func_dryfield_dilapidated_house_8017E014(Task* task);
static void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);
static void func_dryfield_dilapidated_house_80180B84(Task* task);
static void func_dryfield_dilapidated_house_80180F5C(Task* arg0);
static void func_dryfield_dilapidated_house_8018118C(Task* arg0);
static void func_dryfield_dilapidated_house_80181264(Task* arg0);
static void func_dryfield_dilapidated_house_80181340(Task* arg0);
static void func_dryfield_dilapidated_house_801813DC(Task* task);
static void func_dryfield_dilapidated_house_8018142C(Task* task);
static void func_dryfield_dilapidated_house_801814B4(Task* arg0);
static void func_dryfield_dilapidated_house_80181584(Task* task);
static void func_dryfield_dilapidated_house_801815B8(Task* arg0);
static void func_dryfield_dilapidated_house_80182A18(GfxCoord* coord, s16 arg1, s16 arg2);
static void func_dryfield_dilapidated_house_801832A8(GfxCoord* coord, s16 arg1, s16 arg2, s16 arg3);
static void func_dryfield_dilapidated_house_80182F14(GfxCoord* coord, s16 arg1, s16 arg2);
static void func_dryfield_dilapidated_house_80183728(GfxCoord* coord, s16 arg1, s32 arg2, s16 arg3);
static void func_dryfield_dilapidated_house_801815E8(GfxCoord* coord, s16 arg1);
static void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts);
static void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts);
static void func_dryfield_dilapidated_house_801823B8(s16 slot, s16 flags);

extern GpGridParams   D_dryfield_dilapidated_house_801872E4[1];
extern GpObj3A        D_dryfield_dilapidated_house_80189260[1];
extern GpObj4C        D_dryfield_dilapidated_house_80188D08[9];
extern GpObj4C        D_dryfield_dilapidated_house_80188FB4[9];
extern GpRoomBoundVec D_dryfield_dilapidated_house_801899A0[22];
extern GpRoomCoordSet D_dryfield_dilapidated_house_801898FC[1];
extern GpScriptCmd    D_dryfield_dilapidated_house_80189B30[2];
extern GpScriptCmd    D_dryfield_dilapidated_house_80189B40[4];
extern GpScriptCmd    D_dryfield_dilapidated_house_80189B5C[2];
extern GpScriptRec    D_dryfield_dilapidated_house_80189B38[2];
extern GpScriptRec    D_dryfield_dilapidated_house_80189B50[3];
extern GpScriptRec    D_dryfield_dilapidated_house_80189B64[2];
extern SVECTOR        D_dryfield_dilapidated_house_80189CA0[40];
s32                   func_dryfield_dilapidated_house_8017E56C(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_dryfield_dilapidated_house_8017E574(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32                   func_dryfield_dilapidated_house_8017E684(Task*, s32, GpMessageArg, GpMessageArg);
s32                   func_dryfield_dilapidated_house_8017E68C(Task*, s32, GpMsg13EF*, GpMessageArg);
void                  func_dryfield_dilapidated_house_8017D64C(Task*);
void                  func_dryfield_dilapidated_house_8017DE88(Task*);
void                  func_dryfield_dilapidated_house_8017E144(Task*);
void                  func_dryfield_dilapidated_house_8017E2B0(Task*);
void                  func_dryfield_dilapidated_house_8017E6DC(Task*);
void                  func_dryfield_dilapidated_house_8017E780(Task*);
void                  func_dryfield_dilapidated_house_8017E858(Task*);
void                  func_dryfield_dilapidated_house_8017E8A8(s32);
void                  func_dryfield_dilapidated_house_8017E8C8(void);
void                  func_dryfield_dilapidated_house_8017E8E8(s32);
void                  func_dryfield_dilapidated_house_8017E970(s32);
void                  func_dryfield_dilapidated_house_8017EA10(s32);
void                  func_dryfield_dilapidated_house_8017EA7C(void);
void                  func_dryfield_dilapidated_house_80180F04(Task*);
void                  func_dryfield_dilapidated_house_80181134(Task*);
void                  func_dryfield_dilapidated_house_801812E8(Task*);
void                  func_dryfield_dilapidated_house_8018145C(Task*);

TaskDesc D_dryfield_dilapidated_house_80183E48[2] = {
    { 0, 192, func_dryfield_dilapidated_house_8017D64C, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_dryfield_dilapidated_house_80183E60 = 256;

TaskDesc D_dryfield_dilapidated_house_80183E64[2] = {
    { 0, 32, func_dryfield_dilapidated_house_8017DE88, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

RECT D_dryfield_dilapidated_house_80183E7C = { 0, 0, 320, 240 };

RECT D_dryfield_dilapidated_house_80183E84 = { 0, 0, 16, 240 };

GpMsgEntry D_dryfield_dilapidated_house_80183E8C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_dilapidated_house_8017E574 },
    { 5105, func_dryfield_dilapidated_house_8017E56C },
    { 5103, func_dryfield_dilapidated_house_8017E68C },
    { 5104, func_dryfield_dilapidated_house_8017E684 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_dilapidated_house_80183EB4[4] = {
    { 0, 97, func_dryfield_dilapidated_house_8017E6DC, { .model = NULL } },
    { 0, 192, func_dryfield_dilapidated_house_8017E780, { .model = NULL } },
    { 0, 192, func_dryfield_dilapidated_house_8017E144, { .model = NULL } },
    { 0, 192, func_dryfield_dilapidated_house_8017E2B0, { .model = NULL } },
};

TaskDesc D_dryfield_dilapidated_house_80183EE4[2] = {
    { 0, 192, func_dryfield_dilapidated_house_8017E858, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 D_dryfield_dilapidated_house_80183EFC = 0;

AnimationSet* D_dryfield_dilapidated_house_80183F00[16] = {
    NULL,
    &D_actor_521100_80138F88,
    NULL,
    NULL,
    &D_actor_521100_80137294,
    NULL,
    &D_actor_521100_801377E4,
    NULL,
    &D_actor_521100_80137DB8,
    NULL,
    &D_actor_521100_8013805C,
    &D_actor_521100_801399C4,
    &D_actor_521100_8013A02C,
    &D_actor_521100_8013AE50,
    &D_actor_521100_8013AFE0,
    NULL,
};

GpCopyArg D_dryfield_dilapidated_house_80183F40 = { { .sets = D_dryfield_dilapidated_house_80183F00 }, 16 };

AnimationPlayRequest D_dryfield_dilapidated_house_80183F48[4] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_80183F98 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FAC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FC0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FD4 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FE8 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80183FFC[2] = {
    { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_80184024 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184038 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018404C = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184060 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184074 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184088 = { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018409C = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801840B0 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801840C4 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801840D8[5] = {
    { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_8018413C = { { .index = 0 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184150 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184164 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184178 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018418C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841A0 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841B4 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841C8 = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841DC = { { .index = 0 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_801841F0 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184204 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184218[2] = {
    { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_dryfield_dilapidated_house_80184240 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184254 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184268 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_8018427C = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_dilapidated_house_80184290 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_dryfield_dilapidated_house_801842A4 = { { .loc = { 2, 9 } }, 0 };

ActorCommand D_dryfield_dilapidated_house_801842A8 = { { .loc = { 2, 9 } }, 1 };

ActorCommand D_dryfield_dilapidated_house_801842AC = { { .loc = { 2, 9 } }, 2 };

ActorCommand D_dryfield_dilapidated_house_801842B0 = { { .loc = { 2, 9 } }, 3 };

ActorCommand D_dryfield_dilapidated_house_801842B4 = { { .loc = { 2, 9 } }, 4 };

GpXformArg D_dryfield_dilapidated_house_801842B8 = { { -5304, 0, -1940, 0 }, { 0, 796, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_801842D0 = { { -2630, 0, -1740, 0 }, { 0, 910, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_801842E8 = { { -2630, 0, -1900, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184300 = { { 2900, 0, -120, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184318 = { { 2900, 0, -408, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184330 = { { 0, 0, -860, 0 }, { 0, -1137, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184348 = { { -1300, 0, -1300, 0 }, { 0, -1137, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184360 = { { -1300, 0, -1800, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184378 = { { -1880, 0, -1460, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_80184390 = { { 130, 0, -930, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_801843A8 = { { -370, 0, -1100, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_801843C0 = { { 500, 0, 300, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_dryfield_dilapidated_house_801843D8 = { { 1000, 0, 0, 0 }, { 0, 1024, 0, 0 } };

GpOverrideArg D_dryfield_dilapidated_house_801843F0 = { 19, 1 };

GpOverlayIds D_dryfield_dilapidated_house_801843F8 = { 2, 11, 11 };

GpOverlayIds D_dryfield_dilapidated_house_80184400 = { 2, 12, 11 };

GpEvsCmd D_dryfield_dilapidated_house_80184408[89] = {
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_dilapidated_house_80183F40 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 9 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184290 }, { .value = 0 } },
    { 12, { .overlays = &D_dryfield_dilapidated_house_801843F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_dilapidated_house_801842B8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_80184330 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_80184390 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_dilapidated_house_801842D0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184024 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184088 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_80184348 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183FC0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_dilapidated_house_801842E8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_80184360 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8A8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801840B0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A8 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801840C4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183FE8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842B4 } }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80183F98 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184164 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_dryfield_dilapidated_house_80189B30 }, { .padRecords = D_dryfield_dilapidated_house_80189B38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018427C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_801843A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184178 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801841C8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801841DC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_80184378 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_8018413C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842AC } }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184150 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_8018418C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801841A0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801841B4 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_801843C0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_dilapidated_house_80184C60[24] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842B0 } }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8A8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_801843C0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_dilapidated_house_801842E8 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018427C }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_dilapidated_house_8017E8C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_dilapidated_house_80184EA0[78] = {
    { 13, { .callbackNoArg = func_dryfield_dilapidated_house_8017EA7C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_dryfield_dilapidated_house_80183F40 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184290 }, { .value = 0 } },
    { 12, { .overlays = &D_dryfield_dilapidated_house_80184400 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184254 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_dilapidated_house_80184318 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018427C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2004 }, { .storage = &D_dryfield_dilapidated_house_801843D8 }, { .value = 0 } },
    { 36, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_801841F0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 19, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184204 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = -2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_dryfield_dilapidated_house_80189B40 }, { .padRecords = D_dryfield_dilapidated_house_80189B50 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 180 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_dryfield_dilapidated_house_80184300 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 39, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 37, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184038 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_8018404C }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017EA10 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A4 } }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017EA10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184268 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_dryfield_dilapidated_house_80184240 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_dilapidated_house_80184060 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 14, { .padCommands = D_dryfield_dilapidated_house_80189B5C }, { .padRecords = D_dryfield_dilapidated_house_80189B64 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_dryfield_dilapidated_house_801842A8 } }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 46, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 35, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 37, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_dilapidated_house_801855F0[17] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E8E8 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017E970 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_dryfield_dilapidated_house_8017EA10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 7, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 34, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 38, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_dilapidated_house_80185788[4] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

Task* D_dryfield_dilapidated_house_801857E8 = NULL;

TmdBone D_dryfield_dilapidated_house_801857EC[1] = {
#include "assets/dryfield_dilapidated_house_model_08A40_skeleton.inc"
};

u32 D_dryfield_dilapidated_house_80185810[1] = {
#include "assets/dryfield_dilapidated_house_model_08A40_partVerts.inc"
};

SVECTOR D_dryfield_dilapidated_house_80185814[40] = {
#include "assets/dryfield_dilapidated_house_model_08A40_verts.inc"
};

SVECTOR D_dryfield_dilapidated_house_80185954[128] = {
#include "assets/dryfield_dilapidated_house_model_08A40_normals.inc"
};

u32 D_dryfield_dilapidated_house_80185D54[171] = {
#include "assets/dryfield_dilapidated_house_model_08A40_stream.inc"
};

TmdSource D_dryfield_dilapidated_house_80186000 = {
    0,
    1160,
    0,
    1,
    D_dryfield_dilapidated_house_80185810,
    D_dryfield_dilapidated_house_80185814,
    D_dryfield_dilapidated_house_80185954,
    D_dryfield_dilapidated_house_801857EC,
    D_dryfield_dilapidated_house_80185D54,
};

TmdBone D_dryfield_dilapidated_house_80186024[1] = {
#include "assets/dryfield_dilapidated_house_model_08FB8_skeleton.inc"
};

u32 D_dryfield_dilapidated_house_80186048[1] = {
#include "assets/dryfield_dilapidated_house_model_08FB8_partVerts.inc"
};

SVECTOR D_dryfield_dilapidated_house_8018604C[40] = {
#include "assets/dryfield_dilapidated_house_model_08FB8_verts.inc"
};

SVECTOR D_dryfield_dilapidated_house_8018618C[40] = {
#include "assets/dryfield_dilapidated_house_model_08FB8_normals.inc"
};

u32 D_dryfield_dilapidated_house_801862CC[171] = {
#include "assets/dryfield_dilapidated_house_model_08FB8_stream.inc"
};

TmdSource D_dryfield_dilapidated_house_80186578 = {
    0,
    1160,
    0,
    1,
    D_dryfield_dilapidated_house_80186048,
    D_dryfield_dilapidated_house_8018604C,
    D_dryfield_dilapidated_house_8018618C,
    D_dryfield_dilapidated_house_80186024,
    D_dryfield_dilapidated_house_801862CC,
};

SVECTOR D_dryfield_dilapidated_house_8018659C[32] = {
    { -35, 11, 91, 0 },
    { -34, 26, 85, 0 },
    { -66, 44, 227, 0 },
    { -52, 46, 231, 0 },
    { -51, 57, 226, 0 },
    { -65, 56, 223, 0 },
    { -12, 13, 97, 0 },
    { -11, 28, 90, 0 },
    { -16, 5, 99, 0 },
    { -31, 3, 96, 0 },
    { -30, 33, 82, 0 },
    { -14, 34, 86, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 29, 9, 90, 0 },
    { 28, 25, 85, 0 },
    { 58, 43, 226, 0 },
    { 48, 44, 229, 0 },
    { 48, 56, 225, 0 },
    { 58, 55, 222, 0 },
    { 8, 12, 96, 0 },
    { 8, 27, 91, 0 },
    { 14, 3, 98, 0 },
    { 24, 1, 95, 0 },
    { 23, 31, 83, 0 },
    { 12, 33, 87, 0 },
};

DdhRoomRec D_dryfield_dilapidated_house_8018669C = { D_dryfield_dilapidated_house_8018659C, NULL, D_dryfield_dilapidated_house_80189CA0, NULL, 40, 0, 0, 32 };

SVECTOR D_dryfield_dilapidated_house_801866B4[8] = {
    { -81, -162, -82, 0 },
    { -150, -232, -355, 0 },
    { -150, -162, -720, 0 },
    { 351, -622, -720, 0 },
    { 351, -348, -605, 0 },
    { 351, 6, -113, 0 },
    { -351, 6, -113, 0 },
    { -81, -162, -82, 0 },
};

s8 D_dryfield_dilapidated_house_801866F4[16][4] = {
    { 0, 6, 1, 11 },
    { 0, 6, 5, 7 },
    { 0, 3, 1, 2 },
    { 0, 3, 5, 4 },
    { 6, 9, 7, 8 },
    { 6, 9, 11, 10 },
    { 2, 1, 14, 13 },
    { 3, 2, 15, 14 },
    { 3, 4, 15, 16 },
    { 4, 5, 16, 17 },
    { 5, 7, 17, 19 },
    { 8, 7, 20, 19 },
    { 9, 8, 21, 20 },
    { 9, 10, 21, 22 },
    { 10, 11, 22, 23 },
    { 1, 11, 13, 23 },
};

u8 D_dryfield_dilapidated_house_80186734[24][4] = {
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 120, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 255, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 255, 255, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_dryfield_dilapidated_house_80186794[2] = {
    { -81, -162, -82, 0 },
    { -150, -232, -355, 0 },
};

SVECTOR D_dryfield_dilapidated_house_801867A4[6] = {
    { 0, 0, 0, 0 },
    { 12, 0, 0, 0 },
    { 6, 5, 0, 0 },
    { 0, 5, 0, 0 },
    { -6, 5, 0, 0 },
    { -12, 0, 0, 0 },
};

SVECTOR D_dryfield_dilapidated_house_801867D4[6] = {
    { 0, 0, 0, 0 },
    { -5, 0, 0, 0 },
    { -3, -2, 0, 0 },
    { 0, -4, 0, 0 },
    { 3, -2, 0, 0 },
    { 5, 0, 0, 0 },
};

s32 D_dryfield_dilapidated_house_80186804[16] = {
    128,
    256,
    324,
    238,
    455,
    224,
    348,
    380,
    155,
    370,
    445,
    317,
    288,
    200,
    426,
    222,
};

SVECTOR D_dryfield_dilapidated_house_80186844[2] = {
    { 0, -164, -83, 0 },
    { 0, -164, -280, 0 },
};

TaskDesc D_dryfield_dilapidated_house_80186854[4] = {
    { 1, 192, func_dryfield_dilapidated_house_80180F04, { .model = &D_dryfield_dilapidated_house_80186000 } },
    { 1, 192, func_dryfield_dilapidated_house_80181134, { .model = &D_dryfield_dilapidated_house_80186578 } },
    { 2, 192, func_dryfield_dilapidated_house_801812E8, { .model = NULL } },
    { 2, 192, func_dryfield_dilapidated_house_8018145C, { .model = NULL } },
};

SVECTOR D_dryfield_dilapidated_house_80186884[24] = {
    { -5500, -2250, -3030, 0 },
    { -4500, -2250, -3030, 0 },
    { -4500, -1030, -3030, 0 },
    { -5500, -1030, -3030, 0 },
    { -5780, 0, -180, 0 },
    { -5000, 0, -180, 0 },
    { -4770, 0, -1660, 0 },
    { -5660, 0, -1660, 0 },
    { -3000, -2250, -3030, 0 },
    { -2000, -2250, -3030, 0 },
    { -2000, -1030, -3030, 0 },
    { -3000, -1030, -3030, 0 },
    { -3370, 0, -180, 0 },
    { -2500, 0, -180, 0 },
    { -2770, 0, -1660, 0 },
    { -3140, 0, -1660, 0 },
    { -500, -2250, -3030, 0 },
    { 500, -2250, -3030, 0 },
    { 500, -1030, -3030, 0 },
    { -500, -1030, -3030, 0 },
    { -870, 0, -180, 0 },
    { 0, 0, -180, 0 },
    { 230, 0, -1660, 0 },
    { -650, 0, -1660, 0 },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_dryfield_dilapidated_house_80186944[2] = {
    { -128, 96, 0, 0 },
    { -896, 96, 0, 0 },
};

GpRoomObjRec D_dryfield_dilapidated_house_80186954[1] = {
    { D_dryfield_dilapidated_house_801872E4, D_dryfield_dilapidated_house_80188D08, D_dryfield_dilapidated_house_80188FB4, D_dryfield_dilapidated_house_80189260 },
};

u8* D_dryfield_dilapidated_house_80186964[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_dilapidated_house_80186968[1] = {
    { { .bytes = { 21, 0 } } },
};

GpRoomCoordRec D_dryfield_dilapidated_house_8018696C[1] = {
    { D_dryfield_dilapidated_house_801898FC, D_dryfield_dilapidated_house_801899A0 },
};

GpWarpRec D_dryfield_dilapidated_house_80186974[2] = {
    { { .words = { 0, 1596, 0, -2540 } }, { 0, 0, 0, 0 }, { .words = { 0, 2296, 0, -1578 } }, { 0, 0, 0, 0 }, 0x52090002, 0x52090001, 0, 6, 0, 467 },
    { { .words = { 1024, -5403, 2, -400 } }, { 0, 0, 0, 0 }, { .words = { 2048, -5184, 2, 200 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_dryfield_dilapidated_house_801869E4[12] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_normals.inc"
};

SVECTOR D_dryfield_dilapidated_house_80186A44[116] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_verts.inc"
};

GpGridFace D_dryfield_dilapidated_house_80186DE4[70] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_faces.inc"
};

s16 D_dryfield_dilapidated_house_8018712C[208] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_dilapidated_house_8018712C[i])
s16* D_dryfield_dilapidated_house_801872CC[6] = {
#include "assets/dryfield_dilapidated_house_collision_09D24_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_dilapidated_house_801872E4[1] = {
    { NULL, D_dryfield_dilapidated_house_801869E4, D_dryfield_dilapidated_house_80186A44, D_dryfield_dilapidated_house_80186DE4, D_dryfield_dilapidated_house_801872CC, 6000, 3200, 3, 2, 4000, 70 },
};

GpViewRec D_dryfield_dilapidated_house_80187308[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 900, 0x4E20, 0 } }, 490 },
    { { { { 3872, 0, -1335 }, { -1200, 1793, -3481 }, { 584, 3682, 1695 } }, { 5380, 4300, 2840 } }, 207 },
    { { { { -2538, 0, 3214 }, { 1329, 3729, 1049 }, { -2926, 1694, -2310 } }, { 200, 2400, -200 } }, 257 },
    { { { { -1264, 0, 3896 }, { 486, 4063, 158 }, { -3865, 511, -1254 } }, { -3950, 2000, -1850 } }, 230 },
    { { { { -1264, 0, -3896 }, { -486, 4063, 158 }, { 3865, 511, -1254 } }, { 3950, 2000, -1850 } }, 230 },
    { { { { -3842, 0, -1419 }, { -415, 3916, 1123 }, { 1357, 1198, -3674 } }, { -600, 2000, -600 } }, 257 },
    { { { { -2926, 0, 2866 }, { 1386, 3585, 1415 }, { -2508, 1980, -2561 } }, { -412, 2960, -716 } }, 230 },
    { { { { -2291, 0, -3395 }, { -66, 4095, 45 }, { 3394, 80, -2291 } }, { 3808, 1373, 933 } }, 329 },
    { { { { -60, 0, 4095 }, { -794, 4018, -11 }, { -4017, -794, -59 } }, { 1221, 1047, 1902 } }, 329 },
    { { { { -138, 0, -4093 }, { 165, 4092, -5 }, { 4090, -165, -137 } }, { 1372, 1030, 834 } }, 257 },
    { { { { 2330, 0, 3368 }, { 49, 4095, -34 }, { -3367, 60, 2330 } }, { -638, 1112, 2964 } }, 289 },
    { { { { 2266, 0, -3411 }, { 1314, 3780, 872 }, { 3148, -1577, 2091 } }, { 2575, 366, 1974 } }, 289 },
    { { { { 4092, 0, -172 }, { -3, 4095, -76 }, { 172, 76, 4091 } }, { 2270, 220, 2900 } }, 289 },
    { { { { -1589, 0, 3775 }, { -499, 4060, -210 }, { -3741, -541, -1575 } }, { -3420, 920, -690 } }, 257 },
    { { { { -1116, 0, -3940 }, { 314, 4082, -89 }, { 3928, -327, -1113 } }, { -1680, 1200, -170 } }, 329 },
    { { { { -3467, 0, -2179 }, { 653, 3907, -1040 }, { 2079, -1228, -3308 } }, { -2000, 700, -1330 } }, 257 },
    { { { { -3364, 0, 2336 }, { 2068, 1903, 2978 }, { -1085, 3626, -1563 } }, { -1220, 2760, -480 } }, 257 },
    { { { { 3587, 0, -1975 }, { 248, 4063, 451 }, { 1960, -515, 3559 } }, { -2080, 920, 1450 } }, 289 },
    { { { { -2069, 0, 3534 }, { -442, 4063, -259 }, { -3507, -513, -2052 } }, { -3190, 1000, -1140 } }, 257 },
    { { { { 4087, 0, -269 }, { 38, 4054, 580 }, { 266, -581, 4045 } }, { -3270, 670, 2170 } }, 257 },
    { { { { 4095, 0, -58 }, { -41, 2903, -2888 }, { 41, 2889, 2903 } }, { -2480, 2960, 2650 } }, 257 },
};

SpriteBatch D_dryfield_dilapidated_house_801875FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_8018760C[67] = {
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -48, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 64, -56, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, -40, 625, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -16, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, 8, 625, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -120, 1125, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -24, -120, 1125, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -120, 1125, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1000, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 937, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -120, 875, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 8, -64, 937, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -8, 956, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -120, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -64, 875, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -8, 1136, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 40, -120, 625, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, -8, 1250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 40, 8, 1147, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 56, -120, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -120, 250, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 250, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 104, -120, 250, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -120, 250, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -120, 250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -120, 250, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 72, -64, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 0, 550, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -64, 425, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -64, 425, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -64, 375, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, -64, 350, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, -64, 329, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 522, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 0, 463, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 0, 416, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 136, 0, 378, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 152, 0, 353, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 24, 1125, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 24, 250, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 24, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 24, 250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 24, 250, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 144, 24, 250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, 24, 250, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 48, 750, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 56, 32, 750, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 24, 750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 88, 24, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 104, 24, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -40, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -64, 687, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1204, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -64, 687, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 56, 250, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 350, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, 72, 425, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -32, 64, 425, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, 64, 425, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, 56, 400, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 16, 56, 425, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 32, 48, 425, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 48, 40, 250, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, 32, 250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 24, 250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, 24, 250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 112, 32, 250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80187B48[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 49, 0, 0, { 2, 0 } },
    { 54, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_80187B70[63] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 0, 1125, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -48, 1125, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -48, 1125, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -16, -120, 1125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1125, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -72, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1000, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 24, -120, 1000, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -120, 1000, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -120, 1000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1000, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -72, 1062, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -16, 1125, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -120, 875, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -64, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 72, -8, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -120, 875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -64, 875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -8, 875, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, -120, 750, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 120, -120, 750, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 136, -120, 750, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 152, -120, 750, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, -8, 1392, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 1187, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -8, 1250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, -24, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -8, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -8, 8, 1325, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 72, 750, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -88, 56, 750, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 16, 750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 8, 750, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 56, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -128, 8, 750, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -128, 56, 750, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 8, 750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 750, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 64, 750, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 16, 750, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 64, 937, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 48, 937, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 72, 8, 937, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -48, 875, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 80, 8, 937, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -88, 812, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, 24, 937, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -88, 812, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, -32, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 104, 24, 937, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 120, 24, 937, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -32, 812, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 120, -88, 750, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -88, 750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 136, -32, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 136, 24, 937, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 24, 937, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -32, 750, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, -88, 750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_8018805C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 1, 0 } },
    { 31, 32, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_8018807C[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -56, 2125, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -136, 8, 1625, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -120, 8, 1625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -104, 0, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 8, 1625, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -8, 2125, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -56, -56, 2125, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -56, 2125, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -24, -56, 2125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -8, -56, 2125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 8, -56, 1915, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 24, -56, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 40, -56, 2125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -56, 2125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 72, -56, 2125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 88, -56, 2125, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 104, -56, 2125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_801881D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_801881E8[6] = {
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 1425, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 0, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 120, 0, 1475, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 128, 8, 1375, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 136, 8, 1312, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 144, 8, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188260[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_80188278[10] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 750, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 48, 750, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 8, 750, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, 48, 750, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 104, -16, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 48, 750, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, -8, 875, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 48, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 0, 875, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 48, 875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188340[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_80188358[15] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 112, -120, 1100, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 96, -120, 1100, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 80, -120, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -32, 1300, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 64, -120, 1300, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -120, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -56, 1450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 32, -120, 1250, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -120, 1350, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -56, 1375, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 0, 1375, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -64, 1475, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -32, 1100, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 96, -32, 1100, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 80, -32, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188484[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_8018849C[7] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -120, 375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, -120, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -120, 375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -16, -88, 375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 0, -88, 750, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 16, -88, 750, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188528[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_80188540[23] = {
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -160, -48, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, -48, 625, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -48, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -112, -48, 625, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -96, -48, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -48, 625, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -64, -48, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, 40, 625, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -144, 40, 625, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, 40, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 625, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, 40, 625, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, 40, 625, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 40, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 625, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 72, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 72, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 80, 72, 625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 625, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 72, 625, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 625, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_8018870C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_80188734[18] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 112, 175, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 40, 312, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -152, 32, 337, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -160, 24, 350, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 48, 287, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 56, 300, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 64, 312, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 80, 175, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 80, 175, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 80, 175, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 80, 175, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 175, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -88, 80, 175, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -80, 80, 175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 88, 175, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 96, 175, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 96, 175, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 104, 175, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_8018889C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_801888B4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_801888C4[21] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 125, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 72, 125, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -128, 56, 125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -112, 40, 125, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, 40, 125, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, 48, 125, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, 56, 125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -48, 56, 125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -32, 56, 125, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, 56, 125, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, 48, 125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 16, 40, 125, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 32, 48, 125, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 48, 56, 125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, 56, 125, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 80, 48, 125, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 40, 125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 104, 125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 112, 40, 125, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 48, 125, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 48, 125, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188A68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_dilapidated_house_80188A80[13] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -32, 2000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -16, -32, 2125, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, 24, 1875, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -8, -32, 2050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, 24, 2000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, -32, 2000, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 56, 2000, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -32, 1875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, 24, 1875, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 40, -32, 1750, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 24, 1750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1750, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -32, 1750, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_dilapidated_house_80188B84[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188B9C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BAC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BBC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BCC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BDC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BEC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_dilapidated_house_80188BFC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_dilapidated_house_80188C0C[21] = {
    { { .empty = D_dryfield_dilapidated_house_801875FC }, D_dryfield_dilapidated_house_801875FC, NULL },
    { { .elements = D_dryfield_dilapidated_house_8018760C }, D_dryfield_dilapidated_house_80187B48, NULL },
    { { .elements = D_dryfield_dilapidated_house_80187B70 }, D_dryfield_dilapidated_house_8018805C, NULL },
    { { .elements = D_dryfield_dilapidated_house_8018807C }, D_dryfield_dilapidated_house_801881D0, NULL },
    { { .elements = D_dryfield_dilapidated_house_801881E8 }, D_dryfield_dilapidated_house_80188260, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188278 }, D_dryfield_dilapidated_house_80188340, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188358 }, D_dryfield_dilapidated_house_80188484, NULL },
    { { .elements = D_dryfield_dilapidated_house_8018849C }, D_dryfield_dilapidated_house_80188528, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188540 }, D_dryfield_dilapidated_house_8018870C, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188724 }, D_dryfield_dilapidated_house_80188724, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188734 }, D_dryfield_dilapidated_house_8018889C, NULL },
    { { .empty = D_dryfield_dilapidated_house_801888B4 }, D_dryfield_dilapidated_house_801888B4, NULL },
    { { .elements = D_dryfield_dilapidated_house_801888C4 }, D_dryfield_dilapidated_house_80188A68, NULL },
    { { .elements = D_dryfield_dilapidated_house_80188A80 }, D_dryfield_dilapidated_house_80188B84, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188B9C }, D_dryfield_dilapidated_house_80188B9C, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BAC }, D_dryfield_dilapidated_house_80188BAC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BBC }, D_dryfield_dilapidated_house_80188BBC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BCC }, D_dryfield_dilapidated_house_80188BCC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BDC }, D_dryfield_dilapidated_house_80188BDC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BEC }, D_dryfield_dilapidated_house_80188BEC, NULL },
    { { .empty = D_dryfield_dilapidated_house_80188BFC }, D_dryfield_dilapidated_house_80188BFC, NULL },
};

GpObj4C D_dryfield_dilapidated_house_80188D08[9] = {
    { NULL, NULL, NULL, { -4256, -2000, -2432, 0 }, { { 0, -3056, -2464, 0 }, { 0, -3056, 2432, 0 }, { 0, 3056, -2432, 0 }, { 0, 3056, 2464, 0 } }, { 4100, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3924, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -4064, -2080, -2368, 0 }, { { 0, -3104, 2272, 0 }, { 0, -3104, -2272, 0 }, { 0, 3104, 2272, 0 }, { 0, 3104, -2272, 0 } }, { -4106, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3840, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -1937, -1376, -1937, 0 }, { { -2182, -2400, 2208, 0 }, { 2183, -2400, -2207, 0 }, { -2182, 2400, 2208, 0 }, { 2183, 2400, -2207, 0 } }, { -2920, 0, -2887, 0 }, { 0, 0, 4096, 0 }, 3916, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -2017, -1360, -2017, 0 }, { { 2183, -2384, -2207, 0 }, { -2182, -2384, 2208, 0 }, { 2183, 2384, -2207, 0 }, { -2182, 2384, 2208, 0 } }, { 2913, 0, 2880, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 220, -1392, 318, 0 }, { { -30, -2416, -3113, 0 }, { 4, -2416, 3091, 0 }, { -30, 2416, -3113, 0 }, { 4, 2416, 3091, 0 } }, { 4098, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3932, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 2974, -1312, -1986, 0 }, { { -2235, -2336, -137, 0 }, { 2226, -2336, 127, 0 }, { -2235, 2336, -137, 0 }, { 2226, 2336, 127, 0 } }, { 241, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 318, -1360, 352, 0 }, { { 13, -2384, 3100, 0 }, { -23, -2384, -3109, 0 }, { 13, 2384, 3100, 0 }, { -23, 2384, -3109, 0 } }, { -4098, 0, 23, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 2975, -1472, -2048, 0 }, { { 2226, -2496, 127, 0 }, { -2235, -2496, -137, 0 }, { 2226, 2496, 127, 0 }, { -2235, 2496, -137, 0 } }, { -243, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { -4961, -2048, -2225, 0 }, { { -670, -3056, -1440, 0 }, { 660, -3056, 1411, 0 }, { -659, 3056, -1410, 0 }, { 671, 3056, 1441, 0 } }, { 3712, 1, -1733, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 3, 2, 129, 0 },
};

GpObj4C D_dryfield_dilapidated_house_80188FB4[9] = {
    { NULL, NULL, NULL, { 1504, -52, -2768, 0 }, { { -768, 0, -400, 0 }, { 768, 0, -400, 0 }, { -768, 0, 400, 0 }, { 768, 0, 400, 0 } }, { 0, 4116, 0, 0 }, { 0, 0, 4096, 0 }, 865, 0, 5, 20, 2, 0 },
    { NULL, NULL, NULL, { -5600, -52, 192, 0 }, { { 224, 0, -624, 0 }, { 224, 0, 624, 0 }, { -224, 0, -624, 0 }, { -224, 0, 624, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 662, 0, 7, 34, 2, 0 },
    { NULL, NULL, NULL, { -4688, -64, 352, 0 }, { { -512, 0, -608, 0 }, { 512, 0, -608, 0 }, { -512, 0, 608, 0 }, { 512, 0, 608, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 794, 0x4002, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { -4704, -64, -944, 0 }, { { -512, 0, -560, 0 }, { 512, 0, -560, 0 }, { -512, 0, 560, 0 }, { 512, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { -4077, 0, -402, 0 }, 757, 0x4002, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { -3472, -64, -496, 0 }, { { -336, 0, -928, 0 }, { 1456, 0, -928, 0 }, { -336, 0, 928, 0 }, { 1136, 0, 928, 0 } }, { 0, 4107, 0, 0 }, { -4091, 0, 201, 0 }, 1722, 0x4002, 5, 0, 4, 0 },
    { NULL, NULL, NULL, { -1328, -64, -2288, 0 }, { { -912, 0, -640, 0 }, { 912, 0, -640, 0 }, { -912, 0, 640, 0 }, { 912, 0, 640, 0 } }, { 0, 4103, 0, 0 }, { -4091, 0, 201, 0 }, 1108, 0x4002, 18, 0, 4, 0 },
    { NULL, NULL, NULL, { -3969, -64, -2273, 0 }, { { -502, 0, -1200, 0 }, { 521, 0, -1249, 0 }, { -520, 0, 1249, 0 }, { 502, 0, 1199, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 1348, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { -5168, -64, 768, 0 }, { { -800, 0, -480, 0 }, { 800, 0, -480, 0 }, { -800, 0, 480, 0 }, { 800, 0, 480, 0 } }, { 0, 4100, 0, 0 }, { -201, 0, -4091, 0 }, 931, 0x4002, 17, 0, 2, 0 },
    { NULL, NULL, NULL, { -5232, -64, -2816, 0 }, { { -720, 0, -352, 0 }, { 720, 0, -352, 0 }, { -720, 0, 352, 0 }, { 720, 0, 352, 0 } }, { 0, 4098, 0, 0 }, { 201, 0, 4091, 0 }, 799, 0x4002, 16, 0, 130, 0 },
};

GpObj3A D_dryfield_dilapidated_house_80189260[1] = {
    { NULL, NULL, { -4096, -2000, 1232, 0 }, { { 0, 2576, -2544, 0 }, { 0, -2576, -2544, 0 }, { 0, 2576, 2544, 0 }, { 0, -2576, 2544, 0 } }, { 4097, 0, 0, 0 }, { 36, 14 }, 129, 0 },
};

GpPointLight D_dryfield_dilapidated_house_8018929C[8] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -1500, 500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1847, 1847, 1847, { 0, 0 } }, 2000, 3549 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2666, 2666, 2666, { 0, 0 } }, 2256, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2666, 2666, 2666, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } }, 2256, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1500, -2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2666, 2666, 2666, { 0, 0 } }, 2256, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } }, 2000, 6400 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } }, 2000, 3003 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2500, -1500, 2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1028, 1028, 1028, { 0, 0 } }, 2000, 3000 },
};

DryfieldDilapidatedHouseSpotLightStorage D_dryfield_dilapidated_house_8018959C = { { { { { .coord = { 0, { { { -4096, 0, 0 }, { 0, -2902, 2901 }, { 0, 2901, 2901 } }, { -5000, -2500, -4000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } }, { 0, 2896, 2896, 0 }, 100, 3000, 625 } }, { 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 164, 80, 25, 128, 1, 0, 0, 0, 164, 83, 0, 0, 45, 252, 128, 16, 216, 255, 0, 0, 85, 255, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 190, 231, 48, 243, 222, 21, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 212, 3, 0, 0, 249, 255, 0, 0, 45, 252, 0, 0, 7, 0, 0, 0, 212, 3, 208, 16, 249, 255, 0, 0, 0, 0, 0, 0, 235, 239, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 52, 17, 0, 0, 6, 5, 0, 0, 224, 84, 25, 128, 72, 84, 25, 128, 248, 48, 7, 128, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 3, 224, 17, 58, 255, 0, 0, 0, 0, 0, 0, 81, 240, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 56, 18, 0, 0, 7, 6, 97, 0, 44, 85, 25, 128, 148, 84, 25, 128, 0, 0, 0, 0, 93, 232, 48, 242, 60, 254, 0, 0, 181, 3, 48, 238, 12, 255, 0, 0, 75, 252, 0, 0, 245, 0, 0, 0, 181, 3, 208, 17, 12, 255, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 120, 85, 25, 128, 224, 84, 0, 0, 0, 0, 0, 0, 64, 235, 96, 242, 111, 244, 0, 0, 248, 255, 128, 239, 124, 247, 0, 0, 8, 0, 128, 239, 132, 8, 0, 0, 248, 255, 128, 16, 0, 0, 0, 0, 8, 0, 128, 16, 132, 8, 0, 0, 2, 16, 0, 0, 240, 255, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 140, 18, 0, 0, 2, 7, 97, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 247, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 248, 255, 128, 16, 12, 247, 0, 0, 250, 239, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 194, 18, 0, 0, 0, 0, 0, 0, 16, 86, 25, 128, 120, 85, 25, 128, 248, 48, 7, 128, 64, 235, 224, 244, 224, 40, 0, 0, 235, 0, 128, 239, 206, 250, 0, 0, 16, 255, 128, 239, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 82, 17, 0, 0, 0, 0, 0, 0, 92, 86, 25, 128, 196, 85, 0, 0, 248, 48, 7, 128, 160, 235, 0, 0, 64, 41, 0, 0, 17, 255, 128, 239, 46, 5, 0, 0, 0, 0, 0, 0, 207, 250, 0, 0, 17, 255, 128, 16, 46, 5, 0, 0, 236, 0, 128, 16, 207, 250, 0, 0, 63, 240, 0, 0, 46, 253, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 34, 5, 96, 239, 163, 2, 0, 0, 0, 0, 0, 0, 93, 253, 0, 0, 34, 5, 0, 0, 163, 2, 0, 0, 223, 250, 0, 0, 93, 253, 0, 0, 170, 248, 0, 0, 67, 14, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 140, 17, 0, 0, 4, 3, 97, 0, 244, 86, 25, 128, 92, 86, 0, 0, 248, 48, 7, 128, 204, 251, 64, 243, 109, 20, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 120, 7, 0, 0, 208, 241, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 111, 17, 0, 0, 3, 4, 97, 0, 64, 87, 0, 0, 168, 86, 25, 128, 248, 48, 7, 128, 64, 236, 96, 243 } };

GpRoomCoordSet D_dryfield_dilapidated_house_801898FC[1] = {
    { 0, NULL, 8, D_dryfield_dilapidated_house_8018929C, 1, D_dryfield_dilapidated_house_8018959C.active },
};

GpAreaTmdRec D_dryfield_dilapidated_house_80189914[3] = {
    { 34, 211, 4, 0, { 0, 0 }, D_8015F6E4 },
    { 29, 212, 4, 0, { 0, 0 }, D_8016A388 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_dilapidated_house_80189938[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AF64, D_dryfield_dilapidated_house_80189914 },
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

GpRoomBoundVec D_dryfield_dilapidated_house_801899A0[22] = {
    { 21, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 1000, 1000, 1000, 1000 },
    { 16, 16, 16, 16 },
    { 0, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 650, 650, 650, 650 },
    { 650, 650, 650, 650 },
    { 650, 650, 650, 650 },
    { 650, 650, 650, 650 },
    { 3000, 3000, 600, 2700 },
    { 2000, 750, 550, 1193 },
    { 650, 650, 650, 650 },
    { 750, 750, 750, 750 },
};

s32 D_dryfield_dilapidated_house_80189A50[3] = {
    0x10000045,
    0x10000047,
    0x10000045,
};

s32 D_dryfield_dilapidated_house_80189A5C[3] = {
    0x1000004D,
    0x1000004F,
    0x1000004D,
};

GpRoomParamRec D_dryfield_dilapidated_house_80189A68[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_dilapidated_house_80189A70[1] = {
    { 0, 0, 1, 0, D_dryfield_dilapidated_house_80189A50 },
};

GpRoomParamRec D_dryfield_dilapidated_house_80189A78[1] = {
    { 0, 0, 1, 0, D_dryfield_dilapidated_house_80189A5C },
};

GpRoomParamRec* D_dryfield_dilapidated_house_80189A80[8] = {
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A70,
    D_dryfield_dilapidated_house_80189A78,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
    D_dryfield_dilapidated_house_80189A68,
};

GpAreaApplyRec D_dryfield_dilapidated_house_80189AA0[33] = {
    { 3, 1, 1, 0 },
    { 3, 2, 1, 17 },
    { 3, 2, 7, 33 },
    { 3, 3, 1, 1 },
    { 3, 5, 1, 17 },
    { 3, 5, 7, 33 },
    { 3, 6, 1, 1 },
    { 3, 7, 1, 1 },
    { 3, 9, 1, 0 },
    { 3, 11, 1, 1 },
    { 3, 12, 3, 1 },
    { 3, 13, 1, 1 },
    { 3, 14, 1, 1 },
    { 3, 15, 3, 0 },
    { 3, 16, 1, 1 },
    { 3, 17, 1, 0 },
    { 3, 18, 2, 1 },
    { 3, 19, 1, 1 },
    { 3, 20, 1, 1 },
    { 3, 21, 1, 0 },
    { 3, 22, 1, 1 },
    { 3, 23, 1, 0 },
    { 3, 24, 1, 1 },
    { 3, 25, 1, 1 },
    { 3, 26, 1, 0 },
    { 3, 27, 1, 0 },
    { 3, 28, 1, 1 },
    { 3, 29, 1, 1 },
    { 3, 30, 1, 0 },
    { 3, 31, 1, 0 },
    { 3, 32, 1, 1 },
    { 3, 34, 1, 1 },
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_dryfield_dilapidated_house_80189B24[3] = {
    { 3, 38, 2, 17 },
    { 3, 38, 7, 33 },
    { 255, 0, 0, 0 },
};

GpScriptCmd D_dryfield_dilapidated_house_80189B30[2] = {
    { 1, 257 },
    { 0, 0 },
};

GpScriptRec D_dryfield_dilapidated_house_80189B38[2] = {
    { 0, 0, 5, 0 },
    { 200, 255, 8, 1 },
};

GpScriptCmd D_dryfield_dilapidated_house_80189B40[4] = {
    { 0, 1 },
    { 513, 514 },
    { 4, 257 },
    { 0, 0 },
};

GpScriptRec D_dryfield_dilapidated_house_80189B50[3] = {
    { 200, 255, 7, 1 },
    { 200, 90, 5, 1 },
    { 180, 60, 1, 0 },
};

GpScriptCmd D_dryfield_dilapidated_house_80189B5C[2] = {
    { 1, 257 },
    { 0, 512 },
};

GpScriptRec D_dryfield_dilapidated_house_80189B64[2] = {
    { 0, 0, 5, 0 },
    { 200, 255, 8, 1 },
};

s32 D_dryfield_dilapidated_house_80189B6C = 0;

s32 D_dryfield_dilapidated_house_80189B70 = 0;

OverlayWaveCtx* D_dryfield_dilapidated_house_80189B74 = NULL;

Task* D_dryfield_dilapidated_house_80189B78 = NULL;

Task* D_dryfield_dilapidated_house_80189B7C = NULL;

DryfieldDilapidatedHouseSpawnState D_dryfield_dilapidated_house_80189B80 = { 0, 0 };

OverlayWaveRec6 D_dryfield_dilapidated_house_80189B84[13] = { 0 };

OverlayWaveRec6 D_dryfield_dilapidated_house_80189BD4[32] = { 0 };

OverlayWaveCtx D_dryfield_dilapidated_house_80189C94 = { 0 };

SVECTOR D_dryfield_dilapidated_house_80189CA0[40];

GfxCoord D_dryfield_dilapidated_house_80189DE0[8];

GfxCoord D_dryfield_dilapidated_house_8018A060[8];

/// Prism corners in model space, eight per prism: `[0..3]` the lit ring and
/// `[4..7]` the far ring. Callers pick a prism by passing 0, 8 or 0x10.
extern SVECTOR D_dryfield_dilapidated_house_80186884[];

/// Eight-slot trail coordinates, one array per end of the pair. Every entry is
/// parented to `gGfxViewCoord`.
extern GfxCoord D_dryfield_dilapidated_house_80189DE0[8];

extern GfxCoord D_dryfield_dilapidated_house_8018A060[8];

/// Task that ripples the whole screen: it redraws the frame just rendered as a
/// 10 by 30 grid of textured quads whose corners are pushed around by sine
/// waves. The first frame gives every column and row edge a random phase
/// offset and speed, takes its context from `spawnArg2` and passes -8 to
/// `displaySetShakeY`. Afterwards the context's mode ramps the strength
/// up to its limit (mode 0), back down to zero and on to mode 2 (mode 1), or
/// ends the task and passes 0 back (mode 2); the displacement is the ramp's
/// share of the context's peak. A non-zero tint flag shades the quads with the
/// context's colour instead of drawing them unlit. The grid is bracketed by
/// draw-mode packets that switch mask-bit setting on at the back of the order
/// table and off again at the front.
void func_dryfield_dilapidated_house_8017D64C(Task* arg0)
{
    OverlayWaveCtx* ctx;
    POLY_FT4*       p;
    DR_STP*         stp;
    s32             i, j, k;
    s32             drawY;
    s32             tpage0, tpage1;
    s32             u0, u1, v0, v1;
    s32             waveX0, waveY0, waveX1, waveY1;
    s32             waveX2, waveY2, waveX3, waveY3;
    s32*            state;

    CdCmd_Queue.imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
    /* Through a pointer rather than as `arg0->state`: a member load is struct
       memory, which the scheduler lets rise above the store before it, and the
       original keeps the two in source order. */
    state = &arg0->state;
    switch (*state) {
        case 0:
            for (i = 0; i < 11; i++) {
                D_dryfield_dilapidated_house_80189B84[i].phase  = 0;
                D_dryfield_dilapidated_house_80189B84[i].offset = (u32)rand() >> 3;
                D_dryfield_dilapidated_house_80189B84[i].speed  = (rand() * 100 + 20) >> 15;
            }
            for (i = 0; i < 30; i++) {
                D_dryfield_dilapidated_house_80189BD4[i].phase  = 0;
                D_dryfield_dilapidated_house_80189BD4[i].offset = (u32)rand() >> 3;
                D_dryfield_dilapidated_house_80189BD4[i].speed  = (rand() * 100 + 20) >> 15;
            }
            D_dryfield_dilapidated_house_80183E60        = 0;
            D_dryfield_dilapidated_house_80189B74        = arg0->spawnArg2.pointer;
            D_dryfield_dilapidated_house_80189B74->frame = 0;
            D_dryfield_dilapidated_house_80189B74->state = 0;
            displaySetShakeY(DISPLAY_SHAKE_MIN);
            arg0->state++;
            break;
        case 1:
            ctx = D_dryfield_dilapidated_house_80189B74;
            switch (ctx->state) {
                case 0:
                    if (ctx->frame < ctx->span) {
                        ctx->frame++;
                    }
                    break;
                case 1:
                    if (ctx->frame > 0) {
                        ctx->frame--;
                    } else {
                        ctx->state = 2;
                    }
                    break;
                case 2:
                    taskKill(arg0);
                    displaySetShakeY(0);
                    break;
            }
            D_dryfield_dilapidated_house_80183E60 = D_dryfield_dilapidated_house_80189B74->frame * D_dryfield_dilapidated_house_80189B74->scale / D_dryfield_dilapidated_house_80189B74->span;
            for (i = 0; i < 11; i++) {
                D_dryfield_dilapidated_house_80189B84[i].phase += D_dryfield_dilapidated_house_80189B84[i].speed;
            }
            for (i = 0; i < 30; i++) {
                D_dryfield_dilapidated_house_80189BD4[i].phase += D_dryfield_dilapidated_house_80189BD4[i].speed;
            }
            tpage0 = getTPage(2, 0, 0, gDisplayState.drawBuffer << 8);
            tpage1 = getTPage(2, 0, 128, gDisplayState.drawBuffer << 8);
            for (j = -1; j < 29; j++) {
                for (k = 0; k < 10; k++) {
                    p              = gGpuPrimCursor;
                    gGpuPrimCursor = p + 1;
                    setPolyFT4(p);
                    if (D_dryfield_dilapidated_house_80189B74->blend == ANIMATION_BLEND_RESET) {
                        setShadeTex(p, 1);
                    } else {
                        setShadeTex(p, 0);
                        p->r0 = D_dryfield_dilapidated_house_80189B74->r;
                        p->g0 = D_dryfield_dilapidated_house_80189B74->g;
                        p->b0 = D_dryfield_dilapidated_house_80189B74->b;
                    }
                    u0 = k * 32;
                    u1 = (k + 1) * 32;
                    if (u1 == 320)
                        u1 = 319;
                    if (u0 < 128) {
                        p->tpage = tpage0;
                    } else {
                        p->tpage = tpage1;
                        u0      -= 128;
                        u1      -= 128;
                    }
                    if (j != -1) {
                        v1     = (j + 1) * 8 + gDisplayState.drawBuffer * 16;
                        v0     = j * 8 + gDisplayState.drawBuffer * 16;
                        waveX0 = D_dryfield_dilapidated_house_80183E60 * (rsin((j << 9) + D_dryfield_dilapidated_house_80189B84[k].phase + D_dryfield_dilapidated_house_80189B84[k].offset) << 3);
                        p->x0  = k * 32 + (s16)((waveX0 >> 20) - 160);
                        waveY0 = D_dryfield_dilapidated_house_80183E60 * (rsin((k << 10) + D_dryfield_dilapidated_house_80189BD4[j].phase + D_dryfield_dilapidated_house_80189BD4[j].offset) << 3);
                        p->y0  = j * 8 + (s16)((ABS(waveY0) >> 20) - 104);
                        waveX1 = D_dryfield_dilapidated_house_80183E60 * (rsin((j << 9) + D_dryfield_dilapidated_house_80189B84[k + 1].phase + D_dryfield_dilapidated_house_80189B84[k + 1].offset) << 3);
                        p->x1  = (k + 1) * 32 + (s16)((waveX1 >> 20) - 160);
                        waveY1 = D_dryfield_dilapidated_house_80183E60 * (rsin(((k + 1) << 10) + D_dryfield_dilapidated_house_80189BD4[j].phase + D_dryfield_dilapidated_house_80189BD4[j].offset) << 3);
                        p->y1  = j * 8 + (s16)((ABS(waveY1) >> 20) - 104);
                    } else {
                        drawY = gDisplayState.drawBuffer * 16;
                        p->x0 = k * 32 - 160;
                        p->y0 = -112;
                        p->x1 = (k + 1) * 32 - 160;
                        p->y1 = -112;
                        v0    = drawY + 8;
                        v1    = drawY;
                    }
                    {

                        waveX2 = D_dryfield_dilapidated_house_80183E60 * (rsin(((j + 1) << 9) + D_dryfield_dilapidated_house_80189B84[k].phase + D_dryfield_dilapidated_house_80189B84[k].offset) << 3);
                        p->x2  = k * 32 + (s16)((waveX2 >> 20) - 160);
                        waveY2 = D_dryfield_dilapidated_house_80183E60 * (rsin((k << 10) + D_dryfield_dilapidated_house_80189BD4[j + 1].phase + D_dryfield_dilapidated_house_80189BD4[j + 1].offset) << 3);
                        p->y2  = (j + 1) * 8 + (s16)((ABS(waveY2) >> 20) - 104);
                        waveX3 = D_dryfield_dilapidated_house_80183E60 * (rsin(((j + 1) << 9) + D_dryfield_dilapidated_house_80189B84[k + 1].phase + D_dryfield_dilapidated_house_80189B84[k + 1].offset) << 3);
                        p->x3  = (k + 1) * 32 + (s16)((waveX3 >> 20) - 160);
                        waveY3 = D_dryfield_dilapidated_house_80183E60 * (rsin(((k + 1) << 10) + D_dryfield_dilapidated_house_80189BD4[j + 1].phase + D_dryfield_dilapidated_house_80189BD4[j + 1].offset) << 3);
                        p->y3  = (j + 1) * 8 + (s16)((ABS(waveY3) >> 20) - 104);
                    }
                    p->u0 = u0;
                    p->v0 = v0;
                    p->u1 = u1;
                    p->v1 = v0;
                    p->u2 = u0;
                    p->v2 = v1;
                    p->u3 = u1;
                    p->v3 = v1;
                    addPrim(&gGpuCurrentOt[3], p);
                }
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
}

/// The room's capture task, the body the actor family also carries as
/// `func_actor_460200_80131E24`: the whole image area is written into the
/// display buffer strip by strip and then desaturated in place.
///
/// It is spawned from entry 0 of `D_dryfield_dilapidated_house_80183E64` with
/// the `OverlayCaptureArgs` block as its `spawnArg2`. State 0 seeds the countdown
/// from the block's duration, picks the strip origin's y out of `gDisplayState`
/// (`field_1f` non-zero selects 0, clear selects 0x110) and hands the twenty
/// 0x1E00-byte strips of `Fs_ImgBuffers` to `StoreImage` -- or, while the buffer
/// is being read back (`gDisplayState.debugMode` is negative), only the single flat
/// `D_dryfield_dilapidated_house_80183E7C` rectangle -- and then marks the
/// display busy in `field_104`. State 1 waits for the transfer with `DrawSync`
/// and runs the desaturating invert. State 2 counts the duration down in
/// `Task::killCountdown`, raises the block's `done` when it runs out, and on
/// `done` releases `field_104` and kills the task.
void func_dryfield_dilapidated_house_8017DE88(Task* task)
{
    OverlayCaptureArgs* args;
    s32                 i;
    u_long*             strip;

    args = task->spawnArg2.pointer;
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                args->done          = 0;
                task->killCountdown = args->duration;
                if (gDisplayState.drawBuffer != 0) {
                    D_dryfield_dilapidated_house_80183E84.y = 0;
                } else {
                    D_dryfield_dilapidated_house_80183E84.y = 0x110;
                }
                if (gDisplayState.debugMode < 0) {
                    StoreImage(&D_dryfield_dilapidated_house_80183E7C, Fs_ImgBuffers->words);
                } else {
                    strip = Fs_ImgBuffers->words;
                    for (i = 0; i < 20; i++) {
                        D_dryfield_dilapidated_house_80183E84.x = i * 16;
                        StoreImage(&D_dryfield_dilapidated_house_80183E84, strip);
                        strip += 1920;
                    }
                }
                gDisplayState.skipDraw = 1;
                goto advance;
            case 1:
                DrawSync(0);
                func_dryfield_dilapidated_house_8017E48C();
            advance:
                task->state++;
                break;
            case 2:
                if (--task->killCountdown <= 0) {
                    args->done = 1;
                }
                if (args->done != 0) {
                    taskKill(task);
                    gDisplayState.skipDraw = 0;
                }
                break;
        }
    }
}

/// State handlers of the room task, indexed by `Task::state`: set-up, the room
/// gate, then `taskKill`.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D5C4 = {
    { func_dryfield_dilapidated_house_8017EAB4, func_dryfield_dilapidated_house_8017E014, taskKill },
};

/// Room gate task. While the session is in the room (`gGameSession->eventState`
/// is 0) it walks `D_dryfield_dilapidated_house_80183EFC` from 1 to 2 and then
/// to 3: the 1 -> 2 step is unconditional, the 2 -> 3 step waits for the room's
/// message (0x7D6) to be dispatched and answered with 0 by the slot-0 object,
/// and for no sound to be playing; reaching 3 spawns entry 3 of the room's task
/// table. Independently, once the stream file is open it starts the named
/// sequences `"AUNT"` and `"Player"` on the two slot objects.
static void func_dryfield_dilapidated_house_8017E014(Task* task)
{
    if (gGameSession->eventState == 0) {
        if (D_dryfield_dilapidated_house_80183EFC == 1) {
            D_dryfield_dilapidated_house_80183EFC = 2;
        } else if ((D_dryfield_dilapidated_house_80183EFC == 2) &&
                   (Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0)) {
            if (Gp_StateC08.field_A != 1) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    D_dryfield_dilapidated_house_80183EFC += 1;
                    Task_SpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 3, 0, 0);
                }
            }
        }
    }
    if ((gDisplayState.debugMode != 0) && (Gp_LookupSlot4(1) != 0)) {
        func_80724608(Gp_LookupSlot4(1), -0x8C, 0xA, "AUNT");
        func_80724608(gameGetPtrSlot(3), -0x8C, 0x14, "Player");
    }
}

/// Screen-blackout timer of task-table entry 2: `func_dryfield_dilapidated_house_8017E970`
/// arms it by writing state 2 and a frame count into `spawnArg1` (a 0 arg resets
/// it to state 0 instead). State 2 copies that count into the shared countdown
/// `D_dryfield_dilapidated_house_80189B70` and falls through to state 3, whose
/// `var_s1` is the shared "paint the screen black" flag; state 4 runs the
/// countdown and at 0 calls `func_dryfield_dilapidated_house_8017E9A4(0xF)`,
/// which starts the room's captured-image scene, then raises the flag again once
/// the count is 15 frames past that hand-off, keeping the screen black over it.
/// The flag paints the whole frame with a zeroed `TILE` carved out of
/// `gGpuPrimCursor` and links it into `gGpuCurrentOt`. When `D_801156F9` is set
/// the task does nothing at all.
void func_dryfield_dilapidated_house_8017E144(Task* task)
{
    TILE* tile;
    s32   var_s1;
    s32   temp_v0;
    s32   temp_v1;

    var_s1 = 0;
    if (D_801156F9 == 0) {
        temp_v1 = task->state;
        switch (temp_v1) {
            case 0:
                task->state = task->state + 1;
                break;
            case 1:
                break;
            case 2:
                D_dryfield_dilapidated_house_80189B70 = task->spawnArg1.value;
                task->state                           = task->state + 1;
                /* fallthrough */
            case 3:
                var_s1      = 1;
                task->state = task->state + var_s1;
                break;
            case 4:
                temp_v0                               = D_dryfield_dilapidated_house_80189B70 - 1;
                D_dryfield_dilapidated_house_80189B70 = temp_v0;
                if (temp_v0 == 0) {
                    func_dryfield_dilapidated_house_8017E9A4(0xF);
                }
                if (D_dryfield_dilapidated_house_80189B70 < -0xF) {
                    var_s1 = 1;
                }
                break;
        }
        if (var_s1 != 0) {
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            SetTile(tile);
            tile->x0 = -0xA0;
            tile->y0 = -0x80;
            tile->w  = 0x140;
            tile->h  = 0x100;
            tile->r0 = 0;
            tile->g0 = 0;
            tile->b0 = 0;
            addPrim(gGpuCurrentOt, tile);
        }
    }
}

/// Scene-clear task: the room's hand-off to the rest of the game. State 0
/// starts the streamed scene named by the two blocks `func_800E8634` takes,
/// state 1 fires when the session is back in play (`gGameSession->eventState`
/// is 2) and hands slot 0 the release event 0x1B, state 6 waits for the room
/// message (`gGameSession->field_126`), and state 7 -- reached once the save
/// has not already banked this clear (`Mc_SaveData[0].state.demoScene`) -- applies the
/// room's two area records, raises the progression flags, refills the party
/// and hands off to the results screen with `Task_Spawn(0, 0x11, 0, 0)`.
/// States 0..6 share the `advance` tail that walks the task one state on;
/// `goto advance` from state 1 is the `acropolis_patio` idiom, and the
/// `do/while (0)` around the shared increment is this project's allocation
/// lever, not a loop: it weights the task pointer's references by loop depth
/// so it outranks the `Mc_SaveData` base and takes `$s0` instead of `$s1`.
void func_dryfield_dilapidated_house_8017E2B0(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634(D_dryfield_dilapidated_house_80184EA0, 0, D_dryfield_dilapidated_house_801855F0);
            task->state += 1;
            return;
        case 1:
            if (gGameSession->eventState == 2) {
                Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0x1B);
                Gp_StateF0.prefix.bytes.field_1 = 3;
                goto advance;
            }
            return;
        case 2:
        case 3:
        case 4:
        case 5:
            task->state += 1;
            return;
        case 6:
            if (gGameSession->field_126 == 0) {
                return;
            }
        advance:
            do {
                task->state += 1;
            } while (0);
            return;
        case 7:
            if (Mc_SaveData[0].state.demoScene != 9) {
                Gp_ApplyAreaRecs(D_dryfield_dilapidated_house_80189AA0);
                if (GameFlag_GetNibble(0xCE) != 0) {
                    Gp_ApplyAreaRecs(D_dryfield_dilapidated_house_80189B24);
                }
                GameFlag_SetNibble(0x4B, 6);
                GameFlag_SetNibble(0x4C, 1);
                GameFlag_SetNibble(0x45, 1);
                GameFlag_SetNibble(0x62, 1);
                GameFlag_SetNibble(0x59, 1);
                GameFlag_SetNibble(0x5A, 2);
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 0);
                Gp_FillPlayerHpMp();
                Gp_FillAllyHp();
                Mc_SaveData[0].state.sceneEvent    = 1;
                Mc_SaveData[0].state.at4.loc.stage = 2;
                Mc_SaveData[0].state.at4.loc.warp  = 1;
                Mc_SaveData[0].state.at4.loc.room  = 1;
                Mc_SaveData[0].state.at4.loc.area  = 8;
                gDisplayState.spriteVariant        = 1;
                Task_Spawn(0, 0x11, 0, 0);
            }
            taskKill(task);
            return;
    }
}

/// Inverts the grey of the whole image buffer in place, two 16-bit texels per
/// step. Each packed pair is averaged with weights 3:4:1 over its R, G and B
/// fields, the average is complemented against the 5-bit field mask, and the
/// result is spread back over 15 bits. The 0x4B00 passes cover the buffer's
/// 38400 words exactly.
static void func_dryfield_dilapidated_house_8017E48C(void)
{
    s32     i;
    u_long* p0;
    u_long* p1;
    u32     hi;
    u32     lo;
    u32     gray;
    u32     t;

    p0 = Fs_ImgBuffers->words;
    i  = 0;
    p1 = p0 + 1;
    do {
        i++;
        hi    = *p1;
        lo    = *p0;
        t     = hi & 0x001F001F;
        t   <<= 8;
        t    |= lo & 0x001F001F;
        gray  = t * 3;
        t     = hi & 0x03E003E0;
        t   <<= 3;
        lo  >>= 5;
        t    |= lo & 0x001F001F;
        gray += t * 4;
        hi  >>= 2;
        t     = hi & 0x1F001F00;
        lo  >>= 5;
        t    |= lo & 0x001F001F;
        gray += t;
        gray  = (gray >> 3) & 0x1F1F1F1F;
        gray  = 0x1F1F1F1F - gray;

        lo   = gray & 0x001F001F;
        lo  |= (lo << 10) | (lo << 5);
        hi   = gray & 0x1F001F00;
        hi >>= 8;
        hi  |= (hi << 10) | (hi << 5);
        *p0  = lo;
        *p1  = hi;
        p1  += 2;
        p0  += 2;
    } while (i < 0x4B00);
}

s32 func_dryfield_dilapidated_house_8017E56C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message gate for the room's second hotspot. It copies the incoming record to
/// the outgoing one and then writes the answer the caller acts on to the copy's
/// `room`, returning 0 when the message was consumed and 1 when it was not.
///
/// The copy is the `RoomEventMsg` assignment; the rest is two independent id
/// checks. While the session is in the room (`gGameSession->at4.loc.stage` is 2), a
/// type-7 record with no sub-id answers 1, or the session's own value when flag
/// nibble 0x3C is set. A type-7 record in play (`Gp_StateF0.prefix.bytes.field_0` is 1) runs
/// CAP command 0x14 and a type-5 record runs 0x13, each only when the sub-id is
/// clear; everything else is left to the caller and answers 1.
s32 func_dryfield_dilapidated_house_8017E574(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->at4.loc.stage;
    if (s1 == 2) {
        if (in->areaId == 7) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(0x3C) == 0) {
                    out->room = 1;
                } else {
                    out->room = s1;
                }
            }
        }
    }
    if ((in->areaId == 7) && (Gp_StateF0.prefix.bytes.field_0 == 1)) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SpawnIfCapIdle(0x14, 0);
        }
        return 0;
    }
    if (in->areaId == 5) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SpawnIfCapIdle(0x13, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_dryfield_dilapidated_house_8017E684(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_dilapidated_house_8017E68C(Task* task, s32 msgId, GpMsg13EF* arg2, GpMessageArg arg3)
{
    u8 temp_v1;

    temp_v1 = arg2->field_2;
    if ((temp_v1 == 1) && (D_dryfield_dilapidated_house_80183EFC == 0)) {
        D_dryfield_dilapidated_house_80183EFC = (s32)temp_v1;
        func_800E8634(D_dryfield_dilapidated_house_80184408, 0, D_dryfield_dilapidated_house_80184C60);
    }
    return 0;
}

void func_dryfield_dilapidated_house_8017E6DC(Task* arg0)
{
    Task* temp_s1;
    Task* temp_a1;
    s32   temp_v1;

    temp_s1 = gameGetPtrSlot(3);
    temp_a1 = Gp_LookupSlot4(1);
    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            arg0->spawnArg1.value = 0;
            arg0->state          += 1;
            return;
        case 2:
            func_800B0928(temp_s1, temp_a1, 0x200, 0x180, 0x1000);
            /* fallthrough */
        case 1:
            return;
    }
}

void func_dryfield_dilapidated_house_8017E780(Task* arg0)
{
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a0;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            D_dryfield_dilapidated_house_80189B6C = arg0->spawnArg1.value;
            arg0->state                          += 1;
            return;
        case 1:
            var_a0 = (s32)(D_dryfield_dilapidated_house_80189B6C * 3) / (s32)arg0->spawnArg1.value;
            if (D_dryfield_dilapidated_house_80189B6C & 1) {
                var_a0 = -var_a0;
            }
            displaySetShakeY((s8)var_a0);
            temp_v0                               = D_dryfield_dilapidated_house_80189B6C - 1;
            D_dryfield_dilapidated_house_80189B6C = temp_v0;
            if (temp_v0 == 0) {
                taskKill(arg0);
            }
            return;
    }
}

void func_dryfield_dilapidated_house_8017E858(Task* arg0)
{
    s32 var_v0;

    var_v0 = arg0->spawnArg1.value;
    if (var_v0 < 0) {
        Stage_SetEndingFlag();
        taskKill(arg0);
        var_v0 = arg0->spawnArg1.value;
    }
    var_v0                = var_v0 - 1;
    arg0->spawnArg1.value = var_v0;
}

/// State handlers of the task `func_dryfield_dilapidated_house_80181134` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D61C = {
    { func_dryfield_dilapidated_house_8018118C, func_dryfield_dilapidated_house_80181264, taskKill },
};

/// State handlers of the task `func_dryfield_dilapidated_house_801812E8` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D628 = {
    { func_dryfield_dilapidated_house_80181340, func_dryfield_dilapidated_house_801813DC,
      func_dryfield_dilapidated_house_8018142C },
};

/// State handlers of the task `func_dryfield_dilapidated_house_8018145C` dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D634 = {
    { func_dryfield_dilapidated_house_801814B4, func_dryfield_dilapidated_house_80181584,
      func_dryfield_dilapidated_house_801815B8 },
};

/// State handlers of the task `func_dryfield_dilapidated_house_80180F04`
/// dispatches.
static const TaskFuncTable3 D_dryfield_dilapidated_house_8017D640 = {
    { func_dryfield_dilapidated_house_80180B84, func_dryfield_dilapidated_house_80180F5C, taskKill },
};

/// Script command that moves the cutscene task to the given state; does
/// nothing when that task was never spawned.
void func_dryfield_dilapidated_house_8017E8A8(s32 arg0)
{
    if (D_dryfield_dilapidated_house_80189B78 != NULL) {
        D_dryfield_dilapidated_house_80189B78->state = arg0;
    }
}

/// Script command that calls `Gp_PulseState1C`.
void func_dryfield_dilapidated_house_8017E8C8(void)
{
    Gp_PulseState1C();
}

/// Message handler for the start-countdown cue; actor 136300 carries the same
/// body. A positive argument is latched
/// and nothing else happens; otherwise the CD command queue is dropped into
/// Mdec_DecodeToVram mode 2 and -- except for the -2 "already ran" message --
/// the spawn block is filled and the `D_dryfield_dilapidated_house_80183E48` entry started.
///
/// Both halves of the block are written in *each* arm of the countdown test so
/// that each arm is a complete two-store address session: jump optimization
/// then merges the identical tails and the countdown collapses to one `li` per
/// arm, which is what puts the block's `lui` in the delay slot of the entry
/// test. Hoisting `unk2` out of the arms compiles to a different allocation.
void func_dryfield_dilapidated_house_8017E8E8(s32 arg0)
{
    CdCmdQueue* queue;

    queue = &CdCmd_Queue;
    if (arg0 <= 0) {
        queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        if (arg0 != -2) {
            if (arg0 == 0) {
                D_dryfield_dilapidated_house_80189C94.span  = 0x64;
                D_dryfield_dilapidated_house_80189C94.scale = 0x100;
            } else {
                D_dryfield_dilapidated_house_80189C94.span  = 5;
                D_dryfield_dilapidated_house_80189C94.scale = 0x100;
            }
            Task_SpawnFromTable(D_dryfield_dilapidated_house_80183E48, 0, 0, &D_dryfield_dilapidated_house_80189C94);
        }
    } else {
        D_dryfield_dilapidated_house_80189C94.state = arg0;
    }
}

void func_dryfield_dilapidated_house_8017E970(s32 arg0)
{
    if (arg0 == 0) {
        D_dryfield_dilapidated_house_80189B7C->state = 0;
        D_dryfield_dilapidated_house_80189B80.active = 1;
        return;
    }
    D_dryfield_dilapidated_house_80189B7C->state           = 2;
    D_dryfield_dilapidated_house_80189B7C->spawnArg1.value = arg0;
}

static void func_dryfield_dilapidated_house_8017E9A4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_SpawnScript18(&D_80114A24, &D_80114A34);
        D_dryfield_dilapidated_house_80189B80.spawnArg = arg0;
        Task_SpawnFromTable(D_dryfield_dilapidated_house_80183E64, 0, 0,
                            &D_dryfield_dilapidated_house_80189B80.spawnArg);
        return;
    }
    D_dryfield_dilapidated_house_80189B80.active = 1;
}

void func_dryfield_dilapidated_house_8017EA10(s32 arg0)
{
    if (arg0 != 0) {
        D_dryfield_dilapidated_house_801857E8 =
            Task_SpawnFromTable(D_dryfield_dilapidated_house_80186854, 0, 3, gameGetPtrSlot(3));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        taskKill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}

/// Script command that calls `Gp_PulseState1C` and sets bit 0 of
/// `Gp_StateC08.field_6`.
void func_dryfield_dilapidated_house_8017EA7C(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

static void func_dryfield_dilapidated_house_8017EAB4(Task* arg0)
{
    arg0->msgTable = D_dryfield_dilapidated_house_80183E8C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(1) != 0) {
        D_dryfield_dilapidated_house_80189B78 =
            Task_SpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 0, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C94.state = 2;
    D_dryfield_dilapidated_house_80189B7C =
        Task_SpawnFromTable(D_dryfield_dilapidated_house_80183EB4, 2, 0, 0);
    gGameSession->flowFlags = 0x83;
    arg0->state            += 1;
}

/// The room task: runs its current state out of
/// `D_dryfield_dilapidated_house_8017D5C4`, copied onto the stack - setup, the
/// room gate, then `taskKill`.
void func_dryfield_dilapidated_house_8017EB60(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D5C4;
    sp.funcs[task->state](task);
}

/// Projects the eight local-space markers at
/// `D_dryfield_dilapidated_house_801866B4` through the parent task's
/// `DdhCoordWork` matrix and `gGfxViewCoord.workm`, then queues two red
/// `LINE_F2`s as an X at each screen point in `gGpuCurrentOt[10]`.
static void func_dryfield_dilapidated_house_8017EBB8(Task* task)
{
    struct {
        SVECTOR vec;
        s32     sxy;
        s32     dp;
        s32     flag;
        s32     otz;
    } sc;
    MATRIX*  mtx;
    LINE_F2* line;
    u16      sx;
    s32      sy;
    s16      x0;
    s16      y0;
    s16      x1;
    s16      y1;
    s32      i;

    i   = 0;
    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2.pointer)->work)->mtx;
    do {
        sc.vec.vx = D_dryfield_dilapidated_house_801866B4[i].vx;
        sc.vec.vy = D_dryfield_dilapidated_house_801866B4[i].vy;
        sc.vec.vz = D_dryfield_dilapidated_house_801866B4[i].vz;
        gte_SetRotMatrix(mtx);
        gte_ldv0(&sc.vec);
        gte_rtv0();
        gte_stsv(&sc.vec);
        sc.vec.vx = (u16)sc.vec.vx + (u16)mtx->t[0];
        sc.vec.vy = (u16)sc.vec.vy + (u16)mtx->t[1];
        sc.vec.vz = (u16)sc.vec.vz + (u16)mtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&sc.vec);
        gte_rtps();
        gte_stsxy(&sc.sxy);
        gte_stdp(&sc.dp);
        gte_stflg(&sc.flag);
        gte_stszotz(&sc.otz);
        line           = gGpuPrimCursor;
        sx             = sc.sxy;
        sy             = sc.sxy >> 16;
        gGpuPrimCursor = line + 1;
        x0             = sx - 5;
        y0             = sy - 5;
        x1             = sx + 5;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        y1       = sy + 5;
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        i++;
        line->x0 = x1;
        line->y0 = y0;
        line->x1 = x0;
        line->y1 = y1;
        addPrim(gGpuCurrentOt + 10, line);
    } while (i < 8);
}

/// Debug view of the two cubic Bezier segments whose control points start at
/// `D_dryfield_dilapidated_house_801866B4`: samples each at 21 positions,
/// projects every point through the parent task's `DdhCoordWork` matrix and
/// `gGfxViewCoord.workm`, and queues a small `LINE_F2` X at it in
/// `gGpuCurrentOt[10]` - green for the first segment, blue for the second.
static void func_dryfield_dilapidated_house_8017EE58(Task* task)
{
    SVECTOR  vec;
    DVECTOR  sx;
    DVECTOR  sy;
    s32      out[3];
    s32      sxy;
    s32      dp;
    s32      flag;
    s32      otz;
    MATRIX*  mtx;
    LINE_F2* line;
    s32      i;

    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2.pointer)->work)->mtx;
    for (i = 20; i >= 0; i--) {
        func_dryfield_dilapidated_house_8017F418(D_dryfield_dilapidated_house_801866B4, D_dryfield_dilapidated_house_801866B4 + 3, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_rtv0();
        gte_stsv(&vec);
        vec.vx = (u16)vec.vx + (u16)mtx->t[0];
        vec.vy = (u16)vec.vy + (u16)mtx->t[1];
        vec.vz = (u16)vec.vz + (u16)mtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&vec);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0xFF, 0);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
    for (i = 20; i >= 0; i--) {
        func_dryfield_dilapidated_house_8017F418(D_dryfield_dilapidated_house_801866B4 + 3, D_dryfield_dilapidated_house_801866B4 + 6, 20, i, out);
        vec.vx = out[0];
        vec.vy = out[1];
        vec.vz = out[2];
        gte_SetRotMatrix(mtx);
        gte_ldv0(&vec);
        gte_rtv0();
        gte_stsv(&vec);
        vec.vx = (u16)vec.vx + (u16)mtx->t[0];
        vec.vy = (u16)vec.vy + (u16)mtx->t[1];
        vec.vz = (u16)vec.vz + (u16)mtx->t[2];
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&vec);
        gte_rtps();
        gte_stsxy(&sxy);
        gte_stdp(&dp);
        gte_stflg(&flag);
        gte_stszotz(&otz);
        sx.vx = sxy;
        sy.vx = sxy >> 16;

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx - 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx + 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);

        line           = gGpuPrimCursor;
        gGpuPrimCursor = line + 1;
        setLineF2(line);
        setRGB0(line, 0, 0, 0xFF);
        line->x0 = sx.vx + 1;
        line->y0 = sy.vx - 1;
        line->x1 = sx.vx - 1;
        line->y1 = sy.vx + 1;
        addPrim(gGpuCurrentOt + 10, line);
    }
}

/// Evaluates a cubic Bezier segment at frame `pos` of `len`: control points
/// `pts[0..2]` and `p3`, with `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
static void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        func_dryfield_dilapidated_house_80181290(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_dryfield_dilapidated_house_80181290(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_dryfield_dilapidated_house_80181290(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

static void func_dryfield_dilapidated_house_8017F568(Task* task, SVECTOR* verts, s32 arg2)
{
    CVECTOR   colors[24];
    s8*       quad;
    CVECTOR*  col;
    POLY_G4*  poly;
    DR_TPAGE* tpage;
    s32       i;
    u16       scale;
    s32       a, b, c, d;

    quad                 = D_dryfield_dilapidated_house_801866F4[0];
    scale                = ((DdhCoordWork*)((Task*)task->spawnArg2.pointer)->work)->field_4;
    task->killCountdown += 0x40;
    if (task->killCountdown >= 0x800) {
        task->killCountdown = 0;
    }
    rsin(task->killCountdown);
    for (i = 0; i < 24; i++) {
        colors[i].r = (D_dryfield_dilapidated_house_80186734[i][0] * (s16)scale) >> 12;
        colors[i].g = (D_dryfield_dilapidated_house_80186734[i][1] * (s16)scale) >> 12;
        colors[i].b = (D_dryfield_dilapidated_house_80186734[i][2] * (s16)scale) >> 12;
    }
    col = colors;
    for (i = 0; i < 6; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = verts[a].vx;
        poly->y0   = verts[a].vy;
        poly->x1   = verts[b].vx;
        poly->y1   = verts[b].vy;
        poly->x2   = verts[c].vx;
        poly->y2   = verts[c].vy;
        poly->x3   = verts[d].vx;
        poly->y3   = verts[d].vy;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + 3, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000425;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + 3, tpage);
        quad += 4;
    }
    for (i = 6; i < 16; i++) {
        a              = quad[0];
        b              = quad[1];
        c              = quad[2];
        d              = quad[3];
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 8);
        poly->code = 0x3A;
        poly->r0   = col[a].r;
        poly->g0   = col[a].g;
        poly->b0   = col[a].b;
        poly->r1   = col[b].r;
        poly->g1   = col[b].g;
        poly->b1   = col[b].b;
        poly->r2   = col[c].r;
        poly->g2   = col[c].g;
        poly->b2   = col[c].b;
        poly->r3   = col[d].r;
        poly->g3   = col[d].g;
        poly->b3   = col[d].b;
        poly->x0   = verts[a].vx;
        poly->y0   = verts[a].vy;
        poly->x1   = verts[b].vx;
        poly->y1   = verts[b].vy;
        poly->x2   = verts[c].vx;
        poly->y2   = verts[c].vy;
        poly->x3   = verts[d].vx;
        poly->y3   = verts[d].vy;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + 3, poly);
        tpage          = gGpuPrimCursor;
        gGpuPrimCursor = tpage + 1;
        setlen(tpage, 1);
        tpage->code[0] = 0xE1000465;
        addPrim((&gGpuCurrentOt[((((u32)(arg2 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]) + 3, tpage);
        quad += 4;
    }
}

/// Lays out 24 screen-space points in `verts` as four rings of six around two
/// ends of a segment. The ends come from `D_dryfield_dilapidated_house_80186794`,
/// mirrored in x when the task's spawn arg 1 is 1, rotated by the parent task's
/// `DdhCoordWork` matrix; the far end is pulled toward the near one by the
/// parent's `field_4` ramp before both are moved by the matrix translation and
/// projected. Each ring's offsets are rotated to the segment's screen angle and
/// scaled by the projection distance over the last projected depth, which is
/// left in `*arg2` (`*arg3` gets the GTE flags). Rings 0 and 1 use the tables at
/// their natural size, rings 2 and 3 scaled by a factor that pulses with
/// `killCountdown`.
static void func_dryfield_dilapidated_house_8017FAD4(Task* task, SVECTOR* verts, s32* arg2, s32* arg3)
{
    SVECTOR        a;
    SVECTOR        b;
    OverlayMat     rot;
    DdhScreenPoint proj[2];
    DdhCoordWork*  work;
    MATRIX*        mtx;
    SVECTOR*       src;
    s16            t;
    s16            r;
    s32            scale;
    GpMtxWords*    words;
    s32            i;
    u16            f;
    s16            x0;
    s32            y0;
    s16            x1;
    s32            y1;
    s32            dx;
    s32            dy;
    s32            side;

    side = task->spawnArg1.value;
    work = ((Task*)task->spawnArg2.pointer)->work;
    mtx  = &work->mtx;
    f    = work->field_4;
    a.vx = D_dryfield_dilapidated_house_80186794[0].vx;
    a.vy = D_dryfield_dilapidated_house_80186794[0].vy;
    a.vz = D_dryfield_dilapidated_house_80186794[0].vz;
    src  = &D_dryfield_dilapidated_house_80186794[1];
    b.vx = src->vx;
    b.vy = src->vy;
    b.vz = src->vz;
    if (side == 1) {
        a.vx *= -1;
        b.vx *= -1;
    }
    gte_SetRotMatrix(mtx);
    gte_ldv0(&a);
    gte_rtv0();
    gte_stsv(&a);
    gte_ldv0(&b);
    gte_rtv0();
    gte_stsv(&b);
    t     = (s16)f * 0.75 + 1024.0;
    b.vx  = a.vx + (b.vx - a.vx) * t / 4096;
    b.vy  = a.vy + (b.vy - a.vy) * t / 4096;
    b.vz  = a.vz + (b.vz - a.vz) * t / 4096;
    a.vx += mtx->t[0];
    a.vy += mtx->t[1];
    a.vz += mtx->t[2];
    b.vx += mtx->t[0];
    b.vy += mtx->t[1];
    b.vz += mtx->t[2];
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&a);
    gte_rtps();
    gte_stsxy(&proj[0].sxy);
    gte_stdp(&proj[0].depthCue);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    gte_ldv0(&b);
    gte_rtps();
    gte_stsxy(&proj[1].sxy);
    gte_stdp(&proj[0].depthCue);
    gte_stflg(arg3);
    gte_stszotz(arg2);
    dy                = proj[0].sxy.vy - proj[1].sxy.vy;
    x1                = proj[1].sxy.vx;
    x0                = proj[0].sxy.vx;
    dx                = x1 - x0;
    y0                = proj[0].sxy.vy;
    y1                = proj[1].sxy.vy;
    i                 = ratan2(dx, dy);
    scale             = gDisplayState.screenDistance;
    rot.ident.m00_m01 = 0x1000;
    rot.ident.m02_m10 = 0;
    words             = &rot.ident;
    words->m11_m12    = 0x1000;
    rot.ident.m20_m21 = 0;
    words->m22        = 0x1000;
    RotMatrixZ(i, &rot.mat);
    gte_SetRotMatrix(&rot.mat);
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867A4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867A4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i].vx = b.vx + x0;
        verts[i].vy = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = D_dryfield_dilapidated_house_801867D4[i].vx * scale / *arg2;
        a.vy = D_dryfield_dilapidated_house_801867D4[i].vy * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 6].vx = b.vx + x1;
        verts[i + 6].vy = b.vy + y1;
    }
    r = 4096.0 - rsin(task->killCountdown) * 0.5 + 4096.0;
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867A4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867A4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 12].vx = b.vx + x0;
        verts[i + 12].vy = b.vy + y0;
    }
    for (i = 0; i < 6; i++) {
        a.vx = ((D_dryfield_dilapidated_house_801867D4[i].vx * r) >> 12) * scale / *arg2;
        a.vy = ((D_dryfield_dilapidated_house_801867D4[i].vy * r) >> 12) * scale / *arg2;
        gte_ldv0(&a);
        gte_rtv0();
        gte_stsv(&b);
        verts[i + 18].vx = b.vx + x1;
        verts[i + 18].vy = b.vy + y1;
    }
}

/// Projects the two 16-vertex rings in `verts` (inner at 0..15, outer at
/// 16..31) and joins them with 16 semi-transparent `POLY_G4`s, wrapping the
/// last quad back to vertex 0. The inner edge is a grey whose level is the
/// parent task's `DdhCoordWork::field_8` clamped to 0x400 and scaled to 0..0xFF;
/// the outer edge is black. Each quad goes into the ordering table four entries
/// past its average depth, preceded by a `DR_TPAGE` selecting blend mode 3.
static void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts)
{
    s32       sxy[32];
    s32       sz[16];
    CVECTOR   c0;
    CVECTOR   c1;
    POLY_G4*  prim;
    DR_TPAGE* tp;
    s16       level;
    s32       i;
    s32*      xy;
    SVECTOR*  v;
    s32*      p;
    s32*      z;

    v     = verts;
    p     = sxy;
    z     = sz;
    level = ((DdhCoordWork*)((Task*)task->spawnArg2.pointer)->work)->field_8;
    SetRotMatrix(&gGfxViewCoord.workm);
    SetTransMatrix(&gGfxViewCoord.workm);
    for (i = 0; i < 16; i++) {
        gte_ldv3(v, v + 1, v + 16);
        gte_rtpt();
        gte_stsxy3(p, p + 1, p + 16);
        gte_stszotz(z);
        p++;
        z++;
        v++;
    }
    if (level > 0x400) {
        level = 0x400;
    }
    c0.r = level * 0xFF / 0x400;
    c0.g = level * 0xFF / 0x400;
    c0.b = 0;
    c1.r = 0;
    c1.g = 0;
    c1.b = 0;
    for (i = 0; i < 16; i++) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 8);
        setcode(prim, 0x3A);
        prim->r0 = c0.r;
        prim->g0 = c0.g;
        prim->b0 = c0.b;
        prim->r1 = c0.r;
        prim->g1 = c0.g;
        prim->b1 = c0.b;
        prim->r2 = c1.r;
        prim->g2 = c1.g;
        prim->b2 = c1.b;
        prim->r3 = c1.r;
        prim->g3 = c1.g;
        prim->b3 = c1.b;
        // Packed screen words, one per vertex; each `xy` word is two words past
        // the previous one because a colour word sits between them.
        xy = (s32*)&prim->x0;
        if (i < 15) {
            xy[0] = sxy[i];
            xy[2] = sxy[i + 1];
            xy[4] = sxy[i + 16];
            xy[6] = sxy[i + 17];
        } else {
            xy[0] = sxy[15];
            xy[2] = sxy[0];
            xy[4] = sxy[31];
            xy[6] = sxy[16];
        }
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sz[i] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + 4, prim);
        tp             = gGpuPrimCursor;
        gGpuPrimCursor = tp + 1;
        setlen(tp, 1);
        tp->code[0] = 0xE1000465;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sz[i] << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + 4, tp);
    }
}

static void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts)
{
    DdhAngleStep* work;
    DdhCoordWork* src;
    MATRIX*       mtx;
    SVECTOR*      ofs;
    SVECTOR*      ofs2;
    SVECTOR       pos[2];
    SVECTOR*      v0;
    SVECTOR*      v1;
    s16           tx;
    s16           ty;
    s16           tz;
    s32           i;
    s32           ang;
    s32           c;
    s32           s;

    v0 = verts;
    v1 = &verts[16];

    work = (DdhAngleStep*)task->work;
    src  = (DdhCoordWork*)((Task*)task->spawnArg2.pointer)->work;

    ofs       = D_dryfield_dilapidated_house_80186844;
    ofs2      = D_dryfield_dilapidated_house_80186844 + 1;
    pos[0].vx = ofs->vx;
    pos[0].vy = ofs->vy;
    pos[0].vz = ofs->vz;
    pos[1].vx = ofs2->vx;
    pos[1].vy = ofs2->vy;
    pos[1].vz = ofs2->vz;

    mtx = &src->mtx;

    tx = mtx->t[0];
    ty = mtx->t[1];
    tz = mtx->t[2];

    gte_SetRotMatrix(mtx);

    for (i = 0; i < 16; i++) {
        ang = (i << 12) >> 4;
        c   = rcos(ang);
        s   = rsin(ang);

        v0->vx = pos[0].vx + ((c * 0x96) >> 12);
        v0->vy = pos[0].vy + ((s * 0x4B) >> 12);
        v0->vz = pos[0].vz;

        gte_ldv0(v0);
        gte_rtv0();
        gte_stsv(v0);

        v0->vx += tx;
        v0->vy += ty;
        v0->vz += tz;
        v0++;

        v1->vx = pos[1].vx + ((c * 0xFA) >> 12);
        v1->vy = pos[1].vy + ((s * 0x7D) >> 12);
        v1->vz = pos[1].vz + ((rsin(work->step[i] >> 2) * 0x64) >> 12);

        work->step[i] = (work->step[i] + D_dryfield_dilapidated_house_80186804[i]) & 0x3FFF;

        gte_ldv0(v1);
        gte_rtv0();
        gte_stsv(v1);

        v1->vx += tx;
        v1->vy += ty;
        v1->vz += tz;
        v1++;
    }
}

/// Morphs the task's model by `arg2` (0..0x1000): restores `rec`'s vertex
/// snapshot into the model from part `rec->field_14`, interpolates those
/// vertices toward `rec->field_0` through the GTE, and, when `rec->field_4` is
/// set, blends each normal between `rec->field_4` and the snapshot in
/// `rec->field_C`.
static void func_dryfield_dilapidated_house_80180A0C(Task* task, DdhRoomRec* rec, s32 arg2)
{
    s32        i;
    s32        count;
    s32        first;
    TmdSource* src;
    u16*       dst;
    u16*       from;
    u16*       dstMid;
    u16*       fromMid;
    SVECTOR*   nrm;
    SVECTOR*   nrmA;
    SVECTOR*   nrmB;
    SVECTOR*   nrmDst;
    s32        blend;
    s32        inv;
    u16        vx;
    u16        vz;

    i     = 0;
    count = rec->field_16;
    src   = task->extra.tmd->source;
    first = rec->field_14;
    from  = (u16*)&rec->field_8[first];
    nrm   = src->normals;
    dst   = (u16*)&src->verts[first];
    if (count > 0) {
        fromMid = from + 2;
        dstMid  = dst + 2;
        do {
            vx         = *from;
            from      += 4;
            i         += 1;
            *dst       = vx;
            dst       += 4;
            dstMid[-1] = fromMid[-1];
            vz         = fromMid[0];
            fromMid   += 4;
            dstMid[0]  = vz;
            dstMid    += 4;
        } while (i < count);
    }
    blend = arg2;
    inv   = 0x1000 - blend;
    gteMIMefunc(src->verts + rec->field_14, rec->field_0, rec->field_16, blend);
    nrmA = rec->field_4;
    if (nrmA != NULL) {
        count = rec->field_12;
        nrmB  = rec->field_C;
        i     = 0;
        if (count > 0) {
            do {
                gte_lddp(blend);
                gte_ldsv(nrmA);
                gte_gpf12();
                nrmDst = nrm + i;
                gte_lddp(inv);
                gte_ldsv(nrmB);
                gte_gpl12();
                nrmB++;
                i++;
                nrmA++;
                gte_stsv(nrmDst);
            } while (i < count);
        }
    }
}

static void func_dryfield_dilapidated_house_80180B84(Task* task)
{
    Task*         parent;
    TmdObject*    obj;
    TmdObject*    parentObj;
    GfxCoord*     coord;
    GfxCoord*     parentCoord;
    DdhCoordWork* work;
    DdhRoomRec*   rec;
    TmdSource*    source;
    SVECTOR*      dst;
    SVECTOR*      dst2;
    SVECTOR*      src2;
    SVECTOR*      verts;
    TaskDesc*     table;
    Task*         spawned;
    GfxCoord*     childCoord;
    u16           flags;
    s32           i;

    parent      = (Task*)task->spawnArg2.pointer;
    obj         = task->extra.tmd;
    parentObj   = parent->extra.tmd;
    coord       = obj->coords;
    parentCoord = parentObj->coords;
    work        = (DdhCoordWork*)Mem_Malloc(0x6C, false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = work;
    work->field_0 = 0;
    flags         = obj->flags | TMD_OBJECT_HIDDEN;
    obj->flags    = flags;
    if (!(parentObj->flags & TMD_OBJECT_HIDDEN)) {
        obj->flags = flags & 0xFF7F;
    }
    obj->otOffset       = 4;
    obj->flags         |= TMD_OBJECT_SEMI_TRANS;
    parentCoord        += task->spawnArg1.value;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = parentCoord;
    obj->lightMtx       = parentObj->lightMtx;
    obj->colorMtx       = parentObj->colorMtx;
    Task_Reparent(parent, task);

    rec    = &D_dryfield_dilapidated_house_8018669C;
    source = task->extra.tmd->source;
    dst    = rec->field_8;
    dst2   = rec->field_C;
    verts  = source->verts;
    for (i = 0; i < rec->field_10; i++) {
        dst[i].vx = verts[i].vx;
        dst[i].vy = verts[i].vy;
        dst[i].vz = verts[i].vz;
    }
    if (rec->field_4 != 0) {
        src2 = source->normals;
        for (i = 0; i < rec->field_12; i++) {
            dst2[i].vx = src2[i].vx;
            dst2[i].vy = src2[i].vy;
            dst2[i].vz = src2[i].vz;
        }
    }

    func_dryfield_dilapidated_house_80180FD8(task);

    table   = D_dryfield_dilapidated_house_80186854;
    spawned = Task_SpawnFromTable(table, 3, 9, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 3, 0x11, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 0, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 1, task);
    if (spawned != NULL) {
        childCoord        = spawned->extra.tmd->coords;
        childCoord->coord = work->mtx;
    }

    task->exitCallback = func_dryfield_dilapidated_house_80180FB8;
    task->state       += 1;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D640`,
/// copied onto the stack: `func_dryfield_dilapidated_house_80180B84`,
/// `func_dryfield_dilapidated_house_80180F5C`, then `taskKill`.
void func_dryfield_dilapidated_house_80180F04(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D640;
    sp.funcs[task->state](task);
}

static void func_dryfield_dilapidated_house_80180F5C(Task* arg0)
{
    DdhCoordWork* work;
    s32           temp_v0;

    work = (DdhCoordWork*)arg0->work;
    func_dryfield_dilapidated_house_801810F8(arg0->extra.tmd,
                                             ((Task*)arg0->spawnArg2.pointer)->extra.tmd);
    func_dryfield_dilapidated_house_80181028(arg0);
    temp_v0       = func_dryfield_dilapidated_house_80180FD8(arg0);
    work->field_0 = temp_v0;
    work->field_8 = temp_v0;
    work->field_4 = temp_v0;
}

/// Exit callback `func_dryfield_dilapidated_house_80180B84` installs on its
/// task: it kills the task.
static void func_dryfield_dilapidated_house_80180FB8(Task* task)
{
    taskKill(task);
}

/// Steps the task's 0..0x1000 ramp by 0x44, saturating at 0x1000, and feeds the
/// distance still to run (`0x1000 - ramp`) to the room record's matrix/vertex
/// interpolator. Returns the ramp value, which the caller stores into its
/// `DdhCoordWork`.
static s32 func_dryfield_dilapidated_house_80180FD8(Task* task)
{
    s32 ramp;

    ramp = task->killCountdown + 0x44;
    if (ramp >= 0x1001) {
        ramp = 0x1000;
    }
    task->killCountdown = ramp;
    func_dryfield_dilapidated_house_80180A0C(task, &D_dryfield_dilapidated_house_8018669C, 0x1000 - ramp);
    return ramp;
}

/// Rebuilds the work block's `mtx` as the identity, then composes it against
/// the parent model's `GfxCoord` chain: each node's `coord` rotation is
/// multiplied in, and its translation is rotated by the accumulated matrix and
/// added to `mtx.t`. Steps one coordinate record at a time from the head of the
/// parent's array up to the record this task's own `coord` links with `parent`.
static void func_dryfield_dilapidated_house_80181028(Task* task)
{
    VECTOR        vec;
    GfxCoord*     coord;
    DdhCoordWork* work;
    GfxCoord*     node;
    MATRIX*       mtx;

    coord                  = task->extra.tmd->coords;
    work                   = (DdhCoordWork*)task->work;
    node                   = ((Task*)task->spawnArg2.pointer)->extra.tmd->coords;
    mtx                    = &work->mtx;
    *(s32*)&work->mtx      = ONE;
    MATRIX_PAIR(mtx, 0, 2) = 0;
    MATRIX_PAIR(mtx, 1, 1) = ONE;
    MATRIX_PAIR(mtx, 2, 0) = 0;
    mtx->m[2][2]           = ONE;
    mtx->t[0]              = 0;
    mtx->t[1]              = 0;
    mtx->t[2]              = 0;
    do {
        ApplyMatrixLV(mtx, (VECTOR*)node->coord.t, &vec);
        mtx->t[0] += vec.vx;
        mtx->t[1] += vec.vy;
        mtx->t[2] += vec.vz;
        MulMatrix0(mtx, &node->coord, mtx);
    } while (node++ != coord->parent);
}

static void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src)
{
    if (!(src->flags & TMD_OBJECT_HIDDEN)) {
        dst->flags &= ~TMD_OBJECT_HIDDEN;
        return;
    }
    dst->flags |= TMD_OBJECT_HIDDEN;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D61C`,
/// copied onto the stack: `func_dryfield_dilapidated_house_8018118C`,
/// `func_dryfield_dilapidated_house_80181264`, then `taskKill`.
void func_dryfield_dilapidated_house_80181134(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D61C;
    sp.funcs[task->state](task);
}

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D61C`:
/// snapshots the placed model coordinate's matrix into a fresh `DdhModelWork`,
/// seeds its 0x1000 word, marks the model's `TmdObject` hidden (bit 0x80 of
/// `field_C`), re-parents the task that spawned this one under it and advances
/// to state 1.
static void func_dryfield_dilapidated_house_8018118C(Task* arg0)
{
    TmdObject*    obj;
    GfxCoord*     coord;
    DdhModelWork* work;

    obj   = arg0->extra.tmd;
    coord = obj->coords;
    work  = (DdhModelWork*)Mem_Malloc(0x24, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work     = work;
    work->field_20 = 0x1000;
    work->mtx      = coord->coord;
    obj->flags    |= TMD_OBJECT_HIDDEN;
    Task_Reparent((Task*)arg0->spawnArg2.pointer, arg0);
    arg0->state += 1;
}

static void func_dryfield_dilapidated_house_80181264(Task* arg0)
{
    func_dryfield_dilapidated_house_8017EBB8(arg0);
    func_dryfield_dilapidated_house_8017EE58(arg0);
}

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
static void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D628`,
/// copied onto the stack.
void func_dryfield_dilapidated_house_801812E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D628;
    sp.funcs[task->state](task);
}

static void func_dryfield_dilapidated_house_80181340(Task* arg0)
{
    GfxCoord* coord;
    void*     work;

    coord = arg0->extra.tmd->coords;
    work  = Mem_Malloc(4, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work    = work;
    coord->parent = ((Task*)arg0->spawnArg2.pointer)->extra.tmd->coords;
    Task_Reparent((Task*)arg0->spawnArg2.pointer, arg0);
    arg0->exitCallback = func_dryfield_dilapidated_house_8018142C;
    arg0->state       += 1;
}

static void func_dryfield_dilapidated_house_801813DC(Task* task)
{
    SVECTOR verts[24];
    s32     sp0;
    s32     sp1;

    func_dryfield_dilapidated_house_8017FAD4(task, verts, &sp0, &sp1);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
}

static void func_dryfield_dilapidated_house_8018142C(Task* arg0)
{
    GfxCoord* coord;

    coord         = arg0->extra.tmd->coords;
    coord->parent = &gGfxViewCoord;
    taskKill(arg0);
}

/// Runs the task's current state out of `D_dryfield_dilapidated_house_8017D634`,
/// copied onto the stack.
void func_dryfield_dilapidated_house_8018145C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_dilapidated_house_8017D634;
    sp.funcs[task->state](task);
}

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D634`,
/// dispatched by `func_dryfield_dilapidated_house_8018145C`: fills a fresh
/// `DdhAngleStep` with the shared per-part angle table scaled by this task's spawn
/// arg (each wrapped into the 0x4000 angle period), links the model coordinate
/// this task works on to the parent model's coordinate array, and re-parents the
/// task that spawned this one under it.
static void func_dryfield_dilapidated_house_801814B4(Task* arg0)
{
    DdhAngleStep* work;
    GfxCoord*     coord;
    s32           i;

    coord = arg0->extra.tmd->coords;
    work  = (DdhAngleStep*)Mem_Malloc(0x40, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = work;
    for (i = 0; i < 0x10; i++) {
        work->step[i] = (D_dryfield_dilapidated_house_80186804[i] * arg0->spawnArg1.value) & 0x3FFF;
    }
    coord->parent = ((Task*)arg0->spawnArg2.pointer)->extra.tmd->coords;
    Task_Reparent((Task*)arg0->spawnArg2.pointer, arg0);
    arg0->state += 1;
}

static void func_dryfield_dilapidated_house_80181584(Task* task)
{
    SVECTOR verts[32];

    func_dryfield_dilapidated_house_80180738(task, verts);
    func_dryfield_dilapidated_house_801803A4(task, verts);
}

static void func_dryfield_dilapidated_house_801815B8(Task* arg0)
{
    GfxCoord* coord;

    coord         = arg0->extra.tmd->coords;
    coord->parent = &gGfxViewCoord;
    taskKill(arg0);
}

/// Draws one prism from `D_dryfield_dilapidated_house_80186884[arg1..]` as five
/// gouraud `POLY_G4`: four sides joining the lit ring to the far ring, then a
/// cap over the lit ring. Each corner is rotated by `coord`'s `workm` and moved
/// by its translation before projection through `GsWSMATRIX`. The lit corners
/// share a grey that pulses with the display frame; the far corners are black.
static void func_dryfield_dilapidated_house_801815E8(GfxCoord* coord, s16 arg1)
{
    RoomQuadScratch* blk;
    POLY_G4*         prim;
    s32              i;
    s32              next;
    s32              far;
    s32              farNext;
    u8               shade;

    SCRATCH_PUSH(RoomQuadScratch);
    blk = SCRATCH_HEAD(RoomQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    shade = (rsin(gDisplayState.animFrame << 10) >> 11) + 0x14;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + i]);
        gte_rtv0();
        gte_stsv(&blk->v[0]);
        blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
        blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
        blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        next = (i + 1) & 3;
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + next]);
        gte_rtv0();
        gte_stsv(&blk->v[1]);
        blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
        blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
        blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        far = i + 4;
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + far]);
        gte_rtv0();
        gte_stsv(&blk->v[2]);
        blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
        blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
        blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&coord->workm);
        farNext = next + 4;
        gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + farNext]);
        gte_rtv0();
        gte_stsv(&blk->v[3]);
        blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
        blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
        blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        setRGB0(prim, shade, shade, shade);
        setRGB1(prim, shade, shade, shade);
        setRGB2(prim, 0, 0, 0);
        setRGB3(prim, 0, 0, 0);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1]);
    gte_rtv0();
    gte_stsv(&blk->v[0]);
    blk->v[0].vx = (u16)blk->v[0].vx + (u16)coord->workm.t[0];
    blk->v[0].vy = (u16)blk->v[0].vy + (u16)coord->workm.t[1];
    blk->v[0].vz = (u16)blk->v[0].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + 1]);
    gte_rtv0();
    gte_stsv(&blk->v[1]);
    blk->v[1].vx = (u16)blk->v[1].vx + (u16)coord->workm.t[0];
    blk->v[1].vy = (u16)blk->v[1].vy + (u16)coord->workm.t[1];
    blk->v[1].vz = (u16)blk->v[1].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + 3]);
    gte_rtv0();
    gte_stsv(&blk->v[2]);
    blk->v[2].vx = (u16)blk->v[2].vx + (u16)coord->workm.t[0];
    blk->v[2].vy = (u16)blk->v[2].vy + (u16)coord->workm.t[1];
    blk->v[2].vz = (u16)blk->v[2].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&D_dryfield_dilapidated_house_80186884[arg1 + 2]);
    gte_rtv0();
    gte_stsv(&blk->v[3]);
    blk->v[3].vx = (u16)blk->v[3].vx + (u16)coord->workm.t[0];
    blk->v[3].vy = (u16)blk->v[3].vy + (u16)coord->workm.t[1];
    blk->v[3].vz = (u16)blk->v[3].vz + (u16)coord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    setRGB0(prim, shade, shade, shade);
    setRGB1(prim, shade, shade, shade);
    setRGB2(prim, shade, shade, shade);
    setRGB3(prim, shade, shade, shade);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
    SCRATCH_POP(RoomQuadScratch);
}

/// Near and far trail offsets. `[0]` seeds the object's coordinate on the first
/// frame and `[1]` the second ring; `D_dryfield_dilapidated_house_80186944[1]` is
/// `[1]` under its own name, because the per-frame path in state 1 rebuilds
/// its address from scratch.

/// Per-frame twin trail. State 0 places the object's coordinate at
/// `D_dryfield_dilapidated_house_80186944[0]` and the second ring at `[1]`,
/// then seeds all sixteen trail slots with that pose. State 1 re-poses both
/// frames every frame, writes them into slot `field_22 & 7`, re-runs the whole
/// ring so the older slots follow their parents, and hands the ribbon to
/// `func_dryfield_dilapidated_house_801823B8`. The task frees itself once
/// `age` reaches spawn arg 1. It idles whole while `Gp_State1C->effectControl`
/// is 2 or more.
void func_dryfield_dilapidated_house_80181F08(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        work->age++;
        switch (task->state) {
            case 0:
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_dryfield_dilapidated_house_80186944[0].vx;
                objCoord->coord.t[1]   = D_dryfield_dilapidated_house_80186944[0].vy;
                objCoord->coord.t[2]   = D_dryfield_dilapidated_house_80186944[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_dryfield_dilapidated_house_80186944[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &D_dryfield_dilapidated_house_80189DE0[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &D_dryfield_dilapidated_house_8018A060[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                return;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_dryfield_dilapidated_house_80186944[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &D_dryfield_dilapidated_house_80189DE0[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &D_dryfield_dilapidated_house_8018A060[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &D_dryfield_dilapidated_house_80189DE0[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &D_dryfield_dilapidated_house_8018A060[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_dryfield_dilapidated_house_801823B8(work->age & 7, 0x210);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `slot`. Each quad spans `workm.t` of two
/// adjacent slots on `D_dryfield_dilapidated_house_80189DE0` and
/// `D_dryfield_dilapidated_house_8018A060`. Dropped when `gte_stszotz` is
/// closer than 0x11. `flags` is the beam colour, three 2-bit channels at
/// bits 8, 4 and 0 that each multiply the 0x40-9i fade.
static void func_dryfield_dilapidated_house_801823B8(s16 slot, s16 flags)
{
    OverlayFlaggedQuadScratch* blk;
    GfxCoord*                  a;
    GfxCoord*                  b;
    POLY_G4*                   prim;
    s32                        i;
    s32                        j;
    s32                        i0;
    s32                        i1;
    s32                        hi;
    s32                        lo;
    s32                        fade;

    SCRATCH_PUSH(OverlayFlaggedQuadScratch);
    blk = SCRATCH_HEAD(OverlayFlaggedQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 7; i++) {
        j            = slot - i;
        i0           = j & 7;
        i1           = (j - 1) & 7;
        a            = &D_dryfield_dilapidated_house_80189DE0[i0];
        blk->v[0].vx = (u16)a->workm.t[0];
        blk->v[0].vy = (u16)a->workm.t[1];
        b            = &D_dryfield_dilapidated_house_8018A060[i0];
        blk->v[0].vz = (u16)a->workm.t[2];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        a            = &D_dryfield_dilapidated_house_80189DE0[i1];
        blk->v[1].vz = (u16)b->workm.t[2];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        b            = &D_dryfield_dilapidated_house_8018A060[i1];
        blk->v[2].vz = (u16)a->workm.t[2];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG4(prim);
        gte_stsxy(&prim->x0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            fade = 0x40 - i * 9;
            hi   = fade & 0xFF;
            lo   = (fade - 9) & 0xFF;
            setRGB0(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB1(prim, hi * (flags >> 8), hi * ((flags >> 4) & 3), hi * (flags & 3));
            setRGB2(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            setRGB3(prim, lo * (flags >> 8), lo * ((flags >> 4) & 3), lo * (flags & 3));
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
    }
    SCRATCH_POP(OverlayFlaggedQuadScratch);
}

/// Per-frame state machine of the ``DdhEffWork`` effect family's fade-in
/// handler: state 0 seeds the work block (0xC0 / 0x500 scale and angle, a
/// 12-bit `Gp_LcgState` draw as the third ramp value, a `Gp_SpawnEff` and a
/// fade quad), maps the task's own coordinate onto
/// `Gp_RoomCoords[0]` and spawns the ring of `0x60275` flame effects, then
/// re-parents each onto this task. State 1 steps the angle by 0x40 per frame
/// and runs two more draws against the same coordinate. While the
/// `Gp_State1C` fade is armed the frame counter is rolled back and the work
/// block is released as soon as the fade reaches 4 or the angle passes
/// 0x580.
void func_dryfield_dilapidated_house_80182744(Task* task)
{
    DdhEffWork*   work;
    GfxCoord*     coord;
    GpCoord64*    rc;
    GpPointLight* tail;
    GpEffWork*    eff;
    u16           tick;
    u16           tick1;
    s16           size;
    s32           angle;
    s32           i;
    u8            rgb[3];

    work           = task->spawnArg2.pointer;
    coord          = task->extra.coordBody->coord;
    tick           = work->field_22;
    tick1          = tick + 1;
    work->field_22 = tick1;
    rc             = &Gp_RoomCoords[0];
    tail           = &rc->light;

    switch (task->state) {
        case 0:
            if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->field_22 = tick;
                if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    Gp_ReleaseState1CMem(work, task);
                }
                return;
            }
            work->field_24 = 0xC0;
            work->field_26 = 0x500;
            work->field_20 = 0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_28 = (Gp_LcgState >> 16) & 0xFFF;
            Gp_SpawnEff(0x60274, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            Gp_RoomCoords[0].framesLeft         = 4;
            tail->inner                         = 0x200;
            tail->outer                         = 0x2000;
            Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
            size                                = ((Gp_LcgState >> 16) & 0x700) + 0x800;
            tail->head.r                        = size;
            tail->head.g                        = size >> 1;
            tail->head.b                        = size >> 2;
            tail->head.u.coord.coord.t[0]       = coord->coord.t[0];
            tail->head.u.coord.coord.t[1]       = coord->coord.t[1];
            tail->head.u.coord.coord.t[2]       = coord->coord.t[2];
            rc->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            i                                   = 0;
            func_dryfield_dilapidated_house_801832A8(coord, (s16)work->field_22, work->field_26, work->field_28);
            func_dryfield_dilapidated_house_80182F14(coord, work->field_26, (s16)(u16)work->field_24 >> 1);
            work->field_26 = 0x380;
            do {
                eff = Gp_SpawnEff(0x60275, coord, i, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->task);
                }
                i += 0x2AA;
            } while (i < 0x556);
            task->state = 1;
            return;
        case 1:
            if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->field_22 = tick;
                if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                    Gp_ReleaseState1CMem(work, task);
                }
                return;
            }
            func_dryfield_dilapidated_house_801832A8(coord, (s16)tick1, work->field_26, work->field_28);
            func_dryfield_dilapidated_house_80182F14(coord, work->field_26, (s16)(u16)work->field_24 >> 1);
            func_dryfield_dilapidated_house_80182F14(coord, (s16)((u16)work->field_26 * 2), (s16)(u16)work->field_24 >> 1);
            angle          = (u16)work->field_26;
            angle         += 0x40;
            work->field_26 = angle;
            if ((s16)angle >= 0x581) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws the flame column: two 16-vertex rings of radius `arg1` and
/// `arg1 + 0x100` are built in the XY plane (`vz` 0x100 / 0) by `rsin` /
/// `rcos`, rotated by `arg0`'s `workm` and offset by its translation, then
/// each of the 16 segments is projected through `GsWSMATRIX` as one `POLY_G4`.
/// The inner edge carries the unsigned `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)`
/// and the outer edge fades to black; a negative `gte_stflg` drops the segment.
/// Same body as `func_pyrokinesis_8012FC34`.
static void func_dryfield_dilapidated_house_80182A18(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;
    u32            ramp;
    u8             red;
    u8             grn;
    u8             blu;

    /* The ramp halves are unsigned: writing them as `(u16)arg2 >> 1` folds the
     * widening into an `andi`, where the ROM shifts the value up and back. */
    ramp  = (u32)arg2 << 16;
    red   = arg2;
    grn   = ramp >> 17;
    blu   = ramp >> 18;
    r1    = arg1 + 0x100;
    block = SCRATCH_PUSH(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = (rcos(ang) * r0) >> 12;
        block->inner[i].vz = 0x100;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx = (u16)block->inner[i].vx + (u16)arg0->workm.t[0];
        block->inner[i].vy = (u16)block->inner[i].vy + (u16)arg0->workm.t[1];
        block->inner[i].vz = (u16)block->inner[i].vz + (u16)arg0->workm.t[2];
        block->outer[i].vx = (rsin(ang) * r1) >> 12;
        op                 = &block->inner[i] + 16;
        op->vy             = (rcos(ang) * r1) >> 12;
        op->vz             = 0;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx = (u16)block->outer[i].vx + (u16)arg0->workm.t[0];
        op->vy             = (u16)op->vy + (u16)arg0->workm.t[1];
        op->vz             = (u16)op->vz + (u16)arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpBandScratch);
}

/// Draws the flame ring: `arg0`'s origin is projected once through
/// `GsWSMATRIX` and eight `POLY_G4` blades are swept around it, each spanning
/// a 0x200 arc of radius `(arg1 * 64) / otz`. Only the third vertex carries
/// colour, the rest of the blade fading to black, and that colour is the
/// `arg2` ramp `(arg2, arg2 >> 1, arg2 >> 2)` - a red-biased fire tint. A
/// negative `gte_stflg` drops the whole ring. Same body as
/// `func_pyrokinesis_80130130`.
static void func_dryfield_dilapidated_house_80182F14(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    GpRingScratch* block;
    POLY_G4*       prim;
    s32            ang;

    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->step = (arg1 * 64) / block->otz;
        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2, arg2 >> 1, arg2 >> 2);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->step * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->step * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->step * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->step * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->step * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->step * rcos(ang + 0x200)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a spinning textured sprite at `arg0`'s `workm` translation, projected
/// once through `GsWSMATRIX`. The `POLY_FT4` is taken from the primitive
/// cursor before the projection flag is checked, so a dropped sprite (negative
/// `gte_stflg`) still consumes its slot. The low bit of `arg1` alternates two
/// semi-transparent looks: odd draws the 0x428B cell tinted
/// `(0xC0, 0x60, 0x40)`, even draws the 0x428C cell untinted. The corners sit
/// `arg2 * 55 / otz` from the projected centre along `arg3` and
/// `arg3 + 0x400`, so the sprite shrinks with depth.
static void func_dryfield_dilapidated_house_801832A8(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    GpFxQuadScratch* block;
    POLY_FT4*        prim;

    block         = SCRATCH_PUSH(GpFxQuadScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        if (arg1 & 1) {
            setRGB0(prim, 0xC0, 0x60, 0x40);
            prim->tpage = 0x29;
            prim->clut  = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
            setSemiTrans(prim, 1);
        } else {
            prim->tpage = 0x29;
            prim->clut  = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            setSemiTrans(prim, 1);
            setShadeTex(prim, 1);
        }
        block->dx = (((arg2 * 55) / block->otz) * rsin(arg3)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(arg3)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        block->dx = (((arg2 * 55) / block->otz) * rsin(arg3 + 0x400)) >> 12;
        block->dy = (((arg2 * 55) / block->otz) * rcos(arg3 + 0x400)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpFxQuadScratch);
}

/// Draws the flame band: two 16-vertex rings of radius `arg1` and
/// `arg1 + arg2` are built in the XZ plane by `rsin` / `rcos`, rotated by
/// `arg0`'s `workm` and offset by its translation, then each of the 16
/// segments is projected through `GsWSMATRIX` as one `POLY_G4`. The inner
/// edge carries the `arg3` ramp `(arg3, arg3 >> 1, arg3 >> 2)` and the outer
/// edge fades to black; a negative `gte_stflg` drops the segment. Same body
/// as `func_pyrokinesis_801312B4`.
static void func_dryfield_dilapidated_house_80183728(GfxCoord* arg0, s16 arg1, s32 arg2, s16 arg3)
{
    GpBandScratch* block;
    SVECTOR*       op;
    POLY_G4*       prim;
    s32            i;
    s32            next;
    s32            ang;
    s16            r0;
    s16            r1;

    r1    = arg1 + arg2;
    block = SCRATCH_PUSH(GpBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < 16; i++) {
        ang                = i << 8;
        block->inner[i].vx = (rsin(ang) * r0) >> 12;
        block->inner[i].vy = 0;
        block->inner[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->inner[i]);
        gte_rtv0();
        gte_stsv(&block->inner[i]);
        block->inner[i].vx += arg0->workm.t[0];
        block->inner[i].vy += arg0->workm.t[1];
        block->inner[i].vz += arg0->workm.t[2];
        block->outer[i].vx  = (rsin(ang) * r1) >> 12;
        op                  = &block->inner[i] + 16;
        op->vy              = 0;
        op->vz              = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&arg0->workm);
        gte_ldv0(&block->outer[i]);
        gte_rtv0();
        gte_stsv(&block->outer[i]);
        block->outer[i].vx += arg0->workm.t[0];
        op->vy             += arg0->workm.t[1];
        op->vz             += arg0->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < 16; i++) {
        gte_ldv0(&block->inner[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & 0xF;
        gte_ldv3(&block->inner[next], &block->outer[i], &block->outer[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            block->otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, arg3, arg3 >> 1, arg3 >> 2);
            setRGB1(prim, arg3, arg3 >> 1, arg3 >> 2);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(GpBandScratch);
}

void func_dryfield_dilapidated_house_80183BF8(Task* arg0)
{
    GfxCoord* coord;
    s32       mask;

    mask  = 1 << gGameSession->at4.loc.view;
    coord = arg0->extra.coordBody->coord;
    if (mask & 0x84A9C) {
        func_dryfield_dilapidated_house_801815E8(coord, 0);
    }
    if (mask & 0x104B98) {
        func_dryfield_dilapidated_house_801815E8(coord, 8);
    }
    if (mask & 0xA55F8) {
        func_dryfield_dilapidated_house_801815E8(coord, 0x10);
    }
}

/// Per-frame handler that runs the `DdhEffWork` effect block one step further:
/// an early out while `Gp_State1C` is armed. It counts frames in `field_22`,
/// seeds the 0xC0 / 0x100 scale/angle pair on the first frame, feeds the pair to
/// `func_dryfield_dilapidated_house_80182A18` and then steps the scale by -0x10
/// and the angle by +0x40. Once the scale falls below 0x10 - and immediately
/// when the state word has already reached 4 - it releases the work block.
void func_dryfield_dilapidated_house_80183C8C(Task* arg0)
{
    DdhEffWork* mem;
    s16         flag;
    s32         scale;
    s32         angle;

    mem  = arg0->spawnArg2.pointer;
    flag = Gp_State1C->effectControl;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0xC0;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }
    func_dryfield_dilapidated_house_80182A18(arg0->extra.coordBody->coord, mem->field_26, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x40;
    scale        -= 0x10;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 0x10) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

/// Per-frame handler of the effect family whose work block is `DdhEffWork`
/// (`task->spawnArg2.pointer`). While the `Gp_State1C` state word at 0x4 is clear it
/// seeds the ramp (0x80 / 0x100) on the first frame and then, every frame,
/// clears the task coordinate's update flag, refreshes the coordinate and feeds
/// the angle/scale pair to `func_dryfield_dilapidated_house_80183728`, stepping
/// the scale by -8 and the angle by +0x80. Once the scale drops below 9 - and
/// immediately when that state word has already reached 4 - it releases the work
/// block through `Gp_ReleaseState1CMem`.
void func_dryfield_dilapidated_house_80183D5C(Task* arg0)
{
    DdhEffWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         scale;
    s32         angle;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1.value, 0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    func_dryfield_dilapidated_house_80183728(coord, mem->field_26, 0x100, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
