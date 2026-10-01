#include "actors/actor_403200.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_contacts.h"
#include "../../shared/incinerator_boss.h"

extern s8 D_actor_403200_8015F8E0[8];

typedef struct Actor403200Obj Actor403200Obj;

/// Reference positions used by the distance-based view selector.
typedef struct Actor403200ViewPoints {
    /* 0x00 */ SVECTOR v[4];
} Actor403200ViewPoints;
STATIC_ASSERT_SIZEOF(Actor403200ViewPoints, 0x20);

/// One of the nine models and spawn arguments used by the formation spawner.
typedef struct Actor403200SpawnRec {
    /* 0x0 */ TmdSource* model;
    /* 0x4 */ s16        spawnArg;
    /* 0x6 */ s16        field_6;
} Actor403200SpawnRec;
STATIC_ASSERT_SIZEOF(Actor403200SpawnRec, 0x8);

/// 0x20-byte scratchpad frame the state-selecting tick
/// `func_actor_403200_8013EB64` carves off the scratch-pad stack. `delta` is the
/// player-relative offset whose length is `dist`, the range the three
/// `field_F08` sub-states door the enemy through;
/// `view` is the camera-relative offset the yaw written to `field_7C4` is
/// taken from. Both are read back out of the frame rather than kept in
/// registers, which is what puts them in the scratch in the first place.
typedef struct Actor403200ApproachScratch {
    /* 0x00 */ VECTOR  delta; // player position minus this part's, in world units
    /* 0x10 */ SVECTOR view;  // camera position minus this part's
    /* 0x18 */ s32     dist;  // length of `delta`
    /* 0x1C */ byte    pad_1C[0x4];
} Actor403200ApproachScratch;
STATIC_ASSERT_SIZEOF(Actor403200ApproachScratch, 0x20);

/// 0xC-byte scratchpad frame the launch state's reset half
/// `func_actor_403200_8013B3C8` carves off the scratch-pad stack for the one yaw it
/// takes this tick. `dir` is the player-relative offset of the host model's
/// root part, the pair `ratan2` turns into the yaw written to `field_7C4`; the
/// trailing word is not read back, and is only here because the frame the code
/// carves is 0xC, not the 8 the vector alone needs.
typedef struct Actor403200TurnScratch {
    /* 0x00 */ SVECTOR dir; // player position minus the host root part's
    /* 0x08 */ s32     field_8;
} Actor403200TurnScratch;
STATIC_ASSERT_SIZEOF(Actor403200TurnScratch, 0xC);

/// 0x54-byte scratchpad frame the launch tick carves off the scratch-pad stack. `dir`
/// starts as the player-relative offset in the arena plane, is renormalised
/// and then scaled by the per-frame pull the animation frame selects; `pos` is
/// the host's fifth part in view space, which the yaw `angle` and the message
/// 0x3E9 placement are both built from. `push` is the 32-bit triple
/// `func_80105B74` copies onto the player, `dist` is the offset's length, and
/// `pull` / `period` are the phase strength and the script-spawn interval.
typedef struct Actor403200DragScratch {
    /* 0x00 */ VECTOR3 push;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ SVECTOR pos;
    /* 0x20 */ byte    pad_20[0x20];
    /* 0x40 */ s32     dist;
    /* 0x44 */ byte    pad_44[0x4];
    /* 0x48 */ s16     angle;
    /* 0x4A */ byte    pad_4A[0x2];
    /* 0x4C */ s16     pull;
    /* 0x4E */ s16     i;
    /* 0x50 */ s16     period;
    /* 0x52 */ byte    pad_52[0x2];
} Actor403200DragScratch;
STATIC_ASSERT_SIZEOF(Actor403200DragScratch, 0x54);

/// Exit callback of the boss task, installed by its spawn state.

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_80174D58;

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Non-zero while the overlay is shutting down: the spawn states tear their
/// enemies down instead of standing them up, and the state-selecting tick
/// holds `field_6` at zero and re-rolls its sub-state.
extern s16 gIncinBossEnded;

/// Set while the grab's animation is installed on the player; the player's
/// side clears it on release.
extern s32 gIncinBossGrabActive;

/// Counter the launch state sets and the tick after it winds down; the escort
/// pose driver floors it, drops the escort's body by a fifth of it and picks
/// each phase's targets by its range.
extern s16 gIncinBossLimbReach;

/// Cleared by both halves of the launch state and exposed through the setter /
/// getter pair `func_actor_403200_80141108` and `func_actor_403200_80141114`;
/// the spinner enemies wait for it to be 1 and die once it is 0.
extern s16 gIncinBossSpinnersReleased;

/// Script pairs spawned on the per-frame body's and the death sequence's cues,
/// and on the frames the launch tick's phase selects.
extern PadScriptCmd              D_actor_403200_80141C5C[2];
extern PadScriptVibrationSegment D_actor_403200_80141C64[2];
extern PadScriptCmd              D_actor_403200_80141C6C[2];
extern PadScriptVibrationSegment D_actor_403200_80141C74[2];
extern PadScriptCmd              D_actor_403200_80141C7C[3];
extern PadScriptVibrationSegment D_actor_403200_80141C88[2];

/// Pair descriptors the host and its escorts publish as `Enemy::param`;
/// `hpMax` is the hit-point pool each one starts with.
extern EnemyParams D_actor_403200_80141C00;
extern EnemyParams D_actor_403200_80141C20;
extern EnemyParams D_actor_403200_80141C30;
extern EnemyParams D_actor_403200_80141C40;

/// Per-animation reset argument, a `[?][0x2D]` table indexed by the id that
/// was playing before the switch and the id being switched to.
extern s8 gIncinBossAnimTransitions[][0x2D];

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern AnimationSet* D_actor_403200_8015E484[];
extern AnimationSet* D_actor_403200_8015E53C[];
extern AnimationSet* D_actor_403200_8015E5F4[];

/// Animation table the stand-up tick publishes to the player in its message
/// 0x3FF, and the one handed over when the placement yaw is outside +/-0x400.
extern AnimationSet* D_actor_403200_8015E6AC[];
extern AnimationSet* D_actor_403200_8015E6CC[];

/// Drop-point group the falling enemies use this round, rerolled whenever a
/// spawn arrives with `spawnArg1` 0.
extern u8 gIncinBossRainGroup;

/// Animation-set table the grab states send the player as message 0x3FF;
/// entry 2 is refreshed from the player's own weapon block.
extern AnimationSet* gIncinBossCaughtAnimSets[];

/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_403200_8015E72C[];

/// Per-`spawnArg1` offset from the host model to the point the falling enemy
/// is stood up at.
extern SVECTOR gIncinBossRainLaunchOffsets[];
/// The drop points: `vz` is added to the ring x coordinate and `vx` (less
/// 0x189C) becomes the z coordinate.
extern SVECTOR gIncinBossRainPoints[];
/// `[group][spawnArg1]` index into `gIncinBossRainPoints`.
extern u8 gIncinBossRainPointIndex[][8];

/// Enemy spawn table the three launch states of `func_actor_403200_8013D9EC`
/// draw from.
extern TaskDesc D_actor_403200_8015E858[];

/// The enemy task's message-handler table, parked in `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(void);
        s32  (*call1)(Task*);
        s32  (*call2)(Task*, s32, ActorCommand* request);
        s32  (*call3)(Task*, s32, ActorTransform*);
        s32  (*call4)(Task*, s32, s32);
        void (*call5)(void);
    } handler;
} Actor403200MessageEntry;
STATIC_ASSERT_SIZEOF(Actor403200MessageEntry, 8);

extern Actor403200MessageEntry D_actor_403200_8015F770[8];

/// Three formations of nine positions, and each member's model/spawn argument.
extern SVECTOR             D_actor_403200_8015F7B0[3][9];
extern Actor403200SpawnRec D_actor_403200_8015F888[9];

/// Non-zero once the launch state has published the enemy's position to the
/// player, and cleared again when it restarts.

/// The enemy task itself, published for the overlay's other code.
extern Task* D_actor_403200_8015F8F0;

/// Shared 0x7DA payload buffer.
extern ActorCommand D_actor_403200_8015F8F4;

/// View-space point the launch tick clears and fills from the host's fourth
/// model part on a state change; the spinner enemies home on it.
extern SVECTOR gIncinBossSpinnerTarget;

/// Reply buffers the rise state and the stand-up tick pass with their message
/// 0x3F8.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    GpDelayArg value;
    u8         retained[8];
} Actor403200StorageF900;
STATIC_ASSERT_SIZEOF(Actor403200StorageF900, 32);

extern Actor403200StorageF900 gIncinBossGrabQuery;

/// The scratch coordinate the debris effect of `func_actor_403200_8013DC3C` is
/// built on: `F920` is the whole `GfxCoord` and `F924` its `coord` matrix,
/// which splat names separately because the code takes that address directly.
extern GfxCoord D_actor_403200_8015F920;

extern Actor403200DropCoord D_actor_403200_8015F970;

/// Position and Euler rotation the launch tick sends the player as message
/// 0x3E9.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    ActorTransform value;
    u8             retained[40];
} Actor403200StorageF9C0;
STATIC_ASSERT_SIZEOF(Actor403200StorageF9C0, 64);

extern Actor403200StorageF9C0 D_actor_403200_8015F9C0;

extern GpDelayArg D_actor_403200_8015FA00;

/// Handwritten overlay-local follow helper. `arg1`/`arg2` select the axis pair
/// and `arg3` the mode; takes the task, not the work block.
static void func_actor_403200_801408D8(Task* arg0, s16 arg1, s16 arg2, s16 arg3);

extern AnimationSet D_actor_403200_8014F7E0;
extern AnimationSet D_actor_403200_8014F88C;
extern AnimationSet D_actor_403200_8014F938;
extern AnimationSet D_actor_403200_8014FC20;
extern AnimationSet D_actor_403200_8014FEE4;
extern AnimationSet D_actor_403200_801501C0;
extern AnimationSet D_actor_403200_80150510;
extern AnimationSet D_actor_403200_801506F0;
extern AnimationSet D_actor_403200_801508D0;
extern AnimationSet D_actor_403200_80150BA8;
extern AnimationSet D_actor_403200_80150C80;
extern AnimationSet D_actor_403200_80150D5C;
extern AnimationSet D_actor_403200_80151144;
extern AnimationSet D_actor_403200_80151504;
extern AnimationSet D_actor_403200_80151818;
extern AnimationSet D_actor_403200_80151BFC;
extern AnimationSet D_actor_403200_80152054;
extern AnimationSet D_actor_403200_8015247C;
extern AnimationSet D_actor_403200_80152828;
extern AnimationSet D_actor_403200_80152B30;
extern AnimationSet D_actor_403200_80152E18;
extern AnimationSet D_actor_403200_80153080;
extern AnimationSet D_actor_403200_8015314C;
extern AnimationSet D_actor_403200_80153218;
extern AnimationSet D_actor_403200_801534F0;
extern AnimationSet D_actor_403200_80153734;
extern AnimationSet D_actor_403200_80153974;
extern AnimationSet D_actor_403200_80153B7C;
extern AnimationSet D_actor_403200_80153D08;
extern AnimationSet D_actor_403200_80153E9C;
extern AnimationSet D_actor_403200_801542D4;
extern AnimationSet D_actor_403200_80154618;
extern AnimationSet D_actor_403200_80154930;
extern AnimationSet D_actor_403200_80154C78;
extern AnimationSet D_actor_403200_80154FAC;
extern AnimationSet D_actor_403200_801552D8;
extern AnimationSet D_actor_403200_801557B8;
extern AnimationSet D_actor_403200_80155A4C;
extern AnimationSet D_actor_403200_80155D58;
extern AnimationSet D_actor_403200_80156E1C;
extern AnimationSet D_actor_403200_801579B8;
extern AnimationSet D_actor_403200_80157AC0;
extern AnimationSet D_actor_403200_80157BA0;
extern AnimationSet D_actor_403200_80157C80;
extern AnimationSet D_actor_403200_80157D88;
extern AnimationSet D_actor_403200_80157E50;
extern AnimationSet D_actor_403200_80157F18;
extern AnimationSet D_actor_403200_80158010;
extern AnimationSet D_actor_403200_801580D8;
extern AnimationSet D_actor_403200_801581A0;
extern AnimationSet D_actor_403200_801588D8;
extern AnimationSet D_actor_403200_80159D24;
extern AnimationSet D_actor_403200_8015B1F8;
extern AnimationSet D_actor_403200_8015B990;
extern AnimationSet D_actor_403200_8015C214;
extern AnimationSet D_actor_403200_8015CAA0;
extern AnimationSet D_actor_403200_8015CB80;
extern AnimationSet D_actor_403200_8015CC0C;
extern AnimationSet D_actor_403200_8015CC98;
extern AnimationSet D_actor_403200_8015DC70;

extern AnimationSet D_actor_403200_8015D464;
extern TmdSource    D_actor_403200_80144EF8;
extern TmdSource    D_actor_403200_8014607C;
extern TmdSource    D_actor_403200_80147838;
extern TmdSource    D_actor_403200_80148FF4;
extern TmdSource    D_actor_403200_8014A384;
extern TmdSource    D_actor_403200_8014A858;
extern TmdSource    D_actor_403200_8014ACB8;
extern TmdSource    D_actor_403200_8014B588;
extern TmdSource    D_actor_403200_8014C1AC;
extern TmdSource    D_actor_403200_8014E048;
s32                 func_actor_403200_801341E8(Task*, s16);
s32                 func_actor_403200_80134374(Task*, s16);
s32                 func_actor_403200_801344C4(Task*, s16);
s32                 func_actor_403200_80134748(Task*, s16);
s32                 func_actor_403200_80134900(Task*, s16);
s32                 func_actor_403200_80134A14(Task*, s16);
s32                 func_actor_403200_80141124(Task*, s16);
s32                 func_actor_403200_80141180(Task*, s16);
s32                 func_actor_403200_801411A8(Task*, s16);
void                func_actor_403200_80141430(Task*);
void                func_actor_403200_8014148C(Task*);
void                func_actor_403200_801414E8(Task*);
void                func_actor_403200_80141564(Task*);
void                func_actor_403200_80141670(Task*);
void                func_actor_403200_80141778(Task*);
void                func_actor_403200_80141868(Task*);

s32  func_actor_403200_80138468(Task*, s32, s32);
s32  func_actor_403200_80138748(Task*, s32, ActorCommand* msg);
s32  func_actor_403200_80141974(Task*);
s32  func_actor_403200_80141A94(Task*, s32, s32);
s32  func_actor_403200_80141B30(void);
void func_actor_403200_80140E6C(Task*);
void func_actor_403200_8014196C(void);

extern DamageAttack D_actor_403200_80141BE8[6];

DamageAttack D_actor_403200_80141BE8[6] = {
    { 0, 0 },
    { 30, 0 },
    { 25, 3 },
    { 9999, 0 },
    { 50, 11 },
    { 35, 0 },
};

EnemyParams D_actor_403200_80141C00 = { D_actor_403200_80141BE8, 3000, 500, 200, 100, 100, 0, 0, 0 };

EnemyParams D_actor_403200_80141C10 = { D_actor_403200_80141BE8, 3000, 700, 200, 100, 100, 0, 0, 0 };

EnemyParams D_actor_403200_80141C20 = { D_actor_403200_80141BE8, 120, 0, 0, 0, 50, 0, 0, 0 };

EnemyParams D_actor_403200_80141C30 = { D_actor_403200_80141BE8, 120, 0, 0, 0, 50, 0, 0, 0 };

EnemyParams D_actor_403200_80141C40 = { D_actor_403200_80141BE8, 200, 0, 0, 0, 10, 0, 0, 0 };

s16 gIncinBossEnded = 0;

s32 gIncinBossGrabActive = 0;

s16 gIncinBossLimbReach = 0;

s16 gIncinBossSpinnersReleased = 0;

PadScriptCmd D_actor_403200_80141C5C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_403200_80141C64[2] = {
    { 0, 0, 7, 0 },
    { 255, 53, 27, 1 },
};

