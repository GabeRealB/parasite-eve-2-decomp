#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/lighting_work.h"
#include "gameplay/object_fields.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
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

#include "rooms/mist_shooting_gallery.h"

/* The controller task this actor is reparented to is the Mist shooting
 * gallery's, so the counter at +0xE of its work block is that room's. */

/// Work block this overlay hangs off `Task::work`. The display node at
/// +0x60 is the one the exit callback `func_actor_107600_80134920` hands back
/// to `Gp_UnlinkObj`. The state pair at +0x158/+0x15A is what
/// `func_actor_107600_80134B98` writes: the new state in `field_158` and its
/// sub-state counter cleared. `field_13E` is a free-running counter that
/// `func_actor_107600_80132CB8` bumps by one, and `field_144` is the phase the
/// destroy callback `func_actor_107600_80132AC0` tests against 2. The three
/// angles at +0x40 are what `func_actor_107600_80132B7C` rebuilds the model
/// root's rotation from.
/// The trio at +0x50 is a second rotation set: `func_actor_107600_80134A50`
/// wraps each to 12 bits and feeds them to `Gfx_RotMatrixX/Y/Z` in turn.
///
/// `rec18` is the collision table `obj.context.contacts` points at and
/// `func_actor_107600_80134958` hands to `Gp_InitRec18Table` with count 8, so
/// it really runs to +0x140 and `field_13E` sits inside its last record.
/// `field_162` is the spawn variant `func_actor_107600_80132ED0` takes from
/// the low nibble of the task's own `Task::spawnArg1` high halfword
/// (`lhu 0x36` then `andi 0xF`): `func_actor_107600_80134958` picks the
/// display node's `field_1C` from it and `func_actor_107600_80134C54`
/// switches on it.
typedef struct Actor107600Work {
    /// Colour / light matrix pair `func_actor_107600_80132ED0` hangs off the
    /// display object's `TmdObject.colorMtx` / `lightMtx` so the actor draws
    /// with its own light instead of `Gp_BindDefaultMtx`'s.
    /* 0x000 */ MATRIX                matrix_0;  // color matrix for the child models
    /* 0x020 */ MATRIX                matrix_20; // light matrix for the child models
    /* 0x040 */ u16                   pitch;     // fed to RotMatrixX
    /* 0x042 */ s16                   yaw;       // fed to RotMatrixY
    /* 0x044 */ u16                   roll;      // fed to RotMatrixZ
    /* 0x046 */ byte                  pad_46[0x2];
    /* 0x048 */ u16                   field_48;  // spawn position x
    /* 0x04A */ u16                   field_4A;  // spawn position y
    /* 0x04C */ u16                   field_4C;  // spawn position z
    /* 0x04E */ byte                  pad_4E[0x2];
    /* 0x050 */ u16                   field_50;  // fed to Gfx_RotMatrixX
    /* 0x052 */ u16                   field_52;  // fed to gfxRotMatrixY
    /* 0x054 */ u16                   field_54;  // fed to Gfx_RotMatrixZ
    /* 0x056 */ byte                  pad_56[0x2];
    /* 0x058 */ s16                   field_58;  // spin velocity added to field_50 while tumbling
    /* 0x05A */ s16                   field_5A;  // spin velocity added to field_52
    /* 0x05C */ s16                   field_5C;  // spin velocity added to field_54
    /* 0x05E */ byte                  pad_5E[0x2];
    /* 0x060 */ WorldCollisionBody    obj;
    /* 0x080 */ WorldCollisionContact rec18[1];  // collision table; count 8 passed to Gp_InitRec18Table
    /* 0x098 */ byte                  pad_98[0xA2];
    /* 0x13A */ u16                   field_13A; // frame counter / countdown of func_actor_107600_80132160
    /* 0x13C */ byte                  pad_13C[0x2];
    /* 0x13E */ u16                   field_13E;
    /// The spawn state stores the model root here as a word, while
    /// `func_actor_107600_80132D54` counts its sub-phase in the low halfword.
    /* 0x140 */ union {
        GfxCoord* coord; // model root, stored by the spawn state
        s16       step;  // sub-phase of func_actor_107600_80132D54
    } field_140;
    /* 0x144 */ s16  field_144;
    /* 0x146 */ s16  field_146; // written 2 beside field_144 by the spawn state
    /* 0x148 */ s16  field_148; // waypoint index into the D_actor_107600_80135624 path
    /* 0x14A */ u8   field_14A; // rotating flag: gates the yaw advance in func_actor_107600_80132CD4
    /* 0x14B */ s8   field_14B; // scale percent applied to the model root coord.m[1][1]
    /* 0x14C */ s32  field_14C; // XZ distance to the gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] actor's coord
    /* 0x150 */ s16  field_150; // Gp_GetIdParam2 of the last hit's id
    /* 0x152 */ byte pad_152[0x2];
    /* 0x154 */ u16  field_154; // countdown before the sub-state's sound cue
    /* 0x156 */ s16  field_156;
    /* 0x158 */ s16  field_158;
    /* 0x15A */ s16  field_15A;
    /* 0x15C */ s16  field_15C;
    /* 0x15E */ u16  field_15E;
    /* 0x160 */ s16  field_160; // damage of the last hit
    /* 0x162 */ s16  field_162; // spawn variant; 1 selects the 0x220 obj.radius
    /* 0x164 */ byte pad_164[0x2];
    /* 0x166 */ u16  field_166; // frame counter of the post-death light cycle
    /* 0x168 */ u8   field_168; // percent scale applied to coord.m[0][0]
    /* 0x169 */ u8   field_169; // percent scale applied to coord.m[2][1]
    /* 0x16A */ u8   field_16A; // rolled 0..7 alongside field_168
    /* 0x16B */ u8   field_16B;
} Actor107600Work;

/// The hit position `func_actor_107600_80133DC4` copies out of a collision
/// record as three words over `Actor107600Work.pitch`/`yaw`/`roll`: the same
/// 0x40 slot read as `s32`s, sign-extended from the record's halfwords.
typedef struct Actor107600HitPos {
    /* 0x0 */ s32 vx;
    /* 0x4 */ s32 vy;
    /* 0x8 */ s32 vz;
} Actor107600HitPos;

/// One waypoint of the paths in `D_actor_107600_80135624`: the X/Z target the
/// model root steps towards at `step` units per frame; an `x` of -1 ends the path.
typedef struct Actor107600Waypoint {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
    /* 0x4 */ s16 step;
} Actor107600Waypoint;

/// Entry of the effect-offset table `func_actor_107600_80133024` copies into
/// an `SVECTOR`'s `vx`/`vy`.
typedef struct Actor107600Pair {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
} Actor107600Pair;

/// 0x34-byte scratch block `func_actor_107600_80134248` takes from
/// the scratch stack to draw one `POLY_FT4`. `v` holds the four corners
/// (the offset table plus the coordinate's translation and the caller's
/// position), projected through `workm` by one `RTPS` and one `RTPT` into
/// `sxy` (each a packed `gte_stsxy` word, x low and y high). `otz` is the
/// `gte_stszotz` less 0x40, which picks the OT bucket.
typedef struct Actor107600QuadScratch {
    /* 0x00 */ s32     sxy[4];
    /* 0x10 */ s32     otz;
    /* 0x14 */ SVECTOR v[4];
} Actor107600QuadScratch;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).

void        func_actor_107600_801328CC(Task* arg0);
static void func_actor_107600_80132A7C(Task* arg0);
static void func_actor_107600_80132AC0(Task* arg0);
static void func_actor_107600_80132B0C(Task* arg0);
static void func_actor_107600_80132B7C(Task* arg0);
static void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst);
static void func_actor_107600_80132CB8(Task* arg0);
static void func_actor_107600_80132CD4(Task* arg0);
static void func_actor_107600_80132D54(Task* arg0);
static void func_actor_107600_80132DF0(Enemy* arg0, s32 arg1, s32 arg2);
static void func_actor_107600_80132ED0(Task* arg0);
static void func_actor_107600_80133024(Task* arg0);
static void func_actor_107600_801332D4(Task* arg0);
static void func_actor_107600_80133668(Task* arg0);
static void func_actor_107600_801337FC(Task* arg0);
static void func_actor_107600_801339A4(Task* arg0);
static void func_actor_107600_80133FA8(GfxCoord* arg0, SVECTOR* arg1);
static void func_actor_107600_80134248(GfxCoord* arg0, SVECTOR* arg1);
static void func_actor_107600_80134608(struct Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void        func_actor_107600_801348A0(Task* arg0);
static void func_actor_107600_80134904(Task* arg0);
static void func_actor_107600_80134920(Task* arg0);
static void func_actor_107600_80134958(Task* arg0);
static void func_actor_107600_801349E0(Task* arg0);
static void func_actor_107600_80134A50(Task* arg0);
static void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst);
static void func_actor_107600_80134B98(Task* arg0, s16 arg1);
static s32  func_actor_107600_80134BAC(Task* arg0);
static void func_actor_107600_80134C54(Task* arg0);
static void func_actor_107600_80134D10(Task* arg0);
static void func_actor_107600_80134D30(Task* arg0);
static void func_actor_107600_80134D50(Task* arg0);
static void func_actor_107600_80134D70(Task* arg0);
static void func_actor_107600_80134D9C(Task* arg0);
static void func_actor_107600_80134E5C(GfxCoord* arg0);
static void func_actor_107600_80134EF4(Task* arg0);

