#include "rooms/acropolis_sanctuary.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/actor_210700.h"

#include "actors/task_tables.h"

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
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"

extern GpAnimSet* D_acropolis_sanctuary_80180918[9];

extern GpObj4C D_acropolis_sanctuary_80183AE4[17];

/// 0xC work block of the sanctuary's cutscene task, hung off the `Task::work`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `memCalloc(0xC)` block `func_acropolis_sanctuary_8017DA40` allocates and
/// zeroes before publishing the owning task in
/// `D_acropolis_sanctuary_80186C90`. Reach it with `(AcsCutsceneWork*)task->work`.
///
/// `target` is the slot-3 task the block's messages are addressed to, captured
/// once from `gameGetPtrSlot(3)`. `phase` is the script step the driver in
/// `func_acropolis_sanctuary_8017DA40` runs -- it only acts on phase 2, and
/// then only while `step` is still 0, bumping `step` once the scene has been
/// dispatched so it fires exactly once.
typedef struct AcsCutsceneWork {
    /* 0x0 */ Task* target;
    /* 0x4 */ u16   phase;
    /* 0x6 */ u16   step;
    /* 0x8 */ s32   field_8;
} AcsCutsceneWork;
STATIC_ASSERT_SIZEOF(AcsCutsceneWork, 0xC);

/// Scratch payload `func_acropolis_sanctuary_8017DA40` builds on its own stack
/// for the slot-3 messages it sends: `rec` is the 0x14-byte record msgs 0x3E8
/// (weapon) and 0x3F4 take, `place` the position + Euler rotation msg 0x3E9
/// takes. One buffer serves both because the task only ever has one message in
/// flight, and the union is what makes the 0x18-byte frame slot the two share
/// explicit.
typedef union AcsMsgArg {
    /* 0x0 */ AnimationPlayRequest rec;
    /* 0x0 */ GpXformArg           place;
} AcsMsgArg;
STATIC_ASSERT_SIZEOF(AcsMsgArg, 0x18);

/// One corner of an `AcsQuad`, laid out like an `SVECTOR` but read unsigned:
/// every consumer either copies the component into an `SVECTOR` verbatim or
/// subtracts it from a value that is truncated back to 16 bits, so the sign of
/// the load never reaches the result.
typedef struct AcsQuadCorner {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad;
} AcsQuadCorner;
STATIC_ASSERT_SIZEOF(AcsQuadCorner, 8);

/// A size class of the sanctuary's mosaic effect: the four corner offsets a
/// tile of that class is drawn with. `D_acropolis_sanctuary_80182710` holds two
/// of them, a small one and a double-sized one, and `AcsTile::quad` picks
/// between them. `func_acropolis_sanctuary_8017E134` only needs `corner[0]`,
/// the origin the tile's grid position is measured from.
typedef struct AcsQuad {
    /* 0x0 */ AcsQuadCorner corner[4];
} AcsQuad;
STATIC_ASSERT_SIZEOF(AcsQuad, 0x20);

/// One tile of the sanctuary's mosaic, from the 72-entry table at
/// `D_acropolis_sanctuary_80182320`. `row` and `col` are grid coordinates that
/// `func_acropolis_sanctuary_8017E134` scales by 1145/128 and 2147/256 into the
/// spawn offset, and `quad` selects the tile's size class in
/// `D_acropolis_sanctuary_80182710`.
typedef struct AcsTile {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 row;
    /* 0x6 */ s16 col;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 quad;
} AcsTile;
STATIC_ASSERT_SIZEOF(AcsTile, 0xE);

/// The offset `func_acropolis_sanctuary_8017DD78` adds to every corner of the
/// blocker cage once it has been copied. Both variants are positive, and the
/// components are read back unsigned because only the low 16 bits of the sum
/// reach the `s16` corner they are added to.
typedef struct AcsBlockerShift {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
} AcsBlockerShift;

/// Per-frame scratch the sanctuary's mosaic-shard task builds at
/// `G_SCRATCH_HEAD`: `v` holds the three corners of the shard's triangle,
/// first scaled by `GpEffWork::angle` through the GTE's `gpf` interpolator
/// and rotated by the task's own `workm`, then offset by that matrix's
/// translation, and `otz` is the depth (`SZ3 >> 2`) the ordering-table slot is
/// taken from. The block is 0x20 bytes even though only 0x1C are used, because
/// that is the amount the task reserves off the scratch head.
typedef struct AcsMosaicScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR v[3];
    /* 0x1C */ s32     pad;
} AcsMosaicScratch;
STATIC_ASSERT_SIZEOF(AcsMosaicScratch, 0x20);

/// Per-frame scratch the sanctuary's mosaic-tile task builds at
/// `G_SCRATCH_HEAD`: `v` holds the four corners of the tile's quad, each
/// rotated by the task's own `workm` and then offset by that matrix's
/// translation, and `otz` is the depth (`SZ3 >> 2`) the ordering-table slot is
/// taken from. Unlike `AcsMosaicScratch` the corners are not scaled, because a
/// whole tile is always drawn at its size class's own dimensions. The block is
/// 0x28 bytes even though only 0x24 are used, because that is the amount the
/// task reserves off the scratch head.
typedef struct AcsTileScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR v[4];
    /* 0x24 */ s32     pad;
} AcsTileScratch;
STATIC_ASSERT_SIZEOF(AcsTileScratch, 0x28);

/// One grey level per sprite variant, indexed by the variant the flame task
/// picked out of `Task::spawnArg1` (bits 8..9). The overlay holds two of these,
/// the base level `D_acropolis_sanctuary_8017D5D8` and the per-frame flicker
/// amplitude `D_acropolis_sanctuary_8017D5DC`; both are copied onto the stack
/// so the variant index can subscript them.
typedef struct AcsSpriteLevels {
    /* 0x0 */ u8 v[3];
} AcsSpriteLevels;

/// Main-executable globals with no module header yet: `Player_Status.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `Gp_StateC08.field_A` gate the cutscene task's setup (the latter is
/// the cutscene/among-us mode flag) and `Mc_SaveData[0].state.characterId` picks which of the two
/// weapon-id bases that record uses. `gDisplayState.spriteVariant` is set to 1 alongside the
/// save writes when the task hands off to task 0x11, the same way the fountain
/// and helicopter-pad rooms set it.

extern GpMsgEntry           D_acropolis_sanctuary_8018081C[];
extern GpXformArg           D_acropolis_sanctuary_801808BC;
extern AnimationPlayRequest D_acropolis_sanctuary_801809F8;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A0C;
extern AnimationPlayRequest D_acropolis_sanctuary_80180AE8;
extern GpEvsCmd             D_acropolis_sanctuary_80180B0C[];
extern GpEvsCmd             D_acropolis_sanctuary_80181664[];
extern GpEvsCmd             D_acropolis_sanctuary_80181814[];
extern TaskDesc             D_acropolis_sanctuary_80182240;
extern GpGridParams         D_acropolis_sanctuary_801822EC;
extern GpMsgEntry           D_acropolis_sanctuary_80182310[];
extern AcsTile              D_acropolis_sanctuary_80182320[];
extern AcsQuad              D_acropolis_sanctuary_80182710[];
extern s16                  D_acropolis_sanctuary_80182750[];
extern s32                  D_acropolis_sanctuary_80182770;
extern SVECTOR              D_acropolis_sanctuary_80182774[];
extern u16                  D_acropolis_sanctuary_801827D4[];
extern GpGridParams         D_acropolis_sanctuary_80183568;
extern GpAreaApplyRec       D_acropolis_sanctuary_80186418[];
extern Task*                D_acropolis_sanctuary_80186C90;

/// Whole-unit X/Y/Z displacement left by the last call of
/// `func_acropolis_sanctuary_8017F974`.
extern SVECTOR D_acropolis_sanctuary_80186C94;

/// Payloads the sanctuary cutscene task sends: `..._801820E4` is the record
/// slot-3 msg 0x3F4 takes and `..._801820F0` / `..._801821C8` the script pair
/// `func_800E8634` is started on.
extern GpAnimSet* D_acropolis_sanctuary_801820E4[1];
extern GpEvsCmd   D_acropolis_sanctuary_801820F0[];
extern GpEvsCmd   D_acropolis_sanctuary_801821C8[];

static void func_acropolis_sanctuary_8017D5E0(Task* task);
static void func_acropolis_sanctuary_8017D930(Task* arg0);
static void func_acropolis_sanctuary_8017DD78(void);
static void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1);

/// State handlers of the room task: set-up, the per-frame entry fixup and
/// `taskKill`.
static const TaskFuncTable3 D_acropolis_sanctuary_8017D5C4 = {
    { func_acropolis_sanctuary_8017D930, func_acropolis_sanctuary_8017D5E0, taskKill },
};

/// Offset the room task's model-coordinate effect is spawned with.
static const SVECTOR D_acropolis_sanctuary_8017D5D0 = { -0x27F6, -0x17CA, -0x1C3E, 0 };

/// The two level tables stay in assembly: each is padded to a word in the ROM,
/// which a 3-byte C object is not, and the second pad byte is non-zero.
static const AcsSpriteLevels D_acropolis_sanctuary_8017D5D8 = { { 0x60, 0x60, 0x10 } };
static const AcsSpriteLevels D_acropolis_sanctuary_8017D5DC = { { 0x10, 0x10, 0x08 } };
/// A non-zero padding byte the original toolchain left. Nothing refers to it.
static const u8 D_acropolis_sanctuary_8017D5DF = 0xF1;

s32 func_acropolis_sanctuary_8017F918(Task*, s32, GpMessageArg, GpMessageArg);

void func_acropolis_sanctuary_8017DA40(Task*);

void func_acropolis_sanctuary_8017DCE0(s32);

extern GpGridParams   D_acropolis_sanctuary_80183568;
extern GpObj4C        D_acropolis_sanctuary_8018358C[18];
extern GpRoomCoordSet D_acropolis_sanctuary_801843EC[1];

extern AnimationPlayRequest D_acropolis_sanctuary_80180904;
extern AnimationPlayRequest D_acropolis_sanctuary_80180944;
extern AnimationPlayRequest D_acropolis_sanctuary_80180958;
extern AnimationPlayRequest D_acropolis_sanctuary_8018096C;
extern AnimationPlayRequest D_acropolis_sanctuary_80180980;
extern AnimationPlayRequest D_acropolis_sanctuary_80180994;
extern AnimationPlayRequest D_acropolis_sanctuary_801809A8;
extern AnimationPlayRequest D_acropolis_sanctuary_801809BC;
extern AnimationPlayRequest D_acropolis_sanctuary_801809D0;
extern AnimationPlayRequest D_acropolis_sanctuary_801809E4;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A20;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A34;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A48;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A5C;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A70;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A84;
extern AnimationPlayRequest D_acropolis_sanctuary_80180A98;
extern AnimationPlayRequest D_acropolis_sanctuary_80180AAC;
extern AnimationPlayRequest D_acropolis_sanctuary_80180AC0;
extern AnimationPlayRequest D_acropolis_sanctuary_80180AD4;
extern GpCopyArg            D_acropolis_sanctuary_8018093C;
extern GpOverlayIds         D_acropolis_sanctuary_80180AFC;
extern GpXformArg           D_acropolis_sanctuary_80180844;
extern GpXformArg           D_acropolis_sanctuary_8018085C;
extern GpXformArg           D_acropolis_sanctuary_8018088C;
extern GpXformArg           D_acropolis_sanctuary_801808A4;
extern GpXformArg           D_acropolis_sanctuary_801808D4;
extern GpXformArg           D_acropolis_sanctuary_801808EC;
void                        func_acropolis_sanctuary_8017D8A0(u32);
void                        func_acropolis_sanctuary_8017D8CC(void);