PadScriptCmd D_actor_403200_80141C6C[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_403200_80141C74[2] = {
    { 186, 74, 32, 1 },
    { 0, 0, 7, 0 },
};

PadScriptCmd D_actor_403200_80141C7C[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_403200_80141C88[2] = {
    { 22, 109, 20, 1 },
    { 80, 31, 34, 1 },
};

TmdBone D_actor_403200_80141C90[8] = {
#include "assets/actor_403200_model_12330_skeleton.inc"
};

u32 D_actor_403200_80141DB0[8] = {
#include "assets/actor_403200_model_12330_partVerts.inc"
};

SVECTOR D_actor_403200_80141DD0[135] = {
#include "assets/actor_403200_model_12330_verts.inc"
};

SVECTOR D_actor_403200_80142208[135] = {
#include "assets/actor_403200_model_12330_normals.inc"
};

u32 D_actor_403200_80142640[1732] = {
#include "assets/actor_403200_model_12330_stream.inc"
};

TmdSource D_actor_403200_80144150 = {
    0,
    8496,
    3588,
    8,
    D_actor_403200_80141DB0,
    D_actor_403200_80141DD0,
    D_actor_403200_80142208,
    D_actor_403200_80141C90,
    D_actor_403200_80142640,
};

TmdBone D_actor_403200_80144174[1] = {
#include "assets/actor_403200_model_130D8_skeleton.inc"
};

u32 D_actor_403200_80144198[1] = {
#include "assets/actor_403200_model_130D8_partVerts.inc"
};

SVECTOR D_actor_403200_8014419C[82] = {
#include "assets/actor_403200_model_130D8_verts.inc"
};

SVECTOR D_actor_403200_8014442C[79] = {
#include "assets/actor_403200_model_130D8_normals.inc"
};

u32 D_actor_403200_801446A4[533] = {
#include "assets/actor_403200_model_130D8_stream.inc"
};

TmdSource D_actor_403200_80144EF8 = {
    0,
    3696,
    0,
    1,
    D_actor_403200_80144198,
    D_actor_403200_8014419C,
    D_actor_403200_8014442C,
    D_actor_403200_80144174,
    D_actor_403200_801446A4,
};

TmdBone D_actor_403200_80144F1C[1] = {
#include "assets/actor_403200_model_1425C_skeleton.inc"
};

u32 D_actor_403200_80144F40[1] = {
#include "assets/actor_403200_model_1425C_partVerts.inc"
};

SVECTOR D_actor_403200_80144F44[90] = {
#include "assets/actor_403200_model_1425C_verts.inc"
};

SVECTOR D_actor_403200_80145214[112] = {
#include "assets/actor_403200_model_1425C_normals.inc"
};

u32 D_actor_403200_80145594[698] = {
#include "assets/actor_403200_model_1425C_stream.inc"
};

TmdSource D_actor_403200_8014607C = {
    0,
    4808,
    0,
    1,
    D_actor_403200_80144F40,
    D_actor_403200_80144F44,
    D_actor_403200_80145214,
    D_actor_403200_80144F1C,
    D_actor_403200_80145594,
};

TmdBone D_actor_403200_801460A0[4] = {
#include "assets/actor_403200_model_15A18_skeleton.inc"
};

u32 D_actor_403200_80146130[4] = {
#include "assets/actor_403200_model_15A18_partVerts.inc"
};

SVECTOR D_actor_403200_80146140[104] = {
#include "assets/actor_403200_model_15A18_verts.inc"
};

SVECTOR D_actor_403200_80146480[115] = {
#include "assets/actor_403200_model_15A18_normals.inc"
};

u32 D_actor_403200_80146818[1032] = {
#include "assets/actor_403200_model_15A18_stream.inc"
};

TmdSource D_actor_403200_80147838 = {
    0,
    6172,
    1048,
    4,
    D_actor_403200_80146130,
    D_actor_403200_80146140,
    D_actor_403200_80146480,
    D_actor_403200_801460A0,
    D_actor_403200_80146818,
};

TmdBone D_actor_403200_8014785C[4] = {
#include "assets/actor_403200_model_171D4_skeleton.inc"
};

u32 D_actor_403200_801478EC[4] = {
#include "assets/actor_403200_model_171D4_partVerts.inc"
};

SVECTOR D_actor_403200_801478FC[104] = {
#include "assets/actor_403200_model_171D4_verts.inc"
};

SVECTOR D_actor_403200_80147C3C[115] = {
#include "assets/actor_403200_model_171D4_normals.inc"
};

u32 D_actor_403200_80147FD4[1032] = {
#include "assets/actor_403200_model_171D4_stream.inc"
};

TmdSource D_actor_403200_80148FF4 = {
    0,
    6172,
    1048,
    4,
    D_actor_403200_801478EC,
    D_actor_403200_801478FC,
    D_actor_403200_80147C3C,
    D_actor_403200_8014785C,
    D_actor_403200_80147FD4,
};

TmdBone D_actor_403200_80149018[8] = {
#include "assets/actor_403200_model_18564_skeleton.inc"
};

u32 D_actor_403200_80149138[8] = {
#include "assets/actor_403200_model_18564_partVerts.inc"
};

SVECTOR D_actor_403200_80149158[82] = {
#include "assets/actor_403200_model_18564_verts.inc"
};

SVECTOR D_actor_403200_801493E8[82] = {
#include "assets/actor_403200_model_18564_normals.inc"
};

u32 D_actor_403200_80149678[835] = {
#include "assets/actor_403200_model_18564_stream.inc"
};

TmdSource D_actor_403200_8014A384 = {
    0,
    4140,
    1872,
    8,
    D_actor_403200_80149138,
    D_actor_403200_80149158,
    D_actor_403200_801493E8,
    D_actor_403200_80149018,
    D_actor_403200_80149678,
};

TmdBone D_actor_403200_8014A3A8[1] = {
#include "assets/actor_403200_model_18A38_skeleton.inc"
};

u32 D_actor_403200_8014A3CC[1] = {
#include "assets/actor_403200_model_18A38_partVerts.inc"
};

SVECTOR D_actor_403200_8014A3D0[36] = {
#include "assets/actor_403200_model_18A38_verts.inc"
};

SVECTOR D_actor_403200_8014A4F0[1] = {
#include "assets/actor_403200_model_18A38_normals.inc"
};

u32 D_actor_403200_8014A4F8[216] = {
#include "assets/actor_403200_model_18A38_stream.inc"
};

TmdSource D_actor_403200_8014A858 = {
    0,
    1560,
    0,
    1,
    D_actor_403200_8014A3CC,
    D_actor_403200_8014A3D0,
    D_actor_403200_8014A4F0,
    D_actor_403200_8014A3A8,
    D_actor_403200_8014A4F8,
};

TmdBone D_actor_403200_8014A87C[1] = {
#include "assets/actor_403200_model_18E98_skeleton.inc"
};

u32 D_actor_403200_8014A8A0[1] = {
#include "assets/actor_403200_model_18E98_partVerts.inc"
};

SVECTOR D_actor_403200_8014A8A4[22] = {
#include "assets/actor_403200_model_18E98_verts.inc"
};

SVECTOR D_actor_403200_8014A954[22] = {
#include "assets/actor_403200_model_18E98_normals.inc"
};

u32 D_actor_403200_8014AA04[173] = {
#include "assets/actor_403200_model_18E98_stream.inc"
};

TmdSource D_actor_403200_8014ACB8 = {
    0,
    1136,
    0,
    1,
    D_actor_403200_8014A8A0,
    D_actor_403200_8014A8A4,
    D_actor_403200_8014A954,
    D_actor_403200_8014A87C,
    D_actor_403200_8014AA04,
};

TmdBone D_actor_403200_8014ACDC[1] = {
#include "assets/actor_403200_model_19768_skeleton.inc"
};

u32 D_actor_403200_8014AD00[1] = {
#include "assets/actor_403200_model_19768_partVerts.inc"
};

SVECTOR D_actor_403200_8014AD04[58] = {
#include "assets/actor_403200_model_19768_verts.inc"
};

SVECTOR D_actor_403200_8014AED4[58] = {
#include "assets/actor_403200_model_19768_normals.inc"
};

u32 D_actor_403200_8014B0A4[313] = {
#include "assets/actor_403200_model_19768_stream.inc"
};

TmdSource D_actor_403200_8014B588 = {
    0,
    2176,
    0,
    1,
    D_actor_403200_8014AD00,
    D_actor_403200_8014AD04,
    D_actor_403200_8014AED4,
    D_actor_403200_8014ACDC,
    D_actor_403200_8014B0A4,
};

TmdBone D_actor_403200_8014B5AC[1] = {
#include "assets/actor_403200_model_1A38C_skeleton.inc"
};

u32 D_actor_403200_8014B5D0[1] = {
#include "assets/actor_403200_model_1A38C_partVerts.inc"
};

SVECTOR D_actor_403200_8014B5D4[70] = {
#include "assets/actor_403200_model_1A38C_verts.inc"
};

u32 D_actor_403200_8014B804[618] = {
#include "assets/actor_403200_model_1A38C_stream.inc"
};

TmdSource D_actor_403200_8014C1AC = {
    0,
    3536,
    0,
    1,
    D_actor_403200_8014B5D0,
    D_actor_403200_8014B5D4,
    &D_actor_403200_8014B5D4[70],
    D_actor_403200_8014B5AC,
    D_actor_403200_8014B804,
};

TmdBone D_actor_403200_8014C1D0[1] = {
#include "assets/actor_403200_model_1C228_skeleton.inc"
};

u32 D_actor_403200_8014C1F4[1] = {
#include "assets/actor_403200_model_1C228_partVerts.inc"
};

SVECTOR D_actor_403200_8014C1F8[135] = {
#include "assets/actor_403200_model_1C228_verts.inc"
};

SVECTOR D_actor_403200_8014C630[135] = {
#include "assets/actor_403200_model_1C228_normals.inc"
};

u32 D_actor_403200_8014CA68[1400] = {
#include "assets/actor_403200_model_1C228_stream.inc"
};

TmdSource D_actor_403200_8014E048 = {
    0,
    9492,
    0,
    1,
    D_actor_403200_8014C1F4,
    D_actor_403200_8014C1F8,
    D_actor_403200_8014C630,
    D_actor_403200_8014C1D0,
    D_actor_403200_8014CA68,
};

TmdBone D_actor_403200_8014E06C[1] = {
#include "assets/actor_403200_model_1C80C_skeleton.inc"
};

u32 D_actor_403200_8014E090[1] = {
#include "assets/actor_403200_model_1C80C_partVerts.inc"
};

SVECTOR D_actor_403200_8014E094[32] = {
#include "assets/actor_403200_model_1C80C_verts.inc"
};

SVECTOR D_actor_403200_8014E194[32] = {
#include "assets/actor_403200_model_1C80C_normals.inc"
};

u32 D_actor_403200_8014E294[230] = {
#include "assets/actor_403200_model_1C80C_stream.inc"
};

TmdSource D_actor_403200_8014E62C = {
    0,
    1664,
    0,
    1,
    D_actor_403200_8014E090,
    D_actor_403200_8014E094,
    D_actor_403200_8014E194,
    D_actor_403200_8014E06C,
    D_actor_403200_8014E294,
};

TmdBone D_actor_403200_8014E650[1] = {
#include "assets/actor_403200_model_1CCD8_skeleton.inc"
};

u32 D_actor_403200_8014E674[1] = {
#include "assets/actor_403200_model_1CCD8_partVerts.inc"
};

SVECTOR D_actor_403200_8014E678[34] = {
#include "assets/actor_403200_model_1CCD8_verts.inc"
};

SVECTOR D_actor_403200_8014E788[22] = {
#include "assets/actor_403200_model_1CCD8_normals.inc"
};

u32 D_actor_403200_8014E838[176] = {
#include "assets/actor_403200_model_1CCD8_stream.inc"
};

TmdSource D_actor_403200_8014EAF8 = {
    0,
    1228,
    0,
    1,
    D_actor_403200_8014E674,
    D_actor_403200_8014E678,
    D_actor_403200_8014E788,
    D_actor_403200_8014E650,
    D_actor_403200_8014E838,
};

TmdBone D_actor_403200_8014EB1C[1] = {
#include "assets/actor_403200_model_1CE64_skeleton.inc"
};

u32 D_actor_403200_8014EB40[1] = {
#include "assets/actor_403200_model_1CE64_partVerts.inc"
};

SVECTOR D_actor_403200_8014EB44[8] = {
#include "assets/actor_403200_model_1CE64_verts.inc"
};

SVECTOR D_actor_403200_8014EB84[8] = {
#include "assets/actor_403200_model_1CE64_normals.inc"
};

u32 D_actor_403200_8014EBC4[48] = {
#include "assets/actor_403200_model_1CE64_stream.inc"
};

TmdSource D_actor_403200_8014EC84 = {
    0,
    312,
    0,
    1,
    D_actor_403200_8014EB40,
    D_actor_403200_8014EB44,
    D_actor_403200_8014EB84,
    D_actor_403200_8014EB1C,
    D_actor_403200_8014EBC4,
};

TmdBone D_actor_403200_8014ECA8[1] = {
#include "assets/actor_403200_model_1D1B0_skeleton.inc"
};

u32 D_actor_403200_8014ECCC[1] = {
#include "assets/actor_403200_model_1D1B0_partVerts.inc"
};

SVECTOR D_actor_403200_8014ECD0[16] = {
#include "assets/actor_403200_model_1D1B0_verts.inc"
};

SVECTOR D_actor_403200_8014ED50[14] = {
#include "assets/actor_403200_model_1D1B0_normals.inc"
};

u32 D_actor_403200_8014EDC0[132] = {
#include "assets/actor_403200_model_1D1B0_stream.inc"
};

TmdSource D_actor_403200_8014EFD0 = {
    0,
    936,
    0,
    1,
    D_actor_403200_8014ECCC,
    D_actor_403200_8014ECD0,
    D_actor_403200_8014ED50,
    D_actor_403200_8014ECA8,
    D_actor_403200_8014EDC0,
};

TmdBone D_actor_403200_8014EFF4[1] = {
#include "assets/actor_403200_model_1D32C_skeleton.inc"
};

u32 D_actor_403200_8014F018[1] = {
#include "assets/actor_403200_model_1D32C_partVerts.inc"
};

SVECTOR D_actor_403200_8014F01C[8] = {
#include "assets/actor_403200_model_1D32C_verts.inc"
};

SVECTOR D_actor_403200_8014F05C[6] = {
#include "assets/actor_403200_model_1D32C_normals.inc"
};

u32 D_actor_403200_8014F08C[48] = {
#include "assets/actor_403200_model_1D32C_stream.inc"
};

TmdSource D_actor_403200_8014F14C = {
    0,
    312,
    0,
    1,
    D_actor_403200_8014F018,
    D_actor_403200_8014F01C,
    D_actor_403200_8014F05C,
    D_actor_403200_8014EFF4,
    D_actor_403200_8014F08C,
};

TmdBone D_actor_403200_8014F170[1] = {
#include "assets/actor_403200_model_1D4A8_skeleton.inc"
};

u32 D_actor_403200_8014F194[1] = {
#include "assets/actor_403200_model_1D4A8_partVerts.inc"
};

SVECTOR D_actor_403200_8014F198[8] = {
#include "assets/actor_403200_model_1D4A8_verts.inc"
};

SVECTOR D_actor_403200_8014F1D8[6] = {
#include "assets/actor_403200_model_1D4A8_normals.inc"
};

u32 D_actor_403200_8014F208[48] = {
#include "assets/actor_403200_model_1D4A8_stream.inc"
};

TmdSource D_actor_403200_8014F2C8 = {
    0,
    312,
    0,
    1,
    D_actor_403200_8014F194,
    D_actor_403200_8014F198,
    D_actor_403200_8014F1D8,
    D_actor_403200_8014F170,
    D_actor_403200_8014F208,
};

TmdBone D_actor_403200_8014F2EC[1] = {
#include "assets/actor_403200_model_1D56C_skeleton.inc"
};

u32 D_actor_403200_8014F310[1] = {
#include "assets/actor_403200_model_1D56C_partVerts.inc"
};

SVECTOR D_actor_403200_8014F314[4] = {
#include "assets/actor_403200_model_1D56C_verts.inc"
};

SVECTOR D_actor_403200_8014F334[2] = {
#include "assets/actor_403200_model_1D56C_normals.inc"
};

u32 D_actor_403200_8014F344[18] = {
#include "assets/actor_403200_model_1D56C_stream.inc"
};

TmdSource D_actor_403200_8014F38C = {
    0,
    104,
    0,
    1,
    D_actor_403200_8014F310,
    D_actor_403200_8014F314,
    D_actor_403200_8014F334,
    D_actor_403200_8014F2EC,
    D_actor_403200_8014F344,
};

TmdBone D_actor_403200_8014F3B0[1] = {
#include "assets/actor_403200_model_1D630_skeleton.inc"
};

u32 D_actor_403200_8014F3D4[1] = {
#include "assets/actor_403200_model_1D630_partVerts.inc"
};

SVECTOR D_actor_403200_8014F3D8[4] = {
#include "assets/actor_403200_model_1D630_verts.inc"
};

SVECTOR D_actor_403200_8014F3F8[2] = {
#include "assets/actor_403200_model_1D630_normals.inc"
};

u32 D_actor_403200_8014F408[18] = {
#include "assets/actor_403200_model_1D630_stream.inc"
};

TmdSource D_actor_403200_8014F450 = {
    0,
    104,
    0,
    1,
    D_actor_403200_8014F3D4,
    D_actor_403200_8014F3D8,
    D_actor_403200_8014F3F8,
    D_actor_403200_8014F3B0,
    D_actor_403200_8014F408,
};

TmdBone D_actor_403200_8014F474[1] = {
#include "assets/actor_403200_model_1D8A8_skeleton.inc"
};

u32 D_actor_403200_8014F498[1] = {
#include "assets/actor_403200_model_1D8A8_partVerts.inc"
};

SVECTOR D_actor_403200_8014F49C[9] = {
#include "assets/actor_403200_model_1D8A8_verts.inc"
};

SVECTOR D_actor_403200_8014F4E4[18] = {
#include "assets/actor_403200_model_1D8A8_normals.inc"
};

u32 D_actor_403200_8014F574[85] = {
#include "assets/actor_403200_model_1D8A8_stream.inc"
};

TmdSource D_actor_403200_8014F6C8 = {
    0,
    528,
    0,
    1,
    D_actor_403200_8014F498,
    D_actor_403200_8014F49C,
    D_actor_403200_8014F4E4,
    D_actor_403200_8014F474,
    D_actor_403200_8014F574,
};

AnimationPackedPose D_actor_403200_8014F6EC[4] = {
#include "assets/actor_403200_animation_1D9C0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8014F71C[7] = {
#include "assets/actor_403200_animation_1D9C0_bank4.inc"
};

AnimationRecord D_actor_403200_8014F738[38] = {
#include "assets/actor_403200_animation_1D9C0_records.inc"
};

u16 D_actor_403200_8014F7D0[8] = {
#include "assets/actor_403200_animation_1D9C0_indices.inc"
};

AnimationSet D_actor_403200_8014F7E0 = {
    D_actor_403200_8014F738,
    D_actor_403200_8014F7D0,
    { NULL, D_actor_403200_8014F6EC, NULL, NULL, D_actor_403200_8014F71C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8014F808[4] = {
#include "assets/actor_403200_animation_1DA6C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8014F838[1] = {
#include "assets/actor_403200_animation_1DA6C_bank4.inc"
};

AnimationRecord D_actor_403200_8014F83C[18] = {
#include "assets/actor_403200_animation_1DA6C_records.inc"
};

u16 D_actor_403200_8014F884[4] = {
#include "assets/actor_403200_animation_1DA6C_indices.inc"
};

AnimationSet D_actor_403200_8014F88C = {
    D_actor_403200_8014F83C,
    D_actor_403200_8014F884,
    { NULL, D_actor_403200_8014F808, NULL, NULL, D_actor_403200_8014F838, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8014F8B4[4] = {
#include "assets/actor_403200_animation_1DB18_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8014F8E4[1] = {
#include "assets/actor_403200_animation_1DB18_bank4.inc"
};

AnimationRecord D_actor_403200_8014F8E8[18] = {
#include "assets/actor_403200_animation_1DB18_records.inc"
};

u16 D_actor_403200_8014F930[4] = {
#include "assets/actor_403200_animation_1DB18_indices.inc"
};

AnimationSet D_actor_403200_8014F938 = {
    D_actor_403200_8014F8E8,
    D_actor_403200_8014F930,
    { NULL, D_actor_403200_8014F8B4, NULL, NULL, D_actor_403200_8014F8E4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8014F960[7] = {
#include "assets/actor_403200_animation_1DE00_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8014F9B4[49] = {
#include "assets/actor_403200_animation_1DE00_bank4.inc"
};

AnimationRecord D_actor_403200_8014FA78[102] = {
#include "assets/actor_403200_animation_1DE00_records.inc"
};

u16 D_actor_403200_8014FC10[8] = {
#include "assets/actor_403200_animation_1DE00_indices.inc"
};

AnimationSet D_actor_403200_8014FC20 = {
    D_actor_403200_8014FA78,
    D_actor_403200_8014FC10,
    { NULL, D_actor_403200_8014F960, NULL, NULL, D_actor_403200_8014F9B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8014FC48[29] = {
#include "assets/actor_403200_animation_1E0C4_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8014FDA4[17] = {
#include "assets/actor_403200_animation_1E0C4_bank4.inc"
};

AnimationRecord D_actor_403200_8014FDE8[61] = {
#include "assets/actor_403200_animation_1E0C4_records.inc"
};

u16 D_actor_403200_8014FEDC[4] = {
#include "assets/actor_403200_animation_1E0C4_indices.inc"
};

AnimationSet D_actor_403200_8014FEE4 = {
    D_actor_403200_8014FDE8,
    D_actor_403200_8014FEDC,
    { NULL, D_actor_403200_8014FC48, NULL, NULL, D_actor_403200_8014FDA4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8014FF0C[30] = {
#include "assets/actor_403200_animation_1E3A0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80150074[18] = {
#include "assets/actor_403200_animation_1E3A0_bank4.inc"
};

AnimationRecord D_actor_403200_801500BC[63] = {
#include "assets/actor_403200_animation_1E3A0_records.inc"
};

u16 D_actor_403200_801501B8[4] = {
#include "assets/actor_403200_animation_1E3A0_indices.inc"
};

AnimationSet D_actor_403200_801501C0 = {
    D_actor_403200_801500BC,
    D_actor_403200_801501B8,
    { NULL, D_actor_403200_8014FF0C, NULL, NULL, D_actor_403200_80150074, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801501E8[20] = {
#include "assets/actor_403200_animation_1E6F0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801502D8[48] = {
#include "assets/actor_403200_animation_1E6F0_bank4.inc"
};

AnimationRecord D_actor_403200_80150398[90] = {
#include "assets/actor_403200_animation_1E6F0_records.inc"
};

u16 D_actor_403200_80150500[8] = {
#include "assets/actor_403200_animation_1E6F0_indices.inc"
};

AnimationSet D_actor_403200_80150510 = {
    D_actor_403200_80150398,
    D_actor_403200_80150500,
    { NULL, D_actor_403200_801501E8, NULL, NULL, D_actor_403200_801502D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80150538[18] = {
#include "assets/actor_403200_animation_1E8D0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80150610[13] = {
#include "assets/actor_403200_animation_1E8D0_bank4.inc"
};

AnimationRecord D_actor_403200_80150644[41] = {
#include "assets/actor_403200_animation_1E8D0_records.inc"
};

u16 D_actor_403200_801506E8[4] = {
#include "assets/actor_403200_animation_1E8D0_indices.inc"
};

AnimationSet D_actor_403200_801506F0 = {
    D_actor_403200_80150644,
    D_actor_403200_801506E8,
    { NULL, D_actor_403200_80150538, NULL, NULL, D_actor_403200_80150610, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80150718[18] = {
#include "assets/actor_403200_animation_1EAB0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801507F0[13] = {
#include "assets/actor_403200_animation_1EAB0_bank4.inc"
};

AnimationRecord D_actor_403200_80150824[41] = {
#include "assets/actor_403200_animation_1EAB0_records.inc"
};

u16 D_actor_403200_801508C8[4] = {
#include "assets/actor_403200_animation_1EAB0_indices.inc"
};

AnimationSet D_actor_403200_801508D0 = {
    D_actor_403200_80150824,
    D_actor_403200_801508C8,
    { NULL, D_actor_403200_80150718, NULL, NULL, D_actor_403200_801507F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801508F8[12] = {
#include "assets/actor_403200_animation_1ED88_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80150988[51] = {
#include "assets/actor_403200_animation_1ED88_bank4.inc"
};

AnimationRecord D_actor_403200_80150A54[81] = {
#include "assets/actor_403200_animation_1ED88_records.inc"
};

u16 D_actor_403200_80150B98[8] = {
#include "assets/actor_403200_animation_1ED88_indices.inc"
};

AnimationSet D_actor_403200_80150BA8 = {
    D_actor_403200_80150A54,
    D_actor_403200_80150B98,
    { NULL, D_actor_403200_801508F8, NULL, NULL, D_actor_403200_80150988, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80150BD0[6] = {
#include "assets/actor_403200_animation_1EE60_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80150C18[4] = {
#include "assets/actor_403200_animation_1EE60_bank4.inc"
};

AnimationRecord D_actor_403200_80150C28[20] = {
#include "assets/actor_403200_animation_1EE60_records.inc"
};

u16 D_actor_403200_80150C78[4] = {
#include "assets/actor_403200_animation_1EE60_indices.inc"
};

AnimationSet D_actor_403200_80150C80 = {
    D_actor_403200_80150C28,
    D_actor_403200_80150C78,
    { NULL, D_actor_403200_80150BD0, NULL, NULL, D_actor_403200_80150C18, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80150CA8[6] = {
#include "assets/actor_403200_animation_1EF3C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80150CF0[5] = {
#include "assets/actor_403200_animation_1EF3C_bank4.inc"
};

AnimationRecord D_actor_403200_80150D04[20] = {
#include "assets/actor_403200_animation_1EF3C_records.inc"
};

u16 D_actor_403200_80150D54[4] = {
#include "assets/actor_403200_animation_1EF3C_indices.inc"
};

AnimationSet D_actor_403200_80150D5C = {
    D_actor_403200_80150D04,
    D_actor_403200_80150D54,
    { NULL, D_actor_403200_80150CA8, NULL, NULL, D_actor_403200_80150CF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80150D84[18] = {
#include "assets/actor_403200_animation_1F324_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80150E5C[72] = {
#include "assets/actor_403200_animation_1F324_bank4.inc"
};

AnimationRecord D_actor_403200_80150F7C[110] = {
#include "assets/actor_403200_animation_1F324_records.inc"
};

u16 D_actor_403200_80151134[8] = {
#include "assets/actor_403200_animation_1F324_indices.inc"
};

AnimationSet D_actor_403200_80151144 = {
    D_actor_403200_80150F7C,
    D_actor_403200_80151134,
    { NULL, D_actor_403200_80150D84, NULL, NULL, D_actor_403200_80150E5C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015116C[39] = {
#include "assets/actor_403200_animation_1F6E4_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80151340[32] = {
#include "assets/actor_403200_animation_1F6E4_bank4.inc"
};

AnimationRecord D_actor_403200_801513C0[79] = {
#include "assets/actor_403200_animation_1F6E4_records.inc"
};

u16 D_actor_403200_801514FC[4] = {
#include "assets/actor_403200_animation_1F6E4_indices.inc"
};

AnimationSet D_actor_403200_80151504 = {
    D_actor_403200_801513C0,
    D_actor_403200_801514FC,
    { NULL, D_actor_403200_8015116C, NULL, NULL, D_actor_403200_80151340, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015152C[32] = {
#include "assets/actor_403200_animation_1F9F8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801516AC[23] = {
#include "assets/actor_403200_animation_1F9F8_bank4.inc"
};

AnimationRecord D_actor_403200_80151708[66] = {
#include "assets/actor_403200_animation_1F9F8_records.inc"
};

u16 D_actor_403200_80151810[4] = {
#include "assets/actor_403200_animation_1F9F8_indices.inc"
};

AnimationSet D_actor_403200_80151818 = {
    D_actor_403200_80151708,
    D_actor_403200_80151810,
    { NULL, D_actor_403200_8015152C, NULL, NULL, D_actor_403200_801516AC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80151840[11] = {
#include "assets/actor_403200_animation_1FDDC_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801518C4[78] = {
#include "assets/actor_403200_animation_1FDDC_bank4.inc"
};

AnimationRecord D_actor_403200_801519FC[124] = {
#include "assets/actor_403200_animation_1FDDC_records.inc"
};

u16 D_actor_403200_80151BEC[8] = {
#include "assets/actor_403200_animation_1FDDC_indices.inc"
};

AnimationSet D_actor_403200_80151BFC = {
    D_actor_403200_801519FC,
    D_actor_403200_80151BEC,
    { NULL, D_actor_403200_80151840, NULL, NULL, D_actor_403200_801518C4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80151C24[47] = {
#include "assets/actor_403200_animation_20234_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80151E58[30] = {
#include "assets/actor_403200_animation_20234_bank4.inc"
};

AnimationRecord D_actor_403200_80151ED0[95] = {
#include "assets/actor_403200_animation_20234_records.inc"
};

u16 D_actor_403200_8015204C[4] = {
#include "assets/actor_403200_animation_20234_indices.inc"
};

AnimationSet D_actor_403200_80152054 = {
    D_actor_403200_80151ED0,
    D_actor_403200_8015204C,
    { NULL, D_actor_403200_80151C24, NULL, NULL, D_actor_403200_80151E58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015207C[46] = {
#include "assets/actor_403200_animation_2065C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801522A4[27] = {
#include "assets/actor_403200_animation_2065C_bank4.inc"
};

AnimationRecord D_actor_403200_80152310[89] = {
#include "assets/actor_403200_animation_2065C_records.inc"
};

u16 D_actor_403200_80152474[4] = {
#include "assets/actor_403200_animation_2065C_indices.inc"
};

AnimationSet D_actor_403200_8015247C = {
    D_actor_403200_80152310,
    D_actor_403200_80152474,
    { NULL, D_actor_403200_8015207C, NULL, NULL, D_actor_403200_801522A4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801524A4[13] = {
#include "assets/actor_403200_animation_20A08_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80152540[77] = {
#include "assets/actor_403200_animation_20A08_bank4.inc"
};

AnimationRecord D_actor_403200_80152674[105] = {
#include "assets/actor_403200_animation_20A08_records.inc"
};

u16 D_actor_403200_80152818[8] = {
#include "assets/actor_403200_animation_20A08_indices.inc"
};

AnimationSet D_actor_403200_80152828 = {
    D_actor_403200_80152674,
    D_actor_403200_80152818,
    { NULL, D_actor_403200_801524A4, NULL, NULL, D_actor_403200_80152540, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80152850[33] = {
#include "assets/actor_403200_animation_20D10_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801529DC[21] = {
#include "assets/actor_403200_animation_20D10_bank4.inc"
};

AnimationRecord D_actor_403200_80152A30[62] = {
#include "assets/actor_403200_animation_20D10_records.inc"
};

u16 D_actor_403200_80152B28[4] = {
#include "assets/actor_403200_animation_20D10_indices.inc"
};

AnimationSet D_actor_403200_80152B30 = {
    D_actor_403200_80152A30,
    D_actor_403200_80152B28,
    { NULL, D_actor_403200_80152850, NULL, NULL, D_actor_403200_801529DC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80152B58[31] = {
#include "assets/actor_403200_animation_20FF8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80152CCC[21] = {
#include "assets/actor_403200_animation_20FF8_bank4.inc"
};

AnimationRecord D_actor_403200_80152D20[60] = {
#include "assets/actor_403200_animation_20FF8_records.inc"
};

u16 D_actor_403200_80152E10[4] = {
#include "assets/actor_403200_animation_20FF8_indices.inc"
};

AnimationSet D_actor_403200_80152E18 = {
    D_actor_403200_80152D20,
    D_actor_403200_80152E10,
    { NULL, D_actor_403200_80152B58, NULL, NULL, D_actor_403200_80152CCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80152E40[13] = {
#include "assets/actor_403200_animation_21260_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80152EDC[38] = {
#include "assets/actor_403200_animation_21260_bank4.inc"
};

AnimationRecord D_actor_403200_80152F74[63] = {
#include "assets/actor_403200_animation_21260_records.inc"
};

u16 D_actor_403200_80153070[8] = {
#include "assets/actor_403200_animation_21260_indices.inc"
};

AnimationSet D_actor_403200_80153080 = {
    D_actor_403200_80152F74,
    D_actor_403200_80153070,
    { NULL, D_actor_403200_80152E40, NULL, NULL, D_actor_403200_80152EDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801530A8[6] = {
#include "assets/actor_403200_animation_2132C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801530F0[5] = {
#include "assets/actor_403200_animation_2132C_bank4.inc"
};

AnimationRecord D_actor_403200_80153104[16] = {
#include "assets/actor_403200_animation_2132C_records.inc"
};

u16 D_actor_403200_80153144[4] = {
#include "assets/actor_403200_animation_2132C_indices.inc"
};

AnimationSet D_actor_403200_8015314C = {
    D_actor_403200_80153104,
    D_actor_403200_80153144,
    { NULL, D_actor_403200_801530A8, NULL, NULL, D_actor_403200_801530F0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80153174[6] = {
#include "assets/actor_403200_animation_213F8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801531BC[5] = {
#include "assets/actor_403200_animation_213F8_bank4.inc"
};

AnimationRecord D_actor_403200_801531D0[16] = {
#include "assets/actor_403200_animation_213F8_records.inc"
};

u16 D_actor_403200_80153210[4] = {
#include "assets/actor_403200_animation_213F8_indices.inc"
};

AnimationSet D_actor_403200_80153218 = {
    D_actor_403200_801531D0,
    D_actor_403200_80153210,
    { NULL, D_actor_403200_80153174, NULL, NULL, D_actor_403200_801531BC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80153240[8] = {
#include "assets/actor_403200_animation_216D0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801532A0[49] = {
#include "assets/actor_403200_animation_216D0_bank4.inc"
};

AnimationRecord D_actor_403200_80153364[95] = {
#include "assets/actor_403200_animation_216D0_records.inc"
};

u16 D_actor_403200_801534E0[8] = {
#include "assets/actor_403200_animation_216D0_indices.inc"
};

AnimationSet D_actor_403200_801534F0 = {
    D_actor_403200_80153364,
    D_actor_403200_801534E0,
    { NULL, D_actor_403200_80153240, NULL, NULL, D_actor_403200_801532A0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80153518[22] = {
#include "assets/actor_403200_animation_21914_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80153620[12] = {
#include "assets/actor_403200_animation_21914_bank4.inc"
};

AnimationRecord D_actor_403200_80153650[55] = {
#include "assets/actor_403200_animation_21914_records.inc"
};

u16 D_actor_403200_8015372C[4] = {
#include "assets/actor_403200_animation_21914_indices.inc"
};

AnimationSet D_actor_403200_80153734 = {
    D_actor_403200_80153650,
    D_actor_403200_8015372C,
    { NULL, D_actor_403200_80153518, NULL, NULL, D_actor_403200_80153620, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015375C[23] = {
#include "assets/actor_403200_animation_21B54_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80153870[10] = {
#include "assets/actor_403200_animation_21B54_bank4.inc"
};

AnimationRecord D_actor_403200_80153898[53] = {
#include "assets/actor_403200_animation_21B54_records.inc"
};

u16 D_actor_403200_8015396C[4] = {
#include "assets/actor_403200_animation_21B54_indices.inc"
};

AnimationSet D_actor_403200_80153974 = {
    D_actor_403200_80153898,
    D_actor_403200_8015396C,
    { NULL, D_actor_403200_8015375C, NULL, NULL, D_actor_403200_80153870, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015399C[9] = {
#include "assets/actor_403200_animation_21D5C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80153A08[33] = {
#include "assets/actor_403200_animation_21D5C_bank4.inc"
};

AnimationRecord D_actor_403200_80153A8C[56] = {
#include "assets/actor_403200_animation_21D5C_records.inc"
};

u16 D_actor_403200_80153B6C[8] = {
#include "assets/actor_403200_animation_21D5C_indices.inc"
};

AnimationSet D_actor_403200_80153B7C = {
    D_actor_403200_80153A8C,
    D_actor_403200_80153B6C,
    { NULL, D_actor_403200_8015399C, NULL, NULL, D_actor_403200_80153A08, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80153BA4[15] = {
#include "assets/actor_403200_animation_21EE8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80153C58[11] = {
#include "assets/actor_403200_animation_21EE8_bank4.inc"
};

AnimationRecord D_actor_403200_80153C84[31] = {
#include "assets/actor_403200_animation_21EE8_records.inc"
};

u16 D_actor_403200_80153D00[4] = {
#include "assets/actor_403200_animation_21EE8_indices.inc"
};

AnimationSet D_actor_403200_80153D08 = {
    D_actor_403200_80153C84,
    D_actor_403200_80153D00,
    { NULL, D_actor_403200_80153BA4, NULL, NULL, D_actor_403200_80153C58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80153D30[15] = {
#include "assets/actor_403200_animation_2207C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80153DE4[12] = {
#include "assets/actor_403200_animation_2207C_bank4.inc"
};

AnimationRecord D_actor_403200_80153E14[32] = {
#include "assets/actor_403200_animation_2207C_records.inc"
};

u16 D_actor_403200_80153E94[4] = {
#include "assets/actor_403200_animation_2207C_indices.inc"
};

AnimationSet D_actor_403200_80153E9C = {
    D_actor_403200_80153E14,
    D_actor_403200_80153E94,
    { NULL, D_actor_403200_80153D30, NULL, NULL, D_actor_403200_80153DE4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80153EC4[22] = {
#include "assets/actor_403200_animation_224B4_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80153FCC[73] = {
#include "assets/actor_403200_animation_224B4_bank4.inc"
};

AnimationRecord D_actor_403200_801540F0[117] = {
#include "assets/actor_403200_animation_224B4_records.inc"
};

u16 D_actor_403200_801542C4[8] = {
#include "assets/actor_403200_animation_224B4_indices.inc"
};

AnimationSet D_actor_403200_801542D4 = {
    D_actor_403200_801540F0,
    D_actor_403200_801542C4,
    { NULL, D_actor_403200_80153EC4, NULL, NULL, D_actor_403200_80153FCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801542FC[33] = {
#include "assets/actor_403200_animation_227F8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80154488[29] = {
#include "assets/actor_403200_animation_227F8_bank4.inc"
};

AnimationRecord D_actor_403200_801544FC[69] = {
#include "assets/actor_403200_animation_227F8_records.inc"
};

u16 D_actor_403200_80154610[4] = {
#include "assets/actor_403200_animation_227F8_indices.inc"
};

AnimationSet D_actor_403200_80154618 = {
    D_actor_403200_801544FC,
    D_actor_403200_80154610,
    { NULL, D_actor_403200_801542FC, NULL, NULL, D_actor_403200_80154488, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80154640[31] = {
#include "assets/actor_403200_animation_22B10_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801547B4[26] = {
#include "assets/actor_403200_animation_22B10_bank4.inc"
};

AnimationRecord D_actor_403200_8015481C[67] = {
#include "assets/actor_403200_animation_22B10_records.inc"
};

u16 D_actor_403200_80154928[4] = {
#include "assets/actor_403200_animation_22B10_indices.inc"
};

AnimationSet D_actor_403200_80154930 = {
    D_actor_403200_8015481C,
    D_actor_403200_80154928,
    { NULL, D_actor_403200_80154640, NULL, NULL, D_actor_403200_801547B4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80154958[5] = {
#include "assets/actor_403200_animation_22E58_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80154994[76] = {
#include "assets/actor_403200_animation_22E58_bank4.inc"
};

AnimationRecord D_actor_403200_80154AC4[105] = {
#include "assets/actor_403200_animation_22E58_records.inc"
};

u16 D_actor_403200_80154C68[8] = {
#include "assets/actor_403200_animation_22E58_indices.inc"
};

AnimationSet D_actor_403200_80154C78 = {
    D_actor_403200_80154AC4,
    D_actor_403200_80154C68,
    { NULL, D_actor_403200_80154958, NULL, NULL, D_actor_403200_80154994, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80154CA0[39] = {
#include "assets/actor_403200_animation_2318C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80154E74[14] = {
#include "assets/actor_403200_animation_2318C_bank4.inc"
};

AnimationRecord D_actor_403200_80154EAC[62] = {
#include "assets/actor_403200_animation_2318C_records.inc"
};

u16 D_actor_403200_80154FA4[4] = {
#include "assets/actor_403200_animation_2318C_indices.inc"
};

AnimationSet D_actor_403200_80154FAC = {
    D_actor_403200_80154EAC,
    D_actor_403200_80154FA4,
    { NULL, D_actor_403200_80154CA0, NULL, NULL, D_actor_403200_80154E74, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80154FD4[38] = {
#include "assets/actor_403200_animation_234B8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015519C[15] = {
#include "assets/actor_403200_animation_234B8_bank4.inc"
};

AnimationRecord D_actor_403200_801551D8[62] = {
#include "assets/actor_403200_animation_234B8_records.inc"
};

u16 D_actor_403200_801552D0[4] = {
#include "assets/actor_403200_animation_234B8_indices.inc"
};

AnimationSet D_actor_403200_801552D8 = {
    D_actor_403200_801551D8,
    D_actor_403200_801552D0,
    { NULL, D_actor_403200_80154FD4, NULL, NULL, D_actor_403200_8015519C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80155300[24] = {
#include "assets/actor_403200_animation_23998_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80155420[88] = {
#include "assets/actor_403200_animation_23998_bank4.inc"
};

AnimationRecord D_actor_403200_80155580[138] = {
#include "assets/actor_403200_animation_23998_records.inc"
};

u16 D_actor_403200_801557A8[8] = {
#include "assets/actor_403200_animation_23998_indices.inc"
};

AnimationSet D_actor_403200_801557B8 = {
    D_actor_403200_80155580,
    D_actor_403200_801557A8,
    { NULL, D_actor_403200_80155300, NULL, NULL, D_actor_403200_80155420, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801557E0[26] = {
#include "assets/actor_403200_animation_23C2C_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80155918[19] = {
#include "assets/actor_403200_animation_23C2C_bank4.inc"
};

AnimationRecord D_actor_403200_80155964[56] = {
#include "assets/actor_403200_animation_23C2C_records.inc"
};

u16 D_actor_403200_80155A44[4] = {
#include "assets/actor_403200_animation_23C2C_indices.inc"
};

AnimationSet D_actor_403200_80155A4C = {
    D_actor_403200_80155964,
    D_actor_403200_80155A44,
    { NULL, D_actor_403200_801557E0, NULL, NULL, D_actor_403200_80155918, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80155A74[30] = {
#include "assets/actor_403200_animation_23F38_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80155BDC[26] = {
#include "assets/actor_403200_animation_23F38_bank4.inc"
};

AnimationRecord D_actor_403200_80155C44[67] = {
#include "assets/actor_403200_animation_23F38_records.inc"
};

u16 D_actor_403200_80155D50[4] = {
#include "assets/actor_403200_animation_23F38_indices.inc"
};

AnimationSet D_actor_403200_80155D58 = {
    D_actor_403200_80155C44,
    D_actor_403200_80155D50,
    { NULL, D_actor_403200_80155A74, NULL, NULL, D_actor_403200_80155BDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80155D80[53] = {
#include "assets/actor_403200_animation_24FFC_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80155FFC[373] = {
#include "assets/actor_403200_animation_24FFC_bank4.inc"
};

AnimationRecord D_actor_403200_801565D0[521] = {
#include "assets/actor_403200_animation_24FFC_records.inc"
};

u16 D_actor_403200_80156DF4[20] = {
#include "assets/actor_403200_animation_24FFC_indices.inc"
};

AnimationSet D_actor_403200_80156E1C = {
    D_actor_403200_801565D0,
    D_actor_403200_80156DF4,
    { NULL, D_actor_403200_80155D80, NULL, NULL, D_actor_403200_80155FFC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80156E44[36] = {
#include "assets/actor_403200_animation_25B98_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80156FF4[255] = {
#include "assets/actor_403200_animation_25B98_bank4.inc"
};

AnimationRecord D_actor_403200_801573F0[360] = {
#include "assets/actor_403200_animation_25B98_records.inc"
};

u16 D_actor_403200_80157990[20] = {
#include "assets/actor_403200_animation_25B98_indices.inc"
};

AnimationSet D_actor_403200_801579B8 = {
    D_actor_403200_801573F0,
    D_actor_403200_80157990,
    { NULL, D_actor_403200_80156E44, NULL, NULL, D_actor_403200_80156FF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801579E0[4] = {
#include "assets/actor_403200_animation_25CA0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157A10[10] = {
#include "assets/actor_403200_animation_25CA0_bank4.inc"
};

AnimationRecord D_actor_403200_80157A38[30] = {
#include "assets/actor_403200_animation_25CA0_records.inc"
};

u16 D_actor_403200_80157AB0[8] = {
#include "assets/actor_403200_animation_25CA0_indices.inc"
};

AnimationSet D_actor_403200_80157AC0 = {
    D_actor_403200_80157A38,
    D_actor_403200_80157AB0,
    { NULL, D_actor_403200_801579E0, NULL, NULL, D_actor_403200_80157A10, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80157AE8[5] = {
#include "assets/actor_403200_animation_25D80_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157B24[8] = {
#include "assets/actor_403200_animation_25D80_bank4.inc"
};

AnimationRecord D_actor_403200_80157B44[21] = {
#include "assets/actor_403200_animation_25D80_records.inc"
};

u16 D_actor_403200_80157B98[4] = {
#include "assets/actor_403200_animation_25D80_indices.inc"
};

AnimationSet D_actor_403200_80157BA0 = {
    D_actor_403200_80157B44,
    D_actor_403200_80157B98,
    { NULL, D_actor_403200_80157AE8, NULL, NULL, D_actor_403200_80157B24, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80157BC8[5] = {
#include "assets/actor_403200_animation_25E60_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157C04[8] = {
#include "assets/actor_403200_animation_25E60_bank4.inc"
};

AnimationRecord D_actor_403200_80157C24[21] = {
#include "assets/actor_403200_animation_25E60_records.inc"
};

u16 D_actor_403200_80157C78[4] = {
#include "assets/actor_403200_animation_25E60_indices.inc"
};

AnimationSet D_actor_403200_80157C80 = {
    D_actor_403200_80157C24,
    D_actor_403200_80157C78,
    { NULL, D_actor_403200_80157BC8, NULL, NULL, D_actor_403200_80157C04, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80157CA8[6] = {
#include "assets/actor_403200_animation_25F68_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157CF0[3] = {
#include "assets/actor_403200_animation_25F68_bank4.inc"
};

AnimationRecord D_actor_403200_80157CFC[31] = {
#include "assets/actor_403200_animation_25F68_records.inc"
};

u16 D_actor_403200_80157D78[8] = {
#include "assets/actor_403200_animation_25F68_indices.inc"
};

AnimationSet D_actor_403200_80157D88 = {
    D_actor_403200_80157CFC,
    D_actor_403200_80157D78,
    { NULL, D_actor_403200_80157CA8, NULL, NULL, D_actor_403200_80157CF0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80157DB0[6] = {
#include "assets/actor_403200_animation_26030_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157DF8[1] = {
#include "assets/actor_403200_animation_26030_bank4.inc"
};

AnimationRecord D_actor_403200_80157DFC[19] = {
#include "assets/actor_403200_animation_26030_records.inc"
};

u16 D_actor_403200_80157E48[4] = {
#include "assets/actor_403200_animation_26030_indices.inc"
};

AnimationSet D_actor_403200_80157E50 = {
    D_actor_403200_80157DFC,
    D_actor_403200_80157E48,
    { NULL, D_actor_403200_80157DB0, NULL, NULL, D_actor_403200_80157DF8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80157E78[6] = {
#include "assets/actor_403200_animation_260F8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157EC0[1] = {
#include "assets/actor_403200_animation_260F8_bank4.inc"
};

AnimationRecord D_actor_403200_80157EC4[19] = {
#include "assets/actor_403200_animation_260F8_records.inc"
};

u16 D_actor_403200_80157F10[4] = {
#include "assets/actor_403200_animation_260F8_indices.inc"
};

AnimationSet D_actor_403200_80157F18 = {
    D_actor_403200_80157EC4,
    D_actor_403200_80157F10,
    { NULL, D_actor_403200_80157E78, NULL, NULL, D_actor_403200_80157EC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80157F40[2] = {
#include "assets/actor_403200_animation_261F0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80157F58[9] = {
#include "assets/actor_403200_animation_261F0_bank4.inc"
};

AnimationRecord D_actor_403200_80157F7C[33] = {
#include "assets/actor_403200_animation_261F0_records.inc"
};

u16 D_actor_403200_80158000[8] = {
#include "assets/actor_403200_animation_261F0_indices.inc"
};

AnimationSet D_actor_403200_80158010 = {
    D_actor_403200_80157F7C,
    D_actor_403200_80158000,
    { NULL, D_actor_403200_80157F40, NULL, NULL, D_actor_403200_80157F58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80158038[4] = {
#include "assets/actor_403200_animation_262B8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80158068[5] = {
#include "assets/actor_403200_animation_262B8_bank4.inc"
};

AnimationRecord D_actor_403200_8015807C[21] = {
#include "assets/actor_403200_animation_262B8_records.inc"
};

u16 D_actor_403200_801580D0[4] = {
#include "assets/actor_403200_animation_262B8_indices.inc"
};

AnimationSet D_actor_403200_801580D8 = {
    D_actor_403200_8015807C,
    D_actor_403200_801580D0,
    { NULL, D_actor_403200_80158038, NULL, NULL, D_actor_403200_80158068, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80158100[4] = {
#include "assets/actor_403200_animation_26380_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80158130[5] = {
#include "assets/actor_403200_animation_26380_bank4.inc"
};

AnimationRecord D_actor_403200_80158144[21] = {
#include "assets/actor_403200_animation_26380_records.inc"
};

u16 D_actor_403200_80158198[4] = {
#include "assets/actor_403200_animation_26380_indices.inc"
};

AnimationSet D_actor_403200_801581A0 = {
    D_actor_403200_80158144,
    D_actor_403200_80158198,
    { NULL, D_actor_403200_80158100, NULL, NULL, D_actor_403200_80158130, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_801581C8[3] = {
#include "assets/actor_403200_animation_26AB8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_801581EC[203] = {
#include "assets/actor_403200_animation_26AB8_bank4.inc"
};

AnimationRecord D_actor_403200_80158518[236] = {
#include "assets/actor_403200_animation_26AB8_records.inc"
};

u16 D_actor_403200_801588C8[8] = {
#include "assets/actor_403200_animation_26AB8_indices.inc"
};

AnimationSet D_actor_403200_801588D8 = {
    D_actor_403200_80158518,
    D_actor_403200_801588C8,
    { NULL, D_actor_403200_801581C8, NULL, NULL, D_actor_403200_801581EC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80158900[279] = {
#include "assets/actor_403200_animation_27F04_bank1.inc"
};

AnimationPackedRotation D_actor_403200_80159614[73] = {
#include "assets/actor_403200_animation_27F04_bank4.inc"
};

AnimationRecord D_actor_403200_80159738[377] = {
#include "assets/actor_403200_animation_27F04_records.inc"
};

u16 D_actor_403200_80159D1C[4] = {
#include "assets/actor_403200_animation_27F04_indices.inc"
};

AnimationSet D_actor_403200_80159D24 = {
    D_actor_403200_80159738,
    D_actor_403200_80159D1C,
    { NULL, D_actor_403200_80158900, NULL, NULL, D_actor_403200_80159614, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_80159D4C[287] = {
#include "assets/actor_403200_animation_293D8_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015AAC0[72] = {
#include "assets/actor_403200_animation_293D8_bank4.inc"
};

AnimationRecord D_actor_403200_8015ABE0[388] = {
#include "assets/actor_403200_animation_293D8_records.inc"
};

u16 D_actor_403200_8015B1F0[4] = {
#include "assets/actor_403200_animation_293D8_indices.inc"
};

AnimationSet D_actor_403200_8015B1F8 = {
    D_actor_403200_8015ABE0,
    D_actor_403200_8015B1F0,
    { NULL, D_actor_403200_80159D4C, NULL, NULL, D_actor_403200_8015AAC0, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015B220[50] = {
#include "assets/actor_403200_animation_29B70_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015B478[120] = {
#include "assets/actor_403200_animation_29B70_bank4.inc"
};

AnimationRecord D_actor_403200_8015B658[202] = {
#include "assets/actor_403200_animation_29B70_records.inc"
};

u16 D_actor_403200_8015B980[8] = {
#include "assets/actor_403200_animation_29B70_indices.inc"
};

AnimationSet D_actor_403200_8015B990 = {
    D_actor_403200_8015B658,
    D_actor_403200_8015B980,
    { NULL, D_actor_403200_8015B220, NULL, NULL, D_actor_403200_8015B478, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015B9B8[104] = {
#include "assets/actor_403200_animation_2A3F4_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015BE98[55] = {
#include "assets/actor_403200_animation_2A3F4_bank4.inc"
};

AnimationRecord D_actor_403200_8015BF74[166] = {
#include "assets/actor_403200_animation_2A3F4_records.inc"
};

u16 D_actor_403200_8015C20C[4] = {
#include "assets/actor_403200_animation_2A3F4_indices.inc"
};

AnimationSet D_actor_403200_8015C214 = {
    D_actor_403200_8015BF74,
    D_actor_403200_8015C20C,
    { NULL, D_actor_403200_8015B9B8, NULL, NULL, D_actor_403200_8015BE98, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015C23C[104] = {
#include "assets/actor_403200_animation_2AC80_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015C71C[54] = {
#include "assets/actor_403200_animation_2AC80_bank4.inc"
};

AnimationRecord D_actor_403200_8015C7F4[169] = {
#include "assets/actor_403200_animation_2AC80_records.inc"
};

u16 D_actor_403200_8015CA98[4] = {
#include "assets/actor_403200_animation_2AC80_indices.inc"
};

AnimationSet D_actor_403200_8015CAA0 = {
    D_actor_403200_8015C7F4,
    D_actor_403200_8015CA98,
    { NULL, D_actor_403200_8015C23C, NULL, NULL, D_actor_403200_8015C71C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015CAC8[3] = {
#include "assets/actor_403200_animation_2AD60_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015CAEC[7] = {
#include "assets/actor_403200_animation_2AD60_bank4.inc"
};

AnimationRecord D_actor_403200_8015CB08[26] = {
#include "assets/actor_403200_animation_2AD60_records.inc"
};

u16 D_actor_403200_8015CB70[8] = {
#include "assets/actor_403200_animation_2AD60_indices.inc"
};

AnimationSet D_actor_403200_8015CB80 = {
    D_actor_403200_8015CB08,
    D_actor_403200_8015CB70,
    { NULL, D_actor_403200_8015CAC8, NULL, NULL, D_actor_403200_8015CAEC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015CBA8[3] = {
#include "assets/actor_403200_animation_2ADEC_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015CBCC[2] = {
#include "assets/actor_403200_animation_2ADEC_bank4.inc"
};

AnimationRecord D_actor_403200_8015CBD4[12] = {
#include "assets/actor_403200_animation_2ADEC_records.inc"
};

u16 D_actor_403200_8015CC04[4] = {
#include "assets/actor_403200_animation_2ADEC_indices.inc"
};

AnimationSet D_actor_403200_8015CC0C = {
    D_actor_403200_8015CBD4,
    D_actor_403200_8015CC04,
    { NULL, D_actor_403200_8015CBA8, NULL, NULL, D_actor_403200_8015CBCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015CC34[3] = {
#include "assets/actor_403200_animation_2AE78_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015CC58[2] = {
#include "assets/actor_403200_animation_2AE78_bank4.inc"
};

AnimationRecord D_actor_403200_8015CC60[12] = {
#include "assets/actor_403200_animation_2AE78_records.inc"
};

u16 D_actor_403200_8015CC90[4] = {
#include "assets/actor_403200_animation_2AE78_indices.inc"
};

AnimationSet D_actor_403200_8015CC98 = {
    D_actor_403200_8015CC60,
    D_actor_403200_8015CC90,
    { NULL, D_actor_403200_8015CC34, NULL, NULL, D_actor_403200_8015CC58, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015CCC0[14] = {
#include "assets/actor_403200_animation_2B644_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015CD68[158] = {
#include "assets/actor_403200_animation_2B644_bank4.inc"
};

AnimationRecord D_actor_403200_8015CFE0[279] = {
#include "assets/actor_403200_animation_2B644_records.inc"
};

u16 D_actor_403200_8015D43C[20] = {
#include "assets/actor_403200_animation_2B644_indices.inc"
};

AnimationSet D_actor_403200_8015D464 = {
    D_actor_403200_8015CFE0,
    D_actor_403200_8015D43C,
    { NULL, D_actor_403200_8015CCC0, NULL, NULL, D_actor_403200_8015CD68, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015D48C[15] = {
#include "assets/actor_403200_animation_2BE50_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015D540[206] = {
#include "assets/actor_403200_animation_2BE50_bank4.inc"
};

AnimationRecord D_actor_403200_8015D878[244] = {
#include "assets/actor_403200_animation_2BE50_records.inc"
};

u16 D_actor_403200_8015DC48[20] = {
#include "assets/actor_403200_animation_2BE50_indices.inc"
};

AnimationSet D_actor_403200_8015DC70 = {
    D_actor_403200_8015D878,
    D_actor_403200_8015DC48,
    { NULL, D_actor_403200_8015D48C, NULL, NULL, D_actor_403200_8015D540, NULL, NULL, NULL },
};

s8 gIncinBossAnimTransitions[45][45] = {
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 20, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AnimationSet* D_actor_403200_8015E484[46] = {
    NULL,
    &D_actor_403200_8014F7E0,
    &D_actor_403200_8014FC20,
    &D_actor_403200_80150510,
    &D_actor_403200_80151BFC,
    &D_actor_403200_80152828,
    &D_actor_403200_80150BA8,
    &D_actor_403200_80157AC0,
    &D_actor_403200_80157D88,
    &D_actor_403200_80158010,
    &D_actor_403200_8015CB80,
    &D_actor_403200_80151144,
    &D_actor_403200_80153080,
    &D_actor_403200_80154C78,
    &D_actor_403200_801534F0,
    &D_actor_403200_80153B7C,
    &D_actor_403200_801542D4,
    &D_actor_403200_801557B8,
    &D_actor_403200_801588D8,
    &D_actor_403200_80151144,
    &D_actor_403200_8015B990,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403200_8015E53C[46] = {
    NULL,
    &D_actor_403200_8014F88C,
    &D_actor_403200_8014FEE4,
    &D_actor_403200_801506F0,
    &D_actor_403200_80152054,
    &D_actor_403200_80152B30,
    &D_actor_403200_80150C80,
    &D_actor_403200_80157BA0,
    &D_actor_403200_80157E50,
    &D_actor_403200_801580D8,
    &D_actor_403200_8015CC0C,
    &D_actor_403200_80151504,
    &D_actor_403200_8015314C,
    &D_actor_403200_80154FAC,
    &D_actor_403200_80153734,
    &D_actor_403200_80153D08,
    &D_actor_403200_80154618,
    &D_actor_403200_80155A4C,
    &D_actor_403200_80159D24,
    &D_actor_403200_80151504,
    &D_actor_403200_8015C214,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403200_8015E5F4[46] = {
    NULL,
    &D_actor_403200_8014F938,
    &D_actor_403200_801501C0,
    &D_actor_403200_801508D0,
    &D_actor_403200_8015247C,
    &D_actor_403200_80152E18,
    &D_actor_403200_80150D5C,
    &D_actor_403200_80157C80,
    &D_actor_403200_80157F18,
    &D_actor_403200_801581A0,
    &D_actor_403200_8015CC98,
    &D_actor_403200_80151818,
    &D_actor_403200_80153218,
    &D_actor_403200_801552D8,
    &D_actor_403200_80153974,
    &D_actor_403200_80153E9C,
    &D_actor_403200_80154930,
    &D_actor_403200_80155D58,
    &D_actor_403200_8015B1F8,
    &D_actor_403200_80151818,
    &D_actor_403200_8015CAA0,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403200_8015E6AC[8] = {
    NULL,
    &D_actor_403200_801579B8,
    &D_actor_403200_8015DC70,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

AnimationSet* D_actor_403200_8015E6CC[7] = {
    NULL,
    &D_actor_403200_80156E1C,
    &D_actor_403200_8015DC70,
    NULL,
    NULL,
    NULL,
    NULL,
};

typedef s32 (*Actor403200ViewFn)(Task* task, s16 arg);

Actor403200ViewFn D_actor_403200_8015E6E8[9] = {
    func_actor_403200_80141180,
    func_actor_403200_80134748,
    func_actor_403200_801344C4,
    func_actor_403200_801341E8,
    func_actor_403200_80134900,
    func_actor_403200_80134374,
    func_actor_403200_80141124,
    func_actor_403200_801411A8,
    func_actor_403200_80134A14,
};

u8 gIncinBossRainGroup = 0;

AnimationSet* gIncinBossCaughtAnimSets[7] = {
    NULL,
    &D_actor_403200_8015D464,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

TaskDesc D_actor_403200_8015E72C[7] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141430, { .model = &D_actor_403200_80148FF4 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141430, { .model = &D_actor_403200_80147838 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141430, { .model = &D_actor_403200_80144EF8 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141430, { .model = &D_actor_403200_8014607C } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141430, { .model = &D_actor_403200_8014A384 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141430, { .model = &D_actor_403200_8014ACB8 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_8014148C, { .model = &D_actor_403200_8014A858 } },
};

SVECTOR gIncinBossRainLaunchOffsets[8] = {
    { -1000, 0, -1800, 0 },
    { 800, 0, -800, 0 },
    { -1300, 0, 200, 0 },
    { 1200, 0, 1000, 0 },
    { 900, 0, -1700, 0 },
    { -800, 0, -880, 0 },
    { 1280, 0, 0, 0 },
    { -1100, 0, 900, 0 },
};

SVECTOR gIncinBossRainPoints[16] = {
    { -2000, 0, -1800, 0 },
    { -1200, 0, -1900, 0 },
    { -80, 0, -1880, 0 },
    { 990, 0, -1790, 0 },
    { 1900, 0, -1650, 0 },
    { -1880, 0, -100, 0 },
    { -1000, 0, 150, 0 },
    { 80, 0, 80, 0 },
    { 1090, 0, -90, 0 },
    { 2100, 0, 50, 0 },
    { -1900, 0, 1100, 0 },
    { -900, 0, -1150, 0 },
    { 0, 0, -1800, 0 },
    { 1290, 0, 1900, 0 },
    { 1700, 0, 1500, 0 },
    { 0, 0, 0, 0 },
};

u8 gIncinBossRainPointIndex[3][8] = {
    { 7, 10, 8, 0, 4, 3, 12, 9 },
    { 2, 5, 12, 0, 6, 14, 13, 10 },
    { 14, 13, 9, 0, 12, 7, 10, 11 },
};

TaskDesc D_actor_403200_8015E858[5] = {
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141564, { .model = &D_actor_403200_8014C1AC } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_403200_80141778, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, func_actor_403200_801414E8, { .value = 0 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141670, { .model = &D_actor_403200_8014E048 } },
    { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80141868, { .model = &D_actor_403200_8014B588 } },
};

AnimationPackedPose D_actor_403200_8015E894[7] = {
#include "assets/actor_403200_animation_2CDE0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015E8E8[65] = {
#include "assets/actor_403200_animation_2CDE0_bank4.inc"
};

AnimationRecord D_actor_403200_8015E9EC[123] = {
#include "assets/actor_403200_animation_2CDE0_records.inc"
};

u16 D_actor_403200_8015EBD8[20] = {
#include "assets/actor_403200_animation_2CDE0_indices.inc"
};

AnimationSet D_actor_403200_8015EC00 = {
    D_actor_403200_8015E9EC,
    D_actor_403200_8015EBD8,
    { NULL, D_actor_403200_8015E894, NULL, NULL, D_actor_403200_8015E8E8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015EC28[2] = {
#include "assets/actor_403200_animation_2CF64_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015EC40[13] = {
#include "assets/actor_403200_animation_2CF64_bank4.inc"
};

AnimationRecord D_actor_403200_8015EC74[58] = {
#include "assets/actor_403200_animation_2CF64_records.inc"
};

u16 D_actor_403200_8015ED5C[20] = {
#include "assets/actor_403200_animation_2CF64_indices.inc"
};

AnimationSet D_actor_403200_8015ED84 = {
    D_actor_403200_8015EC74,
    D_actor_403200_8015ED5C,
    { NULL, D_actor_403200_8015EC28, NULL, NULL, D_actor_403200_8015EC40, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015EDAC[4] = {
#include "assets/actor_403200_animation_2D1D0_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015EDDC[35] = {
#include "assets/actor_403200_animation_2D1D0_bank4.inc"
};

AnimationRecord D_actor_403200_8015EE68[88] = {
#include "assets/actor_403200_animation_2D1D0_records.inc"
};

u16 D_actor_403200_8015EFC8[20] = {
#include "assets/actor_403200_animation_2D1D0_indices.inc"
};

AnimationSet D_actor_403200_8015EFF0 = {
    D_actor_403200_8015EE68,
    D_actor_403200_8015EFC8,
    { NULL, D_actor_403200_8015EDAC, NULL, NULL, D_actor_403200_8015EDDC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_403200_8015F018[16] = {
#include "assets/actor_403200_animation_2D928_bank1.inc"
};

AnimationPackedRotation D_actor_403200_8015F0D8[153] = {
#include "assets/actor_403200_animation_2D928_bank4.inc"
};

AnimationRecord D_actor_403200_8015F33C[249] = {
#include "assets/actor_403200_animation_2D928_records.inc"
};

u16 D_actor_403200_8015F720[20] = {
#include "assets/actor_403200_animation_2D928_indices.inc"
};

AnimationSet D_actor_403200_8015F748 = {
    D_actor_403200_8015F33C,
    D_actor_403200_8015F720,
    { NULL, D_actor_403200_8015F018, NULL, NULL, D_actor_403200_8015F0D8, NULL, NULL, NULL },
};

Actor403200MessageEntry D_actor_403200_8015F770[8] = {
    { 2015, { .call5 = func_actor_403200_8014196C } },
    { 2005, { .call4 = func_actor_403200_80138468 } },
    { 2006, { .call1 = func_actor_403200_80141974 } },
    { 2004, { .call3 = actorMsgPlace } },
    { ACTOR_COMMAND_MESSAGE_APPLY, { .call2 = func_actor_403200_80138748 } },
    { 5108, { .call4 = func_actor_403200_80141A94 } },
    { 2014, { .call0 = func_actor_403200_80141B30 } },
    { 0x7FFFFFFF, { .call0 = NULL } },
};

SVECTOR D_actor_403200_8015F7B0[3][9] = {
    { { 0x2EE0, 0, -1500, 0 }, { 0x32C8, 0, -1600, 0 }, { 0x3E80, 0, -1300, 0 }, { 0x2EE0, 0, -0x2904, 0 }, { 0x32C8, 0, -0x2968, 0 }, { 0x3E80, 0, -0x283C, 0 }, { 0x36B0, 0, -0x2904, 0 }, { 0x3A98, 0, -600, 0 }, { 0x4650, 0, -0x283C, 0 } },
    { { 0x2EE0, 0, -1500, 0 }, { 0x32C8, 0, -1600, 0 }, { 0x3E80, 0, -1300, 0 }, { 0x2EE0, 0, -0x2904, 0 }, { 0x32C8, 0, -0x2968, 0 }, { 0x3E80, 0, -0x283C, 0 }, { 0x36B0, 0, -0x2904, 0 }, { 0x3A98, 0, -600, 0 }, { 0x4650, 0, -0x283C, 0 } },
    { { 0x2EE0, 0, -1500, 0 }, { 0x32C8, 0, -1600, 0 }, { 0x3E80, 0, -1300, 0 }, { 0x2EE0, 0, -0x2904, 0 }, { 0x32C8, 0, -0x2968, 0 }, { 0x3E80, 0, -0x283C, 0 }, { 0x36B0, 0, -0x2904, 0 }, { 0x3A98, 0, -600, 0 }, { 0x4650, 0, -0x283C, 0 } },
};

Actor403200SpawnRec D_actor_403200_8015F888[9] = {
    { &D_actor_403200_8014E62C, 0, 0 },
    { &D_actor_403200_8014EAF8, 1, 0 },
    { &D_actor_403200_8014EC84, 2, 0 },
    { &D_actor_403200_8014EFD0, 2, 0 },
    { &D_actor_403200_8014F14C, 1, 0 },
    { &D_actor_403200_8014F2C8, 0, 0 },
    { &D_actor_403200_8014F38C, 0, 0 },
    { &D_actor_403200_8014F450, 1, 0 },
    { &D_actor_403200_8014F6C8, 2, 0 },
};

// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    TaskDesc value;
    u8       retained[4];
} Actor403200StorageF8D0;
STATIC_ASSERT_SIZEOF(Actor403200StorageF8D0, 16);

Actor403200StorageF8D0 D_actor_403200_8015F8D0 = { { { { TASK_BODY_TMD, 96 } }, func_actor_403200_80140E6C, { .model = &D_actor_403200_80144150 } }, { 0 } };

// Retain seven zero bytes after the accessed state byte.
// Their original role as spare storage or alignment remains unresolved.
s8 D_actor_403200_8015F8E0[8] = { 0 };

SVECTOR ActorContact_ScratchPosition = { 0, 0, 0, 0 };

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

Task* D_actor_403200_8015F8F0 = NULL;

ActorCommand D_actor_403200_8015F8F4 = { { .loc = { 0, 0 } }, 0 };

SVECTOR gIncinBossSpinnerTarget = { 0, 0, 0, 0 };

Actor403200StorageF900 gIncinBossGrabQuery = { { { 0, 0, 0, 0 }, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } };

GfxCoord D_actor_403200_8015F920 = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL };

Actor403200DropCoord D_actor_403200_8015F970 = { .c = { 0, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } };

Actor403200StorageF9C0 D_actor_403200_8015F9C0;

GpDelayArg D_actor_403200_8015FA00;

/// Integer part of the last step `ActorContact_PushContact` applied.
extern SVECTOR ActorContact_ScratchPosition;

extern Actor403200ViewFn D_actor_403200_8015E6E8[];

/// Scratchpad frame the per-frame tick carves off the scratch-pad stack. Only `view`
/// is written: the selector result compared with `Gp_GetViewIndex`.
typedef struct Actor403200TickScratch {
    byte pad_0[0x18];
    s16  view;
    byte pad_1A[0x2];
} Actor403200TickScratch;
STATIC_ASSERT_SIZEOF(Actor403200TickScratch, 0x1C);

/// The 25 state handlers at `D_actor_403200_80132154`. The tick copies the
/// whole table onto the stack and calls `fn[field_0]`.
typedef struct Actor403200StateTable {
    TaskFunc fn[0x19];
} Actor403200StateTable;
STATIC_ASSERT_SIZEOF(Actor403200StateTable, 0x64);

static void func_actor_403200_8013FB54(Enemy* arg0, Task* arg1);

static void func_actor_403200_80141B40(Task* arg0);

static void func_actor_403200_8014122C(Task* arg0);

static void func_actor_403200_80141234(Task* arg0);

static void func_actor_403200_8014123C(Task* arg0);

static __inline__ void Actor403200_StepForward(GfxCoord* coord);
static __inline__ void Actor403200_SeedRootCoord(Task* task, Actor403200Work* work);
static void            func_actor_403200_80134D40(Task* arg0);
static void            func_actor_403200_80138284(Task* arg0);
static void            func_actor_403200_80138AFC(Enemy* enemy, Task* task);
static void            func_actor_403200_80139A60(Task* arg0);
static void            func_actor_403200_80139E94(Task* arg0);
static void            func_actor_403200_8013A4A0(Task* arg0);
static void            func_actor_403200_8013AB70(Task* arg0);
static void            func_actor_403200_8013B23C(Task* arg0);
static void            func_actor_403200_8013B3C8(Task* arg0);
static void            func_actor_403200_8013B740(Task* arg0);
static void            func_actor_403200_8013B8C4(Task* arg0);
static void            func_actor_403200_8013C84C(Task* arg0);
static void            func_actor_403200_8013D028(Task* arg0);
static void            func_actor_403200_8013D78C(Task* arg0);
static void            func_actor_403200_8013D9EC(Task* arg0);
static void            func_actor_403200_8013DC3C(Task* arg0);
static void            func_actor_403200_8013E2FC(Task* arg0);
static void            func_actor_403200_8013E5A8(Task* arg0);
static void            func_actor_403200_8013E9C0(Task* arg0);
static void            func_actor_403200_8013EB64(Task* arg0);
static void            func_actor_403200_8013EF6C(Task* arg0);
static inline void     _actor403200TintEscort(TmdObject* model);
static void            func_actor_403200_8013F700(Task* arg0);
static void            func_actor_403200_80140FD4(s32 arg0, s16 arg1);
static void            func_actor_403200_801410F0(s8 arg0);
static void            func_actor_403200_80141108(s16 arg0);
static s16             func_actor_403200_80141114(void);

/// Walk `coord` 0x19/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flag it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor403200_StepForward(GfxCoord* coord)
{
    u8*      head;
    SVECTOR* dir;

    head                       = SCRATCH_STACK_CURSOR(u8);
    dir                        = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_STACK_CURSOR(void) = dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x19);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0]  += dir->vx;
    coord->coord.t[1]  += dir->vy;
    coord->coord.t[2]  += dir->vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

#include "../../shared/incinerator_boss_inlines.inc.c"

static __inline__ void Actor403200_SeedRootCoord(Task* task, Actor403200Work* work)
{
    GfxCoord*             coord = task->extra.tmd->coords;
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                         = (ActorScaleRotScratch*)(SCRATCH_STACK_CURSOR(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_STACK_CURSOR(ActorScaleRotScratch) = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    gfxRotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;

    work->field_0          = 0;
    task->extra.tmd->flags = 0;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(ActorScaleRotScratch));
}

#include "../../shared/actor_contacts_turn_joint.inc.c"

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/incinerator_boss_wall.inc.c"

#include "../../shared/incinerator_boss_pose_limb.inc.c"

#include "../../shared/incinerator_boss_turn_neck.inc.c"

#include "../../shared/incinerator_boss_pitch_neck.inc.c"

#include "../../shared/incinerator_boss_seed_blend.inc.c"

#include "../../shared/incinerator_boss_switch_anim.inc.c"

#include "../../shared/incinerator_boss_tick_blended.inc.c"

#include "../../shared/incinerator_boss_tick_anim.inc.c"

#include "../../shared/incinerator_boss_hit_effect.inc.c"

s32 func_actor_403200_801341E8(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if (view == 0x21) {
                value = 0x20;
                flag  = dist < 0x189D;
                if (flag) {
                    value = 0x21;
                }
                return value;
            }
            value = 0x21;
            flag  = dist < 0x1770;
            if (!flag) {
                value = 0x20;
            }
            return value;
        case 1:
            if ((view != 9) && (view != 10)) {
                value = 9;
                flag  = dist < 0x27D8;
            } else {
                flag = view;
                if (flag == 9) {
                    value = 0xA;
                    flag  = dist < 0x27D9;
                    if (flag) {
                        value = 9;
                    }
                    return value;
                }
                if (flag == 10) {
                    value = 9;
                    flag  = dist < 0x24EA;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xA;
            }
            return value;
        case 2:
            return 0x1B;
    }
    return 1;
}

s32 func_actor_403200_80134374(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
        case 1:
            if ((view != 7) && (view != 8)) {
                value = 7;
                flag  = dist < 0x26AC;
            } else {
                flag = view;
                if (flag == 7) {
                    value = 8;
                    flag  = dist < 0x26AD;
                    if (flag) {
                        value = 7;
                    }
                    return value;
                }
                if (flag == 8) {
                    value = 7;
                    flag  = dist < 0x2328;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 8;
            }
            return value;
        case 2:
            return 0x1A;
    }
    return 1;
}

s32 func_actor_403200_801344C4(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    Task*     task;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    task   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 2) && (view != 3) && (view != 4)) {
                if (dist < 0x2261) {
                    return 3;
                }
                value = 2;
                flag  = dist < 0x2FA8;
                if (!flag) {
                    value = 4;
                }
                goto done;
            } else {
                flag = view;
                if (flag == 3) {
                    value = 2;
                    flag  = dist < 0x2262;
                    if (flag) {
                        value = 3;
                    }
                    goto done;
                }
                if (flag == 2) {
                    if (dist < 0x1E14) {
                        return 3;
                    }
                    flag = dist < 0x2FA9;
                    if (flag) {
                        value = 2;
                    } else {
                        value = 4;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 2;
                    flag  = dist < 0x2E18;
                    if (!flag) {
                        value = 4;
                    }
                    goto done;
                }
                value = 1;
                goto done;
            }
        case 1:
            flag = view;
            if ((flag != 0x22) && (flag != 4)) {
                value = 0x22;
                flag  = dist < 0x2455;
            } else {
                if (flag == 0x22) {
                    value = 4;
                    flag  = dist < 0x2456;
                    if (flag) {
                        value = 0x22;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 0x22;
                    flag  = dist < 0x2260;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 4;
            }
            goto done;
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                if (flag == 0x25) {
                    value = 0x19;
                    flag  = dist < 0x1E5B;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x19) {
                    value = 0x25;
                    flag  = dist < 0x1B58;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x19;
            }
            goto done;
        case 3:
            flag = view;
            if ((flag != 0x25) && (flag != 0x1E)) {
                flag  = task->extra.tmd->coords->coord.t[0];
                value = 0x25;
                flag  = flag < 0x4268;
            } else {
                if (flag == 0x25) {
                    flag  = task->extra.tmd->coords->coord.t[0];
                    value = 0x1E;
                    flag  = flag < 0x4651;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x1E) {
                    flag  = task->extra.tmd->coords->coord.t[0];
                    value = 0x25;
                    flag  = flag < 0x4268;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x1E;
            }
            goto done;
    }
    value = 1;
done:
    return value;
}

s32 func_actor_403200_80134748(Task* arg0, s16 arg1)
{
    SVECTOR   vec;
    SVECTOR*  vp;
    GfxCoord* coords;
    s32       dist;
    s32       value;
    s32       view;
    s32       flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 5) && (view != 6)) {
                value = 5;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 5) {
                    value = 6;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 5;
                    }
                    return value;
                }
                if (flag == 6) {
                    value = 5;
                    flag  = dist < 0x2198;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 6;
            }
            return value;
        case 1:
            if ((view != 0xB) && (view != 0xC)) {
                value = 0xB;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 0xB) {
                    value = 0xC;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 0xB;
                    }
                    return value;
                }
                if (flag == 0xC) {
                    value = 0xB;
                    flag  = dist < 0x1A90;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xC;
            }
            return value;
        case 2:
            return 0x1C;
    }
    return 1;
}

s32 func_actor_403200_80134900(Task* arg0, s16 arg1)
{
    SVECTOR   pos;
    SVECTOR*  p;
    GfxCoord* coords;
    s32       dist;
    s32       flag;
    s32       value;
    s32       view;

    view   = Gp_GetViewIndex() & 0xFF;
    p      = &pos;
    coords = arg0->extra.tmd->coords;
    p->vx  = gPlayerStatus.coordMtx->t[0] - coords->coord.t[0];
    p->vy  = gPlayerStatus.coordMtx->t[1] - coords->coord.t[1];
    dist   = pos.vx * pos.vx;
    p->vz  = gPlayerStatus.coordMtx->t[2] - coords->coord.t[2];
    dist  += pos.vy * pos.vy;
    dist   = SquareRoot0(dist + (pos.vz * pos.vz));
    switch (arg1) {
        default:
            return 1;
        case 0:
        case 1:
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
                if (!flag) {
                    value = 0x19;
                }
                return value;
            } else if (flag == 0x25) {
                value = 0x25;
                flag  = dist < 0x1E5A;
                if (!flag) {
                    value = 0x19;
                }
                return value;
            } else {
                value = 0x25;
                flag  = dist < 0x1B58;
                if (!flag) {
                    value = 0x19;
                }
                return value;
            }
    }
}

/// Reference positions the view selector below measures the player against.
static const Actor403200ViewPoints D_actor_403200_80131E64 = {
    {
        { 0x10B4, 1, -0x17DD, 0 },
        { 0x1CD0, 1, -0x17DD, 0 },
        { 0x2888, 1, -0x17DD, 0 },
        { 0x2888, 1, -0x17DD, 0 },
    },
};

/// State handlers of the escort model task `func_actor_403200_80141430` and
/// `func_actor_403200_8014148C` dispatch: texture setup, coordinate refresh,
/// teardown.
static const GpEnemyTaskFuncTable3 D_actor_403200_80131E84 = {
    {
        incinBossPropSetup,
        incinBossPropTick,
        Gp_DestroyEnemy,
    },
};

s32 func_actor_403200_80134A14(Task* arg0, s16 arg1)
{
    SVECTOR               vec;
    Actor403200ViewPoints tab;
    Task*                 obj;
    s32                   dist;
    s32                   value;
    s32                   view;
    s32                   flag;

    view   = Gp_GetViewIndex() & 0xFF;
    obj    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    tab    = D_actor_403200_80131E64;
    vec.vx = obj->extra.tmd->coords->coord.t[0] - tab.v[arg1].vx;
    dist   = vec.vx * vec.vx;
    vec.vy = obj->extra.tmd->coords->coord.t[1] - tab.v[arg1].vy;
    dist  += vec.vy * vec.vy;
    vec.vz = obj->extra.tmd->coords->coord.t[2] - tab.v[arg1].vz;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 2) && (view != 3) && (view != 4)) {
                if (dist < 0x2261) {
                    return 3;
                }
                value = 2;
                flag  = dist < 0x2FA8;
                if (!flag) {
                    value = 4;
                }
                goto done;
            } else {
                flag = view;
                if (flag == 3) {
                    value = 2;
                    flag  = dist < 0x2262;
                    if (flag) {
                        value = 3;
                    }
                    goto done;
                }
                if (flag == 2) {
                    if (dist < 0x1E14) {
                        return 3;
                    }
                    flag = dist < 0x2FA9;
                    if (flag) {
                        value = 2;
                    } else {
                        value = 4;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 2;
                    flag  = dist < 0x2E18;
                    if (!flag) {
                        value = 4;
                    }
                    goto done;
                }
                value = 1;
                goto done;
            }
        case 1:
            flag = view;
            if ((flag != 0x22) && (flag != 4)) {
                value = 0x22;
                flag  = dist < 0x2455;
            } else {
                if (flag == 0x22) {
                    value = 4;
                    flag  = dist < 0x2456;
                    if (flag) {
                        value = 0x22;
                    }
                    goto done;
                }
                if (flag == 4) {
                    value = 0x22;
                    flag  = dist < 0x2260;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 4;
            }
            goto done;
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                if (flag == 0x25) {
                    value = 0x19;
                    flag  = dist < 0x1E5B;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x19) {
                    value = 0x25;
                    flag  = dist < 0x1B58;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x19;
            }
            goto done;
        case 3:
            flag = view;
            if ((flag != 0x25) && (flag != 0x1E)) {
                flag  = obj->extra.tmd->coords->coord.t[0];
                value = 0x25;
                flag  = flag < 0x4268;
            } else {
                if (flag == 0x25) {
                    flag  = obj->extra.tmd->coords->coord.t[0];
                    value = 0x1E;
                    flag  = flag < 0x4651;
                    if (flag) {
                        value = 0x25;
                    }
                    goto done;
                }
                if (flag == 0x1E) {
                    flag  = obj->extra.tmd->coords->coord.t[0];
                    value = 0x25;
                    flag  = flag < 0x4268;
                } else {
                    value = 1;
                    goto done;
                }
            }
            if (!flag) {
                value = 0x1E;
            }
            goto done;
    }
    value = 1;
done:
    return value;
}

/// The enemy's walk-out state: a reset request re-arms the block (the two
/// 0xEF4 counters, the 0x7B0 pose flag, the 0xEFA re-arm flag, pose 2 and the
/// 0xE96 yaw target), then the per-frame body runs and the animation frame the
/// mask leaves is tested against 0x12 and 0x18 -- each one-shot cue spawning a
/// script and a type-6 sound with the enemy's pan and half its depth, once per
/// arrival -- before being latched into `field_7D8`. Unless the game is frozen
/// the model is then stepped 0x19/0x1000 forward along its own facing, its
/// `composeStamp` cleared, and once it has run out to x 0x1CCA in state 0 or 0x2882 in
/// state 1 the step advances and re-arms `field_0`.
static void func_actor_403200_80134D40(Task* arg0)
{
    Actor403200Work* work;
    Enemy*           enemy;
    GfxCoord*        model;
    s16              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;

    if (work->field_4 != 0) {
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_7B3 = 2;
        work->field_7B0 = 1;
        work->field_EFE = 0;
        work->field_E96 = 0xE74;
    }

    SCRATCH_STACK_RESERVE_BYTES(0xC);
    incinBossTickAnim(arg0);

    frame = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords) / 2));
    }

    frame = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200001;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords) / 2));
    }

    work->field_7D8 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;

    model = arg0->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen != 1) {
        Actor403200_StepForward(model);
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    switch (work->field_F08) {
        case 0:
            if (arg0->extra.tmd->coords->coord.t[0] >= 0x1CCA) {
                work->field_F08++;
                work->field_0 = 3;
            }
            break;
        case 1:
            if (arg0->extra.tmd->coords->coord.t[0] >= 0x2882) {
                work->field_F08++;
                work->field_0 = 3;
            }
            break;
    }

    if (work->field_6 > 0) {
        work->field_F06 = 8;
    }

    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

#include "../../shared/incinerator_boss_throw_spawn.inc.c"

#include "../../shared/incinerator_boss_throw_fly.inc.c"

/// State handlers of the enemy stood up on the host's first escort: spawn,
/// flight, teardown.
static const GpEnemyTaskFuncTable3 D_actor_403200_80131E90 = {
    {
        incinBossThrowSpawn,
        incinBossThrowFly,
        Gp_DestroyEnemy,
    },
};

#include "../../shared/incinerator_boss_glob_spawn.inc.c"

#include "../../shared/incinerator_boss_glob_fall.inc.c"

#include "../../shared/incinerator_boss_glob_engulf.inc.c"

#include "../../shared/incinerator_boss_glob_hold.inc.c"

/// State handlers of the grab enemy, by state: entry, bounce, rise, hold and
/// teardown.
static const GpEnemyTaskFuncTable5 D_actor_403200_80131E9C = {
    {
        incinBossGlobSpawn,
        incinBossGlobFall,
        incinBossGlobEngulf,
        incinBossGlobHold,
        Gp_DestroyEnemy,
    },
};

#include "../../shared/incinerator_boss_chunk_spawn.inc.c"

#include "../../shared/incinerator_boss_chunk_fall.inc.c"

#include "../../shared/incinerator_boss_chunk_settle.inc.c"

/// State handlers of the enemy dropped from the host's part 3: spawn, fall,
/// settle, teardown.
static const GpEnemyTaskFuncTable4 D_actor_403200_80131F04 = {
    {
        incinBossChunkSpawn,
        incinBossChunkFall,
        incinBossChunkSettle,
        Gp_DestroyEnemy,
    },
};

#include "../../shared/incinerator_boss_rain_spawn.inc.c"

#include "../../shared/incinerator_boss_rain_rise.inc.c"

#include "../../shared/incinerator_boss_rain_fall.inc.c"

#include "../../shared/incinerator_boss_rain_splat.inc.c"

/// State handlers of the enemy that rises out of view and slams back down:
/// spawn, rise, descent, landing, teardown.
static const GpEnemyTaskFuncTable5 D_actor_403200_80131F14 = {
    {
        incinBossRainSpawn,
        incinBossRainRise,
        incinBossRainFall,
        incinBossRainSplat,
        Gp_DestroyEnemy,
    },
};

#include "../../shared/incinerator_boss_spinner_spawn.inc.c"

#include "../../shared/incinerator_boss_spinner_chase.inc.c"

/// State handlers of the spinner enemy: spawn, wait, home, teardown.
static const GpEnemyTaskFuncTable4 D_actor_403200_80131F28 = {
    {
        incinBossSpinnerSpawn,
        incinBossSpinnerWait,
        incinBossSpinnerChase,
        Gp_DestroyEnemy,
    },
};

/// Screen-shake driver for the enemy task: `func_actor_403200_8013FB54` writes a
/// level into `field_EAC`, and a change from the armed level in `field_EAD`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `displaySetShakeY` off the frame counter's
/// low bits, so level 1 alternates 0 / 2, level 2 walks a four-frame 0 / 2 / 3 / 2
/// pattern and level 3 an eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out. Same body as
/// `func_actor_444000_8013A77C`, plus the null test on the work block.
static void func_actor_403200_80138284(Task* arg0)
{
    Actor403200Work* work;
    s32              phase;

    work = (Actor403200Work*)arg0->work;
    if (work == NULL) {
        return;
    }

    if (work->field_EAC != work->field_EAD) {
        switch (work->field_EAC) {
            case 1:
                work->field_EAE = 5;
                break;
            case 2:
                work->field_EAE = 0xA;
                break;
            case 3:
                work->field_EAE = 0x16;
                break;
            case 0:
            default:
                return;
        }
        work->field_EAD = work->field_EAC;
    }

    if (work->field_EAE == 0) {
        displaySetShakeY(0);
        work->field_EAC = 0;
        work->field_EAD = 0;
        return;
    }
    work->field_EAE--;

    switch (work->field_EAC) {
        case 1:
            phase = work->field_EAE;
            if ((phase & 1) == 0) {
                work->field_EAF = 0;
            } else {
                work->field_EAF = 2;
            }
            displaySetShakeY(work->field_EAF);
            break;

        case 2:
            phase = work->field_EAE;
            switch (phase & 3) {
                case 0:
                    work->field_EAF = 0;
                    break;
                case 1:
                    work->field_EAF = 2;
                    break;
                case 2:
                    work->field_EAF = 3;
                    break;
                case 3:
                    work->field_EAF = 2;
                    break;
            }
            displaySetShakeY(work->field_EAF);
            break;

        case 3:
            phase = work->field_EAE;
            switch (phase & 7) {
                case 3:
                case 4:
                    work->field_EAF = 4;
                    break;
                case 2:
                case 5:
                    work->field_EAF = 3;
                    break;
                case 1:
                case 6:
                    work->field_EAF = 1;
                    break;
                case 0:
                case 7:
                    work->field_EAF = 0;
                    break;
            }
            displaySetShakeY(work->field_EAF);
            break;

        case 0:
        default:
            displaySetShakeY(0);
            break;
    }
}

/// The escort-group reset the enemy runs whenever its state changes: it turns
/// the host model's flag word around and pushes it onto all seven escorts'
/// models, differing in what the word becomes and whether the model buffers are
/// (re)allocated first. `work->field_7F3` is cleared on every path, and the two
/// that end with the work block's state index reset are the ones that set the
/// word to 0x80.
///
/// Same body as `func_actor_444000_8013A958` without that sibling's
/// `TmdObject::buffer` buffer tests, so every escort is re-allocated
/// unconditionally.
s32 func_actor_403200_80138468(Task* task, s32 arg1, s32 arg2)
{
    Actor403200Work* work;
    Actor403200Work* buffers;
    Actor403200Work* escorts;
    Actor403200Work* rebuilt;
    s16              i;
    s16              j;

    work = (Actor403200Work*)task->work;
    switch (arg2) {
        case 0:
            buffers = (Actor403200Work*)task->work;
            Tmd_AllocBuffers(task->extra.tmd);
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    Tmd_AllocBuffers(buffers->field_ECC[j]->task->extra.tmd);
                }
            }
            escorts                = (Actor403200Work*)task->work;
            escorts->field_7F3     = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 1:
            escorts                = (Actor403200Work*)task->work;
            escorts->field_7F3     = 0;
            task->extra.tmd->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            rebuilt = (Actor403200Work*)task->work;
            Tmd_AllocBuffers(task->extra.tmd);
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    Tmd_AllocBuffers(rebuilt->field_ECC[j]->task->extra.tmd);
                }
            }
            break;
        case 2:
            work->field_7F3        = 0;
            escorts                = work;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            i                      = 0;
            escorts                = (Actor403200Work*)task->work;
            escorts->field_7F3     = 0;
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            for (; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    escorts->field_ECC[i]->task->extra.tmd->flags = task->extra.tmd->flags;
                }
            }
            break;
    }
    return 0;
}

/// Handles message 0x7DB: records the payload and dispatches the sender's
/// action to reset the escorts, select an attack, or finish the return pose.
s32 func_actor_403200_80138748(Task* task, s32 msgId, ActorCommand* msg)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* rebuilt;
    Enemy*           temp_enemy;
    s16              i;
    s16              j;
    s32              sound;
    s32              pan;
    s32              action;

    work       = (Actor403200Work*)task->work;
    temp_enemy = (Enemy*)task->spawnArg2.pointer;

    work->field_EC4 = msg->context.loc.stage;
    work->field_EC5 = msg->context.loc.area;
    work->field_EC6 = (u8)msg->command;

    if (msg->context.key == 0x2704) {
        action = msg->command;
        switch (action) {
            case 0:
                work->field_0   = 0;
                work->field_E96 = 0xFA0;
                break;

            case 2:
                work->field_0   = 5;
                work->field_7B3 = 0x14;
                work->field_7B0 = 2;
                sound           = ((temp_enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200002;
                pan             = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
                SndEvt_EnqueueType6(sound, pan,
                                    (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
                break;

            case 3:
                work->field_0   = 5;
                work->field_2   = -1;
                work->field_7B3 = 0xD;
                work->field_7B0 = 1;
                break;

            case 5:
                work->field_0          = 0xA;
                escorts                = (Actor403200Work*)task->work;
                escorts->field_7F3     = 0;
                task->extra.tmd->flags = 0;
                for (i = 0; i < 7; i++) {
                    if (escorts->field_ECC[i] != NULL) {
                        escorts->field_ECC[i]->task->extra.tmd->flags =
                            task->extra.tmd->flags;
                    }
                }
                rebuilt = (Actor403200Work*)task->work;
                Tmd_AllocBuffers(task->extra.tmd);
                for (j = 0; j < 7; j++) {
                    if (rebuilt->field_ECC[j] != NULL) {
                        Tmd_AllocBuffers(rebuilt->field_ECC[j]->task->extra.tmd);
                    }
                }
                work->field_F06 = 0;
                work->field_F04 = 0;
                break;

            case 10:
                work->field_0   = 0xC;
                work->field_2   = -1;
                work->field_7B3 = 0x12;
                work->field_7B0 = 2;
                work->field_F06 = 7;
                work->field_F14 = 0;
                work->field_F04 = 1;
                break;

            case 11:
                work->field_0   = 0xC;
                work->field_2   = -1;
                work->field_7B3 = 0x12;
                work->field_F14 = 0x96;
                work->field_7B0 = 2;
                work->field_F04 = 1;
                break;

            case 12:
                work->field_F06 = 7;
                work->field_F04 = 0;
                work->field_0   = 1;

            case 19:
                work->field_0       = 0xC;
                work->field_F06     = 4;
                work->field_2       = -1;
                work->field_7B3     = 0x12;
                work->field_F14     = 0x258;
                work->field_7B0     = 2;
                work->field_F04     = 0;
                gIncinBossLimbReach = 0x640;
                break;
        }
    }

    if (msg->context.key == 0x2804) {
        switch (msg->command) {
            case 0:
                work->field_0 = 0;
                break;

            case 1:
                work->field_7B3 = 0xA;
                work->field_7B0 = 2;
                work->field_7B6 = 0x7F;
                incinBossTickAnim(task);
                while (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                    incinBossTickAnim(task);
                }
                work->field_7B6                       = 0x10;
                task->extra.tmd->coords->coord.t[0]   = -0xBB8;
                task->extra.tmd->coords->coord.t[1]   = 0;
                task->extra.tmd->coords->coord.t[2]   = -0x992;
                task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                work->field_0                         = 6;
                break;
        }
    }
    return 1;
}

/// Spawn state of the arena boss: allocate its work block, wire the host enemy
/// up to the model's root coordinate and its nine collision objects, then spawn
/// the seven escorts that make up the rest of the creature.
static void func_actor_403200_80138AFC(Enemy* enemy, Task* task)
{
    Actor403200Work*       work;
    Actor403200Work*       buffers;
    Actor403200Work*       escorts;
    OverlayMat*            mtx;
    TmdObject*             tmd;
    GfxCoord*              coord;
    GfxCoord*              freeCoord;
    Enemy*                 esc;
    Task*                  escTask;
    WorldCollisionContact* recs2;
    SVECTOR                dir;
    SVECTOR*               gteDir;
    VECTOR                 pos;
    s16                    i;
    s16                    j;

    tmd   = task->extra.tmd;
    coord = tmd->coords;

    work       = memCalloc(0xF24, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    (Gp_IncStateF0Ref)(0);
    task->exitCallback = incinBossExit;

    enemy->field_4    = &task->extra.tmd->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = -0xC8;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &task->extra.tmd->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->reactionFlags = 0;
    enemy->hp            = D_actor_403200_80141C00.hpMax;
    enemy->param         = &D_actor_403200_80141C00;
    enemy->recs          = work->hits[0].recs;

    func_800B3F84(&work->anim0, D_actor_403200_8015E484, tmd, work->aux0, work->slots0);
    func_800B3F84(&work->anim1, D_actor_403200_8015E484, tmd, work->aux1, work->slots1);

    work->field_7B0 = 2;
    work->field_7B3 = 2;
    work->field_EF8 = 1;
    work->field_7B1 = 0;
    work->field_7C4 = work->field_7C8 = 0;
    work->field_7B6 = work->field_7B8 = 0x10;

    func_8010C980(&task->extra.tmd->coords[4], &work->hits[1].obj, work->hits[1].recs, 5, 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[4], &work->hits[0].obj, work->hits[0].recs, 5, 0x20, 0x300);
    func_8010C980(&task->extra.tmd->coords[1], &work->hits[2].obj, work->hits[2].recs, 5, 0x20, 0xBB8);

    work->hits[1].obj.pos.vz = -0x100;
    work->hits[2].obj.pos.vy = 0x400;
    work->hits[1].obj.pos.vx = 0;
    work->hits[1].obj.pos.vy = 0;
    work->hits[2].obj.pos.vx = 0;
    work->hits[2].obj.pos.vz = -0x400;

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &dir);
    dir.vy = 0;
    gteDir = &dir;
    VectorNormalSS(gteDir, gteDir);
    gte_lddp(0x1388);
    gte_ldsv(gteDir);
    gte_gpf12();
    gte_stsv(gteDir);

    work->anim.source.sets          = NULL;
    work->anim.animationId          = 1;
    work->anim.blend                = ANIMATION_BLEND_RESET;
    work->anim.blendFrames          = 3;
    work->anim.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
    work->field_F12                 = 0;
    task->msgTable                  = D_actor_403200_8015F770;
    coord->parent                   = &gGfxViewCoord;
    coord->composeStamp             = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);

    work->field_2 = -1;
    buffers       = task->work;
    Tmd_AllocBuffers(task->extra.tmd);
    for (i = 0; i < 7; i++) {
        if (buffers->field_ECC[i] != NULL) {
            Tmd_AllocBuffers(buffers->field_ECC[i]->task->extra.tmd);
        }
    }

    Actor403200_SeedRootCoord(task, work);

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 0, 0, task->spawnArg2.pointer);
    work->field_ECC[0]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = task->extra.tmd->coords;
    work->field_ECC[0]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[0]->task->extra.tmd->coords->coord.t[1] = 0;
    work->field_ECC[0]->task->extra.tmd->coords->coord.t[2] = 0;
    work->field_ECC[0]->task->extra.tmd->flags              = 0;
    func_800B3F84(&work->anim2, D_actor_403200_8015E53C, work->field_ECC[0]->task->extra.tmd, work->aux2,
                  work->slots2);
    func_800B3F84(&work->anim3, D_actor_403200_8015E53C, work->field_ECC[0]->task->extra.tmd, work->aux3,
                  work->slots3);
    work->field_ECC[0]->field_4    = &task->extra.tmd->coords->coord;
    work->field_ECC[0]->field_48   = 0;
    work->field_ECC[0]->bodyPos.vx = 0xC8;
    work->field_ECC[0]->bodyPos.vy = 0;
    work->field_ECC[0]->bodyPos.vz = 0x3E8;
    work->field_ECC[0]->coord      = &work->field_ECC[0]->task->extra.tmd->coords[1];
    Gp_LinkNode(&work->field_ECC[0]->node);
    work->field_ECC[0]->reactionFlags = 0;
    work->field_ECC[0]->hp            = D_actor_403200_80141C00.hpMax;
    work->field_F0A                   = D_actor_403200_80141C20.hpMax;
    work->field_ECC[0]->param         = &D_actor_403200_80141C20;
    work->field_ECC[0]->recs          = work->hits[3].recs;
    func_8010C980(&work->field_ECC[0]->task->extra.tmd->coords[1], &work->hits[3].obj, work->hits[3].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[0]->task->extra.tmd->coords[2], &work->hits[4].obj, work->hits[4].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[0]->task->extra.tmd->coords[3], &work->hits[5].obj, work->hits[5].recs, 5,
                  0x20, 0x300);

    esc                                                     = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 1, 0, task->spawnArg2.pointer);
    work->field_ECC[1]                                      = esc;
    esc->task->extra.tmd->coords->parent                    = task->extra.tmd->coords;
    work->field_ECC[1]->task->extra.tmd->coords->coord.t[0] = 0;
    work->field_ECC[1]->task->extra.tmd->coords->coord.t[1] = 0;
    work->field_ECC[1]->task->extra.tmd->coords->coord.t[2] = 0;
    work->field_ECC[1]->task->extra.tmd->flags              = 0;
    func_800B3F84(&work->anim4, D_actor_403200_8015E5F4, work->field_ECC[1]->task->extra.tmd, work->aux4,
                  work->slots4);
    func_800B3F84(&work->anim5, D_actor_403200_8015E5F4, work->field_ECC[1]->task->extra.tmd, work->aux5,
                  work->slots5);
    work->field_ECC[1]->field_4    = &task->extra.tmd->coords->coord;
    work->field_ECC[1]->field_48   = 0;
    work->field_ECC[1]->bodyPos.vx = -0xC8;
    work->field_ECC[1]->bodyPos.vy = 0;
    work->field_ECC[1]->bodyPos.vz = 0x3E8;
    work->field_ECC[1]->coord      = &work->field_ECC[1]->task->extra.tmd->coords[1];
    Gp_LinkNode(&work->field_ECC[1]->node);
    work->field_ECC[1]->reactionFlags = 0;
    work->field_ECC[1]->hp            = D_actor_403200_80141C00.hpMax;
    work->field_F0C                   = D_actor_403200_80141C30.hpMax;
    work->field_ECC[1]->param         = &D_actor_403200_80141C30;
    work->field_ECC[1]->recs          = work->hits[6].recs;
    func_8010C980(&work->field_ECC[1]->task->extra.tmd->coords[1], &work->hits[6].obj, work->hits[6].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[1]->task->extra.tmd->coords[2], &work->hits[7].obj, work->hits[7].recs, 5,
                  0x20, 0x300);
    func_8010C980(&work->field_ECC[1]->task->extra.tmd->coords[3], &work->hits[8].obj, work->hits[8].recs, 5,
                  0x20, 0x300);

    esc                = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 2, 0, task->spawnArg2.pointer);
    work->field_ECC[2] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[4];
        work->field_ECC[2]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
        work->field_ECC[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
        work->field_ECC[2]->task->extra.tmd->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 3, 0, task->spawnArg2.pointer);
    work->field_ECC[3] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[3];
        work->field_ECC[3]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[3]->task->extra.tmd->coords->coord.t[1] = 0;
        work->field_ECC[3]->task->extra.tmd->coords->coord.t[2] = 0;
        work->field_ECC[3]->task->extra.tmd->flags              = 0;
        work->field_ECC[3]->field_4                             = &task->extra.tmd->coords->coord;
        work->field_ECC[3]->field_48                            = 0;
        work->field_ECC[3]->bodyPos.vx                          = 0;
        work->field_ECC[3]->bodyPos.vy                          = 0x1F4;
        work->field_ECC[3]->bodyPos.vz                          = 0x384;
        work->field_ECC[3]->coord                               = work->field_ECC[3]->task->extra.tmd->coords;
        Gp_LinkNode(&work->field_ECC[3]->node);
        work->field_ECC[3]->reactionFlags = 0;
        work->field_ECC[3]->hp            = D_actor_403200_80141C00.hpMax;
        work->field_F0E                   = D_actor_403200_80141C40.hpMax;
        work->field_ECC[3]->param         = &D_actor_403200_80141C40;
        work->field_ECC[3]->recs          = work->hits[1].recs;
    }

    esc                = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 4, 0, task->spawnArg2.pointer);
    work->field_ECC[4] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[4];
        work->field_ECC[4]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[4]->task->extra.tmd->coords->coord.t[1] = 0;
        work->field_ECC[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
        work->field_ECC[4]->task->extra.tmd->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 5, 0, task->spawnArg2.pointer);
    work->field_ECC[5] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[2];
        work->field_ECC[5]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
        work->field_ECC[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
        work->field_ECC[5]->task->extra.tmd->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(D_actor_403200_8015E72C, 6, 0, task->spawnArg2.pointer);
    work->field_ECC[6] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->parent                    = &task->extra.tmd->coords[1];
        work->field_ECC[6]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[6]->task->extra.tmd->coords->coord.t[1] = 0x62C;
        work->field_ECC[6]->task->extra.tmd->coords->coord.t[2] = 0x5DC;
        work->field_ECC[6]->task->extra.tmd->flags              = 0;
    }

    work->field_F0C    = 0x3C;
    freeCoord          = &work->field_E3C.c;
    work->field_ECC[6] = NULL;
    work->field_F04    = 0;
    work->field_F06    = 0;
    work->field_F08    = 0;
    work->field_F0A    = 0x32;
    work->field_7F2    = 0;

    work->field_E3C.c.parent      = task->extra.tmd->coords;
    work->field_E3C.ident.m00_m01 = 0x1000;
    mtx                           = (OverlayMat*)&work->field_E3C.c.coord;
    mtx->ident.m02_m10            = 0;
    mtx->ident.m11_m12            = 0x1000;
    mtx->ident.m20_m21            = 0;
    mtx->ident.m22                = 0x1000;
    work->field_E3C.c.coord.t[0] = work->field_E3C.c.coord.t[1] = work->field_E3C.c.coord.t[2] = 0;
    work->field_E3C.c.composeStamp                                                             = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(freeCoord);

    work->d4rec.ends[1].vz    = 0x1B58;
    recs2                     = work->recs2;
    work->d4rec.end0Radius    = 0x258;
    work->d4rec.end1Radius    = 0x258;
    work->d4rec.ends[0].vx    = 0;
    work->d4rec.ends[0].vy    = 0;
    work->d4rec.ends[0].vz    = 0;
    work->d4rec.ends[1].vx    = 0;
    work->d4rec.ends[1].vy    = 0;
    work->d4rec.contacts      = recs2;
    work->obj.coord           = freeCoord;
    work->obj.context.capsule = &work->d4rec;
    work->obj.pos.vx          = 0;
    work->obj.pos.vy          = -0xFA;
    work->obj.pos.vz          = 0x25F;
    work->obj.key             = 0x30000 | 0x20;
    work->obj.radius          = 0;
    work->obj.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(recs2, 5, 0);
    work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    escorts                   = task->work;
    task->extra.tmd->lightMtx = &escorts->lightMtx;
    task->extra.tmd->colorMtx = &escorts->colorMtx;
    for (j = 0; j < 7; j++) {
        esc = escorts->field_ECC[j];
        if (esc != NULL) {
            escTask                      = esc->task;
            escTask->extra.tmd->lightMtx = &escorts->lightMtx;
            escTask->extra.tmd->colorMtx = &escorts->colorMtx;
        }
    }

    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    incinBossTickAnim(task);

    D_actor_403200_8015F8F4.context.loc.stage = 0;
    D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
    D_actor_403200_8015F8F4.command           = 0;
    Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);

    work->field_E94 = work->field_E96 = 0x9C4;
    work->field_E98                   = 0x190;
    D_actor_403200_8015F8F0           = task;
    work->field_F1B = work->field_F1C = 0;
    task->state                      += 1;
}

/// The group-0 hit handler: takes at most one hit this frame and turns it into
/// damage.
///
/// It carves a 0x30-byte `Actor403200HitScratch` off the scratchpad stack and
/// scans the five `WorldCollisionContact` records of `hits[0]` for the first whose `key`
/// high halfword is attack kind 2 -- the contact point goes into the frame's
/// `pos` and the id is kept. A record with `key` 0 ends the scan with no
/// hit. The scan is written with labels rather than a `for` so `loop.c` parks
/// the match arm out of line; the same shape as
/// `func_actor_444000_8013C060`'s.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C, and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires. The contact point is re-read relative to the
/// part's world translation and `ratan2` of the pair against the part's facing
/// gives the yaw `angle`, wrapped to +/-0x800. The damage is doubled, applied
/// through `func_800E2C78` and `func_800DA6E8`, and the host's remaining HP is
/// mirrored onto the three escorts sharing its pool.
///
/// The second arm runs the same tick when `reactionFlags` has damage over time
/// set, which `Gp_TickObjFlag4` turns into damage of its own; that one only comes off the
/// host.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads (the scheduler cannot hoist a load
/// past a store through an unknown pointer). Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
static void func_actor_403200_80139A60(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    Enemy*                 enemy;
    WorldCollisionContact* recs;
    PlayerStatus*          cfg;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    i;
    s16                    param;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg   = &gPlayerStatus;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor403200Work*)arg0->work;
    sc    = (Actor403200HitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200HitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].key.value == 0) {
        goto missed;
    }
    if ((recs[i].key.value & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key.value;
        goto found;
    }
    i++;
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        incinBossHitEffect(work->hits[0].obj.coord, id);
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);
        if (Gp_RollEnemyChance(enemy, sc->id, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
            sc->rot.vy = 0x320;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, &enemy->task->extra.tmd->coords[3], 3, &sc->rot);
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(arg0->extra.tmd->coords);
        sc->rot.vx = sc->pos.vx - arg0->extra.tmd->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - arg0->extra.tmd->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - arg0->extra.tmd->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                       arg0->extra.tmd->coords->workm.m[2][2]);
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        sc->angle = angle;

        work->field_7C8 = 0;
        work->field_7C4 = 0;
        sc->damage     *= 2;
        func_800E2C78(enemy, sc->id, sc->damage, 0);
        enemy->hp -= sc->damage;
        func_800DA6E8(&enemy->node, sc->damage, 0);
        esc3     = work->field_ECC[3];
        hp       = enemy->hp;
        esc0     = work->field_ECC[0];
        esc1     = work->field_ECC[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
    }

    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        sc->damage = Gp_TickObjFlag4(enemy);
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
        if (sc->damage != 0) {
            func_800E2C78(enemy, sc->id, sc->damage, 0);
            enemy->hp -= sc->damage;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200HitScratch));
}

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `func_actor_403200_80139A60` runs for group 0, done twice: group 1 first, and
/// group 2 only if nothing landed on group 1. The second scan carries its own
/// `recs2` / `pos2` / `i2`, because sharing `recs` / `pos` / `i` with the first
/// gives both loops one pseudo each and the wrong registers. Both scans are
/// written as real `for` loops rather than the group-0 handler's labels so
/// `find_and_verify_loops` parks the match arm out of line.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires, and zeroed unless the attack kind came back 2.
/// The damage also comes off the work block's `field_F0E` pool and the host's
/// remaining HP is mirrored onto the three escorts sharing its pool.
/// `sc->angle` is the yaw of the contact point relative to the fourth escort's
/// facing, wrapped to +/-0x800.
///
/// The attack kind drives a sub-state change: kinds 4 and 6 roll `gRandomLcgState`
/// and take the boss out of state 3 into 8 one time in six, kind 2 does it
/// outright, and both are gated on the `field_F1C` re-arm countdown.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads. Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
///
/// The `do` / `while (0)` around the angle wrap is load-bearing, not stylistic.
/// Its body sits at loop depth 1, so `flow.c`'s `REG_N_REFS (regno) +=
/// loop_depth` gives `sc` one reference more than the unwrapped form (37
/// against 36, `work` sitting at 37 on a longer live range). That is what ranks
/// `sc` above `work` in global-alloc and puts it in `$s1`; unwrapped the two
/// exchange registers and the function stops at 99.06%.
static void func_actor_403200_80139E94(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    Enemy*                 host;
    PlayerStatus*          cfg;
    GfxCoord*              coord;
    WorldCollisionContact* recs;
    WorldCollisionContact* recs2;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    param;
    u16                    roll;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg  = &gPlayerStatus;
    host = (Enemy*)arg0->spawnArg2.pointer;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key.value == 0) {
            goto missed1;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key.value;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[1].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[2].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key.value == 0) {
            goto missed2;
        }
        if ((recs2[i2].key.value & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key.value;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    coord = work->hits[2].obj.coord;
hit:
    incinBossHitEffect(coord, id);
    if (sc->id != 0) {
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
        switch (Gp_GetIdParam0(sc->id) & 0xFFFF) {
            case 0:
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 9:
                break;

            case 4:
            case 6:
                state = work->field_0;
                if (state != 3) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    roll            = (gRandomLcgState >> 16) % 6;
                    if (roll == 0 && work->field_F1C == 0) {
                        work->field_0 = 8;
                        work->field_2 = -1;
                    }
                }
                break;

            case 2:
                if (work->field_F1C == 0 && (state = work->field_0, state != 3)) {
                    work->field_0 = 8;
                    work->field_2 = -1;
                }
                break;
        }

        sc->delta.vx = cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, work->field_ECC[3]->task->extra.tmd->coords, 0, &sc->rot);
            if (work->field_0 != 9 && work->field_F1C == 0) {
                work->field_0 = 8;
                work->field_2 = -1;
            }
            sc->damage *= 4;
        } else if ((Gp_GetIdParam0(sc->id) & 0xFFFF) != 2) {
            sc->damage = 0;
        }

        func_800E2C78(host, sc->id, sc->damage, 0);
        host->hp -= sc->damage;
        func_800DA6E8(&work->field_ECC[3]->node, sc->damage, 0);
        work->field_F0E                                          -= sc->damage;
        esc3                                                      = work->field_ECC[3];
        hp                                                        = host->hp;
        esc0                                                      = work->field_ECC[0];
        esc1                                                      = work->field_ECC[1];
        esc3->hp                                                  = hp;
        esc1->hp                                                  = hp;
        esc0->hp                                                  = hp;
        work->field_ECC[3]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(work->field_ECC[3]->task->extra.tmd->coords);
        sc->rot.vx = sc->pos.vx - work->field_ECC[3]->task->extra.tmd->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - work->field_ECC[3]->task->extra.tmd->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - work->field_ECC[3]->task->extra.tmd->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                       arg0->extra.tmd->coords->workm.m[2][2]);
        do {
            sc->angle = angle;
            if (angle < 0) {
            wrapUp:
                if (angle < -0x800) {
                    angle += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (angle > 0x800) {
                    angle -= 0x1000;
                    goto wrapDown;
                }
            }
        } while (0);
        sc->angle = angle;

        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200HitScratch));
}

/// The hit handler for collision groups 3, 4 and 5 -- `func_actor_403200_80139E94`
/// done three times over the parts it does not cover, each group only scanned
/// when the previous one landed nothing and the part it hit reported no attack
/// id back. Like the sibling actor's `func_actor_444000_8013CA60`, this one runs
/// no `Gp_GetIdParam0` switch: the call is made and its kind thrown away, so
/// every hit is treated alike, and the group 3 arm is the one that gives up and
/// leaves the frame once it comes back empty.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the two escorts sharing its pool and `field_F0A`.
/// Emptying that pool spawns the same effect again and refills it to 0x32. Both
/// effect spawns and the state change to 0xE are skipped while the boss is in
/// one of the seven states that ignore hits, while the player hold is armed, or
/// unless `gSceneCombatState.battleRefs` is 1.
///
/// `pos` / `pos2` / `pos3` are all `&sc->pos`, and are not spare: each group's
/// scan writes the contact point through its own pointer, which is what keeps
/// the three `sh` pairs in `a3` then `a2` twice. `esc3` / `esc0` / `esc1` and
/// the `hp` load are the sibling's arrangement, but evaluated before
/// `func_800DA6E8` so `host->field_40` is still in a register and the three
/// stores reuse it; the pool subtraction after them carries the same `field_40`
/// value for the same reason.
static void func_actor_403200_8013A4A0(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    Enemy*                 host;
    PlayerStatus*          cfg;
    WorldCollisionContact* recs;
    WorldCollisionContact* recs2;
    WorldCollisionContact* recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;
    s16                    param;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg  = &gPlayerStatus;
    host = (Enemy*)arg0->spawnArg2.pointer;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key.value == 0) {
            goto missed1;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key.value;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        incinBossHitEffect(work->hits[3].obj.coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key.value == 0) {
            goto missed2;
        }
        if ((recs2[i2].key.value & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key.value;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        incinBossHitEffect(work->hits[4].obj.coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key.value == 0) {
            goto missed3;
        }
        if ((recs3[i3].key.value & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key.value;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    incinBossHitEffect(work->hits[5].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    param           = Gp_GetIdParam2(sc->id);
    work->field_E90 = param;
    work->field_E8E = param;
    work->field_E8C = param;
    work->field_E92 = param;
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0]) + 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[0], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 &&
        gSceneCombatState.battleRefs == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &work->field_ECC[0]->task->extra.tmd->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78(host, sc->id, sc->damage, 0);
    host->hp        -= sc->damage;
    esc3             = work->field_ECC[3];
    hp               = host->hp;
    esc0             = work->field_ECC[0];
    esc1             = work->field_ECC[1];
    esc3->hp         = hp;
    esc1->hp         = hp;
    esc0->hp         = hp;
    work->field_F0A -= sc->damage;
    if (work->field_F0A <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && gSceneCombatState.battleRefs == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &work->field_ECC[0]->task->extra.tmd->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = 0x32;
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    work->field_ECC[0]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(work->field_ECC[0]->task->extra.tmd->coords);
    sc->rot.vx = sc->pos.vx - work->field_ECC[0]->task->extra.tmd->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - work->field_ECC[0]->task->extra.tmd->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - work->field_ECC[0]->task->extra.tmd->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                   arg0->extra.tmd->coords->workm.m[2][2]);
    do {
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
    } while (0);
    sc->angle = angle;

    work->field_7C8 = 0;
    work->field_7C4 = 0;
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200HitScratch));
}

/// The hit handler for collision groups 6, 7 and 8 -- the same three-scan shape
/// as `func_actor_403200_8013A4A0` runs for groups 3, 4 and 5, with the next
/// group only scanned when the previous one landed nothing and the part it hit
/// reported no attack id back. Like the sibling actor's
/// `func_actor_444000_8013D128`, the first two groups share one call site
/// through `coord`, and `Gp_GetIdParam0` is called and its kind thrown away.
///
/// Damage is the distance-scaled hit -- measured from an offset point rather
/// than the model origin -- quadrupled when `Gp_RollEnemyChance` fires, then
/// divided by six (never down to zero unless it already was), and comes off the
/// host, the two escorts sharing its pool and `field_F0C`. Emptying that pool
/// spawns the same effect again and refills it to 0x3C. Both effect spawns and
/// the state change to 0xE are skipped while the boss is in one of the seven
/// states that ignore hits, while the player hold is armed, or while
/// `gSceneCombatState.battleRefs` is not 1.
///
/// The second escort carries the damage and the effect, but `sc->angle` is the
/// yaw of the contact point relative to the first escort's facing. `pos` /
/// `pos2` / `pos3` are all `&sc->pos` and are not spare: each group's scan
/// writes the contact point through its own pointer. `esc3` / `esc0` / `esc1`
/// and the `hp` load sit after `func_800DA6E8`, unlike the group 3-5 handler.
static void func_actor_403200_8013AB70(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    Enemy*                 host;
    PlayerStatus*          cfg;
    GfxCoord*              coord;
    WorldCollisionContact* recs;
    WorldCollisionContact* recs2;
    WorldCollisionContact* recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;
    s16                    param;
    u16                    hp;
    Enemy*                 esc3;
    Enemy*                 esc0;
    Enemy*                 esc1;

    cfg  = &gPlayerStatus;
    host = (Enemy*)arg0->spawnArg2.pointer;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[6].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key.value == 0) {
            goto missed1;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key.value;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[6].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[7].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key.value == 0) {
            goto missed2;
        }
        if ((recs2[i2].key.value & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key.value;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[7].obj.coord;
    hit:
        incinBossHitEffect(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[8].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key.value == 0) {
            goto missed3;
        }
        if ((recs3[i3].key.value & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key.value;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    incinBossHitEffect(work->hits[8].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    param           = Gp_GetIdParam2(sc->id);
    work->field_E90 = param;
    work->field_E8E = param;
    work->field_E8C = param;
    work->field_E92 = param;
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0]) - 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[1], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 &&
        gSceneCombatState.battleRefs == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &work->field_ECC[1]->task->extra.tmd->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78(host, sc->id, sc->damage, 0);
    host->hp        -= sc->damage;
    work->field_F0C -= sc->damage;
    if (work->field_F0C <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && gSceneCombatState.battleRefs == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &work->field_ECC[1]->task->extra.tmd->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0C = 0x3C;
    }

    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
    esc3                                                      = work->field_ECC[3];
    hp                                                        = host->hp;
    esc0                                                      = work->field_ECC[0];
    esc1                                                      = work->field_ECC[1];
    esc3->hp                                                  = hp;
    esc1->hp                                                  = hp;
    esc0->hp                                                  = hp;
    work->field_ECC[1]->task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(work->field_ECC[1]->task->extra.tmd->coords);
    sc->rot.vx = sc->pos.vx - work->field_ECC[0]->task->extra.tmd->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - work->field_ECC[0]->task->extra.tmd->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - work->field_ECC[0]->task->extra.tmd->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-arg0->extra.tmd->coords->workm.m[2][0],
                   arg0->extra.tmd->coords->workm.m[2][2]);
    do {
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
    } while (0);
    sc->angle = angle;

    work->field_7C8 = 0;
    work->field_7C4 = 0;
out:
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200HitScratch));
}

/// Reset handler: pushes the host model's `field_C` onto each of the seven
/// escorts, and once the sub-state counter has reached 2 releases the host's and
/// every escort's model buffers. Same shape as
/// `func_actor_403200_80141B40` with a second arm keyed on `field_6`.
///
/// The `modelFlag` copy is not redundant: the second arm's `0x80` has to reach
/// the store as a 32-bit value of its own, or the two arms merge it into the
/// first arm's constant and the second `li $v0, 0x80` disappears.
static void func_actor_403200_8013B23C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    TmdObject*       tmd;
    s32              flag;
    s32              modelFlag;
    s16              i;
    s16              j;

    work = (Actor403200Work*)arg0->work;
    tmd  = arg0->extra.tmd;
    if (work->field_4 != 0) {
        tmd->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        escorts                = (Actor403200Work*)arg0->work;
        i                      = 0;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = (flag = TMD_OBJECT_SKIP_ACTIVE_DRAW);
        for (; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        work->field_6 = 0;
        return;
    }
    if (work->field_6 == 2) {
        tmd->flags             = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        escorts                = (Actor403200Work*)arg0->work;
        modelFlag              = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        i                      = 0;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = (flag = modelFlag);
        for (; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_FreeBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_FreeBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
    }
}

/// State-change reset for the enemy's launch state: `func_actor_403200_8013B23C`'s
/// reset half with a yaw servo in the middle. It arms the stand-up pair
/// (`field_F1D` 2, `field_7B3` 3), turns animation slot 2 on, clears the host
/// model's flag word and walks the seven escorts pushing that word onto each of
/// their models, allocates the host's and every escort's buffers, and only then
/// turns the enemy to face the player -- the host root part's position made
/// relative to the player's root coordinate, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `field_7C4`. On the way out it
/// runs the per-frame body, re-arms the state to 0xA on the animation slot's
/// flag, and latches `field_F06` once the state counter is past 0x14.
///
/// The switch is on the state counter and spawns from
/// `D_actor_403200_8015E858`, each of the eight counter values picking its own
/// table index; the spawned enemy is dropped, unlike the arena reset's. The
/// `state` copy is what keeps the switch index 16-bit, as in
/// `func_actor_403200_8013D9EC`.
static void func_actor_403200_8013B3C8(Task* arg0)
{
    Actor403200Work*        work;
    Actor403200Work*        escorts;
    Actor403200Work*        dying;
    GfxCoord*               model;
    GfxCoord*               facing;
    Actor403200TurnScratch* sc;
    s16                     i;
    s16                     j;
    s16                     state;
    s16                     ang;

    sc   = (Actor403200TurnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200TurnScratch));
    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        work->field_F1D        = 2;
        work->field_7B3        = 3;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFA = 0;
    }
    state = work->field_6 - 0x13;
    switch (state) {
        case 0:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 0, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 7:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 1, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 9:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 2, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x10:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 3, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x1F:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 4, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x37:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 5, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x3B:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 6, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
        case 0x3F:
            Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 1, 7, arg0->spawnArg2.pointer)->workType = ENEMY_WORK_PLAIN;
            break;
    }
    incinBossTickAnim(arg0);
    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 1;
    }
    model      = arg0->extra.tmd->coords;
    sc->dir.vx = gPlayerStatus.coordMtx->t[0] - model->coord.t[0];
    sc->dir.vy = gPlayerStatus.coordMtx->t[1] - model->coord.t[1];
    sc->dir.vz = gPlayerStatus.coordMtx->t[2] - model->coord.t[2];
    facing     = arg0->extra.tmd->coords;
    ang        = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (ang < 0) {
    wrapUp:
        if (ang < -0x800) {
            ang += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (ang > 0x800) {
            ang -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = ang;
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200TurnScratch));
}

/// Spawns up to nine enemies in a randomly selected formation, stopping when
/// a spawn fails. Each member's index becomes the high nibble of its place key.
static void func_actor_403200_8013B740(Task* arg0)
{
    Actor403200Work* work;
    Enemy*           enemy;
    s16              i;
    s16              formation;

    work = (Actor403200Work*)arg0->work;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    formation       = (gRandomLcgState >> 16) & 3;
    if (formation == 3) {
        formation = 0;
    }

    for (i = 0; i < 9; i++) {
        D_actor_403200_8015E858[4].data.model = D_actor_403200_8015F888[i].model;
        enemy                                 = Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 4, D_actor_403200_8015F888[i].spawnArg, NULL);
        work->field_EF0                       = enemy;
        if (enemy == NULL) {
            break;
        }
        enemy->task->extra.tmd->coords->coord.t[0]           = D_actor_403200_8015F7B0[formation][i].vx;
        work->field_EF0->task->extra.tmd->coords->coord.t[1] = D_actor_403200_8015F7B0[formation][i].vy;
        work->field_EF0->task->extra.tmd->coords->coord.t[2] = D_actor_403200_8015F7B0[formation][i].vz;
        work->field_EF0->workType                            = ENEMY_WORK_PLAIN;
        work->field_EF0->placeKey                           |= i << ENEMY_PLACE_INDEX_SHIFT;
    }
}

/// Per-frame body of the launch state's pull. A state change re-arms the
/// escorts and tells the scene (message 0x7DA, action 0x2C). Each tick yaws
/// the enemy toward the player and scales a pull from the animation frame;
/// inside the swipe window, once the player accepts message 0x3F8, it places
/// them (0x3E9) and hands over an animation (0x3F4).
static void func_actor_403200_8013B8C4(Task* arg0)
{
    Actor403200Work*        work;
    Actor403200Work*        escorts;
    Actor403200Work*        dying;
    Enemy*                  enemy;
    Task*                   task;
    PlayerStatus*           cfg;
    Actor403200DragScratch* sc;
    s16                     i;
    s16                     j;

    work  = (Actor403200Work*)arg0->work;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg   = &gPlayerStatus;
    sc    = (Actor403200DragScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200DragScratch));

    if (work->field_4 != 0) {
        work->field_F1D        = 3;
        work->field_7B3        = 3;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EFE            = 0;
        work->field_EF4            = 1;
        work->field_EF6            = 1;
        work->field_F04            = 0;
        work->field_EFA            = 0;
        gIncinBossSpinnerTarget.vz = 0;
        gIncinBossSpinnerTarget.vy = 0;
        gIncinBossSpinnerTarget.vx = 0;
        actorLocalToView(&arg0->extra.tmd->coords[3], &gIncinBossSpinnerTarget);
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 2;
        Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        {
            s16 armed                  = 1;
            work->field_E96            = 0xC80;
            gIncinBossSpinnersReleased = armed;
        }
    }

    incinBossTickAnim(arg0);

    work->field_7C4 = actorPositionYaw(arg0, &sc->dir, &gPlayerStatus);

    sc->dir.vz = 0;
    sc->dir.vy = 0;
    sc->dir.vx = 0;
    actorLocalToView(&arg0->extra.tmd->coords[4], &sc->dir);

    sc->dir.vx = (u16)task->extra.tmd->coords->coord.t[0] - (u16)sc->dir.vx;
    sc->dir.vy = (u16)task->extra.tmd->coords->coord.t[1] - (u16)sc->dir.vy;
    sc->dir.vz = (u16)task->extra.tmd->coords->coord.t[2] - (u16)sc->dir.vz;
    sc->dist   = sc->dir.vx * sc->dir.vx;
    sc->dist  += sc->dir.vz * sc->dir.vz;
    sc->dist   = SquareRoot0(sc->dist);
    VectorNormalSS(&sc->dir, &sc->dir);

    switch (work->field_F08) {
        case 0:
            sc->period = 0x14;
            break;
        case 1:
            sc->period = 0x10;
            break;
        case 2:
        default:
            sc->period = 0xC;
            break;
    }
    if (((u32)((work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xA) < 9U) && ((work->field_6 % sc->period) == 0)) {
        Gp_SpawnScript18(D_actor_403200_80141C7C, D_actor_403200_80141C88);
    }

    switch (work->field_F08) {
        case 0:
            sc->pull = 0;
            break;
        case 1:
            sc->pull = 5;
            break;
        case 2:
        default:
            sc->pull = 0xA;
            break;
    }

    if (work->field_6 == 0xA) {
        s32 sfx;
        s32 pan;

        sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_6 == 0x3C) {
        s32 sfx;
        s32 pan;

        sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->field_6 == 0xE8) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
    }

    work->field_EFA = 1;
    switch (work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) {
        case 9:
            gte_lddp(-(sc->pull + 0x19) / 4);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x180;
            break;
        case 10:
            gte_lddp(-(sc->pull + 0x19) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            break;
        case 11:
        case 13:
        case 15:
            gte_lddp(-(sc->pull + 0x19));
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x2B2;
            break;
        case 12:
        case 14:
            gte_lddp(-((sc->pull + 0x19) * 3) / 2);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x500;
            break;
        case 16:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x100;
            break;
        case 17:
        case 18:
            gte_lddp(-(sc->pull + 0x19) / 3);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0x400;
            break;
        case 19:
        case 20:
            sc->dir.vz = 0;
            sc->dir.vx = 0;
            gte_lddp(-(sc->pull + 0x19) / 6);
            gte_ldsv(&sc->dir);
            gte_gpf12();
            gte_stsv(&sc->dir);
            work->field_EFE = 0;
            break;
        default:
            work->field_EFA = 0;
            sc->dir.vz      = 0;
            sc->dir.vx      = 0;
            break;
    }

    if (((u32)((work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - 0xB) < 5U) && (sc->dist < 0x4B0) && (enemy->hp > 0) &&
        (Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &D_actor_403200_8015FA00, 0) == 0)) {
        SVECTOR* dirp;
        s16      ang;

        work->field_0   = 0xD;
        work->field_EC8 = 1;
        sc->pos.vz      = 0;
        sc->pos.vy      = 0;
        sc->pos.vx      = 0;
        actorLocalToView(&arg0->extra.tmd->coords[4], &sc->pos);

        sc->dir.vx      = (u16)task->extra.tmd->coords->coord.t[0] - (u16)sc->pos.vx;
        sc->dir.vy      = 0;
        sc->dir.vz      = (u16)task->extra.tmd->coords->coord.t[2] - (u16)sc->pos.vz;
        ang             = actorViewYaw(arg0->extra.tmd->coords, &sc->dir);
        dirp            = &sc->dir;
        work->field_7C4 = ang;
        VectorNormalSS(dirp, dirp);
        gte_lddp(0x384);
        gte_ldsv(dirp);
        gte_gpf12();
        gte_stsv(dirp);

        D_actor_403200_8015F9C0.value.pos.vx = sc->pos.vx + sc->dir.vx;
        D_actor_403200_8015F9C0.value.pos.vy = task->extra.tmd->coords->coord.t[1];
        {
            s32 pz = sc->pos.vz;
            s32 dz = sc->dir.vz;

            D_actor_403200_8015F9C0.value.rot.vx = 0;
            D_actor_403200_8015F9C0.value.rot.vz = 0;
            D_actor_403200_8015F9C0.value.pos.vz = pz + dz;
        }
        {
            u16 px = (u16)sc->pos.vx;
            u16 mx = (u16)D_actor_403200_8015F9C0.value.pos.vx;

            sc->dir.vy = 0;
            sc->dir.vx = px - mx;
        }
        sc->dir.vz = (u16)sc->pos.vz - (u16)D_actor_403200_8015F9C0.value.pos.vz;
        ang        = actorViewYaw(task->extra.tmd->coords, dirp);
        {
            s32 ext = ang;

            sc->angle = ext;
            if (abs(ext) < 0x400) {
                D_actor_403200_8015F9C0.value.rot.vy = ratan2((s32)sc->dir.vx, (s32)sc->dir.vz);
                work->anim.source.sets               = D_actor_403200_8015E6AC;
            } else {
                D_actor_403200_8015F9C0.value.rot.vy = ratan2((s32)sc->dir.vx, (s32)sc->dir.vz) + 0x800;
                work->anim.source.sets               = D_actor_403200_8015E6CC;
            }
        }
        if (cfg->hp > 0) {
            Gp_DispatchMsgPtr(task, 0x3E9, &D_actor_403200_8015F9C0.value, 0);
        }
        work->anim.animationId = 1;
        work->anim.blend       = ANIMATION_BLEND_RESET;
        work->anim.blendFrames = 0;
        work->field_F02        = 1;
        Gp_DispatchMsgPtr(task, 0x3F4, &work->anim, 0);
        work->field_7CA = 0;
    }

    if (sc->dir.vx != 0 || sc->dir.vz != 0) {
        sc->push.vx = sc->dir.vx;
        sc->push.vy = 0;
        sc->push.vz = sc->dir.vz;
        func_80105B74(&sc->push);
    }

    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 3;
        Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        gIncinBossSpinnersReleased = 0;
        work->field_7F2            = 0;
        work->field_0              = 0xA;
        for (sc->i = 0; sc->i < 2; sc->i++) {
            work->field_EE8[sc->i] = NULL;
        }
        work->field_F1C = 0;
    }
    if (work->field_6 == 0x14) {
        work->field_F06 = 2;
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200DragScratch));
}

/// State-change reset for the enemy's launch state, and the tick that walks it
/// out of sub-state 0xF into 0xE.
///
/// The reset half is `func_actor_403200_8013B23C`'s with a yaw servo in the
/// middle: it tells the scene (message 0x7DA, action 0x2C), arms sub-state 0xF
/// with animation 2, clears the host model's flag word and walks the seven
/// escorts pushing that word onto each of their models, allocates the host's and
/// every escort's buffers, and only then turns the enemy to face the player --
/// the fourth model part's position carried into view space, made relative to
/// the player's root coordinate with y zeroed, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `field_7C4`. It tells the scene
/// a second time, arms `field_E96`, raises bit 0 of `Gp_StateC08.field_6`,
/// pulses the state and clears the node slot of the host and of escorts 3, 0
/// and 1.
///
/// The tick runs the per-frame body, steps 0xF to 0xE on the second animation
/// slot's flag, and while still in 0xF hands the player the launch message
/// (0x3F9) with `gPlayerStatus.hp` as its gate: the two arms either side
/// of that dispatch write the ramp timings into `gGameSession` and stamp escort
/// 3. The four one-shot cues all latch on the third animation slot's frame,
/// masked to ten bits, against the frame `field_7A8` saw last, and once the
/// state counter is past 0x18 the type-7 cue and the 0x3FF animation message go
/// out together.
///
/// Three things here are load-bearing. The yaw's arguments are read through
/// `posp` and the matrix half through `coord`: read straight off `view` the
/// stores would be forwarded into both arguments (two `sll`/`sra` pairs),
/// while through the pointer each stays a load out of the struct, which is what
/// the target does -- the second is reloaded from its slot, the first is folded
/// back onto `a0`, and `coord` is what keeps `field_8` in `s0` across the call.
/// The cue locals are declared inside each arm so local-alloc colours them per
/// block; hoisted to the top of the function they become one global pseudo and
/// the id and pan come out in each other's registers. And in the second 0x7DA
/// block `D_actor_403200_8015F8E0[0]` is cleared before the `field_7C4` store, so
/// its address is the one computed first.
static void func_actor_403200_8013C84C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    Enemy*           enemy;
    Task*            task;
    PlayerStatus*    cfg;
    SVECTOR          view;
    SVECTOR*         posp;
    GfxCoord*        coord;
    s16              i;
    s16              j;
    s16              yaw;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg   = &gPlayerStatus;
    if (work->field_4 != 0) {
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 3;
        Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        gIncinBossSpinnersReleased = 0;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        work->field_7B3        = 0xF;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EF6 = 1;
        work->field_F06 = 6;
        work->field_EFE = 0;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        view.vz         = 0;
        view.vy         = 0;
        view.vx         = 0;
        actorLocalToView(&arg0->extra.tmd->coords[4], &view);
        view.vx = task->extra.tmd->coords[0].coord.t[0] - view.vx;
        view.vy = 0;
        view.vz = task->extra.tmd->coords[0].coord.t[2] - view.vz;
        posp    = &view;
        coord   = arg0->extra.tmd->coords;
        yaw     = ratan2(posp->vx, posp->vz) -
              ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (yaw < 0) {
        wrapUp:
            if (yaw < -0x800) {
                yaw += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (yaw > 0x800) {
                yaw -= 0x1000;
                goto wrapDown;
            }
        }
        work->field_7C4                           = yaw;
        D_actor_403200_8015F8E0[0]                = 0;
        D_actor_403200_8015F8F4.context.loc.stage = 0;
        D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command           = 3;
        Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
        work->field_E96            = 0x9C4;
        gIncinBossSpinnersReleased = 0;
        Gp_StateC08.field_6       |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        return;
    }

    SCRATCH_STACK_RESERVE_BYTES(0x3C);
    incinBossTickAnim(arg0);
    if ((work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) && (work->field_7B3 == 0xF)) {
        work->field_7B0 = 2;
        work->field_7B3 = 0xE;
    }
    if (work->field_7B3 == 0xF) {
        if (cfg->hp > 0) {
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F9, Gp_PackObjPair(enemy, 3), 0);
            if (cfg->hp <= 0) {
                ((GameActor*)task->work)->state   = 0xA;
                gGameSession->deathSoundCountdown = 0x1E;
                gGameSession->deathFadeFrames     = 0x36;
                gGameSession->deathRestartDelay   = 0x5A;
            }
        }
        if (((work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x19) && (work->field_7A8 != (work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 sfx;
            s32 pan;
            s32 depth;

            sfx   = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200011;
            pan   = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            depth = (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sfx, pan, depth);
        }
        work->field_7A8 = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->field_7B3 == 0xE) {
        if (((work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1E) && (work->field_7A8 != (work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x23) && (work->field_7A8 != (work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x27) && (work->field_7A8 != (work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200012;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        work->field_7A8 = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if ((Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) && (cfg->hp > 0)) {
        D_actor_403200_8015F9C0.value.pos.vx = arg0->extra.tmd->coords[0].coord.t[0];
        D_actor_403200_8015F9C0.value.pos.vy = arg0->extra.tmd->coords[0].coord.t[1];
        D_actor_403200_8015F9C0.value.pos.vz = arg0->extra.tmd->coords[0].coord.t[2];
        Gp_DispatchMsgPtr(task, 0x3E9, &D_actor_403200_8015F9C0.value, 0);
        D_actor_403200_8015F8E0[0] = 1;
    }
    if (work->field_6 < 0x18) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        Gp_DispatchMsgPtr(task, 0x3FF, &work->anim, 0);
        work->field_7CA = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x3C);
}

/// State-change reset for the enemy's stand-up, plus the swipe tick that runs
/// on every step afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers,
/// rebuilds the free coordinate `field_E3C` from `field_7C8` and arms `field_E96`
/// at 0xC80 before playing the entry cue.
///
/// Every step then clears that coordinate's flag and updates it, and each of the
/// two swipe sub-states watches one animation slot's frame: sub-state 4 raises
/// bit 0x8000 of the tenth collision object's `flags` and fires its two cues once
/// `slots0[1]` reaches frame 0xC, sub-state 5 clears `field_EFA` and fires its
/// single cue on `slots0[2]` frame 0x1C. Both cues are positioned on the first
/// escort's second coordinate at half depth, and whichever sub-state is live is
/// the one whose frame `field_7AC` is refreshed from -- the shared mask is what
/// makes the pair one-shot. The switch on `field_6` arms the escort pose index
/// `field_7A4` for seven states, 0x14 and 0xDC also seeding the shared countdown
/// `gIncinBossLimbReach` and re-arming `field_0`, and the 0x29..0x2E window
/// raises that countdown by 0x258 while it is still under 0x1770.
///
/// The tail runs the per-frame body, scans the tenth collision object's five
/// `recs2` records for one whose high half is 0x10000, and -- when it finds one,
/// the enemy's HP is positive and the player's 0x3F8 query comes back zero --
/// asks the player for the hold (0x3F9) and re-sends it the animation, stamping
/// the player's `field_956` when the hold was taken. Past frame 0x39 the shared
/// countdown is walked down 0x1E, or 0xC8 once it is past 0xBB9, and past 0x15
/// the state arms `field_F06`.
///
/// The countdown's two arms are load-bearing: the `>= 0xBB9` test reads the
/// halfword signed (`lh`) while each arm subtracts from it zero-extended
/// (`lhu`), and writing the pair as one assignment off a shared temp lets CSE
/// fold the compare onto the earlier zero-extended load, which costs an
/// `sll`/`sra` re-extension pair the original does not have.
static void func_actor_403200_8013D028(Task* arg0)
{
    Actor403200Work*       work;
    Actor403200Work*       escorts;
    Actor403200Work*       dying;
    WorldCollisionContact* recs;
    Enemy*                 enemy;
    Task*                  task;
    Task*                  target;
    s16                    i;
    s16                    j;
    s16                    k;
    s16                    frame;
    s16                    frame2;
    s16                    reply;
    s32                    found;
    s32                    resetId;
    s32                    resetPan;
    s32                    swipeId;
    s32                    swipePan;
    s32                    swipe2Id;
    s32                    swipe2Pan;
    s32                    hitId;
    s32                    hitPan;
    s32                    cueId;
    s32                    cuePan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    task  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    SCRATCH_STACK_RESERVE_BYTES(0x30);

    if (work->field_4 != 0) {
        work->field_F1D        = 0xB;
        work->field_7B3        = 4;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 1;
        work->field_EF8 = 1;
        gfxRotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&work->field_E3C.c);
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan        = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    work->field_E3C.c.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->field_E3C.c);

    if (work->field_7B3 == 4 && (frame = work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0xC &&
        work->field_7AC != frame) {
        work->field_EAC  = 3;
        work->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);
        swipeId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200019;
        swipePan = (s8)worldCoordGetOriginAudioPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                 2));
        swipe2Id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001A;
        swipe2Pan = (s8)worldCoordGetOriginAudioPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                 2));
    } else {
        work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    if (work->field_7B3 == 5 && (frame2 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EFA = 0;
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x7F, 8);
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020001B;
        hitPan = (s8)worldCoordGetOriginAudioPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(worldCoordGetOriginAudioDepth(
                     &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    } else {
        work->field_7AC = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }

    switch (work->field_6) {
        case 0x14:
            gIncinBossLimbReach = 0x640;
            work->field_7A4     = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200018;
            cuePan          = (s8)worldCoordGetOriginAudioPan(
                &work->field_ECC[0]->task->extra.tmd->coords[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(worldCoordGetOriginAudioDepth(
                         &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->field_7A4 = 2;
            break;
        case 0x38:
            work->field_7A4 = 4;
            break;
        case 0x44:
            work->field_7A4 = 3;
            break;
        case 0xDC:
            work->field_0 = 0xA;
            break;
    }

    if ((u32)((u16)work->field_6 - 0x29) < 6 && gIncinBossLimbReach < 0x1770) {
        gIncinBossLimbReach = (u16)gIncinBossLimbReach + 0x258;
    }

    incinBossTickAnim(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
        if (recs[k].key.value == 0) {
            goto missed;
        }
        if ((recs[k].key.value & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && enemy->hp > 0 &&
        Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &D_actor_403200_8015FA00, 0) == 0) {
        target          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        reply           = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair(enemy, 4), 0);
        work->field_ECA = reply;
        if (reply == 1) {
            ((GameActor*)task->work)->state = 0xA;
        }
        work->anim.source.sets = D_actor_403200_8015E6AC;
        work->field_EC8        = 1;
        work->anim.animationId = 2;
        work->anim.blend       = ANIMATION_BLEND_RESET;
        work->anim.blendFrames = 0;
        Gp_DispatchMsgPtr(task, 0x3FF, &work->anim, 0);
        work->field_7CA = 0;
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
        if (gIncinBossLimbReach >= 0xBB9) {
            gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0xC8;
        } else {
            gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0x1E;
        }
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 4;
    }

    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// State-change reset for the enemy's stand-up, and the height servo that runs
/// on every tick afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers
/// and then parks the root coordinate at x 0x1068, y 0x7D0, z -0x1770, arming
/// `field_E96` at 0xFA0.
///
/// The servo steps that root y by +0x50 / -0x64 while `field_6` is at or past
/// 0x3D, and by the gentler +0x14 / -0x1E while it is between 0x15 and 0x3D, so
/// the enemy eases back to the ground as it finishes standing up; below 0x15 it
/// stops moving. Which way each step goes is the frame's position inside its
/// group of four -- `frame % 4 < 2` on the `s16` local, whose 16-bit
/// truncation is what puts the `sll 16` / `sra 16` pair in front of the `slti`.
static void func_actor_403200_8013D78C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    s16              i;
    s16              j;
    s16              frame;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        work->field_7B3        = 1;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        arg0->extra.tmd->coords->coord.t[1] = 0x7D0;
        arg0->extra.tmd->coords->coord.t[0] = 0x1068;
        arg0->extra.tmd->coords->coord.t[2] = -0x1770;
        work->field_E96                     = 0xFA0;
    }
    incinBossTickAnim(arg0);
    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 1;
    }
    if (arg0->extra.tmd->coords->coord.t[1] > 0) {
        frame = work->field_6;
        if (frame >= 0x3D) {
            arg0->extra.tmd->coords->coord.t[1] +=
                ((frame % 4) < 2) ? 0x50 : -0x64;
        } else if (frame >= 0x15) {
            arg0->extra.tmd->coords->coord.t[1] +=
                ((frame % 4) < 2) ? 0x14 : -0x1E;
        }
    }
    if (arg0->extra.tmd->coords->coord.t[1] < 0) {
        arg0->extra.tmd->coords->coord.t[1] = 0;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// State-change reset for the enemy's stand-up. It clears the host model's flag
/// word, walks the seven escorts pushing that word onto each of their models,
/// allocates every escort's model buffers and then arms the block -- `field_EF6`
/// and `field_EF4` at 1, `field_EFA` at 0, `field_E96` at 0xC80 -- before
/// playing the type-6 cue built from the spawn record's `field_8`. Same shape as
/// `func_actor_403200_8013B23C`'s reset half, with the buffer allocator on the
/// second walk in place of the release.
///
/// The state then writes its two cue frames, and the three states at 0x39, 0x45
/// and 0x4C spawn `field_EF0` from `D_actor_403200_8015E858`; every other state
/// in the 0x39..0x4C window falls through to the dispatcher.
///
/// The `state` copy is what keeps the switch index 16-bit: switched on
/// `field_6 - 0x39` directly the index is an `int`, and the `lh` the load
/// becomes carries the sign extension the original does with a separate
/// `sll`/`sra` pair (dropping 2 instructions and 2.8% of the match).
static void func_actor_403200_8013D9EC(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    Enemy*           enemy;
    Enemy*           spawned;
    s16              i;
    s16              j;
    s16              state;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_F1D        = 6;
        work->field_7B3        = 6;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFA = 0;
        work->field_E96 = 0xC80;
        sfx             = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan,
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    state = work->field_6 - 0x39;
    switch (state) {
        case 6:
            work->field_7B3 = 0xC;
            work->field_7B0 = 1;
            break;
        case 13:
            work->field_7B3 = 0xC;
            work->field_7B0 = 2;
            break;
        case 0:
        case 12:
        case 19:
            spawned           = Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 0, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->field_EF0   = spawned;
            break;
    }
    incinBossTickAnim(arg0);
    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 7;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 3;
    }
}

/// The state that rains debris on the arena `func_actor_403200_8013D9EC` opens.
///
/// A reset request re-arms the block on animation 0xB, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, and plays the
/// entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned on part 1 of
/// the first escort. From 0x3D on the state also drops debris: every third step
/// the shared scratch coordinate is rebuilt on that same part -- its rotation
/// accumulated up the parent chain, its origin carried into view space, then
/// turned a quarter turn each way so `Gfx_MatrixCol2` yields the launch
/// direction, which is normalised and scaled to 0x320 before being added to the
/// origin -- and an effect is spawned on it. Every tenth step a fresh enemy is
/// spawned from `D_actor_403200_8015E858` and remembered in `field_EF0`.
///
/// The tick then runs the per-frame body and hands over to state 0xA once the
/// second animation slot raises its flag.
static void func_actor_403200_8013DC3C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    Enemy*           enemy;
    Enemy*           spawned;
    SVECTOR          pos;
    SVECTOR*         posp;
    s16              i;
    s16              j;
    s32              resetId;
    s32              resetPan;
    s32              cueId;
    s32              cuePan;
    s32              hitId;
    s32              hitPan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_F1D        = 7;
        work->field_7B3        = 0xB;
        work->field_7B0        = 2;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
        resetPan        = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200016;
        cuePan = (s8)worldCoordGetOriginAudioPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            cueId, cuePan,
            (s8)worldCoordGetOriginAudioDepth(
                &work->field_ECC[0]->task->extra.tmd->coords[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D;
        hitPan = (s8)worldCoordGetOriginAudioPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)worldCoordGetOriginAudioDepth(
                &work->field_ECC[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 3) == 0) {
            actorAccumulateToView(
                &work->field_ECC[0]->task->extra.tmd->coords[1],
                &D_actor_403200_8015F920.coord);

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            actorLocalToView(&work->field_ECC[0]->task->extra.tmd->coords[1],
                             &pos);

            D_actor_403200_8015F920.parent     = &gGfxViewCoord;
            D_actor_403200_8015F920.coord.t[0] = pos.vx;
            D_actor_403200_8015F920.coord.t[1] = pos.vy;
            D_actor_403200_8015F920.coord.t[2] = pos.vz;
            gfxRotMatrixY(&D_actor_403200_8015F920.coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_403200_8015F920.coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_403200_8015F920.coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12();
            gte_stsv(posp);

            D_actor_403200_8015F920.coord.t[0]  += pos.vx;
            D_actor_403200_8015F920.coord.t[1]  += pos.vy;
            D_actor_403200_8015F920.coord.t[2]  += pos.vz;
            D_actor_403200_8015F920.composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&D_actor_403200_8015F920);
            Gp_SpawnEff(0x60199, &D_actor_403200_8015F920, 0x97A0D680, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned           = Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 2, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->field_EF0   = spawned;
        }
    }

    incinBossTickAnim(arg0);

    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 5;
    }
}

static void func_actor_403200_8013E2FC(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    OverlayMat*      mtx;
    GfxCoord*        coords;
    s32              state;
    s32              frame;
    s16              i;
    s16              j;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        escorts                = (Actor403200Work*)arg0->work;
        work->field_7F3        = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_7B6 = 0x10;
        work->field_F04 = 1;
        work->field_7A4 = 0;
        work->field_E96 = 0xFA0;
    }
    if (work->field_7B3 == 0xD) {
        frame = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18(D_actor_403200_80141C6C, D_actor_403200_80141C74);
        }
        work->field_7D8 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                 = arg0->extra.tmd->coords;
        D_actor_403200_8015F970.ident.m00_m01  = 0x1000;
        mtx                                    = (OverlayMat*)&D_actor_403200_8015F970.c.coord;
        mtx->ident.m02_m10                     = 0;
        mtx->ident.m11_m12                     = 0x1000;
        mtx->ident.m20_m21                     = 0;
        mtx->ident.m22                         = 0x1000;
        D_actor_403200_8015F970.c.coord.t[1]   = -0x64;
        D_actor_403200_8015F970.c.coord.t[0]   = 0;
        D_actor_403200_8015F970.c.coord.t[2]   = 0x64;
        D_actor_403200_8015F970.c.composeStamp = GRAPHICS_COORD_DIRTY;
        D_actor_403200_8015F970.c.parent       = &coords[4];
        Gp_UpdateCoord(&D_actor_403200_8015F970.c);
    }
    state = work->field_7B3;
    if (state == 0x14) {
        if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->field_7B3 = 0xD;
            work->field_7B0 = 1;
        }
        if (work->field_7B3 == state && work->field_7B0 == 2) {
            work->field_7B6 = 0x60;
            incinBossTickAnim(arg0);
            while ((u32)(work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) < 0x34) {
                incinBossTickAnim(arg0);
            }
            work->field_7B6 = 0x10;
        }
    }
    if (gIncinBossLimbReach >= 0x191) {
        gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0xC8;
    }
    incinBossTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// State 0x12, the enemy's death sequence: the model is torn down and rebuilt
/// so the collapse animation can run on it.
///
/// A reset request clears the host model's `field_C` and pushes the cleared
/// word onto each of the seven escorts' own model objects, allocates the host's
/// and every escort's model buffers, forces `field_F14 / 4` extra per-frame
/// steps -- stopping early once `slots0[1].flags` bit 0 is set -- and then re-arms the
/// animation slot at 0x10, plays the type-7 death cue and leaves the yaw target
/// at 0xFA0 and the escort pose cleared.
///
/// The rest of the tick winds the shared `gIncinBossLimbReach` counter down
/// by 0xC8 once it has passed 0x1F4, runs the per-frame body, clears the host
/// coordinate's rebuild flag, and on frame 0x1C of `slots0[2]` arms the screen
/// shake at level 3 and spawns the `D_actor_403200_80141C5C` script pair. While
/// `field_7B3` is still 0x12 four one-shot cues fire on frames 0x33, 0x3D, 0x4E
/// and 0x71 of `slots0[3]`, each latching the frame it saw in `field_7A8`.
static void func_actor_403200_8013E5A8(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    Enemy*           enemy;
    Enemy*           obj;
    s16              i;
    s16              j;
    s32              state;
    s32              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj                    = arg0->spawnArg2.pointer;
        escorts                = (Actor403200Work*)arg0->work;
        work->field_7F3        = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_7B6 = 0x40;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        j               = 0;
        while (j < work->field_F14 / 4) {
            incinBossTickAnim(arg0);
            j++;
            if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
                break;
            }
        }
        work->field_F06 = 7;
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        work->field_7A4 = 0;
        work->field_E96 = 0xFA0;
    }
    if (gIncinBossLimbReach >= 0x1F5) {
        gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0xC8;
    }
    incinBossTickAnim(arg0);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    state                                 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (state == 0x1C && work->field_7D8 != state) {
        work->field_EAC = 3;
        Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
    }
    work->field_7D8 = work->slots0[2].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    if (work->field_7B3 == 0x12) {
        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200013;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200003;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200014;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        frame = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200015;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        work->field_7A8 = work->slots0[3].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    }
}

/// The enemy's attack-launch body: when the dispatcher has flagged the state
/// change it re-arms the work block (`field_7A4` at 3, `field_E96` at 0xC80) and
/// plays the two launch cues -- a type-6 with the object's pan and depth, then
/// type-7s for ids 0x0D and 0x09 -- and otherwise runs the per-frame body,
/// winding the shared `gIncinBossLimbReach` counter down by 0xC8 once it has
/// passed 0x190 and clearing `field_F06` once `field_6` has passed 0x14.
static void func_actor_403200_8013E9C0(Task* arg0)
{
    Actor403200Work* work;
    Enemy*           obj;
    s32              state;
    s32              id;
    s32              pan;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        obj             = arg0->spawnArg2.pointer;
        state           = work->field_7B3;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        work->field_EFA = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
        pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        work->field_7A4 = 3;
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
        SndEvt_EnqueueType7((((u16)obj->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200009, 1);
        return;
    }
    SCRATCH_STACK_RESERVE_BYTES(0xC);
    incinBossTickAnim(arg0);
    if (gIncinBossLimbReach >= 0x191) {
        gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0xC8;
        work->field_7A4     = 0;
    }
    if (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 0;
    }
    SCRATCH_STACK_RELEASE_BYTES(0xC);
}

/// State-selecting tick of the enemy's approach: on the tick the dispatcher has
/// flagged a state change it re-arms the work block -- the two flags, the
/// stagger countdown at 0x28, the yaw target at 0xE10 and the animation slot at
/// 0x10 -- and winds the shared `gIncinBossLimbReach` counter down by 0xC8
/// once it has passed 0x190.
///
/// It then runs the per-frame body and aims the enemy at the camera: the
/// camera's translation minus the part's own translation gives the pair
/// `ratan2` turns into a yaw, taken relative to the part's facing the same way
/// the group-0 hit handler does it, and the result is wrapped to +/-0x800 into
/// `field_7C4`. `gIncinBossEnded` holding `field_6` at zero makes the
/// per-frame body's animation re-arm win the next tick.
///
/// Once the `field_F10` stagger countdown has run out it walks the three
/// `field_F08` sub-states, in which the player-relative range and the enemy's
/// remaining HP pick the next state, and a roll of `gRandomLcgState` breaks the tie
/// between the two strafing states; the state already in `field_F1D` is never
/// re-selected twice in a row. A positive heal counter in `field_F1A` overrides
/// all of it with the heal state 0xF.
///
/// The x range that sub-state 0 tests is the player-relative offset read back
/// out of the frame, not `dist`: the two share only the frame, and the y test
/// carries the -0xFA the z one carries +0x25F, the offsets the hit handler puts
/// on the same pair.
static void func_actor_403200_8013EB64(Task* arg0)
{
    Actor403200ApproachScratch* sc;
    Actor403200Work*            work;
    Enemy*                      enemy;
    Task*                       player;
    GfxCoord*                   coord;
    GfxCoord*                   facing;
    SVECTOR*                    view;
    s16                         angle;

    work   = (Actor403200Work*)arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (work->field_4 != 0) {
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFE = 0;
        if (work->field_F10 == 0) {
            work->field_F10 = 0x28;
        }
        work->field_E96 = 0xE10;
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_EFA = 0;
        work->field_7B6 = 0x10;
    }
    if (gIncinBossLimbReach >= 0x191) {
        gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0xC8;
        work->field_7A4     = 0;
    }
    sc = (Actor403200ApproachScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200ApproachScratch));
    incinBossTickAnim(arg0);

    coord    = arg0->extra.tmd->coords;
    view     = &sc->view;
    view->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    view->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    view->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    facing   = arg0->extra.tmd->coords;
    angle    = ratan2(view->vx, view->vz) -
            ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    if (gIncinBossEnded == 1) {
        work->field_6 = 0;
    }
    if (work->field_F10 <= work->field_6) {
        sc->delta.vx = player->extra.tmd->coords->coord.t[0] -
                       arg0->extra.tmd->coords->coord.t[0];
        sc->delta.vy = (player->extra.tmd->coords->coord.t[1] -
                        arg0->extra.tmd->coords->coord.t[1]) -
                       0xFA;
        sc->delta.vz = (player->extra.tmd->coords->coord.t[2] -
                        arg0->extra.tmd->coords->coord.t[2]) +
                       0x25F;
        sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                               sc->delta.vz * sc->delta.vz);
        switch (work->field_F08) {
            case 0:
                if (enemy->hp < 0x5DC) {
                    work->field_0 = 9;
                } else if (player->extra.tmd->coords->coord.t[0] -
                               arg0->extra.tmd->coords->coord.t[0] >=
                           0x2711) {
                    work->field_0 = 2;
                } else if (work->field_F1D != 3) {
                    work->field_0 = 3;
                } else {
                    work->field_0 = 2;
                }
                break;
            case 1:
                if (enemy->hp < 0x320) {
                    work->field_0 = 9;
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 0xF) == 0) {
                        work->field_0 = 3;
                    } else if (sc->dist >= 0x20D1) {
                        if (work->field_F1D == 6) {
                            work->field_0 = 2;
                        } else {
                            work->field_0 = 6;
                        }
                    } else if (sc->dist >= 0x189D) {
                        if (work->field_F1D == 7) {
                            work->field_0 = 2;
                        } else {
                            work->field_0 = 7;
                        }
                    } else {
                        work->field_0 = 2;
                    }
                }
                break;
            case 2:
                if (sc->dist >= 0x2329) {
                    if (work->field_F1D == 2) {
                        work->field_0 = 6;
                    } else {
                        work->field_0 = 2;
                    }
                } else if (work->field_F1D == 2) {
                    work->field_0 = 0xB;
                } else {
                    work->field_0 = 2;
                }
                break;
        }
        if ((s8)work->field_F1A > 0) {
            work->field_0 = 0xF;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200ApproachScratch));
}

/// Escort-spawn tick of the arena fight. While `gIncinBossEnded` is 1
/// the whole body is skipped; otherwise it carves an
/// `Actor403200SpawnScratch` off the scratch-pad stack.
///
/// On the dispatcher's re-arm tick it tops the two `field_EE8` slots back up
/// to two live escorts (`field_F1C` < 2 and `field_F1B` < 8), dresses each
/// model from the current area record's fourth placement, stamps the slot
/// index into `Enemy::placeKey`, and plays the two type-7 launch cues.
/// Every later tick yaws the host at the player, and at `field_6` 0x46 / 0x78
/// it sends escort 0 or 1 a 0x7DB order whose action is picked from
/// `field_F08` and a coin flip of `gRandomLcgState`.
static void func_actor_403200_8013EF6C(Task* arg0)
{
    Actor403200SpawnScratch* sc;
    Actor403200Work*         work;
    Enemy*                   host;
    Enemy*                   escort;
    PlayerStatus*            cfg;
    GfxCoord*                coord;
    GfxCoord*                facing;
    TmdObject*               model;
    AreaPlacement*           entry;
    GameLocationKey          key;
    GameLocationKey*         sessionKey;
    s32                      cueId;
    s32                      cuePan;
    s32                      blastId;
    s32                      blastPan;
    s32                      rnd;
    s32                      state;
    s16                      angle;
    s16                      sel;
    u32                      frame;

    work = (Actor403200Work*)arg0->work;
    host = arg0->spawnArg2.pointer;
    if (gIncinBossEnded != 1) {
        sc = (Actor403200SpawnScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200SpawnScratch));
        if (work->field_4 != 0) {
            state           = work->field_7B3;
            work->field_EF4 = 0;
            work->field_EF6 = 1;
            work->field_EFA = 0;
            if (state != 0x13) {
                work->field_7B3 = 0x13;
                work->field_7B0 = 1;
            } else {
                work->field_7B0 = 2;
                work->field_7B3 = state;
            }
            work->field_7B6 = 0x10;
            for (sc->i = 0; sc->i < 2; sc->i++) {
                if (work->field_EE8[sc->i] == NULL && work->field_F1C < 2 && (u8)work->field_F1B < 8) {
                    work->field_EE8[sc->i] = Gp_SpawnEnemyFromTable(&D_80174D58, 3, 2, NULL);
                    if (work->field_EE8[sc->i] != NULL) {
                        work->field_F1B++;
                        model      = work->field_EE8[sc->i]->task->extra.tmd;
                        sessionKey = &gGameSession->location.loc;
                        key.stage  = sessionKey->stage;
                        key.area   = sessionKey->area;
                        key.room   = sessionKey->room;
                        key.view   = sessionKey->view;
                        areaSyncLocationVariant(&key);
                        entry                    = &Gp_GetNestedAreaRec(&key)->field_0[3];
                        model->texturePageOffset = entry->texturePageOffset;
                        model->clutRowOffset     = entry->clutRowOffset;
                        if (model->buffer != NULL) {
                            tmdProcessStream(model);
                            tmdProcessStream(model);
                        }
                        work->field_EE8[sc->i]->workType = ENEMY_WORK_PLAIN;
                        escort                           = work->field_EE8[sc->i];
                        escort->placeKey                |= sc->i << ENEMY_PLACE_INDEX_SHIFT;
                        work->field_F1C++;
                    }
                }
            }
            work->field_E96 = 0xC80;
            SndEvt_EnqueueType7((((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
            SndEvt_EnqueueType7((((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200009, 1);
        }
        if (gIncinBossLimbReach >= 0x191) {
            gIncinBossLimbReach = (u16)gIncinBossLimbReach - 0xC8;
        }
        incinBossTickAnim(arg0);
        if (work->field_7B3 == 0x13 && (frame = work->slots0[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) >= 4 && frame < 0xD) {
            work->field_EFA = 1;
        } else {
            work->field_EFA = 0;
        }
        cfg          = &gPlayerStatus;
        coord        = arg0->extra.tmd->coords;
        sc->delta.vx = cfg->coordMtx->t[0] - coord->coord.t[0];
        sc->delta.vy = cfg->coordMtx->t[1] - coord->coord.t[1];
        sc->delta.vz = cfg->coordMtx->t[2] - coord->coord.t[2];
        facing       = arg0->extra.tmd->coords;
        angle        = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        work->field_7C4 = angle;
        if ((work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY) && work->field_7B3 == 0x13) {
            work->field_7B3 = 1;
            work->field_7B0 = 1;
        }
        if (work->field_6 >= 0x14B || (work->field_7B3 == 1 && gSceneCombatState.battleRefs == 1)) {
            work->field_0 = 3;
        }
        if (work->field_6 == 6) {
            cueId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200004;
            cuePan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(cueId, cuePan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        }
        if (work->field_6 == 0x3B) {
            blastId  = (((u16)host->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200010;
            blastPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(blastId, blastPan, (s8)(worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords) / 2));
            work->field_EAC = 3;
            Gp_SpawnScript18(D_actor_403200_80141C5C, D_actor_403200_80141C64);
        }
        if (work->field_6 == 0x23) {
            work->field_F06 = 0;
        }
        if (work->field_6 == 0x46) {
            sc->i = 0;
            goto dispatch;
        }
        if (work->field_6 == 0x78) {
            sc->i = 1;
        dispatch:
            if (work->field_EE8[sc->i] != NULL) {
                D_actor_403200_8015F8F4.context.loc.stage = 0;
                D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
                sel                                       = work->field_F08;
                if (sel == 1) {
                    goto L_case1;
                }
                if (sel >= 2) {
                    goto L_default;
                }
                if (sel != 0) {
                    goto L_default;
                }
                if (sc->i == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 3;
                    } else {
                        D_actor_403200_8015F8F4.command = 4;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 9;
                    } else {
                        D_actor_403200_8015F8F4.command = 0xA;
                    }
                }
                goto L_join;
            L_case1:
                if (sc->i == 0) {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 0xA;
                    } else {
                        D_actor_403200_8015F8F4.command = 0xB;
                    }
                } else {
                    gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    if (!((gRandomLcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 4;
                    } else {
                        D_actor_403200_8015F8F4.command = 5;
                    }
                }
                goto L_join;
            L_default:
                if (sc->i == 0) {
                    D_actor_403200_8015F8F4.command = 5;
                } else {
                    D_actor_403200_8015F8F4.command = 0xB;
                }
            L_join:
                D_actor_403200_8015F8F4.command <<= 8;
                rnd                               = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                D_actor_403200_8015F8F4.command  |= (s16)(((((u32)rnd >> 16) % 3) * 0x10) | 1);
                gRandomLcgState                   = rnd;
                Gp_DispatchMsgPtr(work->field_EE8[sc->i]->task, ACTOR_COMMAND_MESSAGE_APPLY, &D_actor_403200_8015F8F4, 0);
            }
        }
    out:
        SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200SpawnScratch));
    }
}

/// Gives the escort the texture page and palette of the current area's
/// third placement, and refreshes its existing model stream.
static inline void _actor403200TintEscort(TmdObject* model)
{
    AreaPlacement* entry;

    entry                    = &(actorGetCurrentAreaRec()->field_0)[2];
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
}

static void func_actor_403200_8013F700(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    Enemy*           enemy;
    Enemy*           spawned;
    SVECTOR          vec;
    SVECTOR*         v;
    GfxCoord*        coord;
    GfxCoord*        rot;
    s16              i;
    s16              j;
    s16              angle;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_7B3        = 0xE;
        work->field_7B0        = 1;
        escorts                = (Actor403200Work*)arg0->work;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers(arg0->extra.tmd);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers(dying->field_ECC[j]->task->extra.tmd);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        work->field_EFE = 0;
        work->field_E96 = 0xC80;
    }
    switch (work->field_6) {
        case 0x64:
        case 0x104:
            if ((s8)work->field_F1A > 0) {
                work->field_7B3 = 0x10;
                work->field_7B0 = 1;
                work->field_EF4 = 1;
                work->field_F1A--;
            } else {
                work->field_0   = 0xA;
                work->field_EFE = 0;
            }
            break;
        case 0x74:
            sfx = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40200017;
            pan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(
                sfx, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
            break;
        case 0x1A4:
            work->field_0   = 0xA;
            work->field_EFE = 0;
            work->field_F1A = 0;
            break;
        case 0x9B:
        case 0x113:
            work->field_EFE = 0x80;
            break;
        case 0xAF:
        case 0x145:
            spawned           = Gp_SpawnEnemyFromTable(D_actor_403200_8015E858, 3, 0, arg0->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->field_EF0   = spawned;
            if (spawned != NULL) {
                _actor403200TintEscort(spawned->task->extra.tmd);
                work->field_EFE = 0;
            }
            break;
    }
    coord = arg0->extra.tmd->coords;
    v     = &vec;
    v->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
    v->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
    v->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    rot   = arg0->extra.tmd->coords;
    angle = ratan2(v->vx, v->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    incinBossTickAnim(arg0);
    if (work->field_7B3 == 0x10 && (work->slots0[1].flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->field_7B3 = 0xE;
        work->field_7B0 = 1;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 3;
    }
}

/// The host's state handlers, indexed by `field_0`; the last six slots are
/// empty. Two of the handlers take no argument and are called through the
/// table's type anyway.
static const Actor403200StateTable D_actor_403200_80132154 = {
    {
        func_actor_403200_8013B23C,
        func_actor_403200_80141B40,
        func_actor_403200_8013B3C8,
        func_actor_403200_8013B8C4,
        func_actor_403200_8013D78C,
        func_actor_403200_8013E2FC,
        func_actor_403200_8013D9EC,
        func_actor_403200_8013DC3C,
        func_actor_403200_8013E9C0,
        func_actor_403200_80134D40,
        func_actor_403200_8013EB64,
        func_actor_403200_8013D028,
        func_actor_403200_8013E5A8,
        func_actor_403200_8013C84C,
        func_actor_403200_8013EF6C,
        func_actor_403200_8013F700,
        func_actor_403200_8014122C,
        func_actor_403200_80141234,
        func_actor_403200_8014123C,
    },
};

/// The host task's three states -- spawn/setup, per-frame tick and teardown --
/// dispatched through by state.
static const GpEnemyTaskFuncTable3 D_actor_403200_801321B8 = {
    {
        func_actor_403200_80138AFC,
        func_actor_403200_8013FB54,
        Gp_DestroyEnemy,
    },
};

/// Per-frame tick for the enemy task. Updates the host coordinate, hides or
/// shows the escorts, and either returns on the cinematic mode byte or runs
/// the hit handlers, the death handoff and the state in `field_0`.
static void func_actor_403200_8013FB54(Enemy* arg0, Task* arg1)
{
    VECTOR                  pos;
    Actor403200StateTable   states;
    Actor403200Work*        work;
    Actor403200Work*        dying;
    Actor403200Work*        vis;
    Actor403200TickScratch* scratch;
    GpObj4C*                pending;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    Task*                   player;
    Task*                   slot3;
    Enemy*                  colorEnemy;
    s16                     i;
    s16                     j;
    s16                     k;
    s16                     mode;
    u16                     count;
    u8                      viewReady;
    s32                     d801153f4;
    u8                      stateF0;
    SVECTOR*                pendingPos;
    s8                      nodeFlags;
    s32                     t2;

    work   = (Actor403200Work*)arg1->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    states = D_actor_403200_80132154;

    arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(arg1->extra.tmd->coords);

    dying = (Actor403200Work*)arg1->work;
    if (dying->field_7F3 != 0) {
        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        dying->field_7F3--;
        if (dying->field_7F3 == 0) {
            tmd         = arg1->extra.tmd;
            tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            Tmd_FreeBuffers(arg1->extra.tmd);
            for (i = 0; i < 7; i++) {
                if (dying->field_ECC[i] != NULL) {
                    escortTmd         = dying->field_ECC[i]->task->extra.tmd;
                    escortTmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    Tmd_FreeBuffers(dying->field_ECC[i]->task->extra.tmd);
                }
            }
        }
    }

    pos.vx = arg1->extra.tmd->coords[3].workm.t[0];
    pos.vy = arg1->extra.tmd->coords[3].workm.t[1];
    pos.vz = arg1->extra.tmd->coords[3].workm.t[2];

    slot3 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot3->extra.tmd->coords->coord.t[1] > 0) {
        slot3->extra.tmd->coords->coord.t[1] = 0;
    }
    t2 = slot3->extra.tmd->coords->coord.t[2];
    if (t2 >= -0xED7) {
        slot3->extra.tmd->coords->coord.t[2] = -0xED8;
    } else if (t2 < -0x206C) {
        slot3->extra.tmd->coords->coord.t[2] = -0x206C;
    }

    if (work->field_EFA != work->field_EFC) {
        Gp_UpdateActorColor(arg0, &pos, 0, 0);
        Gp_UpdateActorColor(work->field_ECC[3], &pos, 0, 0);
        work->field_EFC = work->field_EFA;
    }
    colorEnemy = arg0;
    if (work->field_EFA == 0) {
        colorEnemy = work->field_ECC[3];
    }
    Gp_UpdateActorColor(colorEnemy, &pos, 0, 0);

    if (work->field_0 == 0xB && gIncinBossLimbReach >= 0x7D1) {
        work->field_ECC[4]->task->extra.tmd->otOffset = -8;
    } else {
        work->field_ECC[4]->task->extra.tmd->otOffset = 0;
    }

    if (((Gp_GetViewIndex() & 0xFF) == 0x1E) || ((Gp_GetViewIndex() & 0xFF) == 0x1D)) {
        vis                    = (Actor403200Work*)arg1->work;
        vis->field_7F3         = 0;
        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        for (j = 0; j < 7; j++) {
            if (vis->field_ECC[j] != NULL) {
                vis->field_ECC[j]->task->extra.tmd->flags =
                    arg1->extra.tmd->flags;
            }
        }
    } else if (work->field_0 != 0) {
        vis                    = (Actor403200Work*)arg1->work;
        vis->field_7F3         = 0;
        arg1->extra.tmd->flags = 0;
        for (k = 0; k < 7; k++) {
            if (vis->field_ECC[k] != NULL) {
                vis->field_ECC[k]->task->extra.tmd->flags =
                    arg1->extra.tmd->flags;
            }
        }
    }

    d801153f4 = gSceneCombatState.actorControl;
    if (d801153f4 == 1) {
        goto clear_and_return;
    }
    if (d801153f4 < 2) {
        goto after_mode;
    }
    if (d801153f4 != 2) {
        goto after_mode;
    }
clear_and_return:
    Gp_ClearRec18Occupied(work->hits[0].recs);
    Gp_ClearRec18Occupied(work->hits[1].recs);
    Gp_ClearRec18Occupied(work->hits[2].recs);
    Gp_ClearRec18Occupied(work->hits[3].recs);
    Gp_ClearRec18Occupied(work->hits[4].recs);
    Gp_ClearRec18Occupied(work->hits[5].recs);
    Gp_ClearRec18Occupied(work->hits[6].recs);
    Gp_ClearRec18Occupied(work->hits[7].recs);
    Gp_ClearRec18Occupied(work->hits[8].recs);
    Gp_ClearRec18Occupied(work->recs2);
    return;
after_mode:

    scratch = (Actor403200TickScratch*)SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor403200TickScratch));

    if (arg0->hp > 0) {
        if (work->field_EC8 != 1 && work->field_0 != 0xD) {
            if (work->field_E92 > 0) {
                work->field_E92 = (s16)((u16)work->field_E92 - 1);
            } else {
                func_actor_403200_80139E94(arg1);
            }
            if (work->field_E8C > 0) {
                work->field_E8C = (s16)((u16)work->field_E8C - 1);
            } else {
                func_actor_403200_80139A60(arg1);
            }
            if (work->field_E8E > 0) {
                work->field_E8E = (s16)((u16)work->field_E8E - 1);
            } else {
                func_actor_403200_8013A4A0(arg1);
            }
            if (work->field_E90 > 0) {
                work->field_E90 = (s16)((u16)work->field_E90 - 1);
            } else {
                func_actor_403200_8013AB70(arg1);
            }
        }
    }
    if (arg0->hp <= 0) {
        if (gPlayerStatus.hp <= 0) {
            arg0->hp        = 1;
            gIncinBossEnded = 0;
        }
    }

    if (work->field_F1C > 0) {
        work->field_F16 = 0x26;
    }
    if (work->field_F16 > 0 && work->field_F1C == 0) {
        work->field_F16 = (s16)((u16)work->field_F16 - 1);
    }

    if (arg0->hp <= 0) {
        if (work->field_F16 > 0) {
            arg0->hp = 1;
        }
        if (arg0->hp <= 0 && gIncinBossEnded == 0) {
            gIncinBossEnded                           = 1;
            D_actor_403200_8015F8F4.context.loc.stage = 0;
            D_actor_403200_8015F8F4.context.loc.area  = 0x2C;
            D_actor_403200_8015F8F4.command           = 3;
            Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_actor_403200_8015F8F4, ACTOR_COMMAND_MESSAGE_APPLY);
            gIncinBossSpinnersReleased = 0;
        }
    }

    if (gIncinBossEnded == 1 && work->field_F16 <= 0 && work->field_EC8 == 0 &&
        work->field_F12 < 0x100) {
        work->field_F12 = (s16)((u16)work->field_F12 + 1);
    }

    if (work->field_F12 == 8) {
        work->field_0                = 0x12;
        gSceneCombatState.battleRefs = 1;
        // Dumping Hole room variant 1 installs ten contiguous pending quads.
        pending               = Gp_PendingObj4C;
        pending[9].field_C.vx = arg1->extra.tmd->coords->coord.t[0] + 0xFA0;
        pendingPos            = &pending[9].field_C;
        pendingPos->vy        = arg1->extra.tmd->coords->coord.t[1] - 0x64;
        pendingPos->vz        = arg1->extra.tmd->coords->coord.t[2];
        SndEvt_EnqueueType7((((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
        SndEvt_EnqueueType7((((u16)arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000D, 1);
    }

    if (arg0->hp <= 0) {
        stateF0 = gSceneCombatState.signals.bytes.battlePhase;
        if (stateF0 == 2) {
            work->field_F06 = stateF0;
            work->field_F08 = 3;
            work->field_F04 = 0;
        }
    }

    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
        work->field_6 = 0;
    } else {
        if (work->field_6 < 0x7FFF) {
            work->field_6 = (s16)((u16)work->field_6 + 1);
        }
        work->field_4 = 0;
    }

    work->field_2 = (u16)work->field_0;
    if (work->field_0 != 0 && work->field_0 != 5 && work->field_0 != 0xC && work->field_0 != 3 &&
        work->field_7F2 == 0) {
        viewReady = gGameSession->viewReady;
        if (viewReady == 1 && work->field_0 != 0 && work->field_0 != 5) {
            work->field_7F2 = viewReady;
            func_actor_403200_8013B740(arg1);
        }
    }

    states.fn[work->field_0](arg1);

    if ((u16)work->field_0 < 2 || work->field_0 == 5 || work->field_0 == 0xC) {
        nodeFlags                                  = 1;
        arg0->node.state.parts.flags               = WORLD_TARGET_NOT_LOCKABLE;
        work->field_ECC[3]->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_ECC[0]->node.state.parts.flags = nodeFlags;
        work->field_ECC[1]->node.state.parts.flags = nodeFlags;
    } else if (work->field_EFA != 0) {
        if (Gp_NodeSlotMask(&work->field_ECC[3]->node) != 0) {
            Gp_AssignNodeSlot0(&arg0->node);
        }
        arg0->node.state.parts.flags               = WORLD_TARGET_HIDE_HP;
        work->field_ECC[3]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->field_ECC[0]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
        work->field_ECC[1]->node.state.parts.flags = (WORLD_TARGET_NOT_LOCKABLE | WORLD_TARGET_KEEP_SCANNED);
    } else {
        if (Gp_NodeSlotMask(&arg0->node) != 0) {
            Gp_AssignNodeSlot0(&work->field_ECC[3]->node);
        }
        arg0->node.state.parts.flags               = WORLD_TARGET_NOT_LOCKABLE;
        nodeFlags                                  = 8;
        work->field_ECC[3]->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
        work->field_ECC[0]->node.state.parts.flags = nodeFlags;
        work->field_ECC[1]->node.state.parts.flags = nodeFlags;
    }

    if (work->field_0 != 0 && work->field_EFA == 1) {
        work->hits[0].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[0].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->field_0 != 0 && work->field_EFA != 1) {
        work->hits[1].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[2].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[1].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[2].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
    if (work->field_0 != 0) {
        work->hits[3].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[4].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[5].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[3].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[4].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[5].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    if (work->field_0 != 0) {
        work->hits[6].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[7].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->hits[8].obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->hits[6].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[7].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->hits[8].obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }

    Gp_ClearRec18Occupied(work->hits[0].recs);
    Gp_ClearRec18Occupied(work->hits[1].recs);
    Gp_ClearRec18Occupied(work->hits[2].recs);
    Gp_ClearRec18Occupied(work->hits[3].recs);
    Gp_ClearRec18Occupied(work->hits[4].recs);
    Gp_ClearRec18Occupied(work->hits[5].recs);
    Gp_ClearRec18Occupied(work->hits[6].recs);
    Gp_ClearRec18Occupied(work->hits[7].recs);
    Gp_ClearRec18Occupied(work->hits[8].recs);
    Gp_ClearRec18Occupied(work->recs2);
    func_actor_403200_80138284(arg1);

    if (work->field_F04 == 0 && work->field_0 != 0) {
        scratch->view = D_actor_403200_8015E6E8[work->field_F06](arg1, work->field_F08);
        if (((Gp_GetViewIndex() & 0xFF) != scratch->view) &&
            (arg1->spawnArg1.value >> 16) == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = scratch->view;
        }
    }

    mode = work->field_EC8;
    if (mode == 1 && work->field_0 != 0xD) {
        count           = work->field_7CA + 1;
        work->field_7CA = count;
        if (work->field_ECA == mode) {
            if (work->anim.animationId == 2) {
                work->anim.source.sets = D_actor_403200_8015E6AC;
                work->anim.blend       = ANIMATION_BLEND_RESET;
                work->anim.blendFrames = 0;
                Gp_DispatchMsgPtr(player, 0x3FF, &work->anim, 0);
                work->field_7CA = 0;
            }
        } else if (work->anim.animationId == 2 && (s16)count < 0x28) {
            work->anim.source.sets = D_actor_403200_8015E6AC;
            work->anim.blend       = ANIMATION_BLEND_RESET;
            work->anim.blendFrames = 0;
            Gp_DispatchMsgPtr(player, 0x3FF, &work->anim, 0);
        }

        if (Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0) {
            switch (work->anim.animationId) {
                case 2:
                    if (work->field_ECA != 1 && (s16)work->field_7CA >= 0x17) {
                        work->anim.source.sets = D_actor_403200_8015E6AC;
                        D_actor_403200_8015E6AC[4] =
                            (Gp_PlayerAnimBlkTbl
                                 [Gp_WeaponIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])
                                ->table.sets[7];
                        work->anim.animationId = 4;
                        work->anim.blend       = ANIMATION_BLEND_INTERPOLATE;
                        work->anim.blendFrames = 3;
                        Gp_DispatchMsgPtr(player, 0x3FF, &work->anim, 0);
                        work->field_7CA = 0;
                    }
                    break;
                case 4:
                    if (work->field_ECA != 1) {
                        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
                        work->field_EC8 = 0;
                    }
                    break;
            }
        }
    }

    if (gIncinBossEnded == 1) {
        if (work->field_EC8 == gIncinBossEnded) {
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
            work->field_EC8 = 0;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor403200TickScratch));
}

static void func_actor_403200_801408D8(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR     dir;
    SVECTOR     normal;
    SVECTOR*    pool  = Gp_GridParams->field_4;
    SVECTOR*    verts = Gp_GridParams->field_8;
    GpGridFace* faces = Gp_GridParams->field_C;
    GpGridFace  face  = {
        { index * 4, index * 4 + 1, index * 4 + 2, index * 4 + 3 }, index, 3
    };
    GpGridFace face2 = {
        { (index + 1) * 4, (index + 1) * 4 + 1, (index + 1) * 4 + 2, (index + 1) * 4 + 3 }, index + 1, 3
    };
    SVECTOR* d;

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, &normal);
    Gfx_MatrixCol0(&task->extra.tmd->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(&normal, &normal);
    gte_lddp(scale);
    gte_ldsv(&normal);
    gte_gpf12();
    gte_stsv(&normal);
    gte_lddp(0xBB8);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + dir.vx + normal.vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy =
        task->extra.tmd->coords->coord.t[1] + dir.vy + normal.vy;
    verts[index * 4].vz = verts[index * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + dir.vz + normal.vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] + normal.vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy =
        task->extra.tmd->coords->coord.t[1] + normal.vy;
    verts[index * 4 + 1].vz = verts[index * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] + normal.vz;

    verts[(index + 1) * 4].vx = verts[(index + 1) * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + normal.vx;
    verts[(index + 1) * 4].vy = verts[(index + 1) * 4 + 2].vy =
        task->extra.tmd->coords->coord.t[1] + normal.vy;
    verts[(index + 1) * 4].vz = verts[(index + 1) * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + normal.vz;

    verts[(index + 1) * 4 + 1].vx = verts[(index + 1) * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] + normal.vx - dir.vx;
    verts[(index + 1) * 4 + 1].vy = verts[(index + 1) * 4 + 3].vy =
        task->extra.tmd->coords->coord.t[1] + normal.vy - dir.vy;
    verts[(index + 1) * 4 + 1].vz = verts[(index + 1) * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] + normal.vz - dir.vz;

    gte_lddp(0x3E8);
    gte_ldsv(&normal);
    gte_gpf12();
    gte_stsv(&normal);

    verts[index * 4].vx = verts[index * 4 + 2].vx += normal.vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy += normal.vy;
    verts[index * 4].vz = verts[index * 4 + 2].vz += normal.vz;
    verts[index * 4 + 5].vx = verts[index * 4 + 7].vx += normal.vx;
    verts[index * 4 + 5].vy = verts[index * 4 + 7].vy += normal.vy;
    verts[index * 4 + 5].vz = verts[index * 4 + 7].vz += normal.vz;

    pool[index].vz = verts[index * 4].vx - verts[index * 4 + 1].vx;
    pool[index].vy = verts[index * 4 + 1].vy - verts[index * 4].vy;
    pool[index].vx = verts[index * 4 + 1].vz - verts[index * 4].vz;
    VectorNormalSS(&pool[index], &pool[index]);

    (&pool[index])[1].vz = verts[(index + 1) * 4].vx - verts[(index + 1) * 4 + 1].vx;
    (&pool[index])[1].vy = verts[(index + 1) * 4 + 1].vy - verts[(index + 1) * 4].vy;
    (&pool[index])[1].vx = verts[(index + 1) * 4 + 1].vz - verts[(index + 1) * 4].vz;
    VectorNormalSS(&(&pool[index])[1], &(&pool[index])[1]);

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;
    verts[index * 4 + 4].vy -= drop;
    verts[index * 4 + 5].vy -= drop;

    face.surfaceClass  = 3;
    face2.surfaceClass = 3;
    faces[index]       = face;
    faces[index + 1]   = face2;
}

/// The enemy's upkeep tick, run by the dispatcher through the same
/// `D_actor_403200_801321B8` table the other tasks in this overlay use. It drops
/// each of the two escort slots whose HP has run out, then walks the work
/// block's `field_E94` toward `field_E96` by 0x32 a tick -- snapping once the
/// two are within 0x33 -- calls the follow helper with the new value, and
/// finally lifts the host's own X up to the escort's so the party never sinks
/// below the enemy. The tick ends by dispatching on `state` through the local
/// copy of the handler table.
void func_actor_403200_80140E6C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;
    Actor403200Work*      work;
    Task*                 player;
    Enemy*                enemy;
    s32                   diff;
    s32                   y;
    GfxCoord*             playerCoord;
    GfxCoord*             selfCoord;

    sp     = D_actor_403200_801321B8;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    work   = (Actor403200Work*)arg0->work;
    enemy  = arg0->spawnArg2.pointer;
    if (work != NULL) {
        if (work->field_EE8[0] != NULL && work->field_EE8[0]->hp <= 0) {
            work->field_EE8[0] = NULL;
        }
        if (work->field_EE8[1] != NULL && work->field_EE8[1]->hp <= 0) {
            work->field_EE8[1] = NULL;
        }
        diff = work->field_E96 - work->field_E94;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= 0x33) {
            if (work->field_E94 < work->field_E96) {
                work->field_E94 = (u16)work->field_E94 + 0x32;
            } else {
                work->field_E94 = (u16)work->field_E94 - 0x32;
            }
        } else {
            work->field_E94 = (u16)work->field_E96;
        }
        func_actor_403200_801408D8(arg0, work->field_E94, work->field_E98, 0);
        playerCoord = player->extra.tmd->coords;
        selfCoord   = arg0->extra.tmd->coords;
        y           = selfCoord->coord.t[0] + work->field_E94;
        if (playerCoord->coord.t[0] < y) {
            playerCoord->coord.t[0] = y;
        }
    }
    sp.funcs[arg0->state](enemy, arg0);
}

/// Set the heights of collision grid quads `arg1` and `arg1 + 1`: 500 for each
/// quad's first two vertices, 800 for the other two. Nothing in the actor calls
/// it.
static void func_actor_403200_80140FD4(s32 arg0, s16 arg1)
{
    SVECTOR* verts;

    verts                  = Gp_GridParams->field_8;
    verts[arg1 * 4].vy     = 500;
    verts[arg1 * 4 + 1].vy = 500;
    verts[arg1 * 4 + 2].vy = 800;
    verts[arg1 * 4 + 3].vy = 800;
    verts[arg1 * 4 + 4].vy = 500;
    verts[arg1 * 4 + 5].vy = 500;
    verts[arg1 * 4 + 6].vy = 800;
    verts[arg1 * 4 + 7].vy = 800;
}

#include "../../shared/incinerator_boss_exit.inc.c"

static void func_actor_403200_801410F0(s8 arg0)
{
    ((Actor403200Work*)D_actor_403200_8015F8F0->work)->field_EAC = arg0;
}

static void func_actor_403200_80141108(s16 arg0)
{
    gIncinBossSpinnersReleased = arg0;
}

static s16 func_actor_403200_80141114(void)
{
    return gIncinBossSpinnersReleased;
}

s32 func_actor_403200_80141124(Task* arg0, s16 arg1)
{
    switch (arg1) {
        case 0:
            return 0x13;
        case 1:
            return 7;
        case 2:
            return 0x25;
    }
    return 1;
}

s32 func_actor_403200_80141180(Task* arg0, s16 arg1)
{
    return (s16)func_actor_403200_801344C4(arg0, arg1);
}

/// Returns 0x25 for the current view, or 0x1E when the slot-3 model's X
/// translation is at or above a threshold that depends on the view index:
/// 0x3A98 for view 0x1E, 0x3E80 otherwise.
s32 func_actor_403200_801411A8(Task* arg0, s16 arg1)
{
    Task* task;
    s32   flag;
    s32   value;
    s32   view;

    view = Gp_GetViewIndex() & 0xFF;
    task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (view == 0x1E) {
        flag  = task->extra.tmd->coords->coord.t[0];
        flag  = flag < 0x3A98;
        value = 0x25;
    } else {
        flag  = task->extra.tmd->coords->coord.t[0];
        flag  = flag < 0x3E80;
        value = 0x25;
    }
    if (flag == 0) {
        value = 0x1E;
    }
    return value;
}

static void func_actor_403200_8014122C(Task* arg0)
{
}

static void func_actor_403200_80141234(Task* arg0)
{
}

/// The state handler `D_actor_403200_80132154` lists for state 8. Re-arms the
/// sub-state counter if the dispatcher saw a state change this tick, runs the
/// per-frame body, and on the tick the counter reaches 8 tells the player's
/// task (message 0x13F4) and plays the actor's cue.
static void func_actor_403200_8014123C(Task* arg0)
{
    Actor403200Work* work;
    Enemy*           enemy;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_6 = 0;
    }
    incinBossTickAnim(arg0);
    if (work->field_6 == 8) {
        Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_ROOM), 0x13F4, 0, 0);
        SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000A, 1);
    }
}

#include "../../shared/incinerator_boss_prop_setup.inc.c"

#include "../../shared/incinerator_boss_prop_tick.inc.c"

void func_actor_403200_80141430(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

void func_actor_403200_8014148C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Dispatcher of the enemy `D_actor_403200_80131E90` drives: run the handler for
/// the task's state, skipped while the global game mode is 2.
void func_actor_403200_801414E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E90;
    switch (gSceneCombatState.actorControl) {
        default:
        case SCENE_COMBAT_ACTORS_RUNNING:
        case SCENE_COMBAT_ACTORS_PAUSED:
            sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}

/// Dispatcher of the grab enemy (`D_actor_403200_80131E9C`): park the model
/// object while the global game mode is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it. Unlike `func_actor_403200_80141670` it guards the bookkeeping on the
/// state being non-zero rather than on the work block existing, and clears the
/// model object's flag word rather than leaving it 2.
void func_actor_403200_80141564(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;
    Actor403200GrabWork*  work;

    sp   = D_actor_403200_80131E9C;
    work = (Actor403200GrabWork*)arg0->work;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg0->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            arg0->extra.tmd->flags = 0;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (arg0->state != 0) {
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Dispatcher of the `D_actor_403200_80131F04` enemy: park the model object
/// while the global game mode is 1 or 2, otherwise note in the work block
/// whether the state changed since the last step and run the handler for it.
/// The model object's flag word is left at 2 while the enemy runs.
void func_actor_403200_80141670(Task* arg0)
{
    GpEnemyTaskFuncTable4 sp;
    Actor403200GrabWork*  work;

    sp = D_actor_403200_80131F04;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (arg0->work != NULL) {
        work = (Actor403200GrabWork*)arg0->work;
        if (work->field_1B4 != arg0->state) {
            work->field_1A8 = 1;
        } else {
            work->field_1A8 = 0;
        }
        work->field_1B4 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

/// Dispatcher of the dropped enemy (`D_actor_403200_80131F14`): run the handler
/// for the task's state, skipped while the global game mode is 1 or 2.
void func_actor_403200_80141778(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_actor_403200_80131F14;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
        case SCENE_COMBAT_ACTORS_HIDDEN:
            break;
    }
}

#include "../../shared/incinerator_boss_spinner_wait.inc.c"

/// Dispatcher of the spinner enemy (`D_actor_403200_80131F28`): park the model
/// object while the global game mode is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it.
void func_actor_403200_80141868(Task* arg0)
{
    GpEnemyTaskFuncTable4   sp;
    Actor403200SpinnerWork* work;

    sp = D_actor_403200_80131F28;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg0->extra.tmd->flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            arg0->extra.tmd->flags = 0;
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }

    if (arg0->work != NULL) {
        work = (Actor403200SpinnerWork*)arg0->work;
        if (work->field_94 != arg0->state) {
            work->field_90 = 1;
        } else {
            work->field_90 = 0;
        }
        work->field_94 = arg0->state;
    }
    sp.funcs[arg0->state](arg0->spawnArg2.pointer, arg0);
}

void func_actor_403200_8014196C(void)
{
}

/// Whether the actor should keep acting: 1 while its enemy still has HP. Once
/// the enemy is down, 0 if the model carries flag 0x80 or flag 2, and 1
/// otherwise.
s32 func_actor_403200_80141974(Task* task)
{
    u16 flags;

    if (((Enemy*)task->spawnArg2.pointer)->hp <= 0) {
        flags = task->extra.tmd->flags;
        if (flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 0;
        }
        if (flags & 2) {
            return 0;
        }
    }
    return 1;
}

#include "../../shared/actor_messages_place.inc.c"

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// the countdown at 0xF1C down and, once it has run out, re-arms the enemy's
/// `field_F16`. Same body as `func_actor_444000_80143E68` without its tracked
/// escort slots.
s32 func_actor_403200_80141A94(Task* arg0, s32 arg1, s32 arg2)
{
    Actor403200Work* work  = (Actor403200Work*)arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;

    switch (arg2) {
        case 0:
            work->field_F1A++;
            func_800DA6E8(&enemy->node, -0x64, 0);
            if (enemy->hp > 0) {
                enemy->hp += 0x64;
            }
            break;
        case 1:
            if (work->field_F1C > 0) {
                work->field_F1C--;
                if (work->field_F1C > 0) {
                    break;
                }
            }
            work->field_F16 = 2;
            break;
    }
    return 1;
}

s32 func_actor_403200_80141B30(void)
{
    gIncinBossGrabActive = 0;
    return 1;
}

/// State-change reset: once the dispatcher has flagged the change in
/// `field_4`, drop the re-arm marker and push the host model's `field_C` onto
/// every live escort's own model object. Same body as
/// `func_actor_444000_80143F4C`.
static void func_actor_403200_80141B40(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    s16              i;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        escorts                = (Actor403200Work*)arg0->work;
        work->field_7F3        = 0;
        arg0->extra.tmd->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                escorts->field_ECC[i]->task->extra.tmd->flags =
                    arg0->extra.tmd->flags;
            }
        }
        work->field_EF4 = 0;
        work->field_EF6 = 0;
    } else {
        incinBossTickAnim(arg0);
    }
}