/* Per-variant waypoint paths `func_actor_107600_80132160` walks, indexed by
 * `Actor107600Work.field_146`; trailing-blob data. */
extern Actor107600Waypoint* D_actor_107600_80135624[];

/* Eight effect offsets `func_actor_107600_80133024` cycles through from
 * `Actor107600Work.field_16A`. */
extern Actor107600Pair D_actor_107600_80135730[];

/* Table `func_actor_107600_80132DF0` spawns from, indexed with `arg1 + 1`; it
 * is the trailing animation/data blob, not the leading rodata. */
extern TaskDesc D_actor_107600_80134F94[];

/* The pair-source record the spawn state hangs off the enemy's `Enemy.param`
 * (a zeroed pointer to `D_actor_107600_8013571C`, 0x32 and 0xFF000000) and the
 * 16-entry HP table it indexes with the spawn variant. Both are trailing-blob
 * data, after the collision tables. */
/* Pair-source record the spawn state hangs off `Enemy.param`. */
extern EnemyParams D_actor_107600_80134F84;
extern EnemyParams D_actor_107600_80135720;
extern u16         D_actor_107600_80135750[];

/* Remaining-enemy count, and the gallery controller task the room overlay
 * publishes (its `Task::work` is the `MistShootingGalleryWork`). */

static void func_actor_107600_80131F10(Task* arg0);
static void func_actor_107600_80132160(Task* arg0);
static void func_actor_107600_80132514(Task* arg0);
static void func_actor_107600_80132930(Task* arg0);
static void func_actor_107600_80133DC4(Task* arg0);

/// The actor's top-level task states, which `func_actor_107600_801328CC` runs:
/// spawn, update, drop and destroy.
static const TaskFuncTable4 D_actor_107600_80131E24 = { {
    func_actor_107600_80131F10,
    func_actor_107600_80132930,
    func_actor_107600_80132A7C,
    func_actor_107600_80132AC0,
} };

/// One entry per `Actor107600Work.field_144` phase, run by
/// `func_actor_107600_80132CD4`.
static const TaskFuncTable3 D_actor_107600_80131E34 = { {
    func_actor_107600_80132160,
    func_actor_107600_80132514,
    func_actor_107600_80132D54,
} };

void func_actor_107600_801328CC(Task*);
void func_actor_107600_801348A0(Task*);

DamageAttack D_actor_107600_80134F80[1] = { 0 };

EnemyParams D_actor_107600_80134F84 = { D_actor_107600_80134F80, 50, 0, 0, 0, 255, 0, 0, 0 };

TaskDesc D_actor_107600_80134F94[19] = {
    { TASK_BODY_TMD, 96, func_actor_107600_801328CC, { .value = -0x7FE77FCC } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE79520 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE79330 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE79140 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE78F50 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE78D60 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE78B70 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE78980 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE78790 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE785A0 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE783B0 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE7784C } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE7765C } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE7746C } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE7727C } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE77E68 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE77D04 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE77BA0 } },
    { TASK_BODY_TMD, 96, func_actor_107600_801348A0, { .value = -0x7FE77A3C } },
};