s32 func_acropolis_sanctuary_8017D73C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_acropolis_sanctuary_8017D808(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_acropolis_sanctuary_8017D810(Task*, s32, s32, GpMessageArg);
s32 func_acropolis_sanctuary_8017D848(Task*, s32, RoomEventMsg*, RoomEventMsg*);

AnimationPackedPose D_acropolis_sanctuary_80180348[8] = {
#include "assets/acropolis_sanctuary_animation_03234_bank1.inc"
};

AnimationPackedRotation D_acropolis_sanctuary_801803A8[115] = {
#include "assets/acropolis_sanctuary_animation_03234_bank4.inc"
};

AnimationRecord D_acropolis_sanctuary_80180574[150] = {
#include "assets/acropolis_sanctuary_animation_03234_records.inc"
};

u16 D_acropolis_sanctuary_801807CC[20] = {
#include "assets/acropolis_sanctuary_animation_03234_indices.inc"
};

GpAnimSet D_acropolis_sanctuary_801807F4 = {
    D_acropolis_sanctuary_80180574,
    D_acropolis_sanctuary_801807CC,
    { NULL, D_acropolis_sanctuary_80180348, NULL, NULL, D_acropolis_sanctuary_801803A8, NULL, NULL, NULL },
};

GpMsgEntry D_acropolis_sanctuary_8018081C[5] = {
    { 5102, func_acropolis_sanctuary_8017D73C },
    { 5104, func_acropolis_sanctuary_8017D810 },
    { 5105, func_acropolis_sanctuary_8017D808 },
    { 5103, func_acropolis_sanctuary_8017D848 },
    { 0x7FFFFFFF, NULL },
};

GpXformArg D_acropolis_sanctuary_80180844 = { { -9700, 0, -7910, 0 }, { 0, 1024, 0, 0 } };

GpXformArg D_acropolis_sanctuary_8018085C = { 0 };

GpXformArg D_acropolis_sanctuary_80180874 = { { 0, 0, -900, 0 }, { 0, -1024, 0, 0 } };

GpXformArg D_acropolis_sanctuary_8018088C = { { -100, 0, -400, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_sanctuary_801808A4 = { { -100, 0, -400, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_sanctuary_801808BC = { { 0, 0, 480, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_sanctuary_801808D4 = { { 0, 0, -900, 0 }, { 0, 0, 0, 0 } };

GpXformArg D_acropolis_sanctuary_801808EC = { { -5724, 0, -8276, 0 }, { 0, -1024, 0, 0 } };

AnimationPlayRequest D_acropolis_sanctuary_80180904 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpAnimSet* D_acropolis_sanctuary_80180918[9] = {
    &D_actor_210700_8014BC4C,
    &D_actor_210700_8014C618,
    &D_actor_210700_8014C974,
    &D_actor_210700_8014CC2C,
    &D_actor_210700_8014CFAC,
    &D_actor_210700_8014D244,
    &D_actor_210700_8014D9F0,
    &D_actor_210700_80150608,
    &D_acropolis_sanctuary_801807F4,
};

GpCopyArg D_acropolis_sanctuary_8018093C = { { .sets = D_acropolis_sanctuary_80180918 }, 9 };

AnimationPlayRequest D_acropolis_sanctuary_80180944 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180958 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_8018096C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180980 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180994 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809A8 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809BC = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809D0 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809E4 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_801809F8 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A0C = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A20 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A34 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A48 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A5C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A70 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A84 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180A98 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AAC = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AC0 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AD4 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_sanctuary_80180AE8 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GpOverlayIds D_acropolis_sanctuary_80180AFC = { 1, 7, 11 };

GpOverlayIds D_acropolis_sanctuary_80180B04 = { 1, 7, 21 };

GpEvsCmd D_acropolis_sanctuary_80180B0C[121] = {
    { 32, { .value = 61 }, { .value = 64 }, { .value = 80 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_sanctuary_8018085C }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_acropolis_sanctuary_8018093C }, { .value = 0 } },
    { 12, { .overlays = &D_acropolis_sanctuary_80180AFC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 31, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_acropolis_sanctuary_8017D8A0 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_acropolis_sanctuary_8017D8A0 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A70 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A20 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_80180844 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 30, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_acropolis_sanctuary_8017D8CC }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A84 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A34 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A48 }, { .value = 0 } },
    { 17, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A5C }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_8018085C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180944 }, { .value = 0 } },
    { 13, { .callbackU32 = func_acropolis_sanctuary_8017D8A0 }, { .value = 272 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackU32 = func_acropolis_sanctuary_8017D8A0 }, { .value = 268 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 5 }, { .value = 0 }, { .value = 3101 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 17, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_8018088C }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180958 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_sanctuary_801808BC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AAC }, { .value = 0 } },
    { 4, { .value = 73 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A98 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_801808A4 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_8018096C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180980 }, { .value = 0 } },
    { 4, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AC0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AD4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A98 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_8018096C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180980 }, { .value = 0 } },
    { 4, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AC0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AD4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A98 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_8018096C }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180980 }, { .value = 0 } },
    { 4, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809A8 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AC0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AD4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A98 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180994 }, { .value = 0 } },
    { 4, { .value = 71 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809BC }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809D0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_801808D4 }, { .value = 0 } },
    { 4, { .value = 56 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_801809E4 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AD4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A98 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { 4, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_801808EC }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_sanctuary_80181664[18] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_acropolis_sanctuary_801808EC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2004 }, { .storage = &D_acropolis_sanctuary_801808BC }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AE8 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 33, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { 18, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_sanctuary_80181814[11] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_sanctuary_80180904 }, { .value = 0 } },
    { 12, { .overlays = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AC0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180AD4 }, { .value = 0 } },
    { 4, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 4 }, { .value = 1 }, { .value = 2003 }, { .storage = &D_acropolis_sanctuary_80180A98 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

AnimationPackedPose D_acropolis_sanctuary_8018191C[7] = {
#include "assets/acropolis_sanctuary_animation_04708_bank1.inc"
};

AnimationPackedRotation D_acropolis_sanctuary_80181970[84] = {
#include "assets/acropolis_sanctuary_animation_04708_bank4.inc"
};

AnimationRecord D_acropolis_sanctuary_80181AC0[120] = {
#include "assets/acropolis_sanctuary_animation_04708_records.inc"
};

u16 D_acropolis_sanctuary_80181CA0[20] = {
#include "assets/acropolis_sanctuary_animation_04708_indices.inc"
};

GpAnimSet D_acropolis_sanctuary_80181CC8 = {
    D_acropolis_sanctuary_80181AC0,
    D_acropolis_sanctuary_80181CA0,
    { NULL, D_acropolis_sanctuary_8018191C, NULL, NULL, D_acropolis_sanctuary_80181970, NULL, NULL, NULL },
};

AnimationPackedPose D_acropolis_sanctuary_80181CF0[8] = {
#include "assets/acropolis_sanctuary_animation_04AFC_bank1.inc"
};

AnimationPackedRotation D_acropolis_sanctuary_80181D50[88] = {
#include "assets/acropolis_sanctuary_animation_04AFC_bank4.inc"
};

AnimationRecord D_acropolis_sanctuary_80181EB0[121] = {
#include "assets/acropolis_sanctuary_animation_04AFC_records.inc"
};

u16 D_acropolis_sanctuary_80182094[20] = {
#include "assets/acropolis_sanctuary_animation_04AFC_indices.inc"
};

GpAnimSet D_acropolis_sanctuary_801820BC = {
    D_acropolis_sanctuary_80181EB0,
    D_acropolis_sanctuary_80182094,
    { NULL, D_acropolis_sanctuary_80181CF0, NULL, NULL, D_acropolis_sanctuary_80181D50, NULL, NULL, NULL },
};

GpAnimSet* D_acropolis_sanctuary_801820E4[1] = {
    &D_acropolis_sanctuary_80181CC8,
};

GpOverlayIds D_acropolis_sanctuary_801820E8 = { 1, 14, 11 };

GpEvsCmd D_acropolis_sanctuary_801820F0[9] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { 12, { .overlays = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_sanctuary_8017DCE0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_acropolis_sanctuary_8017DCE0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_acropolis_sanctuary_801821C8[5] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_acropolis_sanctuary_80182240 = { 0, 192, func_acropolis_sanctuary_8017DA40, { .model = NULL } };

SVECTOR D_acropolis_sanctuary_8018224C[4] = {
#include "assets/acropolis_sanctuary_collision_04D2C_normals.inc"
};

SVECTOR D_acropolis_sanctuary_8018226C[8] = {
#include "assets/acropolis_sanctuary_collision_04D2C_verts.inc"
};

GpGridFace D_acropolis_sanctuary_801822AC[4] = {
#include "assets/acropolis_sanctuary_collision_04D2C_faces.inc"
};

s16 D_acropolis_sanctuary_801822DC[6] = {
#include "assets/acropolis_sanctuary_collision_04D2C_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_sanctuary_801822DC[i])
s16* D_acropolis_sanctuary_801822E8[1] = {
#include "assets/acropolis_sanctuary_collision_04D2C_table.inc"
};
#undef GRID_CELL

GpGridParams D_acropolis_sanctuary_801822EC = { NULL, D_acropolis_sanctuary_8018224C, D_acropolis_sanctuary_8018226C, D_acropolis_sanctuary_801822AC, D_acropolis_sanctuary_801822E8, 4941, 8914, 1, 1, 4000, 4 };

GpMsgEntry D_acropolis_sanctuary_80182310[2] = {
    { 3101, func_acropolis_sanctuary_8017F918 },
    { 0x7FFFFFFF, NULL },
};

AcsTile D_acropolis_sanctuary_80182320[72] = {
    { 29, 31, 0, 0, 0, 24, 1 },
    { 29, 31, 30, 0, 0, 16, 1 },
    { 14, 15, 60, 0, 0, 16, 0 },
    { 14, 15, 75, 0, 0, 16, 0 },
    { 14, 15, 90, 0, 0, 16, 0 },
    { 14, 15, 105, 0, 0, 16, 0 },
    { 29, 31, 120, 0, 0, 16, 1 },
    { 29, 31, 150, 0, 0, 24, 1 },
    { 29, 31, 60, 16, 0, 8, 1 },
    { 29, 31, 90, 16, 0, 8, 1 },
    { 29, 31, 0, 32, 0, 16, 1 },
    { 29, 31, 30, 32, 0, 8, 1 },
    { 29, 31, 120, 32, 0, 8, 1 },
    { 29, 31, 150, 32, 0, 16, 1 },
    { 29, 31, 60, 48, 1, 0, 1 },
    { 29, 31, 90, 48, 1, 0, 1 },
    { 29, 31, 0, 64, 0, 8, 1 },
    { 29, 31, 30, 64, 1, 0, 1 },
    { 29, 31, 120, 64, 1, 0, 1 },
    { 29, 31, 150, 64, 0, 8, 1 },
    { 14, 15, 60, 80, 1, 0, 0 },
    { 29, 31, 75, 80, 1, 0, 1 },
    { 14, 15, 105, 80, 1, 0, 0 },
    { 14, 15, 0, 96, 0, 8, 0 },
    { 29, 31, 15, 96, 1, 0, 1 },
    { 14, 15, 45, 96, 1, 0, 0 },
    { 14, 15, 60, 96, 1, 0, 0 },
    { 14, 15, 105, 96, 1, 0, 0 },
    { 14, 15, 120, 96, 1, 0, 0 },
    { 29, 31, 135, 96, 1, 0, 1 },
    { 14, 15, 165, 96, 0, 8, 0 },
    { 14, 15, 0, 112, 0, 16, 0 },
    { 29, 31, 45, 112, 1, 0, 1 },
    { 14, 15, 75, 112, 1, 0, 0 },
    { 14, 15, 90, 112, 1, 0, 0 },
    { 29, 31, 105, 112, 1, 0, 1 },
    { 14, 15, 165, 112, 0, 16, 0 },
    { 14, 15, 0, 128, 0, 24, 0 },
    { 29, 31, 15, 128, 1, 0, 1 },
    { 14, 15, 75, 128, 1, 0, 0 },
    { 14, 15, 90, 128, 1, 0, 0 },
    { 29, 31, 135, 128, 1, 0, 1 },
    { 14, 15, 165, 128, 0, 24, 0 },
    { 14, 15, 0, 144, 0, 16, 0 },
    { 14, 15, 45, 144, 1, 0, 0 },
    { 14, 15, 60, 144, 1, 0, 0 },
    { 29, 31, 75, 144, 1, 0, 1 },
    { 14, 15, 105, 144, 1, 0, 0 },
    { 14, 15, 120, 144, 1, 0, 0 },
    { 14, 15, 165, 144, 0, 16, 0 },
    { 29, 31, 0, 160, 0, 8, 1 },
    { 29, 31, 30, 160, 1, 0, 1 },
    { 14, 15, 60, 160, 1, 0, 0 },
    { 14, 15, 105, 160, 1, 0, 0 },
    { 29, 31, 120, 160, 1, 0, 1 },
    { 29, 31, 150, 160, 0, 8, 1 },
    { 29, 31, 60, 178, 1, 0, 1 },
    { 29, 31, 90, 178, 1, 0, 1 },
    { 29, 31, 0, 192, 0, 16, 1 },
    { 29, 31, 30, 192, 1, 0, 1 },
    { 29, 31, 120, 192, 1, 0, 1 },
    { 29, 31, 150, 192, 0, 16, 1 },
    { 29, 31, 60, 208, 1, 0, 1 },
    { 29, 31, 90, 208, 1, 0, 1 },
    { 29, 31, 0, 224, 0, 24, 1 },
    { 29, 31, 30, 224, 0, 8, 1 },
    { 29, 31, 120, 224, 0, 8, 1 },
    { 29, 31, 150, 224, 0, 24, 1 },
    { 14, 15, 60, 240, 0, 8, 0 },
    { 14, 15, 75, 240, 0, 16, 0 },
    { 14, 15, 90, 240, 0, 16, 0 },
    { 14, 15, 105, 240, 0, 8, 0 },
};

AcsQuad D_acropolis_sanctuary_80182710[2] = {
    { { { 0, 0xFFB8, 63, 0 }, { 0, 0xFFB8, 0xFFC2, 0 }, { 0, 71, 63, 0 }, { 0, 71, 0xFFC2, 0 } } },
    { { { 0, 0xFF71, 126, 0 }, { 0, 0xFF71, 0xFF83, 0 }, { 0, 143, 126, 0 }, { 0, 143, 0xFF83, 0 } } },
};

s16 D_acropolis_sanctuary_80182750[16] = {
    20,
    22,
    25,
    26,
    27,
    28,
    33,
    34,
    39,
    40,
    44,
    45,
    47,
    48,
    52,
    53,
};

s32 D_acropolis_sanctuary_80182770 = 0;

SVECTOR D_acropolis_sanctuary_80182774[12] = {
    { -0x2738, -2088, -6680, 0 },
    { -0x2738, -2088, -9280, 0 },
    { -2720, -2088, -6460, 0 },
    { -2720, -2088, -9570, 0 },
    { -6690, -2088, -4220, 0 },
    { -8310, -2028, -4220, 0 },
    { -2420, -1630, -7210, 0 },
    { -2420, -1680, -7330, 0 },
    { -2420, -1630, -7450, 0 },
    { -2420, -1630, -8490, 0 },
    { -2420, -1680, -8600, 0 },
    { -2420, -1630, -8730, 0 },
};

u16 D_acropolis_sanctuary_801827D4[12] = {
    2626,
    2130,
    5420,
    300,
    0x3460,
    0x3440,
    4524,
    4524,
    4524,
    428,
    428,
    428,
};

GpRoomObjRec D_acropolis_sanctuary_801827EC[1] = {
    { &D_acropolis_sanctuary_80183568, D_acropolis_sanctuary_8018358C, D_acropolis_sanctuary_80183AE4, NULL },
};

u8* D_acropolis_sanctuary_801827FC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_acropolis_sanctuary_80182800[1] = {
    { { .bytes = { 16, 0 } } },
};

GpRoomCoordRec D_acropolis_sanctuary_80182804[1] = {
    { D_acropolis_sanctuary_801843EC, NULL },
};

GpWarpRec D_acropolis_sanctuary_8018280C[3] = {
    { { .words = { 1024, -9757, 2, -8070 } }, { 0, 0, 0, 0 }, { .words = { 1024, -9757, 2, -8070 } }, { 0, 0, 0, 0 }, 0x510C0002, 0x510C0001, 0, 2, 0, 493 },
    { { .words = { 2048, -7465, 2, -4239 } }, { 0, 0, 0, 0 }, { .words = { 2048, -7465, 2, -4239 } }, { 0, 0, 0, 0 }, 0x510C0004, 0x510C0003, 0, 7, 0, 492 },
    { { .words = { 1024, -9757, 2, -8070 } }, { 0, 0, 0, 0 }, { .words = { 1024, -9757, 2, -8070 } }, { 0, 0, 0, 0 }, 0x510C0002, 0x510C0001, 0, 2, 0, 493 },
};

SVECTOR D_acropolis_sanctuary_801828B4[42] = {
#include "assets/acropolis_sanctuary_collision_05FA8_normals.inc"
};

SVECTOR D_acropolis_sanctuary_80182A04[136] = {
#include "assets/acropolis_sanctuary_collision_05FA8_verts.inc"
};

GpGridFace D_acropolis_sanctuary_80182E44[85] = {
#include "assets/acropolis_sanctuary_collision_05FA8_faces.inc"
};

s16 D_acropolis_sanctuary_80183240[386] = {
#include "assets/acropolis_sanctuary_collision_05FA8_cells.inc"
};

#define GRID_CELL(i) (&D_acropolis_sanctuary_80183240[i])
s16* D_acropolis_sanctuary_80183544[9] = {
#include "assets/acropolis_sanctuary_collision_05FA8_table.inc"
};
#undef GRID_CELL

GpGridParams D_acropolis_sanctuary_80183568 = { NULL, D_acropolis_sanctuary_801828B4, D_acropolis_sanctuary_80182A04, D_acropolis_sanctuary_80182E44, D_acropolis_sanctuary_80183544, 0x2A44, 0x332D, 3, 3, 4000, 85 };

GpObj4C D_acropolis_sanctuary_8018358C[18] = {
    { NULL, NULL, NULL, { -7168, -1376, -8000, 0 }, { { 0, -2048, -1504, 0 }, { 0, 2048, -1504, 0 }, { 0, -2048, 1504, 0 }, { 0, 2048, 1504, 0 } }, { -4097, 0, 0, 0 }, { 0, 0, 0, 0 }, 2534, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -9729, -1344, -5953, 0 }, { { 1242, -2048, -514, 0 }, { 1242, 2048, -514, 0 }, { -1241, -2048, 515, 0 }, { -1241, 2048, 515, 0 } }, { -1570, 0, -3788, 0 }, { 0, 0, 0, 0 }, 2442, 0, 2, 7, 1, 0 },
    { NULL, NULL, NULL, { -9665, -1376, -0x2721, 0 }, { { -1241, -2048, -514, 0 }, { -1241, 2048, -514, 0 }, { 1242, -2048, 514, 0 }, { 1242, 2048, 514, 0 } }, { -1569, 0, 3787, 0 }, { 0, 0, 0, 0 }, 2442, 0, 2, 5, 1, 0 },
    { NULL, NULL, NULL, { -7456, -1088, -7981, 0 }, { { 0, -2048, 1376, 0 }, { 0, 2048, 1376, 0 }, { 0, -2048, -1376, 0 }, { 0, 2048, -1376, 0 } }, { 4105, 0, 0, 0 }, { 0, 0, 0, 0 }, 2455, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -9953, -1344, -9504, 0 }, { { 1242, -2048, 514, 0 }, { 1242, 2048, 514, 0 }, { -1241, -2048, -514, 0 }, { -1241, 2048, -514, 0 } }, { 1568, 0, -3788, 0 }, { 0, 0, 0, 0 }, 2442, 0, 5, 2, 1, 0 },
    { NULL, NULL, NULL, { -9985, -1216, -6529, 0 }, { { -1265, -2048, 453, 0 }, { -1265, 2048, 453, 0 }, { 1265, -2048, -453, 0 }, { 1265, 2048, -453, 0 } }, { 1382, 0, 3859, 0 }, { 0, 0, 0, 0 }, 2442, 0, 7, 2, 1, 0 },
    { NULL, NULL, NULL, { -7203, -1248, -0x2D63, 0 }, { { -5, -2048, 1057, 0 }, { -5, 2048, 1057, 0 }, { 5, -2048, -1056, 0 }, { 5, 2048, -1056, 0 } }, { 4099, 0, 19, 0 }, { 0, 0, 0, 0 }, 2304, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -6850, -1312, -0x2D81, 0 }, { { 5, -2048, -1184, 0 }, { 5, 2048, -1184, 0 }, { -5, -2048, 1185, 0 }, { -5, 2048, 1185, 0 } }, { -4112, 0, -18, 0 }, { 0, 0, 0, 0 }, 2360, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -6704, -1280, -4259, 0 }, { { -5, -2048, 1217, 0 }, { -5, 2048, 1217, 0 }, { 5, -2048, -1216, 0 }, { 5, 2048, -1216, 0 } }, { 4103, 0, 16, 0 }, { 0, 0, 0, 0 }, 2374, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { -6399, -1344, -4258, 0 }, { { 5, -2048, -1312, 0 }, { 5, 2048, -1312, 0 }, { -5, -2048, 1313, 0 }, { -5, 2048, 1313, 0 } }, { -4098, 0, -16, 0 }, { 0, 0, 0, 0 }, 2428, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { -4337, -1344, -5456, 0 }, { { -1472, -2048, -864, 0 }, { -1472, 2048, -864, 0 }, { 1473, -2048, 864, 0 }, { 1473, 2048, 864, 0 } }, { -2078, 0, 3540, 0 }, { 0, 0, 0, 0 }, 2660, 0, 6, 3, 1, 0 },
    { NULL, NULL, NULL, { -4401, -1312, -4928, 0 }, { { 1457, -2048, 896, 0 }, { 1457, 2048, 896, 0 }, { -1456, -2048, -896, 0 }, { -1456, 2048, -896, 0 } }, { 2149, 0, -3494, 0 }, { 0, 0, 0, 0 }, 2660, 0, 3, 6, 1, 0 },
    { NULL, NULL, NULL, { -3907, -1312, -0x2763, 0 }, { { 1810, -2048, -622, 0 }, { 1810, 2048, -622, 0 }, { -1809, -2048, 623, 0 }, { -1809, 2048, 623, 0 } }, { -1334, 0, -3877, 0 }, { 0, 0, 0, 0 }, 2792, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -3970, -1312, -0x2884, 0 }, { { -2065, -2048, 751, 0 }, { -2065, 2048, 751, 0 }, { 2066, -2048, -750, 0 }, { 2066, 2048, -750, 0 } }, { 1405, 0, 3867, 0 }, { 0, 0, 0, 0 }, 2996, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -2955, -1312, -8921, 0 }, { { 1025, -2048, -671, 0 }, { 1025, 2048, -671, 0 }, { -1026, -2048, 670, 0 }, { -1026, 2048, 670, 0 } }, { -2250, 0, -3441, 0 }, { 0, 0, 0, 0 }, 2374, 0, 3, 8, 1, 0 },
    { NULL, NULL, NULL, { -3298, -1344, -6786, 0 }, { { 958, -2048, 764, 0 }, { 958, 2048, 764, 0 }, { -958, -2048, -764, 0 }, { -958, 2048, -764, 0 } }, { 2562, 0, -3214, 0 }, { 0, 0, 0, 0 }, 2374, 0, 8, 3, 1, 0 },
    { NULL, NULL, NULL, { -2979, -1280, -6915, 0 }, { { -953, -2048, -769, 0 }, { -953, 2048, -769, 0 }, { 954, -2048, 770, 0 }, { 954, 2048, 770, 0 } }, { -2582, 0, 3198, 0 }, { 0, 0, 0, 0 }, 2374, 0, 3, 8, 1, 0 },
    { NULL, NULL, NULL, { -3201, -1344, -9089, 0 }, { { -1013, -2048, 690, 0 }, { -1013, 2048, 690, 0 }, { 1013, -2048, -690, 0 }, { 1013, 2048, -690, 0 } }, { 2314, 0, 3398, 0 }, { 0, 0, 0, 0 }, 2374, 0, 8, 3, 129, 0 },
};

GpObj4C D_acropolis_sanctuary_80183AE4[17] = {
    { NULL, NULL, NULL, { -9920, -42, -7968, 0 }, { { -287, 0, -1024, 0 }, { 288, 0, -1024, 0 }, { -287, 0, 1024, 0 }, { 288, 0, 1024, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 1063, 0, 11, 18, 2, 0 },
    { NULL, NULL, NULL, { -7728, -24, -4448, 0 }, { { -735, 0, -256, 0 }, { 736, 0, -256, 0 }, { -735, 0, 256, 0 }, { 736, 0, 256, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 778, 0, 13, 33, 2, 0 },
    { NULL, NULL, NULL, { -3648, -64, -8016, 0 }, { { -319, 0, -432, 0 }, { 320, 0, -432, 0 }, { -319, 0, 432, 0 }, { 320, 0, 432, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 535, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { -5312, -64, -8288, 0 }, { { -543, 0, -896, 0 }, { 544, 0, -896, 0 }, { -543, 0, 896, 0 }, { 544, 0, 896, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1047, 2, 0, 0, 2, 0 },
    { NULL, NULL, NULL, { -2736, -61, -7896, 0 }, { { -655, 0, -680, 0 }, { 656, 0, -1448, 0 }, { -655, 0, 856, 0 }, { 656, 0, 1272, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1588, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -7712, -32, -4416, 0 }, { { -895, 0, -672, 0 }, { 896, 0, -672, 0 }, { -895, 0, 672, 0 }, { 896, 0, 672, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1115, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { -6608, -64, -8086, 0 }, { { -3, 0, 125, 0 }, { -105, 0, -3, 0 }, { 113, 0, -14, 0 }, { -2, 0, -106, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4095, 0 }, 124, 6, 7, 0, 3, 0 },
    { NULL, NULL, NULL, { -4768, -64, -6992, 0 }, { { -751, 0, -720, 0 }, { 752, 0, -720, 0 }, { -751, 0, 720, 0 }, { 752, 0, 720, 0 } }, { 0, 4097, 0, 0 }, { -995, 0, 3973, 0 }, 1039, 2, 0, 0, 2, 0 },
    { NULL, NULL, NULL, { -4768, -64, -8864, 0 }, { { -751, 0, -624, 0 }, { 752, 0, -624, 0 }, { -751, 0, 624, 0 }, { 752, 0, 624, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 976, 2, 0, 0, 2, 0 },
    { NULL, NULL, NULL, { -6446, -64, -7903, 0 }, { { -115, 0, 362, 0 }, { -377, 0, 99, 0 }, { 385, 0, -104, 0 }, { 110, 0, -356, 0 } }, { 0, 4100, 0, 0 }, { 2895, 0, 2895, 0 }, 398, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6800, -64, -8240, 0 }, { { -99, 0, 378, 0 }, { -393, 0, 83, 0 }, { 401, 0, -88, 0 }, { 94, 0, -372, 0 } }, { 0, 4106, 0, 0 }, { -2896, 0, -2896, 0 }, 409, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6802, -64, -7904, 0 }, { { 58, 0, 362, 0 }, { -364, 0, -61, 0 }, { 371, 0, 56, 0 }, { -64, 0, -356, 0 } }, { 0, 4095, 0, 0 }, { -2897, 0, 2895, 0 }, 374, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6448, -64, -8256, 0 }, { { 74, 0, 346, 0 }, { -348, 0, -77, 0 }, { 355, 0, 72, 0 }, { -80, 0, -340, 0 } }, { 0, 4095, 0, 0 }, { 2896, 0, -2896, 0 }, 362, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6633, -64, -7809, 0 }, { { 1, 0, 277, 0 }, { -186, 0, 5, 0 }, { 178, 0, -9, 0 }, { 10, 0, -270, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4093, 0 }, 275, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6617, -64, -8385, 0 }, { { -15, 0, 277, 0 }, { -170, 0, 5, 0 }, { 194, 0, -9, 0 }, { -6, 0, -270, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4093, 0 }, 277, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6305, -64, -8097, 0 }, { { -276, 0, -8, 0 }, { -4, 0, -163, 0 }, { 10, 0, 170, 0 }, { 270, 0, 1, 0 } }, { 0, 4094, 0, 0 }, { 4093, 0, 0, 0 }, 275, 6, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { -6912, -64, -8073, 0 }, { { -276, 0, 0, 0 }, { -4, 0, -155, 0 }, { 10, 0, 146, 0 }, { 270, 0, 9, 0 } }, { 0, 4095, 0, 0 }, { -4093, 0, 0, 0 }, 275, 6, 7, 0, 130, 0 },
};

GpAreaTmdRec D_acropolis_sanctuary_80183FF0[3] = {
    { 27, 107, 0, 0, { 0, 0 }, D_8013BF94 },
    { 102, 107, 1, 0, { 0, 0 }, D_801585CC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_acropolis_sanctuary_80184014[2] = {
    { 102, 107, 1, 0, { 0, 0 }, D_801585CC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_acropolis_sanctuary_8018402C[12] = {
    { NULL, NULL },
    { D_map_akropolis_8017BA5C, D_acropolis_sanctuary_80183FF0 },
    { D_map_akropolis_8017BA8C, D_acropolis_sanctuary_80184014 },
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

GpPointLight D_acropolis_sanctuary_8018408C[9] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7720, -3500, -5973 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 500, 4575 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4989, -3500, -6313 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 500, 4488 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5050, -3500, -0x2732 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 500, 4671 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7720, -3500, -0x2732 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2867, 2867, 2867, { 0, 0 } }, 500, 4284 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2788, -2000, -9282 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3276, { 0, 0 } }, 100, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x277E, -2000, -6674 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3276, { 0, 0 } }, 100, 4499 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2870, -2000, -6481 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3276, { 0, 0 } }, 100, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2870, -2000, -9568 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 3276, { 0, 0 } }, 100, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6180, -3500, -8019 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2048, 2048, 2048, { 0, 0 } }, 0x2710, 0x2710 },
};

GpRoomCoordSet D_acropolis_sanctuary_801843EC[1] = {
    { 0, NULL, 9, D_acropolis_sanctuary_8018408C, 0, NULL },
};

GpSprtCmd D_acropolis_sanctuary_80184404[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80184414[40] = {
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -48, 16, 883, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, 16, 701, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -64, 32, 503, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, 48, 480, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, 64, 411, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -88, 72, 343, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 88, 328, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, 104, 312, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, 16, 853, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 64, 16, 863, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 72, 16, 843, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, 24, 736, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 88, 32, 691, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 96, 32, 638, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 32, 626, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 104, 88, 516, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 112, 48, 504, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 120, 48, 522, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 128, 56, 512, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 136, 64, 480, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 64, 450, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 64, 117, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 56 } }, -160, 0, 857, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 852, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 853, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 874, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 48 } }, 80, 0, 862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 56, -16, 859, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, 40, 852, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 48, 867, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 48, 510, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 512, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 96 } }, -160, 24, 523, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, 0, 510, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -88, 24, 523, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 96, 383, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 104 } }, 112, 16, 504, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 104, 0, 504, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 104, 16, 513, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 192, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80184734[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 3, 0 } },
    { 22, 8, 0, 0, { 0, 0 } },
    { 30, 9, 0, 0, { 2, 0 } },
    { 39, 1, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80184764[50] = {
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -8, -24, 1316, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 0, 1351, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 48, 974, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 317, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 8, 970, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 16, 877, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, 16, 789, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -72, 24, 726, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -80, 32, 693, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, 40, 610, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -96, 40, 602, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, 48, 450, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, 48, 510, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, 56, 427, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, 56, 427, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -136, 56, 445, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -144, 64, 418, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 72, 385, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, 80, 387, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, 16, 915, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 64, 16, 734, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 72, 32, 636, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 80, 48, 512, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 88, 64, 447, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 64, 388, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 72, 352, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 88, 336, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -152, -8, 1073, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -152, 32, 681, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -80, 0, 1043, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, -16, 1000, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 0, 1100, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, -16, 931, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 924, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 24, 729, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 40 } }, 72, 0, 937, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -8, 626, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, -160, 16, 628, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 0, 631, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -96, 16, 632, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -72, 32, 790, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 72, 0, 562, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 80, 80, 478, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, 48, 548, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 56 } }, 80, 24, 585, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, 88, 387, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 64, 207, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 88, 210, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 128, 112, 337, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -8, 668, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80184B4C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 25, 0, 0, { 0, 0 } },
    { 27, 9, 0, 0, { 5, 0 } },
    { 36, 9, 0, 0, { 1, 0 } },
    { 45, 4, 0, 0, { 4, 0 } },
    { 49, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80184B8C[46] = {
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -128, -32, 1356, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -128, -24, 979, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 571, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -104, -16, 960, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 24, 575, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -16, 921, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 0, -8, 918, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -160, 0, 571, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 56 } }, -104, 8, 565, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -40, 8, 601, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -32, 72, 537, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -160, 96, 187, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, 112, 278, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 112, 208, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, -16, 880, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 32, 906, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 0, -16, 901, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 0, 818, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 8, 697, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -24, 0, 527, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, 80, 545, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 88, 554, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 8, 510, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 32, 460, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 40, 420, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 48, 391, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 64, 361, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 64, 341, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 72, 325, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 80, 305, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 88, 288, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 88, 277, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 96, 263, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 96, 253, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 243, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 104, 235, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, 104, 228, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -152, 104, 228, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 8, 612, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -152, 8, 606, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, 8, 598, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, 0, 588, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, 0, 583, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 32, 670, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 56 } }, 56, -120, 354, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -80, 600, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80184F24[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 6, 0 } },
    { 1, 6, 0, 0, { 1, 0 } },
    { 7, 4, 0, 0, { 4, 0 } },
    { 11, 2, 0, 0, { 0, 0 } },
    { 13, 25, 0, 0, { 5, 0 } },
    { 38, 6, 0, 0, { 3, 0 } },
    { 44, 1, 0, 0, { 7, 0 } },
    { 45, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80184F74[41] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 24, 600, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 120, 0, 754, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 0, 607, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 0, 611, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 0, 614, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 0, 619, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 40, 961, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -8, -16, 945, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 32, 916, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 0, 879, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 0, 711, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 16, 0, 579, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 56, 651, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 80, 563, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 16, 572, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 32, 545, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 40, 534, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, 48, 473, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 442, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 64, 413, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 72, 393, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, 72, 374, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, 72, 360, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 80, 347, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 80, 341, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, 64, 322, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 329, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 64, 318, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 72, 319, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 88, 314, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -8, -16, 882, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -24, 886, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 40, 887, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 56 } }, 8, -16, 882, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 88 } }, 48, 0, 592, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, 16, 8, 582, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 24, 96, 478, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 112, 80, 316, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -88, -96, 583, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, -120, 202, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -120, 230, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_801852A8[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 24, 0, 0, { 4, 0 } },
    { 30, 4, 0, 0, { 1, 0 } },
    { 34, 3, 0, 0, { 5, 0 } },
    { 37, 1, 0, 0, { 0, 0 } },
    { 38, 1, 0, 0, { 6, 0 } },
    { 39, 2, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_801852F0[32] = {
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 88, -8, 1250, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 24, 659, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -8, 0, 779, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 64, 776, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 64, 772, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 16, 727, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 16, 619, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 24, 0, 565, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 32, 0, 510, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 473, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 40, 422, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 48, 403, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 48, 389, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 56, 350, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 56, 337, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 64, 325, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 112, 64, 315, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 64, 307, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 72, 275, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 72, 265, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 72, 259, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, 72, 252, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 16, 520, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 24, 492, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -8, 8, 791, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, 8, 813, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 40 } }, 32, 8, 816, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 96, 48, 811, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -8, 805, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, 32, 575, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 32, 8, 5021, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 80 } }, 48, 8, 455, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80185570[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 3, 0 } },
    { 1, 23, 0, 0, { 0, 0 } },
    { 24, 5, 0, 0, { 2, 0 } },
    { 29, 3, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_801855A0[24] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 56, 893, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 80, 735, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 80, 804, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, 8, 488, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 16, 451, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -64, 8, 514, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 16, 618, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 32, 670, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 701, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 32, 726, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 16, 755, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -16, 8, 808, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 8, 876, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 0, 8, 875, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 0, 835, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 48, 894, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -120, 24, 817, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 48, 899, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 40 } }, -112, 8, 887, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 64 } }, -160, 8, 760, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 72, 750, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 80, 750, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -8, 636, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 104 } }, -160, 16, 634, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80185780[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 5, 0, 0, { 3, 0 } },
    { 19, 4, 0, 0, { 2, 0 } },
    { 23, 1, 0, 0, { 4, 0 } },
    { 24, 0, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_801857B8[9] = {
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -80, 24, 614, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -48, 32, 607, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -16, 40, 600, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -64, 766, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -96, 785, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -32, -88, 663, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 0, -80, 651, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -72, 666, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -64, 748, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_8018586C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_8018588C[4] = {
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -48, -48, 585, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 0, -48, 591, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -40, -16, 587, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 0, -16, 591, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_801858DC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_sanctuary_801858F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80185904[8] = {
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -152, -24, 230, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -128, 16, 239, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 64 } }, -152, 32, 227, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -112, 96, 248, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 96 } }, -56, 24, 302, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 24, 16, 423, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 136 } }, 88, -40, 432, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 136, -8, 394, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_801859A4[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_801859BC[32] = {
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, -112, 1294, { .fields = { 88, 88 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -48, -96, 1195, { .fields = { 64, 136 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, -48, -80, 1110, { .fields = { 56, 72 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, -40, -64, 1036, { .fields = { 64, 104 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -16, -48, 1026, { .fields = { 88, 120 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, -32, 1021, { .fields = { 104, 184 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -96, 1373, { .fields = { 120, 248 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 978, { .fields = { 112, 248 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -40, -112, 1329, { .fields = { 88, 168 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -96, 992, { .fields = { 112, 200 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -80, 671, { .fields = { 112, 48 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -64, 913, { .fields = { 112, 152 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -80, 1379, { .fields = { 112, 0 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -56, 1231, { .fields = { 120, 216 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -40, 1095, { .fields = { 112, 24 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -24, -112, 1380, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, -96, 1415, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 8, -80, 1388, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -48, -120, 1200, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -48, -104, 1059, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -56, -88, 907, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, -72, 671, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -56, -64, 664, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -48, -56, 807, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 8 } }, -40, -48, 832, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -24, -40, 845, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -16, -32, 852, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 16, -64, 1293, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 8, -48, 1164, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, -32, 1027, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, 0, -24, 904, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, 8, -16, 906, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80185C3C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 9, 0, 0, { 2, 0 } },
    { 15, 17, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80185C64[5] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, 32, 488, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -120, 32, 499, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 104 } }, -80, -8, 410, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, -24, 40, 497, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -64, 96, 305, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80185CC8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80185CE0[8] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 338, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -8, 88, 325, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 80, 325, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, 72, 325, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 72, 48, 325, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 64, 350, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, 96, 325, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -64, 96, 325, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_80185D80[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_acropolis_sanctuary_80185D98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_sanctuary_80185DA8[38] = {
    { 143, 0x3FC0, { .fields = { 64, 112 } }, -48, -104, 674, { .fields = { 64, 0 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 64, 88 } }, 16, -112, 618, { .fields = { 0, 0 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 72, 80 } }, -56, 8, 622, { .fields = { 112, 88 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 72, 112 } }, 16, -24, 566, { .fields = { 56, 112 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 16, -112, 659, { .fields = { 112, 56 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -104, 637, { .fields = { 40, 8 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -96, 616, { .fields = { 40, 248 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, -88, 581, { .fields = { 88, 224 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -56, 554, { .fields = { 104, 224 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, -24, 544, { .fields = { 72, 128 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -8, 559, { .fields = { 72, 0 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 16, 546, { .fields = { 8, 232 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 80, 40, 506, { .fields = { 112, 168 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, -104, 690, { .fields = { 40, 16 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -96, 705, { .fields = { 24, 240 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -48, -88, 716, { .fields = { 64, 224 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, -56, 697, { .fields = { 120, 224 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -24, 675, { .fields = { 56, 224 } }, 128, 128, 128, 2 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, 8, 651, { .fields = { 24, 168 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -40, 64, 606, { .fields = { 56, 104 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 80, 578, { .fields = { 64, 48 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 24, 48, 562, { .fields = { 88, 104 } }, 128, 128, 128, 2 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 48, 80, 534, { .fields = { 40, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 16 } }, 0, -120, 647, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 48, -120, 599, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 32 } }, 64, -96, 568, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 56 } }, 72, -64, 554, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 72, -8, 534, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 80, 16, 513, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -32, -112, 688, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -48, -96, 699, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 56 } }, -64, -80, 685, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -56, -24, 672, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -64, -8, 644, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -72, 72, 604, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, -56, 72, 508, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 96, 40 } }, -8, 80, 475, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 88, 80, 481, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_acropolis_sanctuary_801860A0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 19, 0, 0, { 2, 0 } },
    { 23, 15, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_acropolis_sanctuary_801860C8[16] = {
    { { .empty = D_acropolis_sanctuary_80184404 }, D_acropolis_sanctuary_80184404, NULL },
    { { .elements = D_acropolis_sanctuary_80184414 }, D_acropolis_sanctuary_80184734, NULL },
    { { .elements = D_acropolis_sanctuary_80184764 }, D_acropolis_sanctuary_80184B4C, NULL },
    { { .elements = D_acropolis_sanctuary_80184B8C }, D_acropolis_sanctuary_80184F24, NULL },
    { { .elements = D_acropolis_sanctuary_80184F74 }, D_acropolis_sanctuary_801852A8, NULL },
    { { .elements = D_acropolis_sanctuary_801852F0 }, D_acropolis_sanctuary_80185570, NULL },
    { { .elements = D_acropolis_sanctuary_801855A0 }, D_acropolis_sanctuary_80185780, NULL },
    { { .elements = D_acropolis_sanctuary_801857B8 }, D_acropolis_sanctuary_8018586C, NULL },
    { { .elements = D_acropolis_sanctuary_8018588C }, D_acropolis_sanctuary_801858DC, NULL },
    { { .empty = D_acropolis_sanctuary_801858F4 }, D_acropolis_sanctuary_801858F4, NULL },
    { { .elements = D_acropolis_sanctuary_80185904 }, D_acropolis_sanctuary_801859A4, NULL },
    { { .elements = D_acropolis_sanctuary_801859BC }, D_acropolis_sanctuary_80185C3C, NULL },
    { { .elements = D_acropolis_sanctuary_80185C64 }, D_acropolis_sanctuary_80185CC8, NULL },
    { { .elements = D_acropolis_sanctuary_80185CE0 }, D_acropolis_sanctuary_80185D80, NULL },
    { { .empty = D_acropolis_sanctuary_80185D98 }, D_acropolis_sanctuary_80185D98, NULL },
    { { .elements = D_acropolis_sanctuary_80185DA8 }, D_acropolis_sanctuary_801860A0, NULL },
};

GpViewRec D_acropolis_sanctuary_80186188[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 6200, 0x7530, 8000 } }, 589 },
    { { { { 345, 0, 4081 }, { 546, 4059, -46 }, { -4044, 548, 342 } }, { 4961, 1400, 8451 } }, 230 },
    { { { { 559, 0, -4057 }, { -549, 4058, -75 }, { 4020, 554, 554 } }, { 9472, 1403, 8386 } }, 230 },
    { { { { 775, 0, -4021 }, { -623, 4046, -120 }, { 3973, 635, 766 } }, { 9087, 1276, 0x2D59 } }, 230 },
    { { { { 656, 0, 4043 }, { 635, 4045, -103 }, { -3992, 643, 648 } }, { 4960, 1280, 0x2D3A } }, 230 },
    { { { { -795, 0, -4017 }, { 65, 4095, -13 }, { 4017, -66, -795 } }, { 8714, 919, 4418 } }, 230 },
    { { { { -580, 0, 4054 }, { -150, 4093, -21 }, { -4051, -151, -579 } }, { 4402, 920, 4439 } }, 230 },
    { { { { 1227, 0, -3907 }, { -3609, 1569, -1134 }, { 1496, 3783, 470 } }, { 4250, 3534, 8412 } }, 230 },
    { { { { -186, 0, -4091 }, { -25, 4095, 1 }, { 4091, 25, -186 } }, { 6573, 667, 7896 } }, 230 },
    { { { { 1471, 0, 3822 }, { 944, 3969, -363 }, { -3704, 1012, 1425 } }, { 7110, 2190, 9120 } }, 680 },
    { { { { 3484, 0, -2153 }, { -524, 3972, -847 }, { 2089, 996, 3379 } }, { 8712, 1215, 9980 } }, 230 },
    { { { { 2699, 0, 3080 }, { -2257, 2787, 1978 }, { -2096, -3001, 1837 } }, { 8528, 459, 9285 } }, 230 },
    { { { { 4014, 0, -810 }, { -213, 3951, -1058 }, { 782, 1079, 3873 } }, { 5552, 1588, 0x2BBF } }, 230 },
    { { { { 4014, 0, -810 }, { -213, 3951, -1058 }, { 782, 1079, 3873 } }, { 8025, 1652, 6179 } }, 230 },
    { { { { 4071, 0, -446 }, { -158, 3830, -1440 }, { 417, 1449, 3808 } }, { 4926, 1150, 9330 } }, 230 },
    { { { { 1261, 0, -3896 }, { 704, 4028, 228 }, { 3832, -740, 1240 } }, { 0x310A, 4285, 8624 } }, 207 },
};

s32 D_acropolis_sanctuary_801863C8[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

s32 D_acropolis_sanctuary_801863D4[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_acropolis_sanctuary_801863E0[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_acropolis_sanctuary_801863E8[1] = {
    { 0, 0, 1, 0, D_acropolis_sanctuary_801863C8 },
};

GpRoomParamRec D_acropolis_sanctuary_801863F0[1] = {
    { 0, 0, 1, 0, D_acropolis_sanctuary_801863D4 },
};

GpRoomParamRec* D_acropolis_sanctuary_801863F8[8] = {
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E8,
    D_acropolis_sanctuary_801863F0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
    D_acropolis_sanctuary_801863E0,
};

GpAreaApplyRec D_acropolis_sanctuary_80186418[11] = {
    { 1, 3, 4, 1 },
    { 1, 4, 4, 1 },
    { 1, 7, 4, 1 },
    { 1, 9, 3, 17 },
    { 1, 9, 7, 33 },
    { 1, 10, 2, 17 },
    { 1, 10, 7, 33 },
    { 1, 11, 2, 17 },
    { 1, 11, 7, 33 },
    { 1, 13, 7, 33 },
    { 255, 0, 0, 0 },
};

TmdBone D_acropolis_sanctuary_80186444[3] = {
#include "assets/acropolis_sanctuary_model_09448_skeleton.inc"
};

u32 D_acropolis_sanctuary_801864B0[3] = {
#include "assets/acropolis_sanctuary_model_09448_partVerts.inc"
};

SVECTOR D_acropolis_sanctuary_801864BC[56] = {
#include "assets/acropolis_sanctuary_model_09448_verts.inc"
};

SVECTOR D_acropolis_sanctuary_8018667C[6] = {
#include "assets/acropolis_sanctuary_model_09448_normals.inc"
};

u32 D_acropolis_sanctuary_801866AC[215] = {
#include "assets/acropolis_sanctuary_model_09448_stream.inc"
};

TmdSource D_acropolis_sanctuary_80186A08 = {
    0,
    1768,
    0,
    3,
    D_acropolis_sanctuary_801864B0,
    D_acropolis_sanctuary_801864BC,
    D_acropolis_sanctuary_8018667C,
    D_acropolis_sanctuary_80186444,
    D_acropolis_sanctuary_801866AC,
};

TmdBone D_acropolis_sanctuary_80186A2C[1] = {
#include "assets/acropolis_sanctuary_model_096A8_skeleton.inc"
};

u32 D_acropolis_sanctuary_80186A50[1] = {
#include "assets/acropolis_sanctuary_model_096A8_partVerts.inc"
};

SVECTOR D_acropolis_sanctuary_80186A54[19] = {
#include "assets/acropolis_sanctuary_model_096A8_verts.inc"
};

SVECTOR D_acropolis_sanctuary_80186AEC[11] = {
#include "assets/acropolis_sanctuary_model_096A8_normals.inc"
};

u32 D_acropolis_sanctuary_80186B44[73] = {
#include "assets/acropolis_sanctuary_model_096A8_stream.inc"
};

TmdSource D_acropolis_sanctuary_80186C68 = {
    0,
    440,
    0,
    1,
    D_acropolis_sanctuary_80186A50,
    D_acropolis_sanctuary_80186A54,
    D_acropolis_sanctuary_80186AEC,
    D_acropolis_sanctuary_80186A2C,
    D_acropolis_sanctuary_80186B44,
};

u32 D_acropolis_sanctuary_80186C8C = 0xB000000;

Task* D_acropolis_sanctuary_80186C90 = NULL;

SVECTOR D_acropolis_sanctuary_80186C94 = { 0, 0, 0, 0 };

static s32  func_acropolis_sanctuary_8017F974(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2);
static s32  func_acropolis_sanctuary_8017FB18(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);
static void func_acropolis_sanctuary_801802E0(Task* task);

/// The room task's per-frame state. Once the session reaches phase 3
/// (`GameFlag_GetNibble(2)` still 0), advances that flag and applies the
/// room's one-shot state, then clears the `GpObj4A` bit 0x40 render filter on
/// the objects that stay hidden while `Gp_GetCurBit2Flag(0x1C)` is 2. `mask` is
/// a local because the target CSEs `~0x40` into a register and uses `and`
/// rather than nine `andi`s.
static void func_acropolis_sanctuary_8017D5E0(Task* task)
{
    s32      mask;
    GpObj4A* p0;
    GpObj4A* p3;
    GpObj4A* p4;
    GpObj4A* p5;
    GpObj4A* p6;
    GpObj4A* p7;
    GpObj4A* p8;
    GpObj4A* p9;
    GpObj4A* p10;

    if (GameFlag_GetNibble(2) == 0 && gGameSession->at4.loc.warp == 3) {
        GameFlag_SetNibble(2, 2);
        func_800E8634(D_acropolis_sanctuary_80180B0C, 0, D_acropolis_sanctuary_80181664);
        Gp_ApplyAreaRecs(D_acropolis_sanctuary_80186418);
        Mc_SaveData[0].state.sceneEvent = 6;
        GameFlag_SetNibble(1, 5);
        GameFlag_SetNibble(0x25, 1);
        func_800E3FAC(0xA2, 6);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 5);
    }
    if (Gp_GetCurBit2Flag(0x1C) == 2) {
        mask = ~0x40;
        p0   = &(D_acropolis_sanctuary_80183AE4 + 6)[0];
        p3   = &(D_acropolis_sanctuary_80183AE4 + 6)[3];
        p4   = &(D_acropolis_sanctuary_80183AE4 + 6)[4];
        p5   = &(D_acropolis_sanctuary_80183AE4 + 6)[5];
        p6   = &(D_acropolis_sanctuary_80183AE4 + 6)[6];
        p7   = &(D_acropolis_sanctuary_80183AE4 + 6)[7];
        p8   = &(D_acropolis_sanctuary_80183AE4 + 6)[8];
        p9   = &(D_acropolis_sanctuary_80183AE4 + 6)[9];
        p10  = &(D_acropolis_sanctuary_80183AE4 + 6)[10];

        p0->field_4A  &= mask;
        p3->field_4A  &= mask;
        p4->field_4A  &= mask;
        p5->field_4A  &= mask;
        p6->field_4A  &= mask;
        p7->field_4A  &= mask;
        p8->field_4A  &= mask;
        p9->field_4A  &= mask;
        p10->field_4A &= mask;
    }
}

/// Message gate for the sanctuary's second hotspot: copies the incoming record
/// to the outgoing one, then answers message 0xB. The first time the message is
/// seen for real (`field_5` == 0) it latches nibble 7 to 2 and raises the room's
/// 0x13 bit-2 flag; the answer written back into `field_3` is 1 while nibble 2
/// is still clear and 2 once it is set.
s32 func_acropolis_sanctuary_8017D73C(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->prefix.packed == 0xB) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(7) == 0) {
                GameFlag_SetNibble(7, 2);
                Gp_SetCurBit2Flag(0x13, 2);
            }
        }
        if (in->prefix.packed == 0xB && in->field_5 == 0) {
            nib = GameFlag_GetNibble(2);
            if (nib == 0) {
                nib = 1;
            } else {
                nib = 2;
            }
            out->field_3 = nib;
        }
    }
    return 1;
}

/// Message-table handler for message 0x13F1: does nothing and answers 0.
s32 func_acropolis_sanctuary_8017D808(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_acropolis_sanctuary_8017D810(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 0 && GameFlag_GetNibble(6) == 0) {
        func_800E8614(D_acropolis_sanctuary_80181814, 0);
    }
    return 0;
}

/// Message gate for the sanctuary hotspot registered under id 0x13EF: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 7 so a
/// second visit does nothing. The record is not copied to the outgoing one -
/// this handler only ever consumes the message (returns 0).
s32 func_acropolis_sanctuary_8017D848(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 1);
        Task_SpawnFromTable(&D_acropolis_sanctuary_80182240, 0, 0, 0);
    }
    return 0;
}

void func_acropolis_sanctuary_8017D8A0(u32 arg0)
{
    func_acropolis_sanctuary_8017DF88((arg0 >> 8) & 0xFF, arg0 & 0xFF);
}

/// Republishes the player's weapon to slot 3: picks the room's 0x3E8 record by
/// the equipped-weapon index in `Player_Status.weapon`, has `Gp_PlayerWeaponId` stamp the
/// current weapon model id into its `field_0`, then sends it.
void func_acropolis_sanctuary_8017D8CC(void)
{
    if (Player_Status.weapon == 2) {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_801809F8.source.index);
        Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &D_acropolis_sanctuary_801809F8, 0);
    } else {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_80180A0C.source.index);
        Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &D_acropolis_sanctuary_80180A0C, 0);
    }
}

/// State 0 of the sanctuary room task: publishes the room's message-handler
/// table under pointer slot 7 and advances to the next state. Unless nibble 6
/// has already reached 1 it also chains slot-4 message list 1 onto itself and,
/// when nibble 2 is set and that slot holds a task, places the actor by sending
/// it the 0x7D3 animation record followed by the 0x7D4 placement.
static void func_acropolis_sanctuary_8017D930(Task* arg0)
{
    Task* slot;

    arg0->msgTable = D_acropolis_sanctuary_8018081C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = arg0->state + 1;
    if (GameFlag_GetNibble(6) != 1) {
        slot = Gp_LookupSlot4(1);
        Gp_MsgSlot4Chain(1, 1);
        if (GameFlag_GetNibble(2) != 0 && slot != NULL) {
            Gp_DispatchMsgPtr(slot, 0x7D3, &D_acropolis_sanctuary_80180AE8, 0);
            Gp_DispatchMsgPtr(slot, 0x7D4, &D_acropolis_sanctuary_801808BC, 0);
        }
    }
    func_acropolis_sanctuary_8017DD78();
}

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table: set-up, the per-frame entry fixup and `taskKill`.
void func_acropolis_sanctuary_8017D9E8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_sanctuary_8017D5C4;
    sp.funcs[task->state](task);
}

/// The sanctuary cutscene task. State 0 allocates the task's `AcsCutsceneWork`
/// block, captures slot 3 in it, publishes the task itself in
/// `D_acropolis_sanctuary_80186C90` and cues the scene: slot 3 is sent the
/// 0x3E8 weapon record for the equipped weapon, the scene's sound event is
/// enqueued and its script pair is started.
///
/// State 1 drives the scene. `GameSession::eventState` reaching 0 instead stops
/// the sound, writes the room's exit into the save and hands off to task 0x11
/// before the task kills itself. Otherwise the scene fires exactly
/// once, when `func_acropolis_sanctuary_8017DCE0` has armed `phase` at 2 and
/// `step` is still 0: the player's effects are dropped, slot 3 is given the
/// 0x3F4 record and then warped to the scene's mark with a 0x3E9 placement, and
/// `step` is bumped so the next frame does nothing.
void func_acropolis_sanctuary_8017DA40(Task* arg0)
{
    AcsMsgArg        weapon;
    AcsMsgArg        rec;
    AcsMsgArg*       msg;
    AcsCutsceneWork* work;
    AcsCutsceneWork* cutscene;
    AcsCutsceneWork* slot;
    AcsCutsceneWork* target;
    s32              state;
    s32              idx;
    s32              weaponId;

    state = arg0->state;
    switch (state) {
        case 0:
            if (Gp_StateC08.field_A != 1 && gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                work       = memCalloc(0xC, 0);
                arg0->work = (TaskIdMap*)work;
                if (work == NULL) {
                    taskKill(arg0);
                } else {
                    Mem_Set(work, 0, 0xC);
                    work->target                   = gameGetPtrSlot(3);
                    D_acropolis_sanctuary_80186C90 = arg0;
                }
                slot     = (AcsCutsceneWork*)arg0->work;
                weaponId = Player_Status.weapon;
                idx      = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;

                weapon.rec.source.index         = idx;
                weapon.rec.animationId          = 1;
                weapon.rec.blend                = ANIMATION_BLEND_INTERPOLATE;
                weapon.rec.blendFrames          = 0xF;
                weapon.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr(slot->target, ANIMATION_MESSAGE_PLAY, &weapon, 0);
                SndEvt_EnqueueType6(0x510C0007, 0, 0);
                func_800E8634(D_acropolis_sanctuary_801820F0, 0, D_acropolis_sanctuary_801821C8);
                arg0->state = arg0->state + 1;
            }
            break;

        case 1:
            if (gGameSession->eventState == 0) {
                SndEvt_EnqueueType7(0x80000000, 0);
                Mc_SaveData[0].state.at4.loc.area  = 0xD;
                Mc_SaveData[0].state.at4.loc.stage = 1;
                Mc_SaveData[0].state.at4.loc.warp  = 2;
                Mc_SaveData[0].state.at4.loc.room  = 1;
                gDisplayState.spriteVariant        = 1;
                Task_Spawn(0, 0x11, 0, 0);
                taskKill(arg0);
                break;
            }
            cutscene = (AcsCutsceneWork*)arg0->work;
            msg      = &rec;
            switch (cutscene->phase) {
                case 0:
                case 1:
                    break;
                case 2:
                    if (cutscene->step == 0) {
                        Gp_KillPlayerEffs();
                        target = (AcsCutsceneWork*)arg0->work;
                        if (target->target != NULL) {
                            rec.rec.source.sets           = D_acropolis_sanctuary_801820E4;
                            rec.rec.animationId           = 0;
                            rec.rec.blend                 = ANIMATION_BLEND_RESET;
                            msg->rec.blendFrames          = 0xF;
                            msg->rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                            Gp_DispatchMsgPtr(target->target, ANIMATION_MESSAGE_INSTALL_AND_PLAY, msg, 0);
                        }
                        rec.place.pos.vx  = -0x1DB0;
                        rec.place.pos.vy  = 0;
                        msg->place.pos.vz = -0x1130;
                        rec.place.rot.vx  = 0;
                        rec.place.rot.vy  = 0;
                        rec.place.rot.vz  = 0;
                        Gp_DispatchMsgPtr(cutscene->target, 0x3E9, msg, 0);
                        Mc_SaveData[0].state.at4.loc.view = 0xE;
                        cutscene->step                    = cutscene->step + 1;
                    }
                    break;
            }
            break;
    }
}

/// Request entry point for the sanctuary cutscene task's work block: 0 and 1
/// arm the script at phase 1 or 2 respectively, rewinding `step` so the driver
/// runs the scene once, while 2 just plays the pair of sound events the scene
/// is cued with.
void func_acropolis_sanctuary_8017DCE0(s32 arg0)
{
    AcsCutsceneWork* work = (AcsCutsceneWork*)D_acropolis_sanctuary_80186C90->work;

    switch (arg0) {
        case 0:
            work->phase = 1;
            work->step  = 0;
            return;
        case 1:
            work->phase = 2;
            work->step  = 0;
            return;
        case 2:
            SndEvt_EnqueueType7(0x510C0007, 0);
            SndEvt_EnqueueType6(0x510C0008, 0, 0);
            return;
    }
}

/// Arms the sanctuary's blocker cage: copies the first four normals, eight
/// corners and four quads of the template at `D_acropolis_sanctuary_801822EC`
/// into the live set at `D_acropolis_sanctuary_80183568`, marking every copied
/// quad live, then slides all eight corners to where the cage belongs. Nibble 6
/// is the sanctuary cutscene flag: before the scene the cage sits across the
/// doorway, afterwards it is pushed 3000 units aside and out of the way.
static void func_acropolis_sanctuary_8017DD78(void)
{
    GpGridParams*   dst = &D_acropolis_sanctuary_80183568;
    GpGridParams*   src = &D_acropolis_sanctuary_801822EC;
    AcsBlockerShift shift;
    s32             i;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx           = src->field_4[i].vx;
        dst->field_4[i].vy           = src->field_4[i].vy;
        dst->field_4[i].vz           = src->field_4[i].vz;
        dst->field_8[i * 2].vx       = src->field_8[i * 2].vx;
        dst->field_8[i * 2].vy       = src->field_8[i * 2].vy;
        dst->field_8[i * 2].vz       = src->field_8[i * 2].vz;
        dst->field_8[(i * 2) + 1].vx = src->field_8[(i * 2) + 1].vx;
        dst->field_8[(i * 2) + 1].vy = src->field_8[(i * 2) + 1].vy;
        dst->field_8[(i * 2) + 1].vz = src->field_8[(i * 2) + 1].vz;
        dst->field_C[i]              = src->field_C[i];
        dst->field_C[i].surfaceClass = 1;
    }

    if (GameFlag_GetNibble(6) == 0) {
        shift.vx = 200;
        shift.vy = 0;
        shift.vz = 380;
    } else {
        shift.vx = 3000;
        shift.vy = 0;
        shift.vz = 0;
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += shift.vx;
        dst->field_8[i].vy += shift.vy;
        dst->field_8[i].vz += shift.vz;
    }
}

/// Toggles a pair of sprite commands for view `arg1` of the current room:
/// `arg0` zero draws the second command and skips the third, non-zero does the
/// reverse. `Gp_LinkViewSprts` reads `field_4` to decide whether to skip
/// OT-linking each command's prims.
static void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1)
{
    GameSession*     g    = gGameSession;
    GameLocationKey* sess = &g->at4.loc;
    GpSprtCmd*       cmd;

    cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][(arg1 & 0xFF) - 1].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
    }
}

/// State 0 of the sanctuary's effect task: spawns the twelve 0x6008B effects
/// the room is decorated with, six with spawn arg `0x200 + i` and six with
/// `0xA00000 + i`, each anchored to the task's own coordinate and offset by its
/// entry in `D_acropolis_sanctuary_80182774`. It then publishes the task's
/// handler table under pointer slot 5 and advances the state so the spawn runs
/// once. Every frame after that it mirrors the session's stage byte into
/// `D_acropolis_sanctuary_80182770`: 1 while the byte is 0x10, held while it is
/// 0xC, 0 otherwise.
void func_acropolis_sanctuary_8017E00C(Task* task)
{
    GfxCoord*         coord;
    GameLocationKey* sess;
    s32              i;

    coord = task->extra.tmd->coords;
    if (task->state == 0) {
        for (i = 0; i < 6; i++) {
            Gp_SpawnEff(0x6008B, coord, i + 0x200, &D_acropolis_sanctuary_80182774[i]);
        }
        for (i = 6; i < 12; i++) {
            Gp_SpawnEff(0x6008B, coord, i + 0xA00000, &D_acropolis_sanctuary_80182774[i]);
        }
        task->msgTable = D_acropolis_sanctuary_80182310;
        Game_SetPtrSlot(task, 5);
        D_acropolis_sanctuary_80182770 = 0;
        task->state                    = task->state + 1;
    }
    sess = &gGameSession->at4.loc;
    if (sess->view == 0x10) {
        D_acropolis_sanctuary_80182770 = 1;
    } else if (sess->view != 0xC) {
        D_acropolis_sanctuary_80182770 = 0;
    }
}

/// State 0 of the sanctuary's mosaic task: spawns one 0x60079 effect per tile,
/// first for all 72 entries of `D_acropolis_sanctuary_80182320` keyed by their
/// own index, then a second pass over the 16 tiles listed in
/// `D_acropolis_sanctuary_80182750` keyed by the tile index itself, so those
/// sixteen get a second effect on top. Each spawn reuses the task's own
/// `GpEffWork` offset triple: x is always 0, y and z come from the tile's grid
/// position scaled by 1145/128 and 2147/256 and shifted by the origin corner of
/// the size class in `quad`. Any state but 0 just releases the work block.
void func_acropolis_sanctuary_8017E134(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    AcsTile*   tile;
    s32        quad;
    s32        i;
    s32        idx;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (arg0->state != 0) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    for (i = 0; i < 0x48; i++) {
        tile         = &D_acropolis_sanctuary_80182320[i];
        quad         = tile->quad;
        mem->move.vx = 0;
        mem->move.vy = ((tile->col * 1145) >> 7) - D_acropolis_sanctuary_80182710[quad].corner[0].vy;
        mem->move.vz = -((tile->row * 2147) >> 8) - D_acropolis_sanctuary_80182710[quad].corner[0].vz;
        Gp_SpawnEff(0x60079, coord, i, &mem->move);
    }
    for (i = 0; i < 0x10; i++) {
        idx          = D_acropolis_sanctuary_80182750[i];
        tile         = &D_acropolis_sanctuary_80182320[idx];
        quad         = tile->quad;
        mem->move.vx = 0;
        mem->move.vy = ((tile->col * 1145) >> 7) - D_acropolis_sanctuary_80182710[quad].corner[0].vy;
        mem->move.vz = -((tile->row * 2147) >> 8) - D_acropolis_sanctuary_80182710[quad].corner[0].vz;
        Gp_SpawnEff(0x60079, coord, idx, &mem->move);
    }
    arg0->state = arg0->state + 1;
}

/// Draws one frame of a whole mosaic tile: a semi-transparent textured quad
/// whose four corners are the size class's own corner offsets, rotated by the
/// task's own `workm` and then projected through `GsWSMATRIX` into an
/// `AcsTileScratch` block taken from `G_SCRATCH_HEAD`. The first corner goes
/// through `rtps` and the other three through `rtpt`; tiles inside `otz` 0x11
/// are dropped. The texture window is the tile's own `row` / `col` origin
/// stretched by its `field_0` / `field_2` extent, so the quad shows its own
/// piece of the mosaic sheet at full size -- this is the intact tile,
/// `func_acropolis_sanctuary_8017EC90` draws the shards it breaks into.
///
/// The drift (`move`) and spin (`pos`) are
/// seeded from the LCG on the first drawn frame, in one of two strengths
/// chosen by the tile's `field_8`: a fast, wide-tumbling one and a slow one
/// whose life (`angle`) also gets a random 0..7 bonus. Life is the tile's
/// `field_A`, and `age` is the frame counter measured against it -- the
/// tile drifts while it is still young, and the work block is released 0x3C
/// frames past that.
///
/// While drifting, a tile of the fast kind (`scale` zero) has a 1-in-60
/// chance per frame -- or a certainty once past y = -0xBFF -- of shedding one
/// to four 0x6007A shards, tagged 0x1000 so they spawn as the airborne
/// variant. Crossing x = -0x2740 above y = -0xED7 either shatters a
/// size-class-1 tile into two to four untagged shards or, for size class 0,
/// bounces it by halving and inverting the vertical step. Either split costs
/// 0x64 of life. Once the room flag is set and the session is not in mode
/// 0x10, tiles past x = -0x28C0 also age by 0x3C, so they clear away.
void func_acropolis_sanctuary_8017E338(Task* arg0)
{
    GpEffWork*      mem;
    GfxCoord*       coord;
    void**          scratch;
    u8*             head;
    AcsTileScratch* blk;
    POLY_FT4*       prim;
    SVECTOR*        sv;
    s32             quad;
    s32             i;
    s32             n;

    mem   = arg0->spawnArg2.pointer;
    quad  = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].quad;
    coord = arg0->extra.tmd->coords;
    Gp_UpdateCoord(coord);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x28;
    blk      = (AcsTileScratch*)(head - 0x28);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        blk->v[i].vx = D_acropolis_sanctuary_80182710[quad].corner[i].vx;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(AcsTileScratch, v));
        sv->vy = D_acropolis_sanctuary_80182710[quad].corner[i].vy;
        sv->vz = D_acropolis_sanctuary_80182710[quad].corner[i].vz;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (mem->age == 0) {
            mem->scale = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_8;
            if (mem->scale != 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -(((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vx  = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vy  = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vz  = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                mem->angle   = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_A;
            } else {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = -(((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = ((u32)Gp_LcgState >> 16) & 7;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 4 - (((u32)Gp_LcgState >> 16) & 7);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vx  = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vy  = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->pos.vz  = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->angle   = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_A +
                             (((u32)Gp_LcgState >> 16) & 7);
            }
        }
        prim->tpage = 0x8C;
        prim->clut  = 0x4200;
        prim->code |= 3;
        prim->u0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row;
        prim->v0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col;
        prim->u1    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_0;
        prim->v1 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col;
        prim->u2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row;
        prim->v2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_2;
        prim->u3 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_0;
        prim->v3 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_2;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x28);
    if (mem->angle + 0x3C < mem->age) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (mem->angle < mem->age) {
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        coord->coord.t[2] += mem->move.vz;
        Gfx_RotMatrixYXZ(&coord->coord, &mem->pos, 0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->move.vy        = mem->move.vy + 3;
        if (mem->scale == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 60U) == 0 || coord->coord.t[1] >= -0xBFF) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                n           = (((u32)Gp_LcgState >> 16) & 3) + 1;
                for (i = 0; i < n; i++) {
                    Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1.value | 0x1000, NULL);
                }
                mem->age = mem->age + 0x64;
            }
        }
    }
    if (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7) {
        if (quad != 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            // The size class is dead once it has been tested, so the shard
            // count reuses its local -- keeping the two apart costs `$s5`.
            quad = ((u32)Gp_LcgState >> 16) & 3;
            if (quad != 0) {
                quad = quad + 1;
                for (i = 0; i < quad; i++) {
                    Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1.value, NULL);
                }
                mem->age = mem->age + 0x64;
            } else {
                coord->coord.t[1] -= mem->move.vy * 2;
                mem->move.vy       = -(mem->move.vy >> 1);
            }
        } else {
            coord->coord.t[1] -= mem->move.vy * 2;
            mem->move.vy       = -(mem->move.vy >> 1);
        }
    }
    if (gGameSession->at4.loc.view != 0x10 && D_acropolis_sanctuary_80182770 != 0 &&
        (coord->coord.t[0] < -0x28C0 ||
         (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7))) {
        mem->age = mem->age + 0x3C;
    }
    mem->age = mem->age + 1;
}

/// Draws one frame of a mosaic shard: a semi-transparent textured triangle
/// whose three corners come from the first three corners of
/// `D_acropolis_sanctuary_80182710`, scaled about the origin by the shard's
/// size (`angle`) with the GTE's `gpf` interpolator and rotated by the
/// task's own `workm`, then projected through `GsWSMATRIX` with `rtpt` into an
/// `AcsMosaicScratch` block taken from `G_SCRATCH_HEAD`; shards inside `otz`
/// 0x11 are dropped. The texture window is the tile's `row` / `col` corner
/// stretched by the same size factor, so the shard shows its own piece of the
/// mosaic sheet.
///
/// `Task::spawnArg1` is unpacked on the first frame: bits 12..15 select the
/// drift pattern, the high halfword is the size (defaulting to 0x1000) and only
/// the low 12 bits are kept, as the index into the tile table. The same frame
/// seeds the per-frame drift (`move`) and spin
/// (`pos`) from the LCG -- pattern 0 falls faster, since its
/// vertical step is seeded negative.
///
/// Each frame the shard drifts by that step, gains 3 of downward speed, and is
/// respun. Crossing x = -0x2740 above y = -0xED7 either bounces it (halving and
/// inverting the vertical step, twice at most) or, for shards at least 0x401
/// big, shatters it into one or two 0x6007A children; a big shard also has a
/// 1-in-60 chance per frame -- or a certainty once past y = -0xBFF -- of
/// splitting into two. Either way the split costs 0x3C of life. Once the room
/// flag is set and the session is not in mode 0x10, shards past x = -0x28C0
/// also age by 0x3C, so they clear away.
void func_acropolis_sanctuary_8017EC90(Task* arg0)
{
    GpEffWork*        mem;
    GfxCoord*         coord;
    void**            scratch;
    u8*               head;
    AcsMosaicScratch* blk;
    POLY_FT3*         prim;
    AcsQuadCorner*    corner;
    s32               size;
    s32               hi;
    s32               i;
    s32               n;
    s32               flags;
    SVECTOR*          sv;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if (mem->age >= 0x3D || mem->index >= 2) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    Gp_UpdateCoord(coord);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x20;
    blk      = (AcsMosaicScratch*)(head - 0x20);
    if (mem->age == 0) {
        mem->scale = (arg0->spawnArg1.value >> 12) & 0xF;
        hi         = (s16)(arg0->spawnArg1.value >> 16);
        size       = 0x1000;
        if ((u16)hi != 0) {
            size = hi;
        }
        mem->angle            = size;
        arg0->spawnArg1.value = arg0->spawnArg1.value & 0xFFF;
        if (mem->scale != 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = ((u32)Gp_LcgState >> 16) & 0xF;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
        } else {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = -(((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
        }
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->pos.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->pos.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->pos.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    corner = D_acropolis_sanctuary_80182710->corner;
    for (i = 0; i < 3; i++) {
        blk->v[i].vx = corner[i].vx;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(AcsMosaicScratch, v));
        sv->vy = corner[i].vy;
        sv->vz = corner[i].vz;
        gte_lddp(mem->angle);
        gte_ldsv(&blk->v[i]);
        gte_gpf12();
        gte_stsv(&blk->v[i]);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt();
    prim           = (POLY_FT3*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 7);
    setcode(prim, 0x24);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x8C;
        prim->clut  = 0x4200;
        prim->code |= 3;
        prim->u0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row;
        prim->v0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col;
        prim->u1    = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row +
                   ((D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_0 * mem->angle) >> 12);
        prim->v1 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col;
        prim->u2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].row;
        prim->v2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].col +
                   ((D_acropolis_sanctuary_80182320[arg0->spawnArg1.value].field_2 * mem->angle) >> 12);
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    coord->coord.t[0] += mem->move.vx;
    coord->coord.t[1] += mem->move.vy;
    SCRATCH_POP_BYTES(0x20);
    coord->coord.t[2] += mem->move.vz;
    Gfx_RotMatrixYXZ(&coord->coord, &mem->pos, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    mem->move.vy        = mem->move.vy + 3;
    if (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        n           = ((u32)Gp_LcgState >> 16) & 1;
        if (mem->angle >= 0x401 && n != 0) {
            n = n + 1;
            for (i = 0; i < n; i++) {
                Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1.value | (mem->angle << 15), NULL);
            }
            mem->age = mem->age + 0x3C;
        } else {
            coord->coord.t[1] -= mem->move.vy * 2;
            mem->move.vy       = -(mem->move.vy >> 1);
            mem->index         = mem->index + 1;
        }
    } else if (mem->angle >= 0x401) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 60U) == 0 || coord->coord.t[1] >= -0xBFF) {
            for (i = 0; i < 2; i++) {
                flags = (mem->angle << 15) | 0x1000;
                Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1.value | flags, NULL);
            }
            mem->age = mem->age + 0x3C;
        }
    }
    if (gGameSession->at4.loc.view != 0x10 && D_acropolis_sanctuary_80182770 != 0 &&
        (coord->coord.t[0] < -0x28C0 ||
         (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7))) {
        mem->age = mem->age + 0x3C;
    }
    mem->age = mem->age + 1;
}

/// Draws one frame of the sanctuary's flame sprite. The task's coordinate is
/// refreshed and projected through `GsWSMATRIX` into an `RoomShaftScratch` block
/// taken from `G_SCRATCH_HEAD`; the projected point becomes the centre of a
/// semi-transparent `POLY_FT4` on tpage 0x2B whose half-extent is
/// `field_24 * 0x27 / otz`, so the flame shrinks with distance and is dropped
/// entirely inside `otz` 0x11. `Task::spawnArg1` is unpacked once, on the first
/// frame: bits 16..27 are the sprite's size (defaulting to 0x280 when zero),
/// bits 8..9 pick one of four 0x28x0x27 cells across the sheet -- and, through
/// `getClut`, the matching 16-colour palette -- and only the low nibble is kept,
/// as the index into `D_acropolis_sanctuary_801827D4`, the per-variant mask of
/// camera views the flame is visible from. The grey level is the variant's base
/// level plus its flicker amplitude on odd frames.
void func_acropolis_sanctuary_8017F4E8(Task* arg0)
{
    GpEffWork*        mem;
    GfxCoord*         coord;
    void**            scratch;
    u8*               head;
    RoomShaftScratch* blk;
    POLY_FT4*         prim;
    AcsSpriteLevels   base;
    AcsSpriteLevels   step;
    s32               param;
    s32               lvl;
    s16               x;
    s16               y;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.tmd->coords;
    if ((D_acropolis_sanctuary_801827D4[arg0->spawnArg1.value & 0xF] >> (gGameSession->at4.loc.view - 1)) & 1) {
        Gp_UpdateCoord(coord);
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        *scratch = head - 0x14;
        blk      = (RoomShaftScratch*)(head - 0x14);
        if (arg0->state == 0) {
            base                  = D_acropolis_sanctuary_8017D5D8;
            step                  = D_acropolis_sanctuary_8017D5DC;
            param                 = arg0->spawnArg1.value;
            mem->scale            = (param & 0x0FFF0000) ? ((param >> 16) & 0xFFF) : 0x280;
            mem->angle            = (arg0->spawnArg1.value >> 8) & 3;
            arg0->spawnArg1.value = arg0->spawnArg1.value & 0xF;
            mem->period           = base.v[mem->angle];
            mem->step             = step.v[mem->angle];
            arg0->state++;
        }
        blk->vec.vx = (u16)coord->workm.t[0];
        blk->vec.vy = (u16)coord->workm.t[1];
        blk->vec.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->vec);
        gte_rtps();
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&blk->sx);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            lvl         = (u8)mem->period + (gDisplayState.animFrame & 1) * mem->step;
            prim->tpage = 0x2B;
            prim->code |= 2;
            setRGB0(prim, lvl, lvl, lvl);
            prim->clut     = getClut(mem->angle * 0x10, 0x10E);
            prim->u0       = mem->angle * 0x28;
            prim->v0       = 0;
            prim->u1       = mem->angle * 0x28 + 0x27;
            prim->v1       = 0;
            prim->u2       = mem->angle * 0x28;
            prim->v2       = 0x27;
            prim->u3       = mem->angle * 0x28 + 0x27;
            prim->v3       = 0x27;
            blk->halfWidth = (mem->scale * 0x27) / blk->otz;
            x              = blk->sx - (u16)blk->halfWidth;
            prim->x2       = x;
            prim->x0       = x;
            x              = blk->sx + (u16)blk->halfWidth;
            prim->x3       = x;
            prim->x1       = x;
            y              = blk->sy - (u16)blk->halfWidth;
            prim->y1       = y;
            prim->y0       = y;
            y              = blk->sy + (u16)blk->halfWidth;
            prim->y3       = y;
            prim->y2       = y;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
        }
        SCRATCH_POP_BYTES(0x14);
    }
}

/// Spawns effect 0x60078 on the room task's model coordinate, seeded with the
/// fixed offset vector `D_acropolis_sanctuary_8017D5D0`. Always consumes the event
/// (returns 0).
s32 func_acropolis_sanctuary_8017F918(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    GfxCoord* coord = task->extra.tmd->coords;
    SVECTOR   vec   = D_acropolis_sanctuary_8017D5D0;

    Gp_SpawnEff(0x60078, coord, 0, &vec);
    return 0;
}

/// Asks `func_800E0C10` for the displacement `rec` imposes and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_sanctuary_80186C94`. Returns non-zero when the X or Z
/// displacement is non-zero.
static s32 func_acropolis_sanctuary_8017F974(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2)
{
    OverlayDeltaFlag* s;
    s32               val;

    s        = SCRATCH_PUSH(OverlayDeltaFlag);
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                += s->delta.vx.w >> 16;
        coord->coord.t[2]                += s->delta.vz.w >> 16;
        D_acropolis_sanctuary_80186C94.vx = s->delta.vx.w >> 16;
        D_acropolis_sanctuary_80186C94.vy = s->delta.vy.w >> 16;
        D_acropolis_sanctuary_80186C94.vz = s->delta.vz.w >> 16;
        val                               = s->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_sanctuary_80186C94.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_sanctuary_80186C94.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_sanctuary_80186C94.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_sanctuary_80186C94.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP(OverlayDeltaFlag);
    return s->moved;
}

/// Measures the bearing of each type-1 or type-3 record in `recs` (up to
/// `count`, or the first zero key) from the coordinate's world position,
/// relative to the direction it faces. For a record that has every other such
/// record within a quarter turn of it, moves the coordinate `push` units back
/// along that record's bearing, in X and Z. Returns non-zero if it moved the
/// coordinate; returns 0 at once while `gGameSession->viewReady` is 1.
static s32 func_acropolis_sanctuary_8017FB18(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push)
{
    OverlayBisectorScratch* st;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_PUSH(OverlayBisectorScratch);
    st         = SCRATCH_HEAD(OverlayBisectorScratch);
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    st->eye.vz = (u16)coord->coord.t[2];

    overlayToWorld(coord->parent, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key.value == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key.value & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->angle[st->i] = ratan2(st->delta.vx, st->delta.vz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)st->aim.vz - (u16)st->eye.vz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, st->delta.vz);

            st->angle[st->i] = overlayWrapAngle(st->angle[st->i]);
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                st->diff = overlayWrapAngle(st->diff);
                if (abs(st->diff) > 0x400) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    hit = st->hit;
    SCRATCH_POP(OverlayBisectorScratch);
    return hit;
}

/// Per-frame visibility gate for the sanctuary's item object: hides the model
/// (`field_C` bit 0x80) while the camera sits on view 0xB or 0xD, or once the
/// item's 2-bit pickup flag has reached 2; otherwise shows it again with the
/// default flags.
void func_acropolis_sanctuary_80180264(Task* task)
{
    GpItemObj8* obj = task->spawnArg2.pointer;
    TmdObject*  tmd = task->extra.tmd;
    s32         flag;
    s32         view;

    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view == 0xB || view == 0xD || flag == 2) {
        tmd->flags = 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
    }
}

/// Per-frame visibility hook for an item object: hides the model (`flags`
/// 0x80) once the item's 2-bit pickup flag has reached 2, otherwise shows it
/// with the default flags. The current view is queried but not used.
static void func_acropolis_sanctuary_801802E0(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2.pointer;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags = 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
    }
}