Actor107600Waypoint D_actor_107600_80135078[2] = {
    { 0, 3000, 0 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135084[3] = {
    { 1500, 6000, 40 },
    { -1500, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135098[3] = {
    { 1500, 4500, 40 },
    { -1500, 4500, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801350AC[3] = {
    { 1500, 3000, 40 },
    { -1500, 3000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801350C0[3] = {
    { 1500, 1500, 40 },
    { -1500, 1500, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801350D4[3] = {
    { 1500, 0, 40 },
    { -1500, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801350E8[2] = {
    { -1400, 5800, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801350F4[2] = {
    { -1400, 3000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135100[2] = {
    { -1400, 200, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013510C[3] = {
    { 1500, 6000, 40 },
    { 4500, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135120[3] = {
    { 1500, 4500, 40 },
    { 6000, 4500, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135134[3] = {
    { 1500, 3000, 40 },
    { 6000, 3000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135148[3] = {
    { 1500, 1500, 40 },
    { 6000, 1500, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013515C[3] = {
    { 1500, 0, 40 },
    { 4500, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135170[2] = {
    { 4600, 5800, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013517C[2] = {
    { 4600, 3000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135188[2] = {
    { 4600, 200, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135194[3] = {
    { 6000, 3000, 40 },
    { 6000, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801351A8[3] = {
    { 4500, 3000, 40 },
    { 4500, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801351BC[3] = {
    { 3000, 3000, 40 },
    { 3000, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801351D0[3] = {
    { 1500, 3000, 40 },
    { 1500, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801351E4[3] = {
    { 0, 3000, 40 },
    { 0, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801351F8[3] = {
    { -1500, 3000, 40 },
    { -1500, 0, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013520C[2] = {
    { 4600, 200, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135218[2] = {
    { 1600, 200, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135224[2] = {
    { -1400, 200, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135230[3] = {
    { 6000, 3000, 40 },
    { 6000, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135244[3] = {
    { 4500, 3000, 40 },
    { 4500, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135258[3] = {
    { 3000, 3000, 40 },
    { 3000, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013526C[3] = {
    { 1500, 3000, 40 },
    { 1500, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135280[3] = {
    { 0, 3000, 40 },
    { 0, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135294[3] = {
    { -1500, 3000, 40 },
    { -1500, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801352A8[2] = {
    { 4600, 5800, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801352B4[2] = {
    { 1600, 5800, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801352C0[2] = {
    { -1400, 5800, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801352CC[5] = {
    { 1500, 6000, 50 },
    { 1500, 3000, 50 },
    { 4500, 3000, 50 },
    { 4500, 0, 50 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801352EC[5] = {
    { 1500, 0, 50 },
    { 1500, 3000, 50 },
    { 4500, 3000, 50 },
    { 4500, 6000, 50 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013530C[8] = {
    { 0, 6000, 30 },
    { 0, 4500, 30 },
    { 1500, 4500, 40 },
    { 1500, 6000, 40 },
    { 4500, 6000, 50 },
    { 4500, 4500, 50 },
    { 6000, 4500, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013533C[8] = {
    { 0, 0, 30 },
    { 0, 1500, 30 },
    { 1500, 1500, 40 },
    { 1500, 0, 40 },
    { 4500, 0, 50 },
    { 4500, 1500, 50 },
    { 6000, 1500, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013536C[10] = {
    { -1500, 0, 40 },
    { 0, 0, 40 },
    { 0, 6000, 50 },
    { 1500, 6000, 50 },
    { 1500, 0, 70 },
    { 3000, 0, 70 },
    { 3000, 6000, 100 },
    { 4500, 6000, 100 },
    { 4500, 0, 100 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801353A8[7] = {
    { 9000, 6000, 40 },
    { 9000, 4500, 40 },
    { 6000, 4500, 50 },
    { 3000, 4500, 50 },
    { 3000, 6000, 50 },
    { -3000, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801353D4[7] = {
    { 9000, 0, 40 },
    { 9000, 1500, 40 },
    { 6000, 1500, 50 },
    { 3000, 1500, 50 },
    { 3000, 0, 50 },
    { -3000, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135400[6] = {
    { 9000, 6000, 40 },
    { 9000, 3000, 40 },
    { 9000, 0, 40 },
    { 0x2EE0, 0, 40 },
    { 0x2EE0, 3000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135424[5] = {
    { 7500, 4500, 40 },
    { 7500, 6000, 70 },
    { 0x2EE0, 6000, 70 },
    { 0x2EE0, 4500, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135444[5] = {
    { 7500, 1500, 40 },
    { 7500, 0, 70 },
    { 0x2EE0, 0, 70 },
    { 0x2EE0, 1500, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135464[11] = {
    { 4500, 6000, 140 },
    { 4500, 4500, 140 },
    { 7500, 4500, 140 },
    { 7500, 1500, 140 },
    { 4500, 1500, 140 },
    { 4500, 0, 140 },
    { -3000, 0, 140 },
    { -3000, 3000, 140 },
    { 4500, 3000, 140 },
    { 7500, 3000, 140 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801354A8[7] = {
    { -3000, 1500, 50 },
    { 0, 1500, 50 },
    { 0, 4500, 50 },
    { 3000, 4500, 50 },
    { 3000, 1500, 50 },
    { 7500, 1500, 50 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801354D4[5] = {
    { 1500, 6000, 40 },
    { 4500, 6000, 40 },
    { 4500, 4500, 40 },
    { 7500, 4500, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801354F4[5] = {
    { 1500, 0, 40 },
    { 4500, 1500, 40 },
    { 4500, 1500, 40 },
    { 7500, 1500, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135514[5] = {
    { 0x2EE0, 0, 40 },
    { 0x2EE0, 6000, 40 },
    { 0x2EE0, 0, 40 },
    { 0x2EE0, 6000, 40 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135534[3] = {
    { 6000, 3000, 70 },
    { 6000, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135548[3] = {
    { 4500, 3000, 70 },
    { 4500, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_8013555C[3] = {
    { 3000, 3000, 70 },
    { 3000, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135570[3] = {
    { 1500, 3000, 70 },
    { 1500, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135584[3] = {
    { 0, 3000, 70 },
    { 0, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135598[3] = {
    { -1500, 3000, 70 },
    { -1500, 0, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801355AC[3] = {
    { 6000, 3000, 70 },
    { 6000, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801355C0[3] = {
    { 4500, 3000, 70 },
    { 4500, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801355D4[3] = {
    { 3000, 3000, 70 },
    { 3000, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801355E8[3] = {
    { 1500, 3000, 70 },
    { 1500, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_801355FC[3] = {
    { 0, 3000, 70 },
    { 0, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint D_actor_107600_80135610[3] = {
    { -1500, 3000, 70 },
    { -1500, 6000, 70 },
    { -1, 0, 0 },
};

Actor107600Waypoint* D_actor_107600_80135624[62] = {
    D_actor_107600_80135078,
    D_actor_107600_80135084,
    D_actor_107600_80135098,
    D_actor_107600_801350AC,
    D_actor_107600_801350C0,
    D_actor_107600_801350D4,
    D_actor_107600_801350E8,
    D_actor_107600_801350F4,
    D_actor_107600_80135100,
    D_actor_107600_8013510C,
    D_actor_107600_80135120,
    D_actor_107600_80135134,
    D_actor_107600_80135148,
    D_actor_107600_8013515C,
    D_actor_107600_80135170,
    D_actor_107600_8013517C,
    D_actor_107600_80135188,
    D_actor_107600_80135194,
    D_actor_107600_801351A8,
    D_actor_107600_801351BC,
    D_actor_107600_801351D0,
    D_actor_107600_801351E4,
    D_actor_107600_801351F8,
    D_actor_107600_8013520C,
    D_actor_107600_80135218,
    D_actor_107600_80135224,
    D_actor_107600_80135230,
    D_actor_107600_80135244,
    D_actor_107600_80135258,
    D_actor_107600_8013526C,
    D_actor_107600_80135280,
    D_actor_107600_80135294,
    D_actor_107600_801352A8,
    D_actor_107600_801352B4,
    D_actor_107600_801352C0,
    D_actor_107600_801352CC,
    D_actor_107600_801352EC,
    D_actor_107600_8013530C,
    D_actor_107600_8013533C,
    D_actor_107600_8013536C,
    D_actor_107600_801353A8,
    D_actor_107600_801353D4,
    D_actor_107600_80135400,
    D_actor_107600_80135424,
    D_actor_107600_80135444,
    D_actor_107600_80135464,
    D_actor_107600_801354A8,
    D_actor_107600_801354D4,
    D_actor_107600_801354F4,
    D_actor_107600_80135514,
    D_actor_107600_80135534,
    D_actor_107600_80135548,
    D_actor_107600_8013555C,
    D_actor_107600_80135570,
    D_actor_107600_80135584,
    D_actor_107600_80135598,
    D_actor_107600_801355AC,
    D_actor_107600_801355C0,
    D_actor_107600_801355D4,
    D_actor_107600_801355E8,
    D_actor_107600_801355FC,
    D_actor_107600_80135610,
};

DamageAttack D_actor_107600_8013571C[1] = { 0 };

EnemyParams D_actor_107600_80135720 = { D_actor_107600_8013571C, 50, 0, 0, 0, 255, 0, 0, 0 };

Actor107600Pair D_actor_107600_80135730[8] = {
    { 0, 0xFEE0 },
    { 96, 0xFF60 },
    { 0xFF90, 0xFFA0 },
    { 192, 0xFEC0 },
    { 0xFFE0, 0xFE40 },
    { 0xFF60, 0xFEA0 },
    { 224, 0xFFD0 },
    { 0, 0 },
};

u16 D_actor_107600_80135750[13] = { 32, 24, 16, 12, 60, 36, 28, 20, 12, 1, 12, 20, 28 };

static void func_actor_107600_801344E8(void* arg0, MATRIX* m, s32 mode);

/// Spawn state of the `D_actor_107600_80131E24` table. The target is dropped
/// (and the gallery's live count given back) when the player is within 0x400 on
/// XZ unless `Task::spawnArg1` bit 0x40000000 forces it, when byte 0 of
/// `spawnArg1` is the 0xFF marker, or when the work block cannot be allocated.
/// Otherwise binds the work block's matrices, records the spawn position, and
/// spawns the child from `func_actor_107600_80132DF0`.
static void func_actor_107600_80131F10(Task* arg0)
{
    TmdObject*       obj;
    Enemy*           enemy;
    GfxCoord*        coord;
    GfxCoord*        target;
    void**           scratch;
    u8*              head;
    VECTOR*          block;
    Actor107600Work* work;

    obj                            = arg0->extra.tmd;
    enemy                          = arg0->spawnArg2.pointer;
    coord                          = obj->coords;
    target                         = (gameGetPtrSlot(3))->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = target->coord.t[0] - coord->coord.t[0];
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->vz                      = target->coord.t[2] - coord->coord.t[2];
    if ((!(arg0->spawnArg1.value & 0x40000000) && func_80103D8C(block->vx, block->vz) < 0x401) || (u8)arg0->spawnArg1.value == 0xFF) {
    fail:
        if ((arg0->spawnArg1.value & 0xF000) != 0x2000) {
            ((MistShootingGalleryWork*)arg0->parent->work)->field_0E--;
        }
        SCRATCH_STACK_RELEASE_BYTES(0x10);
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    work       = memCalloc(0x14C, false);
    arg0->work = work;
    if (work == NULL) {
        goto fail;
    }
    arg0->exitCallback = func_actor_107600_80132AC0;
    work->field_146    = (u8)((u32)arg0->spawnArg1.value >> 16);
    work->field_144    = (s32)(arg0->spawnArg1.value & 0xF000) >> 12;
    obj->lightMtx      = &work->matrix_20;
    obj->colorMtx      = &work->matrix_0;
    enemy->param       = &D_actor_107600_80134F84;
    coord->parent      = &gGfxViewCoord;
    enemy->field_4     = &arg0->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    if (work->field_144 != 2) {
        /* retail passes a 0 the resident definition ignores */
        (Gp_IncStateF0Ref)(0);
    }
    work->field_48 = coord->coord.t[0];
    work->field_4A = coord->coord.t[1];
    work->field_4C = coord->coord.t[2];
    work->yaw      = -0x400;
    if (work->field_144 == 1) {
        work->roll += 0x800;
    }
    arg0->state++;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    func_actor_107600_80132DF0(enemy, arg0->spawnArg1.value & 0xF,
                               work->field_144 | (((u32)arg0->spawnArg1.value >> 16) & 0x2000));
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Path-following phase: grows the
/// `field_14B` scale to 100, bobs the model root for four frames, then once the
/// first child raises bit 0x20 steps the root along the `field_146` path of
/// `D_actor_107600_80135624` one waypoint at a time. A waypoint with no step
/// holds for `30 *` the spawn nibble instead; the -1 terminator, the countdown
/// or the child's bit 0x40 stops the path, and bit 0x80 then shrinks the scale
/// back to 0 and advances the task state.
static void func_actor_107600_80132160(Task* arg0)
{
    Actor107600Work*     work  = (Actor107600Work*)arg0->work;
    Enemy*               enemy = arg0->spawnArg2.pointer;
    GfxCoord*            coord = arg0->extra.tmd->coords;
    Actor107600Waypoint* wp;
    s32                  d;
    s16                  x;
    u16                  z;
    s32                  step;

    switch (work->field_140.step) {
        case 0:
            if (work->field_14B < 100) {
                work->field_14B += 8;
                return;
            }
            work->field_14B = 100;
            work->field_13A = 0;
            work->field_140.step++;
        case 1:
            if (++work->field_13A & 1) {
                coord->coord.t[1] = -0x10;
                return;
            }
            coord->coord.t[1] = 0;
            if ((s16)work->field_13A >= 4) {
                work->field_140.step++;
                enemy->task->firstChild->spawnArg1.value |= 0x10;
            }
            return;
        case 2:
            if (!(enemy->task->firstChild->spawnArg1.value & 0x20)) {
                return;
            }
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            if (arg0->spawnArg1.value & 0x10000000) {
                work->field_14A = 1;
            }
            if (wp->step == 0) {
                work->field_140.step = 4;
                work->field_13A      = (((u32)arg0->spawnArg1.value >> 24) & 0xF) * 30;
                return;
            }
            work->field_140.step++;
        case 3:
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            x   = wp->x;
            if (x == -1) {
            stop:
                work->field_140.step                      = 5;
                work->field_14A                           = 0;
                enemy->task->firstChild->spawnArg1.value |= 0x40;
                return;
            }
            d = (s16)(coord->coord.t[0] - x);
            if (d != 0) {
                step = wp->step;
                if (step >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        goto stop;
                    }
                    coord->coord.t[0] = x;
                    work->field_148++;
                } else if (d < 0) {
                    coord->coord.t[0] += step;
                } else {
                    coord->coord.t[0] -= step;
                }
            }
            d = (s16)(coord->coord.t[2] - (u16)wp->z);
            z = wp->z;
            if (d != 0) {
                step = wp->step;
                if (step >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        goto stop;
                    }
                    coord->coord.t[2] = (s16)z;
                    work->field_148++;
                } else if (d < 0) {
                    coord->coord.t[2] += step;
                } else {
                    coord->coord.t[2] -= step;
                }
            }
            return;
        case 4:
            if ((s16)work->field_13A != 0 && (s16)--work->field_13A <= 0) {
                goto stop;
            }
        case 5:
            if (enemy->task->firstChild->spawnArg1.value & 0x80) {
                if (work->field_14B > 0) {
                    work->field_14B -= 8;
                    return;
                }
                work->field_14B = 0;
                arg0->state++;
            }
            break;
    }
}

/// Twin of `func_actor_107600_80132160` that bobs the model root between
/// -0xF4C and -0xF3C instead of -0x10 and 0.
static void func_actor_107600_80132514(Task* arg0)
{
    Actor107600Work*     work  = (Actor107600Work*)arg0->work;
    Enemy*               enemy = arg0->spawnArg2.pointer;
    GfxCoord*            coord = arg0->extra.tmd->coords;
    Actor107600Waypoint* wp;
    s32                  d;
    s16                  x;
    u16                  z;
    s32                  step;

    switch (work->field_140.step) {
        case 0:
            if (work->field_14B < 100) {
                work->field_14B += 8;
                return;
            }
            work->field_14B = 100;
            work->field_13A = 0;
            work->field_140.step++;
        case 1:
            if (++work->field_13A & 1) {
                coord->coord.t[1] = -0xF4C;
                return;
            }
            coord->coord.t[1] = -0xF3C;
            if ((s16)work->field_13A >= 4) {
                work->field_140.step++;
                enemy->task->firstChild->spawnArg1.value |= 0x10;
            }
            return;
        case 2:
            if (!(enemy->task->firstChild->spawnArg1.value & 0x20)) {
                return;
            }
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            if (arg0->spawnArg1.value & 0x10000000) {
                work->field_14A = 1;
            }
            if (wp->step == 0) {
                work->field_140.step = 4;
                work->field_13A      = (((u32)arg0->spawnArg1.value >> 24) & 0xF) * 30;
                return;
            }
            work->field_140.step++;
        case 3:
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            x   = wp->x;
            if (x == -1) {
            stop:
                work->field_140.step                      = 5;
                work->field_14A                           = 0;
                enemy->task->firstChild->spawnArg1.value |= 0x40;
                return;
            }
            d = (s16)(coord->coord.t[0] - x);
            if (d != 0) {
                step = wp->step;
                if (step >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        goto stop;
                    }
                    coord->coord.t[0] = x;
                    work->field_148++;
                } else if (d < 0) {
                    coord->coord.t[0] += step;
                } else {
                    coord->coord.t[0] -= step;
                }
            }
            d = (s16)(coord->coord.t[2] - (u16)wp->z);
            z = wp->z;
            if (d != 0) {
                step = wp->step;
                if (step >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1.value & 0x40) {
                        goto stop;
                    }
                    coord->coord.t[2] = (s16)z;
                    work->field_148++;
                } else if (d < 0) {
                    coord->coord.t[2] += step;
                } else {
                    coord->coord.t[2] -= step;
                }
            }
            return;
        case 4:
            if ((s16)work->field_13A != 0 && (s16)--work->field_13A <= 0) {
                goto stop;
            }
        case 5:
            if (enemy->task->firstChild->spawnArg1.value & 0x80) {
                if (work->field_14B > 0) {
                    work->field_14B -= 8;
                    return;
                }
                work->field_14B = 0;
                arg0->state++;
            }
            break;
    }
}

/// Task states run by `func_actor_107600_801348A0`, indexed by its
/// `Task::state`.
static const TaskFuncTable4 D_actor_107600_80131E74 = { {
    func_actor_107600_80132ED0,
    func_actor_107600_80133024,
    func_actor_107600_80134904,
    func_actor_107600_80134920,
} };

/// Behaviour states indexed by `Actor107600Work.field_158`, run by
/// `func_actor_107600_80133024`.
static const TaskFuncTable10 D_actor_107600_80131E84 = { {
    func_actor_107600_80134C54,
    func_actor_107600_801332D4,
    func_actor_107600_80133668,
    func_actor_107600_80133668,
    func_actor_107600_80134D10,
    func_actor_107600_80134D30,
    func_actor_107600_80134D50,
    func_actor_107600_801337FC,
    func_actor_107600_80134D70,
    func_actor_107600_801339A4,
} };

void func_actor_107600_801328CC(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E24;
    sp.funcs[arg0->state](arg0);
}

/// Update state of the `D_actor_107600_80131E24` table, switched on the scene
/// mode `Gp_StateF0.field_4`. Mode 0 runs the `field_13E` sub-state, copies the yaw and
/// roll onto the model root, rebuilds its rotation and scales `coord.m[1][1]`
/// by the `field_14B` percent; modes 0 and 1 then refresh the colour and show
/// the model, and mode 2 hides it.
static void func_actor_107600_80132930(Task* arg0)
{
    TmdObject*       ext      = arg0->extra.tmd;
    GfxCoord*        coord    = ext->coords;
    Actor107600Work* work     = (Actor107600Work*)arg0->work;
    TaskFunc         funcs[2] = { func_actor_107600_80132CB8, func_actor_107600_80132CD4 };
    TmdObject*       obj;

    obj = ext;
    switch (Gp_StateF0.field_4) {
        case 0:
            funcs[(s16)work->field_13E](arg0);
            coord->param.rot.vy = work->yaw;
            coord->param.rot.vz = work->roll;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            func_actor_107600_80132B7C(arg0);
            coord->coord.m[1][1] = work->field_14B * (coord->coord.m[1][1] / 100);
        case 1:
            func_actor_107600_80132B0C(arg0);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 2:
            ext->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
}

/// State 2 of the `D_actor_107600_80131E24` table: drops this instance from the
/// spawning gallery's live-target count unless it was spawned already counted
/// (phase 2, the `0x200D` cursor target), then advances to the exit state.
static void func_actor_107600_80132A7C(Task* arg0)
{
    Task*            parent;
    Actor107600Work* work;

    parent = arg0->parent;
    work   = (Actor107600Work*)arg0->work;
    if (work->field_144 != 2) {
        ((MistShootingGalleryWork*)parent->work)->field_0E--;
    }
    arg0->state++;
}

/// Enemy exit callback: releases the shared state slot unless the work block
/// has already reached phase 2, then hands the enemy back for destruction.
static void func_actor_107600_80132AC0(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->work;

    if (work->field_144 != 2) {
        Gp_ReleaseStateF0Add(arg0, 0);
    }
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off the scratch stack and hands it to
/// `Gp_UpdateActorColor` for the enemy in `Task::spawnArg2` with no blend
/// parameters. Same shape as `func_actor_107600_801349E0`, a different callee.
static void func_actor_107600_80132B0C(Task* arg0)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;
    void*     obj;

    obj                            = arg0->spawnArg2.pointer;
    coord                          = arg0->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Rebuilds the model root's rotation from the work block's three angles: wrap
/// each to 12 bits, build the rotation in a scratch matrix carved off
/// the scratch stack, then copy its 3x3 into the part's `GfxCoord::coord`.
/// The copy is a call to `func_actor_107600_80132C4C`.
static void func_actor_107600_80132B7C(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GfxCoord*        coord = arg0->extra.tmd->coords;
    MATRIX*          m;

    work->pitch                 &= 0xFFF;
    work->yaw                   &= 0xFFF;
    work->roll                  &= 0xFFF;
    m                            = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    RotMatrixZ((s16)work->roll, m);
    RotMatrixX((s16)work->pitch, m);
    RotMatrixY((s16)work->yaw, m);
    func_actor_107600_80132C4C(m, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Copies the 3x3 rotation of the scratch matrix `func_actor_107600_80132B7C`
/// just built into the part's `GfxCoord::coord`, leaving the translation
/// row of the destination alone.
static void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

static void func_actor_107600_80132CB8(Task* arg0)
{
    Actor107600Work* work = arg0->work;

    work->field_13E++;
}

/// Runs the `D_actor_107600_80131E34` entry for the work block's `field_144`
/// phase through the same stack-copied table idiom as
/// `func_actor_107600_801328CC`, then advances the model's yaw by 0x20 once the
/// spawn flag at `field_14A` says this instance is rotating.
static void func_actor_107600_80132CD4(Task* arg0)
{
    TaskFuncTable3   sp;
    Actor107600Work* work = (Actor107600Work*)arg0->work;

    sp = D_actor_107600_80131E34;
    sp.funcs[work->field_144](arg0);
    if (work->field_14A != 0) {
        work->yaw += 0x20;
    }
}

/// Phase in which the actor waits for its parent's first child to raise bit
/// 0x80 of `Task::spawnArg1`: the first pass zeroes the model root's Y
/// translation and resets the `field_14B` scale to 100, then each frame with
/// the bit set shrinks it by 8 until it reaches 0 and the task state advances.
static void func_actor_107600_80132D54(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GfxCoord*        coord = arg0->extra.tmd->coords;
    Enemy*           enemy = arg0->spawnArg2.pointer;

    switch (work->field_140.step) {
        case 0:
            work->field_140.step++;
            work->field_14B   = 100;
            coord->coord.t[1] = 0;
        case 1:
            if (enemy->task->firstChild->spawnArg1.value & 0x80) {
                if (work->field_14B > 0) {
                    work->field_14B -= 8;
                    return;
                }
                work->field_14B = 0;
                arg0->state++;
            }
            break;
    }
}

/// Spawns the next instance of this actor's own `D_actor_107600_80134F94`
/// table (`arg1 + 1` is the index) and adopts it as a child of `arg0`: the new
/// task is reparented and its root coordinate's `parent` link is pointed at
/// `arg0`'s own root coordinate, `Task::spawnArg1` is packed from the two
/// arguments, and the spawned model takes `arg1`'s texture page - dropping the
/// CLUT row to 0 once `arg1` reaches 10 - before the stream is processed twice
/// (one half-buffer per call) and the enemy's light is set to 0x900.
static void func_actor_107600_80132DF0(Enemy* arg0, s32 arg1, s32 arg2)
{
    Enemy*     enemy;
    GfxCoord*  coord;
    TmdObject* obj;

    enemy = Gp_SpawnEnemyFromTable(D_actor_107600_80134F94, arg1 + 1, arg2, arg0);
    if (enemy != NULL) {
        Task_Reparent(arg0->task, enemy->task);
        coord                        = enemy->task->extra.tmd->coords;
        coord->parent                = arg0->task->extra.tmd->coords;
        enemy->task->spawnArg1.value = arg1 | (arg2 << 16);
        obj                          = enemy->task->extra.tmd;
        obj->texturePageOffset       = 0;
        if (arg1 < 10) {
            obj->clutRowOffset = 2;
        } else {
            obj->clutRowOffset = 0;
        }
        tmdProcessStream(obj);
        tmdProcessStream(obj);
        enemy->workType = ENEMY_WORK_PLAIN;
    }
}

/// Spawn state: allocates the work block, hangs the two matrices off the
/// display object's `field_1C` / `field_20`, puts the actor's own light on the
/// enemy and links its node in. Both failure paths - the 0xFF "already dead"
/// marker in byte 0 of `Task::spawnArg1` and a failed allocation - destroy the
/// enemy and return before the exit callback is installed. The low nibble of
/// `spawnArg1` selects the HP from thirteen serialized halfwords. The gallery
/// demonstration also requests index 13, just beyond the package; the runtime
/// backing of that read remains unresolved. The high halfword's low nibble is
/// stored separately in `field_162` for `func_actor_107600_80134958`.
static void func_actor_107600_80132ED0(Task* arg0)
{
    Actor107600Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    u16              hp;
    u32              variant;

    obj     = arg0->extra.tmd;
    variant = (u8)arg0->spawnArg1.value;
    enemy   = arg0->spawnArg2.pointer;
    coord   = obj->coords;
    if (variant == 0xFF || (work = (Actor107600Work*)memCalloc(0x16C, false), arg0->work = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    arg0->exitCallback    = func_actor_107600_80134920;
    work->field_162       = ((u32)arg0->spawnArg1.value >> 16) & 0xF;
    obj->lightMtx         = &work->matrix_20;
    obj->colorMtx         = &work->matrix_0;
    enemy->param          = &D_actor_107600_80135720;
    enemy->recs           = work->rec18;
    work->field_140.coord = arg0->extra.tmd->coords;
    work->field_144       = 0x140;
    work->field_146       = 2;
    hp                    = D_actor_107600_80135750[arg0->spawnArg1.value & 0xF];
    enemy->hpMax          = hp;
    enemy->hp             = hp;
    func_actor_107600_80134958(arg0);
    Gp_LinkNode(&enemy->node);
    enemy->field_4                = &coord->workm;
    enemy->bodyPos.vy             = -0x244;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = coord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    func_actor_107600_80134E5C(coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    arg0->state += 1;
}

/// Per-frame update switched on the scene mode `Gp_StateF0.field_4`, like
/// `func_actor_107600_80132930`. Mode 0 runs the `field_158` state out of
/// `D_actor_107600_80131E84`, then (below state 7) takes hits, clears the collision records and enters state 9 once the enemy's
/// HP is gone. Afterwards publishes the enemy's slot mask to the gallery and
/// emits one effect per `field_16B` hit, the last one upward when dead.
static void func_actor_107600_80133024(Task* arg0)
{
    TaskFuncTable10  sp;
    Enemy*           enemy;
    TmdObject*       ext;
    TmdObject*       obj;
    Actor107600Work* work;
    GfxCoord*        coord;
    SVECTOR*         v;
    s32              i;

    enemy = arg0->spawnArg2.pointer;
    ext   = arg0->extra.tmd;
    work  = (Actor107600Work*)arg0->work;
    coord = ext->coords;
    obj   = ext;
    sp    = D_actor_107600_80131E84;
    SCRATCH_STACK_RESERVE_BYTES(8);
    v = SCRATCH_STACK_CURSOR(SVECTOR);
    switch (Gp_StateF0.field_4) {
        case 0:
            sp.funcs[work->field_158](arg0);
            if (work->field_158 < 7) {
                if (work->field_150 == 0) {
                    func_actor_107600_80133DC4(arg0);
                } else {
                    work->field_150--;
                }
                Gp_ClearRec18Occupied(work->rec18);
                if (enemy->hp <= 0) {
                    func_actor_107600_80134B98(arg0, 9);
                }
            }
        case 1:
            func_actor_107600_801349E0(arg0);
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
    }
    if (work->field_162 != 2) {
        ((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->field_1D = Gp_NodeSlotMask(&enemy->node);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_actor_107600_80134A50(arg0);
    func_actor_107600_80134EF4(arg0);
    for (i = 0; i < work->field_16B; i++) {
        if (i == work->field_16B - 1 && enemy->hp <= 0) {
            v->vx = 0;
            v->vy = -0xE0;
            v->vz = 0;
            func_actor_107600_80133FA8(coord, v);
        } else {
            v->vx = D_actor_107600_80135730[(work->field_16A + i) & 7].vx;
            v->vy = D_actor_107600_80135730[(work->field_16A + i) & 7].vy;
            v->vz = 0;
            func_actor_107600_80134248(coord, v);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Sub-state machine in `field_15A`: once `Task::spawnArg1` bit 0x10 is set,
/// grows the `field_168`/`field_169` scales by 0x20 up to 100, plays a cue and
/// eases `field_50` down, alternates `field_50` for four frames and raises bit
/// 0x20. From then on, while bit 0x20000000 is set, `field_166` counts frames:
/// at 120 it switches the light mode, at 210 it spawns an effect on the
/// `gameGetPtrSlot(3)` actor's fifth coordinate and updates that actor.
static void func_actor_107600_801332D4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    Task*            player;
    GameActor*       actor;
    s32              pan;
    s32              flags;
    s16              v;

    if ((s16)func_actor_107600_80134BAC(arg0) != 0) {
        return;
    }
    switch (work->field_15A) {
        case 0:
            if (!(arg0->spawnArg1.value & 0x10)) {
                return;
            }
            work->field_15A++;
        case 1:
            if (work->field_168 < 100) {
                work->field_168 += 0x20;
                return;
            }
            work->field_15A++;
        case 2:
            if (work->field_169 < 100) {
                work->field_169 += 0x20;
                return;
            }
            {
                GfxCoord* o = arg0->extra.tmd->coords;
                s32       p;
                work->field_15A++;
                p = (s8)Gp_GetObjPan(o);
                SndEvt_EnqueueType6(0x51140007, p, (s8)gpGetObjDepth(o));
            }
        case 3: {
            u16 w = work->field_50;
            if ((u16)(w - 1) < 0x400) {
                work->field_50 = w - ((0x420 - (s16)w) >> 2);
                return;
            }
        }
            work->field_154 = 0;
            work->field_15A++;
            return;
        case 4:
            v               = work->field_154 + 1;
            work->field_154 = v;
            if (v & 1) {
                work->field_50 = ((v << 16) >> 13) - 0x38;
            } else {
                work->field_50 = 0;
                if ((s16)work->field_154 >= 4) {
                    work->field_15A++;
                    arg0->spawnArg1.value |= 0x20;
                    Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
                    work->obj.flags              |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
            }
        case 5:
            flags = arg0->spawnArg1.value;
            if (flags & 0x40) {
                func_actor_107600_80134B98(arg0, 7);
                return;
            }
            if (!(flags & 0x20000000)) {
                return;
            }
            work->field_166++;
            if ((s16)work->field_166 == 120) {
                GfxCoord* o = arg0->extra.tmd->coords;
                s32       p;
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                p = (s8)Gp_GetObjPan(o);
                SndEvt_EnqueueType6(0x51140013, p, (s8)gpGetObjDepth(o));
            } else if ((s16)work->field_166 == 210) {
                GfxCoord* c;
                s32       p;
                player          = gameGetPtrSlot(3);
                c               = &player->extra.tmd->coords[4];
                actor           = player->work;
                work->field_166 = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_DEFAULT);
                Gp_SpawnEff(0x601BD, c, 0, NULL);
                p = (s8)Gp_GetObjPan(c);
                SndEvt_EnqueueType6(0x5114000E, p, (s8)gpGetObjDepth(c));
                if (actor->field_954 != 1) {
                    if (Player_Status.hp < 11) {
                        ((MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work)->field_22 = 1;
                        actor->field_96E                                                             = 0;
                    } else {
                        actor->field_96E = 10;
                    }
                    actor->field_96C = 1;
                    actor->field_972 = 5;
                    func_8010A9D0(player);
                    pan = (s8)Gp_GetObjPan(c);
                    SndEvt_EnqueueType6(6, pan, (s8)gpGetObjDepth(c));
                }
            }
            break;
    }
}

/// Hit-flinch sub-state machine in `field_15A`: swings `field_50` for six
/// frames with a step scaled by `field_160` (capped at 0x200), then flickers it
/// on odd frames and hands off to state 1 / sub-state 3.
static void func_actor_107600_80133668(Task* arg0)
{
    Actor107600Work* work = arg0->work;
    s32              step;
    s16              count;

    switch (work->field_15A) {
        case 0:
            work->field_15A++;
            work->field_154 = 6;
        case 1:
            step = work->field_160 * 6;
            if (step > 0x200) {
                step = 0x200;
            }
            count = work->field_154;
            step /= 3;
            if (count >= 4) {
                if ((s16)work->field_50 < 0x200) {
                    work->field_50 += step - step / 3 * (6 - count);
                }
            } else if (count <= 0) {
                work->field_50  = 0;
                work->field_154 = 0;
                work->field_15A++;
            } else if ((s16)work->field_50 > 0) {
                work->field_50 -= step + step / 3 * (3 - count);
            }
            work->field_154--;
            break;
        case 2:
            work->field_154++;
            if (work->field_154 & 1) {
                work->field_50 = ((s16)work->field_154 - 7) * 8;
                return;
            }
            work->field_50  = 0;
            work->field_160 = 0;
            if ((s16)work->field_154 >= 4) {
                work->field_158 = 1;
                work->field_15A = 3;
            }
            break;
    }
}

/// Death sequence sub-state machine in `field_15A`: unlinks the enemy node and
/// waits seven frames, plays the death cue, ramps `field_50` up to 0x400, then
/// shrinks the `field_169`/`field_168` scales by 0x20 until both are <= 20 and
/// raises bit 0x80 of `Task::spawnArg1`.
static void func_actor_107600_801337FC(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    Enemy*           enemy = arg0->spawnArg2.pointer;
    GfxCoord*        obj;
    s32              pan;

    switch (work->field_15A) {
        case 0:
            work->field_15A++;
            arg0->spawnArg1.value |= 0x40;
            work->field_154        = 7;
            Gp_UnlinkNode(&enemy->node);
            enemy->recs      = 0;
            work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        case 1:
            work->field_154--;
            if ((s16)work->field_154 <= 0) {
                obj = arg0->extra.tmd->coords;
                work->field_15A++;
                work->field_16B = 0;
                work->field_15C = 0;
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(0x51140009, pan, (s8)gpGetObjDepth(obj));
            }
            break;
        case 2:
            if ((s16)work->field_50 < 0x400) {
                work->field_50 += 0x80;
                return;
            }
            work->field_50 = 0x400;
            work->field_15A++;
        case 3:
            if (work->field_169 > 20) {
                work->field_169 -= 0x20;
                return;
            }
            if (work->field_168 > 20) {
                work->field_168 -= 0x20;
                return;
            }
            work->field_15A++;
            arg0->spawnArg1.value |= 0x80;
            break;
        case 4:
            break;
    }
}

/// Kill sequence sub-state machine in `field_15A`: bumps the gallery's kill
/// count for the spawn slot, plays the kill cue, rolls random spins into
/// `field_58/5A/5C`, detaches the model to world space with the hit direction
/// as a knock-back velocity, then tumbles and shrinks it for 16 frames before
/// advancing `Task::state` and raising bit 0x80 of `Task::spawnArg1`.
static void func_actor_107600_801339A4(Task* arg0)
{
    Actor107600Work*         work  = (Actor107600Work*)arg0->work;
    Enemy*                   enemy = arg0->spawnArg2.pointer;
    TmdObject*               tmd   = arg0->extra.tmd;
    GfxCoord*                obj   = tmd->coords;
    MistShootingGalleryWork* gal   = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->work;
    Actor107600HitPos*       pos;
    s32                      id;
    s32                      pan;
    s16                      x;
    s16                      y;
    s16                      z;
    s32                      v;

    switch (work->field_15A) {
        case 0:
            work->field_15A++;
            tmd->flags            |= TMD_OBJECT_SEMI_TRANS;
            arg0->spawnArg1.value |= 0x40;
            work->field_16B        = 0;
            work->field_15C        = 0;
            Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
            work->field_154 = 0;
            id              = arg0->spawnArg1.value & 0xF;
            gal->pad_0F[id]++;
            if (id < 9) {
                id = 0x51140011;
            } else if (id == 9) {
                id = 0x51140010;
            } else {
                id = 0x51140012;
            }
            pan = (s8)Gp_GetObjPan(obj);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
            work->field_52  = (obj->parent)->param.rot.vy;
            work->field_54  = (obj->parent)->param.rot.vz;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            x               = (gRandomLcgState >> 16) & 0x7F;
            work->field_58  = x;
            if (!((gRandomLcgState >> 16) & 1)) {
                x = -x;
            }
            work->field_58  = x;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            y               = (gRandomLcgState >> 16) & 0x7F;
            work->field_5A  = y;
            if (!((gRandomLcgState >> 16) & 1)) {
                y = -y;
            }
            work->field_5A  = y;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            z               = (gRandomLcgState >> 16) & 0x7F;
            work->field_5C  = z;
            if (!((gRandomLcgState >> 16) & 1)) {
                z = -z;
            }
            work->field_5C = z;
            func_actor_107600_80134B2C(&obj->parent->coord, &obj->coord);
            obj->coord.t[0] += obj->parent->coord.t[0];
            obj->coord.t[1] += obj->parent->coord.t[1];
            obj->coord.t[2] += obj->parent->coord.t[2];
            obj->parent      = &gGfxViewCoord;
            pos              = (Actor107600HitPos*)&work->pitch;
            VectorNormal((VECTOR*)pos, (VECTOR*)pos);
            ApplyMatrixLV(&obj->coord, (VECTOR*)pos, (VECTOR*)pos);
            ((Actor107600HitPos*)&work->pitch)->vx = 0;
            if (obj->coord.t[1] < -2000) {
                v = ((Actor107600HitPos*)&work->pitch)->vy >> 4;
            } else {
                v = ((Actor107600HitPos*)&work->pitch)->vy >> 2;
            }
            ((Actor107600HitPos*)&work->pitch)->vy = v = -v;
            ((Actor107600HitPos*)&work->pitch)->vz     = 0;
            if (v < -220) {
                ((Actor107600HitPos*)&work->pitch)->vy = -220;
            }
            Gp_UnlinkNode(&enemy->node);
            enemy->recs      = 0;
            work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
        case 1:
            work->field_154++;
            if ((s16)work->field_154 < 0x10) {
                ((Actor107600HitPos*)&work->pitch)->vx -= ((Actor107600HitPos*)&work->pitch)->vx >> 4;
                ((Actor107600HitPos*)&work->pitch)->vy -= ((Actor107600HitPos*)&work->pitch)->vy >> 4;
                ((Actor107600HitPos*)&work->pitch)->vz -= ((Actor107600HitPos*)&work->pitch)->vz >> 4;
                work->field_58                         -= work->field_58 >> 6;
                work->field_50                         += work->field_58;
                work->field_5A                         -= work->field_5A >> 6;
                work->field_52                         += work->field_5A;
                work->field_5C                         -= work->field_5C >> 6;
                work->field_54                         += work->field_5C;
                obj->coord.t[0]                        += ((Actor107600HitPos*)&work->pitch)->vx;
                obj->coord.t[1]                        += ((Actor107600HitPos*)&work->pitch)->vy;
                obj->coord.t[2]                        += ((Actor107600HitPos*)&work->pitch)->vz;
                if (obj->coord.t[1] < -0x40) {
                    obj->coord.t[1] += 0x40;
                }
                if (work->field_169 > 20) {
                    work->field_169 -= 0x20;
                    return;
                }
                if (work->field_168 > 20) {
                    work->field_168 -= 0x20;
                    return;
                }
            } else {
                arg0->state++;
                arg0->spawnArg1.value |= 0x80;
            }
            break;
        case 2:
            break;
    }
}

/// Hit handler: for each collision record tagged 0x2xxxx, stores the hit
/// position, applies `Gp_ComputeDamage` to the enemy's HP, plays the hit sound
/// for the first eight hits and picks a light/heavy reaction in `field_15E`.
static void func_actor_107600_80133DC4(Task* arg0)
{
    Actor107600Work* work;
    Enemy*           enemy;
    GfxCoord*        obj;
    s32              i;
    s16              damage;
    s32              pan;

    work  = (Actor107600Work*)arg0->work;
    enemy = arg0->spawnArg2.pointer;
    SCRATCH_STACK_RESERVE_BYTES(8);
    work->field_156 = 0;
    if (Gp_FindRec18(work->obj.context.contacts, 0) != 0) {
        for (i = 0; i < 8; i++) {
            if ((work->rec18[i].key.value & 0xFFFF0000) == 0x20000) {
                work->field_156                        = 1;
                ((Actor107600HitPos*)&work->pitch)->vx = work->rec18[i].response.normal.vx;
                ((Actor107600HitPos*)&work->pitch)->vy = work->rec18[i].response.normal.vy;
                ((Actor107600HitPos*)&work->pitch)->vz = work->rec18[i].response.normal.vz;
                func_actor_107600_80134D9C(arg0);
                damage          = Gp_ComputeDamage(work->rec18[i].key.value, work->field_14C, 0, 0);
                work->field_150 = Gp_GetIdParam2(work->rec18[i].key.value);
                work->field_160 = damage;
                func_800DA6E8(&enemy->node, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    enemy->hp = 0;
                }
                if (damage > 0) {
                    if (work->field_16B < 8) {
                        obj = arg0->extra.tmd->coords;
                        work->field_16B++;
                        pan = (s8)Gp_GetObjPan(obj);
                        SndEvt_EnqueueType6(0x51140008, pan, (s8)gpGetObjDepth(obj));
                    }
                    if (damage >= 0x14) {
                        work->field_15E = 2;
                    } else {
                        work->field_15E = 1;
                    }
                } else {
                    work->field_156 = 0;
                }
            }
        }
    }
    Gp_ClearRec18Occupied(work->rec18);
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Corner offsets of the quad `func_actor_107600_80133FA8` draws.
static const DVECTOR D_actor_107600_80131ED8[] = {
    { -0x100, -0x100 },
    { -0x100, 0x100 },
    { 0x100, -0x100 },
    { 0x100, 0x100 },
};

/// Same as `func_actor_107600_80134248` with a 0x200-wide square, UVs
/// 0x40..0x67 x 0..0x27 and a 0xA0 depth bias.
static void func_actor_107600_80133FA8(GfxCoord* coord, SVECTOR* pos)
{
    Actor107600QuadScratch* s;
    POLY_FT4*               p;
    s32                     i;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor107600QuadScratch));
    s = SCRATCH_STACK_CURSOR(Actor107600QuadScratch);
    for (i = 0; i < 4; i++) {
        s->v[i].vx = pos->vx + (D_actor_107600_80131ED8[i].vx + coord->coord.t[0]);
        s->v[i].vy = pos->vy + (D_actor_107600_80131ED8[i].vy + coord->coord.t[1]);
        s->v[i].vz = coord->coord.t[2] + pos->vz;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&s->v[0]);
    gte_rtps();
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    gte_stsxy(&s->sxy[0]);
    gte_ldv3(&s->v[1], &s->v[2], &s->v[3]);
    gte_rtpt();
    p->tpage = 0x99;
    p->clut  = 0x3E80;
    setUV4(p, 0x40, 0, 0x67, 0, 0x40, 0x27, 0x67, 0x27);
    setShadeTex(p, 1);
    gte_stsxy3(&s->sxy[1], &s->sxy[2], &s->sxy[3]);
    gte_stszotz(&s->otz);
    s->otz -= 0xA0;
    if (s->otz < 0x40) {
        SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor107600QuadScratch));
        return;
    }
    p->x0 = s->sxy[0];
    p->y0 = s->sxy[0] >> 16;
    p->x1 = s->sxy[1];
    p->y1 = s->sxy[1] >> 16;
    p->x2 = s->sxy[2];
    p->y2 = s->sxy[2] >> 16;
    p->x3 = s->sxy[3];
    p->y3 = s->sxy[3] >> 16;
    addPrim(&gGpuCurrentOt[s->otz >> 4], p);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor107600QuadScratch));
}

/// Corner offsets of the quad `func_actor_107600_80134248` draws; the
/// zero fifth entry is never read.
static const DVECTOR D_actor_107600_80131EE8[] = {
    { -0x60, -0x60 },
    { -0x60, 0x60 },
    { 0x60, -0x60 },
    { 0x60, 0x60 },
    { 0, 0 },
};

/// Projects a 0xC0-wide square centred on `pos` (relative to `coord`'s
/// translation) and links it as an unshaded `POLY_FT4` on tpage 0x99,
/// dropping it when its OT depth lands too close.
static void func_actor_107600_80134248(GfxCoord* coord, SVECTOR* pos)
{
    Actor107600QuadScratch* s;
    POLY_FT4*               p;
    s32                     i;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(Actor107600QuadScratch));
    s = SCRATCH_STACK_CURSOR(Actor107600QuadScratch);
    for (i = 0; i < 4; i++) {
        s->v[i].vx = pos->vx + (D_actor_107600_80131EE8[i].vx + coord->coord.t[0]);
        s->v[i].vy = pos->vy + (D_actor_107600_80131EE8[i].vy + coord->coord.t[1]);
        s->v[i].vz = coord->coord.t[2] + pos->vz;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&s->v[0]);
    gte_rtps();
    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    gte_stsxy(&s->sxy[0]);
    gte_ldv3(&s->v[1], &s->v[2], &s->v[3]);
    gte_rtpt();
    p->tpage = 0x99;
    p->clut  = 0x3E80;
    setUV4(p, 0x68, 0, 0x77, 0, 0x68, 0xF, 0x77, 0xF);
    setShadeTex(p, 1);
    gte_stsxy3(&s->sxy[1], &s->sxy[2], &s->sxy[3]);
    gte_stszotz(&s->otz);
    s->otz -= 0x40;
    if (s->otz < 0x40) {
        SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor107600QuadScratch));
        return;
    }
    p->x0 = s->sxy[0];
    p->y0 = s->sxy[0] >> 16;
    p->x1 = s->sxy[1];
    p->y1 = s->sxy[1] >> 16;
    p->x2 = s->sxy[2];
    p->y2 = s->sxy[2] >> 16;
    p->x3 = s->sxy[3];
    p->y3 = s->sxy[3] >> 16;
    addPrim(&gGpuCurrentOt[s->otz >> 4], p);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor107600QuadScratch));
}

/// Weighted mode collapses each column of `m` to one value plus a
/// `gDisplayState.loopCount`-driven sine pulse; black clears the 3x3 part.
static void func_actor_107600_801344E8(void* arg0, MATRIX* m, s32 mode)
{
    s32 i;
    s16 v;

    switch (mode) {
        case ENEMY_COLOR_DEFAULT:
            break;
        case ENEMY_COLOR_WEIGHTED:
            for (i = 0; i < 3; i++) {
                v          = (m->m[0][i] * 7 + m->m[1][i] * 6 + m->m[2][i] * 3) / 33;
                v         += (s16)(rsin(gDisplayState.loopCount * 198) + 0x1000);
                m->m[0][i] = v;
                m->m[1][i] = v;
                m->m[2][i] = v;
            }
            break;
        case ENEMY_COLOR_BLACK:
            m->m[0][0] = 0;
            m->m[0][1] = 0;
            m->m[0][2] = 0;
            m->m[1][0] = 0;
            m->m[1][1] = 0;
            m->m[1][2] = 0;
            m->m[2][0] = 0;
            m->m[2][1] = 0;
            m->m[2][2] = 0;
            break;
    }
}

/// Recolours the model's colour matrix from the `colorMode` pair, blending
/// the two remaps by `colorBlend` while it counts down; a copy of
/// `Gp_UpdateActorColor`.
static void func_actor_107600_80134608(Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3)
{
    TmdObject*      extra;
    MATRIX*         colorMtx;
    s32             mode;
    GpColorScratch* block;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = arg0->task->extra.tmd;
    colorMtx = extra->colorMtx;
    mode     = arg0->colorMode & ENEMY_COLOR_MODE_MASK;
    if ((!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (extra->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != 1)) {
        block = SCRATCH_STACK_RESERVE_BLOCK(GpColorScratch);
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->colorBlend <= 0) {
            func_actor_107600_801344E8(arg0, colorMtx, mode);
        } else {
            block->mtx.m[0][0] = colorMtx->m[0][0];
            block->mtx.m[0][1] = colorMtx->m[0][1];
            block->mtx.m[0][2] = colorMtx->m[0][2];
            block->mtx.m[1][0] = colorMtx->m[1][0];
            block->mtx.m[1][1] = colorMtx->m[1][1];
            block->mtx.m[1][2] = colorMtx->m[1][2];
            block->mtx.m[2][0] = colorMtx->m[2][0];
            block->mtx.m[2][1] = colorMtx->m[2][1];
            block->mtx.m[2][2] = colorMtx->m[2][2];
            func_actor_107600_801344E8(arg0, colorMtx, mode);
            func_actor_107600_801344E8(arg0, &block->mtx, (arg0->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            w0 = (s8)arg0->colorBlend << 8;
            w1 = 0x1000 - w0;
            for (i = 0; i < 3; i++) {
                block->col0.vx = colorMtx->m[0][i];
                block->col0.vy = colorMtx->m[1][i];
                block->col0.vz = colorMtx->m[2][i];
                block->col1.vx = block->mtx.m[0][i];
                block->col1.vy = block->mtx.m[1][i];
                block->col1.vz = block->mtx.m[2][i];
                gte_lddp(w1);
                gte_ldsv(&block->col0);
                gte_gpf12();
                gte_lddp(w0);
                gte_ldsv(&block->col1);
                gte_gpl12();
                gte_stsv(&block->col0);
                colorMtx->m[0][i] = block->col0.vx;
                colorMtx->m[1][i] = block->col0.vy;
                colorMtx->m[2][i] = block->col0.vz;
            }
            if (Gp_StateF0.field_4 == 0) {
                arg0->colorBlend--;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(GpColorScratch);
    }
}

void func_actor_107600_801348A0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E74;
    sp.funcs[arg0->state](arg0);
}

static void func_actor_107600_80134904(Task* arg0)
{
    TmdObject* obj = arg0->extra.tmd;

    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

static void func_actor_107600_80134920(Task* arg0)
{
    Gp_UnlinkObj(&((Actor107600Work*)arg0->work)->obj);
    Gp_DestroyEnemy(arg0->spawnArg2.pointer, arg0);
}

/// Links this actor's display node the way `func_8010C980` does for the
/// gameplay objects: the node's collision table is the `WorldCollisionContact` run at
/// `work->rec18` (count 8), and its `field_1C` payload is the spawn variant's
/// height, 0x220 for the `field_162 == 1` variant and 0x190 otherwise.
static void func_actor_107600_80134958(Task* arg0)
{
    Actor107600Work*       work  = (Actor107600Work*)arg0->work;
    GfxCoord*              coord = arg0->extra.tmd->coords;
    WorldCollisionContact* rec   = work->rec18;

    work->obj.coord            = coord;
    work->obj.context.contacts = rec;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = -0x250;
    work->obj.pos.vz           = 0;
    work->obj.key              = 0x3004C;
    work->obj.radius           = (work->field_162 == 1) ? 0x220 : 0x190;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off the scratch stack and hands it to
/// `func_actor_107600_80134608` with no blend parameters.
static void func_actor_107600_801349E0(Task* arg0)
{
    GfxCoord* coord;
    void**    scratch;
    u8*       head;
    VECTOR*   block;
    void*     obj;

    obj                            = arg0->spawnArg2.pointer;
    coord                          = arg0->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    func_actor_107600_80134608(obj, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Builds a second rotation from the work block's angle trio at +0x50: wrap
/// each to 12 bits, lay an unscaled `MATRIX` down at the scratchpad head, apply
/// `Gfx_RotMatrixZ`/`X`/`Y` to it in that order, copy its 3x3 into the model's
/// `GfxCoord::coord` through `func_actor_107600_80134B2C`, and hand the
/// scratch block back.
static void func_actor_107600_80134A50(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GfxCoord*        coord = arg0->extra.tmd->coords;
    MATRIX*          m;

    work->field_50              &= 0xFFF;
    work->field_52              &= 0xFFF;
    work->field_54              &= 0xFFF;
    m                            = (MATRIX*)(SCRATCH_STACK_CURSOR(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0)         = 0x1000;
    MATRIX_PAIR(m, 0, 2)         = 0;
    MATRIX_PAIR(m, 1, 1)         = 0x1000;
    MATRIX_PAIR(m, 2, 0)         = 0;
    m->m[2][2]                   = 0x1000;
    SCRATCH_STACK_CURSOR(MATRIX) = m;
    Gfx_RotMatrixZ(m, (s16)work->field_54, 0);
    Gfx_RotMatrixX(m, (s16)work->field_50, 0);
    gfxRotMatrixY(m, (s16)work->field_52, 0);
    func_actor_107600_80134B2C(m, &coord->coord);
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. The actor carries this body twice; the other copy is
/// `func_actor_107600_80132C4C`.
static void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

static void func_actor_107600_80134B98(Task* arg0, s16 arg1)
{
    Actor107600Work* work = arg0->work;

    work->field_158 = arg1;
    work->field_15A = 0;
}

/// Applies the transition `work->field_15E` queues once `field_156` is 1:
/// requests 1..5 open states 2, 3, 4, 6 and 5 through the same stores as
/// `func_actor_107600_80134B98` (written out, since the setter is not
/// inlined). The request is always consumed; returns whether one was pending.
static s32 func_actor_107600_80134BAC(Task* arg0)
{
    Actor107600Work* work = arg0->work;

    if (work->field_156 == 1) {
        switch ((s16)(work->field_15E - 1)) {
            case 0: {
                Actor107600Work* w = arg0->work;

                w->field_158 = 2;
                w->field_15A = 0;
                break;
            }
            case 1: {
                Actor107600Work* w = arg0->work;

                w->field_158 = 3;
                w->field_15A = 0;
                break;
            }
            case 2: {
                Actor107600Work* w = arg0->work;

                w->field_158 = 4;
                w->field_15A = 0;
                break;
            }
            case 3: {
                Actor107600Work* w = arg0->work;

                w->field_158 = 6;
                w->field_15A = 0;
                break;
            }
            case 4: {
                Actor107600Work* w = arg0->work;

                w->field_158 = 5;
                w->field_15A = 0;
                break;
            }
        }
        work->field_15E = 0;
        return 1;
    }
    return 0;
}

/// First entry of the `D_actor_107600_80131E84` state table. Variants
/// (0 and 1) start the state at 1, kick the +0x50 rotation trio off at 0x400,
/// roll `gRandomLcgState` into `field_16A` beside the 10 percent scale pair
/// `func_actor_107600_80134EF4` divides the model root's rotation by, rebuild
/// that rotation through `func_actor_107600_80134A50`, and put the spawned
/// object's light into mode 2 with its blend timer cleared. Variant 2 only
/// starts the state at 8 and leaves the scale pair at 100 percent.
static void func_actor_107600_80134C54(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->work;
    Enemy*           obj  = arg0->spawnArg2.pointer;

    switch (work->field_162) {
        case 0:
        case 1:
            work->field_158 = 1;
            work->field_50  = 0x400;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->field_16A = (gRandomLcgState >> 16) & 7;
            work->field_168 = 10;
            work->field_169 = 10;
            func_actor_107600_80134A50(arg0);
            Gp_SetLightMode(obj, ENEMY_COLOR_BLACK);
            obj->colorBlend = 0;
            break;
        case 2:
            work->field_158 = 8;
            work->field_168 = 100;
            work->field_169 = 100;
            break;
    }
}

static void func_actor_107600_80134D10(Task* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

static void func_actor_107600_80134D30(Task* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

static void func_actor_107600_80134D50(Task* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

static void func_actor_107600_80134D70(Task* arg0)
{
    ((Actor107600Work*)arg0->work)->field_16B = 3;
    func_actor_107600_80134B98(arg0, 7);
}

/// Measures the XZ offset from this model's own attach coordinate to the one on
/// the `gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]` actor's model, in a 0x10-byte `VECTOR` carved off
/// the scratch stack the way `func_actor_107600_80134E5C` carves its block, and
/// leaves the distance in `Actor107600Work.field_14C`. With no slot-0 actor the
/// carve is undone and nothing is measured. The distance is only stored once the
/// scratch block has been handed back, which is the order the original compiled
/// in - moving the store up costs a nop after the reload.
static void func_actor_107600_80134D9C(Task* arg0)
{
    Actor107600Work* work;
    GfxCoord*        self;
    GfxCoord*        target;
    void**           scratch;
    u8*              head;
    VECTOR*          block;
    s32              dist;

    work                           = (Actor107600Work*)arg0->work;
    self                           = arg0->extra.tmd->coords;
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD_AT(scratch, void) = block;
    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER] == NULL) {
        SCRATCH_HEAD_AT(scratch, void) = head;
        return;
    }
    target    = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER]->extra.tmd->coords;
    block->vx = target->coord.t[0] - self->coord.t[0];
    block->vy = target->coord.t[1] - self->coord.t[1];
    block->vz = target->coord.t[2] - self->coord.t[2];
    dist      = func_80103D8C(block->vx, block->vz);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
    work->field_14C = dist;
}

/// Rotates a fixed 0x10-byte offset by the coordinate's own `coord` matrix and
/// leaves the result in that matrix's translation row. The offset is carved off
/// the scratch stack the way `func_actor_107600_80132B0C` carves its VECTOR, but
/// is filled with (0, -0x180, 0) and rotated in place by `ApplyMatrixLV`, which
/// also folds in the matrix's existing translation. `func_actor_107600_80132ED0`
/// calls this on the coordinate it then hands to `Gp_UpdateCoord`.
static void func_actor_107600_80134E5C(GfxCoord* arg0)
{
    void**  scratch;
    u8*     head;
    VECTOR* block;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->vx                      = 0;
    block->vy                      = -0x180;
    block->vz                      = 0;
    ApplyMatrixLV(&arg0->coord, block, block);
    arg0->coord.t[0] = block->vx;
    arg0->coord.t[1] = block->vy;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
    arg0->coord.t[2] = block->vz;
}

/// Scales the model root's rotation by the two percent factors
/// `func_actor_107600_80134C54` rolls into the work block: the diagonal
/// `coord.m[0][0]` and `coord.m[2][1]` halves, each read as a raw 16-bit value
/// and re-signed before the divide so the scale stays signed.
static void func_actor_107600_80134EF4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GfxCoord*        coord = arg0->extra.tmd->coords;
    u16              x     = coord->coord.m[0][0];
    u16              y     = coord->coord.m[2][1];

    coord->coord.m[0][0] = (s16)x / 100 * work->field_168;
    coord->coord.m[2][1] = (s16)y / 100 * work->field_169;
}
