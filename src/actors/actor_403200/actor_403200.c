#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80135990.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

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
void func_actor_403200_80141018(Task* arg0);

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).

/// Player HP the per-frame tick reads before it latches the death cinematic.
extern s16 D_80073BA0;
/// Equipped character, read as an in-struct byte so the weapon-anim load stays
/// ordered against the message-argument store.
extern s8 D_8007218A[];
/// Equipped weapon variant added to `Gp_WeaponIdBase[D_8007218A[0] - 1]`.
extern u8         D_80073BA9;
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];

/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_80174D58;

/// Declared here with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_8010C980(GsCOORDINATE2* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5);

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Non-zero while the overlay is shutting down: the spawn states tear their
/// enemies down instead of standing them up, and the state-selecting tick
/// holds `field_6` at zero and re-rolls its sub-state.
extern s16 D_actor_403200_80141C50;

/// Set while the grab's animation is installed on the player; the player's
/// side clears it on release.
extern s32 D_actor_403200_80141C54;

/// Counter the launch state sets and the tick after it winds down; the escort
/// pose driver floors it, drops the escort's body by a fifth of it and picks
/// each phase's targets by its range.
extern s16 D_actor_403200_80141C58;

/// Cleared by both halves of the launch state and exposed through the setter /
/// getter pair `func_actor_403200_80141108` and `func_actor_403200_80141114`;
/// the spinner enemies wait for it to be 1 and die once it is 0.
extern s16 D_actor_403200_80141C5A;

/// Script pairs spawned on the per-frame body's and the death sequence's cues,
/// and on the frames the launch tick's phase selects.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;
extern s32 D_actor_403200_80141C6C;
extern s32 D_actor_403200_80141C74;
extern s32 D_actor_403200_80141C7C;
extern s32 D_actor_403200_80141C88;

/// Pair descriptors the host and its escorts publish as `GpEnemy::param`;
/// `hpMax` is the hit-point pool each one starts with.
extern GpPairSrcE D_actor_403200_80141C00;
extern GpPairSrcE D_actor_403200_80141C20;
extern GpPairSrcE D_actor_403200_80141C30;
extern GpPairSrcE D_actor_403200_80141C40;

/// Per-animation reset argument, a `[?][0x2D]` table indexed by the id that
/// was playing before the switch and the id being switched to.
extern s8 D_actor_403200_8015DC98[][0x2D];

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern GpAnimSet* D_actor_403200_8015E484[];
extern GpAnimSet* D_actor_403200_8015E53C[];
extern GpAnimSet* D_actor_403200_8015E5F4[];

/// Animation table the stand-up tick publishes to the player in its message
/// 0x3FF, and the one handed over when the placement yaw is outside +/-0x400.
extern GpAnimSet* D_actor_403200_8015E6AC[];
extern GpAnimSet* D_actor_403200_8015E6CC[];

/// Drop-point group the falling enemies use this round, rerolled whenever a
/// spawn arrives with `spawnArg1` 0.
extern u8 D_actor_403200_8015E70C;

/// Animation-set table the grab states send the player as message 0x3FF;
/// entry 2 is refreshed from the player's own weapon block.
extern GpAnimSet* D_actor_403200_8015E710[];

/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_403200_8015E72C;

/// Per-`spawnArg1` offset from the host model to the point the falling enemy
/// is stood up at.
extern SVECTOR D_actor_403200_8015E780[];
/// The drop points: `vz` is added to the ring x coordinate and `vx` (less
/// 0x189C) becomes the z coordinate.
extern SVECTOR D_actor_403200_8015E7C0[];
/// `[group][spawnArg1]` index into `D_actor_403200_8015E7C0`.
extern u8 D_actor_403200_8015E840[][8];

/// Enemy spawn table the three launch states of `func_actor_403200_8013D9EC`
/// draw from.
extern TaskDesc D_actor_403200_8015E858;

/// The enemy task's message-handler table, parked in `Task::msgTable`.
extern void* D_actor_403200_8015F770;

/// Three formations of nine positions, and each member's model/spawn argument.
extern SVECTOR             D_actor_403200_8015F7B0[3][9];
extern Actor403200SpawnRec D_actor_403200_8015F888[9];

/// Non-zero once the launch state has published the enemy's position to the
/// player, and cleared again when it restarts.
extern s8 D_actor_403200_8015F8E0;

/// The enemy task itself, published for the overlay's other code.
extern Task* D_actor_403200_8015F8F0;

/// Shared 0x7DA payload buffer.
extern GpCmdArg D_actor_403200_8015F8F4;

/// View-space point the launch tick clears and fills from the host's fourth
/// model part on a state change; the spinner enemies home on it.
extern SVECTOR D_actor_403200_8015F8F8;

/// Reply buffers the rise state and the stand-up tick pass with their message
/// 0x3F8.
extern GpDelayArg D_actor_403200_8015F900;
extern GpDelayArg D_actor_403200_8015FA00;

/// The scratch coordinate the debris effect of `func_actor_403200_8013DC3C` is
/// built on: `F920` is the whole `GsCOORDINATE2` and `F924` its `coord` matrix,
/// which splat names separately because the code takes that address directly.
extern GsCOORDINATE2 D_actor_403200_8015F920;
extern MATRIX        D_actor_403200_8015F924;

extern Actor403200DropCoord D_actor_403200_8015F970;

/// Position and Euler rotation the launch tick sends the player as message
/// 0x3E9.
extern GpXformArg D_actor_403200_8015F9C0;

/// Handwritten overlay-local follow helper. `arg1`/`arg2` select the axis pair
/// and `arg3` the mode; takes the task, not the work block.
void func_actor_403200_801408D8(Task* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Walk `coord` 0x19/0x1000 of the way along its own forward axis (column 2 of
/// its rotation, normalised and GPF-scaled) and flag it for rebuild. The
/// direction vector lives in an `SVECTOR` carved off the scratch head and
/// handed straight back.
static __inline__ void Actor403200_StepForward(GsCOORDINATE2* coord)
{
    u8*      head;
    SVECTOR* dir;

    head               = SCRATCH_HEAD(u8);
    dir                = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_HEAD(void) = dir;

    Gfx_MatrixCol2(&coord->coord, dir);
    VectorNormalSS(dir, dir);
    gte_lddp(0x19);
    gte_ldsv(dir);
    gte_gpf12();
    gte_stsv(dir);

    coord->coord.t[0] += dir->vx;
    coord->coord.t[1] += dir->vy;
    coord->coord.t[2] += dir->vz;
    coord->flg         = 0;

    SCRATCH_POP_BYTES(sizeof(SVECTOR));
}

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Integer part of the last step `func_actor_403200_801324D0` applied.
extern SVECTOR D_actor_403200_8015F8E8;

void func_actor_403200_801412D0(GpEnemy* enemy, Task* task);

void func_actor_403200_8014139C(GpEnemy* enemy, Task* arg1);

/// Rebuild `coord`'s rotation around the yaw it already faces and rescale it:
/// `ratan2` of the rotation's Z basis gives the yaw, `Gfx_RotMatrixY` rebuilds
/// the rotation from it and `ScaleMatrix` applies `xz` on both horizontal axes
/// and `y` on the vertical one. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, handed back once the rotation has been copied onto the
/// coordinate; as an inline the scratch-head accesses stay absolute.
static __inline__ void Actor403200_ScaleRotation(GsCOORDINATE2* coord, s16 xz, s32 y)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                 = (ActorScaleRotScratch*)(SCRATCH_HEAD(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_HEAD(ActorScaleRotScratch) = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = xz;
    sc->scale.vy = y;
    sc->scale.vz = xz;
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
    coord->flg           = 0;

    SCRATCH_POP_BYTES(sizeof(ActorScaleRotScratch));
}

/// The same rebuild at a uniform half scale.
static __inline__ void Actor403200_ShrinkRotation(GsCOORDINATE2* coord)
{
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                 = (ActorScaleRotScratch*)(SCRATCH_HEAD(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_HEAD(ActorScaleRotScratch) = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = 0x800;
    sc->scale.vy = 0x800;
    sc->scale.vz = 0x800;
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
    coord->flg           = 0;

    SCRATCH_POP_BYTES(sizeof(ActorScaleRotScratch));
}

/// Gap from `coord` to the player's coordinate matrix `Player_Status.coordMtx`, into `out`.
static __inline__ void Actor403200_GapToCamera(GsCOORDINATE2* coord, SVECTOR* out)
{
    out->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    out->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    out->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
}

void func_actor_403200_80141800(GpEnemy* arg0, Task* arg1);

static __inline__ void Actor403200_SeedRootCoord(Task* task, Actor403200Work* work)
{
    GsCOORDINATE2*        coord = task->extra.tmd->coords;
    ActorScaleRotScratch* sc;
    s16                   ang;

    sc                                 = (ActorScaleRotScratch*)(SCRATCH_HEAD(u8) - sizeof(ActorScaleRotScratch));
    SCRATCH_HEAD(ActorScaleRotScratch) = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
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
    coord->flg           = 0;

    work->field_0          = 0;
    task->extra.tmd->flags = 0;
    SCRATCH_POP_BYTES(sizeof(ActorScaleRotScratch));
}

typedef s32 (*Actor403200ViewFn)(Task* task, s16 arg);

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

typedef struct Actor403200PendingPos {
    byte    pad_0[0x2B8];
    SVECTOR pos;
} Actor403200PendingPos;

void func_actor_403200_8013FB54(GpEnemy* arg0, Task* arg1);

void func_actor_403200_80141B40(Task* arg0);

void func_actor_403200_8014122C(Task* arg0);

void func_actor_403200_80141234(Task* arg0);

void func_actor_403200_8014123C(Task* arg0);

/// Set `coord`'s rotation to its view-space orientation turned by `yaw`,
/// expressed back in its parent's frame, and refresh the coordinate. The work
/// matrix is borrowed from the scratchpad stack.
void func_actor_403200_801321C4(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    SCRATCH_PUSH(MATRIX);
    rotation = SCRATCH_HEAD(MATRIX);
    actorAccumulateToView(coord, rotation);
    func_8004BFF8(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    SCRATCH_POP(MATRIX);
}

/// Step `coord` by the movement the first `arg2` records of `rec` resolve to,
/// and latch the integer part of that delta into `D_actor_403200_8015F8E8`.
/// Returns whether it moved: set when the X or Z delta is nonzero, and also
/// when only its fractional half is, in which case the coordinate and the
/// latched step are nudged one unit further from zero.
s32 func_actor_403200_801324D0(GsCOORDINATE2* coord, GpRec18* rec, s32 arg2)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(rec, &s->delta, (s16)arg2, NULL) != 0) {
        coord->coord.t[0]         += ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]         += s->delta.vz.h.hi;
        D_actor_403200_8015F8E8.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_403200_8015F8E8.vy = s->delta.vy.w >> 16;
        D_actor_403200_8015F8E8.vz = s->delta.vz.w >> 16;
        val                        = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_403200_8015F8E8.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_403200_8015F8E8.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_403200_8015F8E8.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_403200_8015F8E8.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Rebuild quad `index` of the collision grid as a wall across the front of
/// the task's model: the edge runs 0x1388 either way along the model's x axis,
/// `scale` out along its z axis, with vertices 0 and 1 sitting `drop` below 2
/// and 3. The quad's grid normal becomes the model's unit z axis, and the face
/// record takes flag 3 in area 0x27 and 2 elsewhere.
void func_actor_403200_80132674(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR     dir;
    GpGridFace  face;
    SVECTOR*    normal;
    SVECTOR*    verts;
    GpGridFace* faces;
    SVECTOR*    d;

    normal = &Gp_GridParams->field_4[index];
    verts  = Gp_GridParams->field_8;
    faces  = Gp_GridParams->field_C;

    face.verts[0] = index * 4;
    face.verts[1] = index * 4 + 1;
    face.verts[2] = index * 4 + 2;
    face.verts[3] = index * 4 + 3;
    face.field_8  = index;
    face.field_A  = 3;

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, normal);
    Gfx_MatrixCol0(&task->extra.tmd->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(normal, normal);
    gte_lddp(scale);
    gte_ldsv(normal);
    gte_gpf12();
    gte_stsv(normal);
    gte_lddp(0x1388);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + dir.vx + normal->vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy = dir.vy + normal->vy;
    verts[index * 4].vz                           = verts[index * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + dir.vz + normal->vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] - dir.vx + normal->vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy = -dir.vy + normal->vy;
    verts[index * 4 + 1].vz                           = verts[index * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] - dir.vz + normal->vz;

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;

    Gfx_MatrixCol2(&task->extra.tmd->coords->coord, normal);
    VectorNormalSS(normal, normal);
    gte_lddp(0x1000);
    gte_ldsv(normal);
    gte_gpf12();
    gte_stsv(normal);

    if (gGameSession->at4.loc.area == 0x27) {
        face.field_A = 3;
    } else {
        face.field_A = 2;
    }

    faces[index] = face;
}

/// Pose the fifth escort -- the seven-part model whose coordinate array hangs
/// off `field_ECC[4]` -- for the phase `field_7A4` names. Every part is reset
/// to the same spot (the whole body dropped by a fifth of the counter
/// `D_actor_403200_80141C58`, floored at 0x1CC here), then the phase picks a
/// target yaw per part in `field_784`, and each `field_794` walks toward its
/// target by at most `field_7A6`, which is what drives the part rotations.
/// Phase 2 targets the angles the parts are already at, so it holds the pose it
/// was handed.
void func_actor_403200_801329CC(Task* task)
{
    Actor403200Work* work = (Actor403200Work*)task->work;
    s16              i;

    if (work->field_ECC[4]->task->extra.tmd->buffer == NULL) {
        return;
    }

    if (D_actor_403200_80141C58 < 0x1CC) {
        D_actor_403200_80141C58 = 0x1CC;
    }

    for (i = 0; i < 7; i++) {
        work->field_ECC[4]->task->extra.tmd->coords[i].coord.t[0]     = work->field_ECC[4]->task->extra.tmd->coords[i].coord.t[1] =
            work->field_ECC[4]->task->extra.tmd->coords[i].coord.t[2] = 0;
        if ((u16)i >= 2) {
            work->field_ECC[4]->task->extra.tmd->coords[i].coord.t[2] = (s16)(D_actor_403200_80141C58 / 5);
        }
        work->field_ECC[4]->task->extra.tmd->coords[i].flg = 0;
        Gp_UpdateCoord(&work->field_ECC[4]->task->extra.tmd->coords[i]);
    }

    switch (work->field_7A4) {
        case 4:
            work->field_7A6    = 2;
            work->field_784[1] = 0x180;
            work->field_784[2] = 0x20;
            work->field_784[3] = 0;
            work->field_784[4] = 0;
            work->field_784[5] = 0;
            work->field_784[6] = 0;
            break;

        case 3:
            work->field_7A6 = 8;
            if (D_actor_403200_80141C58 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_403200_80141C58 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x80;
            } else if (D_actor_403200_80141C58 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 6; i < 7; i++) {
                    work->field_784[i] = -0x60;
                }
            } else if (D_actor_403200_80141C58 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 5; i++) {
                    work->field_784[i] = -0x40;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = 0x190;
                }
            } else if (D_actor_403200_80141C58 < 0x1900) {
                work->field_784[1] = 0x40;
                work->field_784[2] = 0;
                for (i = 3; i < 4; i++) {
                    work->field_784[i] = -0x60;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = 0x190;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x40;
                }
            }
            break;

        case 2:
            work->field_7A6    = 0x10;
            work->field_784[1] = 0xC0;
            work->field_784[2] = 0x60;
            work->field_784[3] = 0x20;
            work->field_784[4] = work->field_794[4];
            work->field_784[5] = work->field_794[5];
            work->field_784[6] = work->field_794[6];
            break;

        case 1:
            work->field_7A6 = 0x59;
            if (D_actor_403200_80141C58 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_403200_80141C58 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = 0x200;
            } else if (D_actor_403200_80141C58 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else if (D_actor_403200_80141C58 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else if (D_actor_403200_80141C58 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0x200;
                }
            }
            break;

        case 0:
            work->field_7A6 = 0x10;
            if (D_actor_403200_80141C58 < 0x258) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_403200_80141C58 < 0x400) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_403200_80141C58 < 0x604) {
                work->field_784[1] = 0x100;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x200;
            } else if (D_actor_403200_80141C58 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_403200_80141C58 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_403200_80141C58 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            }
            break;

        case 5:
        default:
            work->field_7A6 = 0x54;
            if (D_actor_403200_80141C58 < 0x400) {
                work->field_784[1] = -0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = 0;
                }
            } else if (D_actor_403200_80141C58 < 0x604) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->field_784[i] = 0;
                }
                work->field_784[6] = -0x200;
            } else if (D_actor_403200_80141C58 < 0x708) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 5; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_403200_80141C58 < 0xC80) {
                work->field_784[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->field_784[i] = 0;
                }
                for (i = 4; i < 7; i++) {
                    work->field_784[i] = -0x200;
                }
            } else if (D_actor_403200_80141C58 < 0x1900) {
                work->field_784[1] = 0x80;
                work->field_784[2] = 0;
                for (i = 3; i < 7; i++) {
                    work->field_784[i] = -0x17C;
                }
            } else {
                work->field_784[1] = 0x80;
                for (i = 2; i < 7; i++) {
                    work->field_784[i] = -0x17C;
                }
            }
            break;
    }

    for (i = 1; i < 7; i++) {
        if (abs(work->field_794[i] - work->field_784[i]) < work->field_7A6) {
            work->field_794[i] = work->field_784[i];
        } else if (work->field_794[i] < work->field_784[i]) {
            work->field_794[i] = work->field_794[i] + work->field_7A6;
        } else {
            work->field_794[i] = work->field_794[i] - work->field_7A6;
        }
        Gfx_RotMatrixX(&work->field_ECC[4]->task->extra.tmd->coords[i].coord, work->field_794[i], 1);
        work->field_ECC[4]->task->extra.tmd->coords[i].flg = 0;
    }
}

/// Walk the yaw `field_7C8` toward `arg1` (clamped to +/-0x200) by at most 0x71
/// per call, turn model part 3 by it through `func_actor_403200_801321C4`, and
/// refresh part 3, the root of the fifth escort's model and part 4.
void func_actor_403200_80133614(Task* task, s16 arg1)
{
    Actor403200Work* work = (Actor403200Work*)task->work;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    task->extra.tmd->coords[3].flg = 0;
    Gp_UpdateCoord(&task->extra.tmd->coords[3]);
    func_actor_403200_801321C4(&task->extra.tmd->coords[3], work->field_7C8);
    task->extra.tmd->coords[3].flg = 0;
    Gp_UpdateCoord(&task->extra.tmd->coords[3]);
    work->field_ECC[4]->task->extra.tmd->coords[0].flg = 0;
    Gp_UpdateCoord(&work->field_ECC[4]->task->extra.tmd->coords[0]);
    task->extra.tmd->coords[4].flg = 0;
    Gp_UpdateCoord(&task->extra.tmd->coords[4]);
}

/// Walk the pitch `field_F00` toward `arg1` (clamped to 0..0x500) by at most
/// 0x10 per call, then pitch model parts 3 and 4 about x: part 3 to half of
/// it, part 4 against it, each net of the pitch its matrix already carries.
void func_actor_403200_801337A0(Task* task, s16 arg1)
{
    Actor403200Work* work = (Actor403200Work*)task->work;
    s16              value;
    s16              pitch4;
    s16              pitch3;

    value = arg1;
    if (arg1 > 0x500) {
        value = 0x500;
    }
    if (arg1 < 0) {
        value = 0;
    }

    if (work->field_F00 < value) {
        if (value - work->field_F00 >= 0x11) {
            work->field_F00 = work->field_F00 + 0x10;
        } else {
            work->field_F00 = value;
        }
    } else if (value < work->field_F00) {
        if (abs(work->field_F00 - value) >= 0x11) {
            work->field_F00 = work->field_F00 - 0x10;
        } else {
            work->field_F00 = value;
        }
    }

    pitch4 = -ratan2(task->extra.tmd->coords[4].coord.m[1][2],
                     task->extra.tmd->coords[4].coord.m[2][2]);
    pitch3 = -ratan2(task->extra.tmd->coords[3].coord.m[1][2],
                     task->extra.tmd->coords[3].coord.m[2][2]);

    Gfx_RotMatrixX(&task->extra.tmd->coords[3].coord, work->field_F00 / 2 - pitch3, 0);
    task->extra.tmd->coords[3].flg = 0;
    Gfx_RotMatrixX(&task->extra.tmd->coords[4].coord, -work->field_F00 - pitch4, 0);
    task->extra.tmd->coords[4].flg = 0;
}

/// Start a blend: `field_7BE` to 0x30 and `field_7C0` to 0x800. Slots 1 and up
/// of the even member of each of the three animation pairs take that rate, and
/// the same slots of the odd member are reset to animation `field_7BC`.
void func_actor_403200_80133920(Task* task)
{
    Actor403200Work* work;
    s32              i;

    work            = (Actor403200Work*)task->work;
    work->field_7BE = 0x30;
    work->field_7C0 = 0x800;
    i               = 1;
    do {
        work->slots0[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim1, i, work->field_7BC);
        i++;
    } while (i < 8);
    i = 1;
    do {
        work->slots2[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim3, i, work->field_7BC);
        i++;
    } while (i < 4);
    i = 1;
    do {
        work->slots4[i].rate = work->field_7BE;
        Gp_AnimResetSlot(&work->anim5, i, work->field_7BC);
        i++;
    } while (i < 4);
}

/// Reseed every slot of the three even animation members from `field_7B3` when
/// the id it names differs from the latched `field_7B2`, then latch it. Each
/// slot also has its `rate` seeded from `field_7B6`, and the reset argument
/// comes from the `[field_7B2][field_7B3]` transition table.
void func_actor_403200_801339FC(Task* arg0)
{
    Actor403200Work* work = (Actor403200Work*)arg0->work;
    s32              i;

    if (work->field_7B2 != work->field_7B3) {
        for (i = 1; i < 8; i++) {
            work->slots0[i].rate = work->field_7B6;
            func_800B4114(&work->anim0, i, work->field_7B3, 0,
                          D_actor_403200_8015DC98[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots2[i].rate = work->field_7B6;
            func_800B4114(&work->anim2, i, work->field_7B3, 0,
                          D_actor_403200_8015DC98[work->field_7B2][work->field_7B3]);
        }
        for (i = 0; i < 4; i++) {
            work->slots4[i].rate = work->field_7B6;
            func_800B4114(&work->anim4, i, work->field_7B3, 0,
                          D_actor_403200_8015DC98[work->field_7B2][work->field_7B3]);
        }
        work->field_7B2 = work->field_7B3;
    }
}

/// Advance the three animation pairs with the odd member of each blended in at
/// weight `field_7C0`: every slot of the odd member ticks at `field_7BE`, every
/// slot of the even one at `field_7B6` less 3, and the pose written to the even
/// member is the mix of the two.
void func_actor_403200_80133B80(Task* arg0)
{
    GpAnimPose       pose0;
    GpAnimPose       pose1;
    Actor403200Work* work     = (Actor403200Work*)arg0->work;
    s32              blend    = work->field_7C0;
    s32              invBlend = 0x1000 - blend;
    s16              i;

    for (i = 1; i < 8; i++) {
        if (i < 11) {
            work->slots1[i].rate = work->field_7BE;
            work->slots0[i].rate = work->field_7B6 - 3;
            func_800B3448(&work->anim0, i, (s32)&pose0, 0);
            func_800B3448(&work->anim1, i, (s32)&pose1, 0);
            Gp_AnimWritePoseCopy(&work->anim0, i, &pose0, &pose1, blend, invBlend);
        } else {
            work->slots0[i].rate = work->field_7B6 - 3;
            Gp_AnimTickIndex(&work->anim0, i);
        }
    }

    for (i = 0; i < 4; i++) {
        work->slots3[i].rate = work->field_7BE;
        work->slots2[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim2, i, (s32)&pose0, 0);
        func_800B3448(&work->anim3, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim2, i, &pose0, &pose1, blend, invBlend);
    }

    for (i = 0; i < 4; i++) {
        work->slots5[i].rate = work->field_7BE;
        work->slots4[i].rate = work->field_7B6 - 3;
        func_800B3448(&work->anim4, i, (s32)&pose0, 0);
        func_800B3448(&work->anim5, i, (s32)&pose1, 0);
        Gp_AnimWritePoseCopy(&work->anim4, i, &pose0, &pose1, blend, invBlend);
    }
}

/// Per-frame animation step. `field_7B0` 1 re-seeds the block through
/// `func_actor_403200_801339FC`, 2 resets every slot of the three even members
/// from `field_7B3` outright; either way the block is armed (`field_7B0` 3, the
/// frame counter and the 0x20-byte scratch at `field_7D0` cleared). Then the
/// slots are advanced: plainly while `field_7B1` is clear, otherwise through the
/// blended path, which clears `field_7B1` again once the first pair's slot 1
/// reports done. The three trailing flags run the head tracker, the yaw walk
/// and the escort pose driver.
void func_actor_403200_80133DD8(Task* arg0)
{
    Actor403200Work* work = (Actor403200Work*)arg0->work;
    Actor403200Work* w;
    s32              i;

    if (work->field_7B0 == 1) {
        func_actor_403200_801339FC(arg0);
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    } else if (work->field_7B0 == 2) {
        w = (Actor403200Work*)arg0->work;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim0, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim2, i, w->field_7B3);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimResetSlot(&w->anim4, i, w->field_7B3);
        }
        w->field_7B2    = w->field_7B3;
        work->field_7B0 = 3;
        work->field_7B4 = 0;
        Mem_Set(work->field_7D0, 0, 0x20);
    }

    if (work->field_7BA == 2) {
        func_actor_403200_80133920(arg0);
        work->field_7BA = 3;
    }

    work->field_7B4++;

    if (work->field_7B1 == 0) {
        w = (Actor403200Work*)arg0->work;
        for (i = 1; i < 8; i++) {
            w->slots0[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim0, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots2[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim2, i);
        }
        for (i = 0; i < 4; i++) {
            w->slots4[i].rate = w->field_7B6;
            Gp_AnimTickIndex(&w->anim4, i);
        }
    } else {
        func_actor_403200_80133B80(arg0);
        if (work->slots1[1].flags & 1) {
            work->field_7B1 = 0;
        }
    }

    if (work->field_EF4 != 0) {
        func_actor_403200_801337A0(arg0, work->field_EFE);
    }
    if (work->field_EF6 != 0) {
        func_actor_403200_80133614(arg0, work->field_7C4);
    }
    if (work->field_EF8 != 0) {
        func_actor_403200_801329CC(arg0);
    }
}

/// Spawn the hit effect for attack `id` on `coord`. The effect kind comes from
/// the attack's param 1 and its rotation from param 0: kinds 2, 4, 6 and 7 use
/// one fixed rotation, every other kind draws one of three off `Gp_LcgState`.
/// The rotation and the effect argument live in a frame borrowed from the
/// scratchpad stack for the duration of the call.
void func_actor_403200_80134044(GsCOORDINATE2* coord, s32 id)
{
    Actor403200EffScratch* sc = (Actor403200EffScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200EffScratch));
    sc->eff.spawnArgLo        = 0x500;
    sc->eff.coord             = coord;
    sc->eff.spawnArgHi        = 3;

    switch (Gp_GetIdParam0(id) & 0xFFFF) {
        case 2:
        case 4:
        case 6:
        case 7:
            sc->rot.vx = 0;
            sc->rot.vy = -0x190;
            sc->rot.vz = 0x258;
            func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
            break;
        case 0:
        case 1:
        case 3:
        case 5:
        case 8:
        case 9:
        default:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            switch ((u16)(((u32)Gp_LcgState >> 16) % 3U)) {
                case 0:
                    sc->rot.vy = 0;
                    sc->rot.vx = 0;
                    sc->rot.vz = 0x384;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 1:
                    sc->rot.vx = 0x258;
                    sc->rot.vy = -0xC8;
                    sc->rot.vz = 0x2BC;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
                case 2:
                    sc->rot.vx = -0x12C;
                    sc->rot.vy = -0x320;
                    sc->rot.vz = 0x320;
                    func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &sc->rot, &sc->eff);
                    break;
            }
            break;
    }

    SCRATCH_POP_BYTES(sizeof(Actor403200EffScratch));
}

s32 func_actor_403200_801341E8(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = Player_Status.coordMtx->t[2] - coords->coord.t[2];
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
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = Player_Status.coordMtx->t[2] - coords->coord.t[2];
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
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    Task*          task;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    task   = (Task*)gameGetPtrSlot(3);
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = Player_Status.coordMtx->t[2] - coords->coord.t[2];
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
            SOFT_TOUCH_REG(flag);
            if (flag == 0x25) {
                flag  = task->extra.tmd->coords->coord.t[0];
                value = 0x1E;
                flag  = flag < 0x4651;
                if (flag) {
                    value = 0x25;
                }
                goto done;
            }
            flag  = task->extra.tmd->coords->coord.t[0];
            value = 0x25;
            flag  = flag < 0x4268;
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
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = arg0->extra.tmd->coords;
    vp->vx = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    vp->vy = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = Player_Status.coordMtx->t[2] - coords->coord.t[2];
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
    SVECTOR        pos;
    SVECTOR*       p;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            flag;
    s32            value;
    s32            view;

    view   = Gp_GetViewIndex() & 0xFF;
    p      = &pos;
    coords = arg0->extra.tmd->coords;
    p->vx  = Player_Status.coordMtx->t[0] - coords->coord.t[0];
    p->vy  = Player_Status.coordMtx->t[1] - coords->coord.t[1];
    dist   = pos.vx * pos.vx;
    p->vz  = Player_Status.coordMtx->t[2] - coords->coord.t[2];
    dist  += pos.vy * pos.vy;
    dist   = SquareRoot0(dist + (pos.vz * pos.vz));
    switch (arg1) {
        case 0:
        case 1:
        case 2:
            flag = view;
            if ((flag != 0x25) && (flag != 0x19)) {
                value = 0x25;
                SOFT_BARRIER();
                flag = dist < 0x1E5A;
            } else if (flag == 0x25) {
                value = 0x25;
                flag  = dist < 0x1E5A;
            } else {
                value = 0x25;
                flag  = dist < 0x1B58;
            }
            if (!flag) {
                value = 0x19;
            }
            return value;
    }
    return 1;
}

/// Reference positions the view selector below measures the player against.
const Actor403200ViewPoints D_actor_403200_80131E64 = {
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
const GpEnemyTaskFuncTable3 D_actor_403200_80131E84 = {
    {
        func_actor_403200_801412D0,
        func_actor_403200_8014139C,
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
    obj    = (Task*)gameGetPtrSlot(3);
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
            SOFT_TOUCH_REG(flag);
            if (flag == 0x25) {
                flag  = obj->extra.tmd->coords->coord.t[0];
                value = 0x1E;
                flag  = flag < 0x4651;
                if (flag) {
                    value = 0x25;
                }
                goto done;
            }
            flag  = obj->extra.tmd->coords->coord.t[0];
            value = 0x25;
            flag  = flag < 0x4268;
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
/// `flg` cleared, and once it has run out to x 0x1CCA in state 0 or 0x2882 in
/// state 1 the step advances and re-arms `field_0`.
void func_actor_403200_80134D40(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   model;
    s16              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;

    if (work->field_4 != 0) {
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_7B3 = 2;
        work->field_7B0 = 1;
        work->field_EFE = 0;
        work->field_E96 = 0xE74;
    }

    SCRATCH_PUSH_BYTES(0xC);
    func_actor_403200_80133DD8(arg0);

    frame = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x12 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(gpGetObjDepth(arg0->extra.tmd->coords) / 2));
    }

    frame = work->slots0[2].curRec & 0x3FF;
    if (frame == 0x18 && work->field_7D8 != frame) {
        s32 id;
        s32 pan;

        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200001;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)(gpGetObjDepth(arg0->extra.tmd->coords) / 2));
    }

    work->field_7D8 = work->slots0[2].curRec & 0x3FF;

    model = arg0->extra.tmd->coords;
    if (Mc_SaveData.field_5C1 != 1) {
        Actor403200_StepForward(model);
    }
    arg0->extra.tmd->coords->flg = 0;

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

    SCRATCH_POP_BYTES(0xC);
}

/// Spawn state of the enemy dispatched through `D_actor_403200_80131E90`:
/// allocate the work block and stand the model up where the host's first
/// escort's part 1 is, in view space.
///
/// The model is reparented to `gGfxViewCoord`, so both halves of that escort's
/// part 1 have to be resolved by hand: `actorAccumulateToView` walks
/// the part's coordinate chain up to the view coordinate for the rotation and
/// `actorLocalToView` carries its origin along the same chain for the
/// translation. The model is then spun by 0x80 of the 0x1000-unit circle, its
/// single display node is linked with a 0x394 extent, and that node is paired
/// with the owning enemy so collisions against it reach this task.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down, the
/// host actor has left the grab states, or the work block cannot be allocated.
void func_actor_403200_8013509C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor403200Work*     host;
    SVECTOR              pos;
    SVECTOR              vec;

    owner = task->parent->spawnArg2;
    host  = (Actor403200Work*)owner->task->work;

    if (D_actor_403200_80141C50 == 1 || host->field_0 == 0x10 || host->field_0 == 5 ||
        host->field_0 == 0xC || host->field_0 == 0x12 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = (TaskIdMap*)work,
         work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->field_1AC              = 0;
    task->extra.tmd->coords->sub = &gGfxViewCoord;
    task->extra.tmd->flags       = 0;

    actorAccumulateToView(&host->field_ECC[0]->task->extra.tmd->coords[1],
                          &task->extra.tmd->coords->coord);

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&host->field_ECC[0]->task->extra.tmd->coords[1], &vec);

    task->extra.tmd->coords->coord.t[0] = vec.vx;
    task->extra.tmd->coords->coord.t[1] = vec.vy;
    task->extra.tmd->coords->coord.t[2] = vec.vz;
    task->extra.tmd->coords->flg        = 0;

    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, 0x80, 0);
    Gp_UpdateCoord(task->extra.tmd->coords);

    pos.vx = pos.vy = pos.vz = 0;
    actorLinkWorkObj(task->extra.tmd->coords, &work->obj0, &work->rec0, &pos, 0x394, 3,
                     1);

    work->obj0.flags &= 0x7FFF;
    work->obj0.key    = Gp_PackObjPair(owner, 2);
    work->field_1A8   = 1;
    task->state++;
}

/// Flight state of the enemy dispatched through `D_actor_403200_80131E90`:
/// carry the model along its own forward axis until it lands. `field_1A8` (the
/// dispatcher's state-changed flag) re-arms the step counter, the ground marker
/// and the first display node on the frame the state starts.
///
/// While the game is running (`Gp_StateF0.field_4` clear) the model falls 0xA a step,
/// column 2 of its coordinate is normalised into a scratchpad `SVECTOR` and
/// scaled by 0x89/0x1000 through the GTE's GPF, and that is the per-step
/// translation added to the coordinate; past step 0x29 the height is pinned to
/// -0x3E8 instead. The marker grows 0x60 a step and is drawn under the work
/// block's own coordinate, which is parented to `gGfxViewCoord` and tracks the
/// model. After 0x35 steps the display node is handed back and the task steps
/// on. Paused (`Gp_StateF0.field_4` set) only the coordinate is refreshed, and the
/// marker is skipped while the host's `field_F08` step is 6.
///
/// Bails out -- unlinking the display node and stepping the task on -- when the
/// overlay is shutting down or the host has moved to state 5, 0xC, 0x10 or
/// 0x12.
void func_actor_403200_801354A4(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Actor403200Work*     host;
    GpEnemy*             owner;
    u8*                  head;
    SVECTOR*             dir;
    /// Second live alias of `dir`: the GTE operand is kept in its own register
    /// for the whole function, which is what gives this function its seventh
    /// callee-saved slot.
    SVECTOR* gteDir;

    work  = (Actor403200GrabWork*)task->work;
    owner = task->parent->spawnArg2;
    host  = (Actor403200Work*)owner->task->work;

    if (D_actor_403200_80141C50 == 1 || host->field_0 == 0x10 || host->field_0 == 5 ||
        host->field_0 == 0xC || host->field_0 == 0x12) {
        task->state++;
        Gp_UnlinkObj(&work->obj0);
        return;
    }

    head                  = SCRATCH_HEAD(u8);
    dir                   = (SVECTOR*)(head - sizeof(SVECTOR));
    SCRATCH_HEAD(SVECTOR) = dir;
    gteDir                = dir;

    if (work->field_1A8 != 0) {
        work->field_1AC   = 0;
        work->field_1B0   = 0x400;
        work->field_1A8   = 0;
        work->rec0.key    = 0;
        work->obj0.flags |= 0x8000;
    }

    if (Gp_StateF0.field_4 == 0) {
        work->field_1AC++;
        task->extra.tmd->coords->coord.t[1] += 0xA;

        Gfx_MatrixCol2(&task->extra.tmd->coords->coord, dir);
        VectorNormalSS(dir, dir);
        gte_lddp(0x89);
        gte_ldsv(gteDir);
        gte_gpf12();
        gte_stsv(gteDir);

        task->extra.tmd->coords->coord.t[0] += dir->vx;
        task->extra.tmd->coords->coord.t[1] += dir->vy;
        if (work->field_1AC >= 0x29) {
            task->extra.tmd->coords->coord.t[1] = -0x3E8;
        }
        task->extra.tmd->coords->coord.t[2] += dir->vz;
        task->extra.tmd->coords->flg         = 0;

        work->field_1B0 += 0x60;
        Gp_ClearRec18Occupied(&work->rec0);

        work->coord.sub = &gGfxViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = task->extra.tmd->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = task->extra.tmd->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                             Gp_State1C->groundShade);

        if (work->field_1AC >= 0x35) {
            Gp_UnlinkObj(&work->obj0);
            task->state++;
            work->field_1A8 = 1;
        }
    } else {
        work->coord.sub = &gGfxViewCoord;
        Gfx_RotMatrixY(&work->coord.coord, 0, 1);
        work->coord.coord.t[0] = task->extra.tmd->coords->coord.t[0];
        work->coord.coord.t[1] = 0;
        work->coord.coord.t[2] = task->extra.tmd->coords->coord.t[2];
        work->coord.flg        = 0;
        Gp_UpdateCoord(&work->coord);

        if (host->field_F08 != 6) {
            Gp_DrawEffGroundQuad((VECTOR3*)work->coord.workm.t, ((s16)work->field_1B0 >> 3) + 0x100,
                                 Gp_State1C->groundShade);
        }
    }

    SCRATCH_POP_BYTES(sizeof(SVECTOR));
}

/// State handlers of the enemy stood up on the host's first escort: spawn,
/// flight, teardown.
const GpEnemyTaskFuncTable3 D_actor_403200_80131E90 = {
    {
        func_actor_403200_8013509C,
        func_actor_403200_801354A4,
        Gp_DestroyEnemy,
    },
};

/// Entry state of the enemy dispatched through `D_actor_403200_80131E9C`:
/// allocate its work block and drop the model onto the floor of the view
/// coordinate, under escort 1 of the host actor.
///
/// The model is reparented to `gGfxViewCoord`, its texture page cleared and its
/// CLUT row set to 2, and -- once the stream buffers exist -- processed twice
/// before the spawn cue is enqueued at the model's own pan and half its depth
/// with the owner's id in its high half. The task's light and colour matrices
/// are pointed into the work block, the translation is replaced by the world
/// position of part 1 of escort 1's model, and `field_1AA` is a fifteenth of
/// that height. `vel` is the horizontal gap to the player, which the later
/// states spend a fifteenth at a time. The rotation is finally rebuilt at half
/// scale around the yaw the model already faces.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_403200_80135854(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Actor403200Work*     host;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    host   = (Actor403200Work*)owner->task->work;
    player = gameGetPtrSlot(3);

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work       = memCalloc(sizeof(Actor403200GrabWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->sub = &gGfxViewCoord;
    task->extra.tmd->flags       = 0;
    task->extra.tmd->tpage       = 0;
    task->extra.tmd->clut        = 2;

    if (task->extra.tmd->buffer != NULL) {
        tmdProcessStream(task->extra.tmd);
        tmdProcessStream(task->extra.tmd);
        sfx = ((owner->placeKey >> 0xC) << 8) | 0x4020001C;
        pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(gpGetObjDepth(task->extra.tmd->coords) / 2));
    }

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&host->field_ECC[1]->task->extra.tmd->coords[1], &vec);

    task->extra.tmd->coords->coord.t[0] = vec.vx;
    task->extra.tmd->coords->coord.t[1] = vec.vy;
    task->extra.tmd->coords->coord.t[2] = vec.vz;
    task->extra.tmd->coords->flg        = 0;

    work->field_1AA = task->extra.tmd->coords->coord.t[1] / 15;
    work->vel.vx =
        player->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        player->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->field_1AC = 0;
    work->field_1B2 = 0;

    Actor403200_ShrinkRotation(task->extra.tmd->coords);
    task->state++;
}

/// Bounce state of the enemy dispatched through `D_actor_403200_80131E9C`:
/// bounce the model on the floor until it settles. While the model is still below the floor plane (`coord.t[1] > 0`)
/// it is snapped back to -0x32, the step counter is cleared, the impact cue is
/// enqueued with the object's own pan and half its depth, and the task steps
/// on. Otherwise the body keeps falling by `field_1AA`'s magnitude, drifts a
/// fifteenth of `vel` in x and z, has its colour refreshed from the model's
/// world position, damps the two shake terms and has its rotation rebuilt at
/// half scale.
void func_actor_403200_80135CB8(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = (Actor403200GrabWork*)task->work;
    GsCOORDINATE2*       coord;
    VECTOR               pos;
    s32                  sfx;
    s32                  pan;
    s32                  drop;
    s32                  bounce;

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    coord = task->extra.tmd->coords;
    drop  = coord->coord.t[1];
    if (drop > 0) {
        coord->coord.t[1] = -0x32;
        work->field_1AC   = 0;
        sfx               = ((enemy->placeKey >> 0xC) << 8) | 0x4020000C;
        pan               = (s8)Gp_GetObjPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)(gpGetObjDepth(task->extra.tmd->coords) / 2));
        task->state++;
        return;
    }

    bounce            = ABS(work->field_1AA);
    coord->coord.t[1] = drop + bounce;

    task->extra.tmd->coords->coord.t[0] += work->vel.vx / 15;
    task->extra.tmd->coords->coord.t[2] += work->vel.vz / 15;
    task->extra.tmd->coords->flg         = 0;

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    Actor403200_ShrinkRotation(task->extra.tmd->coords);
}

/// Rise state of the enemy dispatched through `D_actor_403200_80131E9C`: for
/// the first nine steps the model is stretched taller and thinner each step --
/// horizontally `step * 400 + 0x800` and vertically `0x800 / step` -- around
/// the yaw it already faces. On step 7 it is squashed to 0x17A0 wide at normal
/// height, and if the player is within 1000 units horizontally, is not in mode
/// 2, still has HP and answers the 0x3F8 query, the overlay's own animation-set
/// table is sent as message 0x3FF and the take-over is latched in `field_1B2`.
/// The task steps on once the count passes ten with no animation installed,
/// once the latched animation has been released, or after 200 steps. Every
/// step refreshes the model's colour from its world position and damps the two
/// shake terms. Bails to `Gp_DestroyEnemy` when the overlay is shutting down,
/// cancelling a still-installed animation on the way out.
void func_actor_403200_80135F98(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    GameActor*           actor;
    PlayerStatus*        cfg;
    SVECTOR              gap;
    VECTOR               pos;
    s16                  step;
    s16                  scale;
    s32                  shrink;

    work   = (Actor403200GrabWork*)task->work;
    player = gameGetPtrSlot(3);
    actor  = (GameActor*)player->work;
    cfg    = &Player_Status;

    if (D_actor_403200_80141C50 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    step = ++work->field_1AC;
    if (step < 10) {
        scale  = step * 0x190 + 0x800;
        shrink = 0x800 / step;
        Actor403200_ScaleRotation(task->extra.tmd->coords, scale, shrink);
    }

    if (work->field_1AC == 7) {
        Actor403200_ScaleRotation(task->extra.tmd->coords, 0x17A0, 0x800);

        gap.vx = task->extra.tmd->coords->coord.t[0] -
                 player->extra.tmd->coords->coord.t[0];
        gap.vy = 0;
        gap.vz = task->extra.tmd->coords->coord.t[2] -
                 player->extra.tmd->coords->coord.t[2];

        if (actorOutOfReach(&gap) == 0 && actor->field_954 != 2 &&
            cfg->hp > 0) {
            D_actor_403200_8015F900.field_14 = 0x28;
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_403200_8015F900, 0) == 0) {
                D_actor_403200_80141C54  = 1;
                work->anim.animBlock.ptr = D_actor_403200_8015E710;
                work->anim.field_4       = 1;
                work->anim.field_8       = 0;
                work->anim.field_C       = 3;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_1B2 = 1;
            }
        }
    }

    if (work->field_1AC >= 11 && work->field_1B2 == 0) {
        task->state++;
    } else if (work->field_1B2 == 1 && D_actor_403200_80141C54 == 0) {
        task->state++;
    } else if (work->field_1AC >= 0xC9) {
        D_actor_403200_80141C54 = 0;
        task->state++;
    }

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}

/// Hold state of the enemy dispatched through `D_actor_403200_80131E9C`: once
/// `field_1A8` says the take-over is armed and `field_1B2` says the player
/// animation is already installed, rebuild the overlay's own animation-set
/// table from the player's current weapon block and (re)send it as message
/// 0x3FF, flagging the model object busy. Then count the step, and after nine
/// of them cancel the animation with message 0x3F1 and step the task on.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down, cancelling a
/// still-installed animation on the way out.
void func_actor_403200_801364F4(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    s32                  armed;

    work   = (Actor403200GrabWork*)task->work;
    player = gameGetPtrSlot(3);
    if (D_actor_403200_80141C50 == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        armed           = work->field_1B2;
        work->field_1AC = 0;
        if (armed != 1) {
            task->state++;
            return;
        }
        D_actor_403200_8015E710[2] =
            ((Actor403200AnimTable*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A[0] - 1] + D_80073BA9])->sets[9];
        work->anim.animBlock.ptr = D_actor_403200_8015E710;
        work->anim.field_4       = 2;
        work->anim.field_8       = armed;
        work->anim.field_C       = 9;
        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        task->extra.tmd->flags = 0x80;
    }

    if (work->field_1AC >= 9) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
        work->field_1B2 = 0;
        task->state++;
    }
    work->field_1AC++;
}

/// State handlers of the grab enemy, by state: entry, bounce, rise, hold and
/// teardown.
const GpEnemyTaskFuncTable5 D_actor_403200_80131E9C = {
    {
        func_actor_403200_80135854,
        func_actor_403200_80135CB8,
        func_actor_403200_80135F98,
        func_actor_403200_801364F4,
        Gp_DestroyEnemy,
    },
};

/// Spawn state of this enemy: allocate the work block, drop the model onto the
/// floor of the view coordinate and hang the two display nodes off it.
///
/// The model is reparented to `gGfxViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `field_1AA` is a ninth of that height and `vel` the
/// horizontal gap to the player, which the later states spend a fifteenth at a
/// time. The landing cue is enqueued at the model's own pan and depth with the
/// owner's id in its high half, the model is spun to a random yaw, and the two
/// nodes are linked with their collision-record tables before the task's colour
/// and light matrices are pointed into the work block.
void func_actor_403200_8013669C(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    GpEnemy*             owner;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2;
    player = gameGetPtrSlot(3);

    if (D_actor_403200_80141C50 == 1 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->sub = &gGfxViewCoord;
    task->extra.tmd->flags       = 0;

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&owner->task->extra.tmd->coords[3], &vec);

    task->extra.tmd->coords->coord.t[0] = vec.vx;
    task->extra.tmd->coords->coord.t[1] = vec.vy;
    task->extra.tmd->coords->coord.t[2] = vec.vz;
    task->extra.tmd->coords->flg        = 0;

    work->field_1AA = task->extra.tmd->coords->coord.t[1] / 9;
    work->vel.vx =
        player->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        player->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->placeKey >> 0xC) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(sfx, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, ((u32)Gp_LcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    actorLinkWorkObj(task->extra.tmd->coords, &work->obj0, &work->rec0, &vec, 0x100, 3, 1);

    work->obj1.coord    = task->extra.tmd->coords;
    work->obj1.ctx.recs = &work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.key      = 0x3000A;
    work->obj1.radius   = 0x100;
    work->obj1.flags    = 1;
    Gp_LinkObj(2, &work->obj1);

    work->obj0.flags |= 0x8000;
    Gp_InitRec18Table(work->obj1.ctx.recs, 3, 0);
    work->obj1.flags |= 0x4000;
    work->obj0.key    = Gp_PackObjPair(owner, 5);

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;
}

/// Fall state of the enemy dispatched through `D_actor_403200_80131F04`, the
/// one after its spawn: once the model's y has passed its apex
/// (gone negative) both display nodes get their draw flags raised and the
/// bounce height `field_1AA` is added back to y as a magnitude each step. When
/// y reaches -0x31 or above it is clamped to -0x32, the step counter is reset,
/// the landing sound is played at the model's own pan and depth, and the task
/// steps on. Collision against `rec1` -- and, in room 0x0427 past x 0x4B65 --
/// kills the horizontal velocity, whatever is left of it moves the model by a
/// ninth per step, and the model's own `workm` translation is handed to
/// `Gp_UpdateActorColor`.
void func_actor_403200_80136ACC(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = (Actor403200GrabWork*)task->work;
    VECTOR               pos;
    s32                  pan;

    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        Gp_SetLightMode(enemy, 0);
        task->extra.tmd->flags = 2;
    }

    if (task->extra.tmd->coords->coord.t[1] < 0) {
        work->obj0.flags                    |= 0x8000;
        work->obj1.flags                    |= 0x4000;
        task->extra.tmd->coords->coord.t[1] += ABS(work->field_1AA);
    }

    if (task->extra.tmd->coords->coord.t[1] >= -0x31) {
        task->extra.tmd->coords->coord.t[1] = -0x32;
        work->field_1AC                     = 0;
        pan                                 = (s8)Gp_GetObjPan(task->extra.tmd->coords);
        SndEvt_EnqueueType6(0x4020000C, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));
        task->state++;
    }

    if (func_actor_403200_801324D0(task->extra.tmd->coords, &work->rec1, 3) != 0) {
        work->vel.vz = 0;
        work->vel.vx = 0;
    }

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        task->extra.tmd->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    Gp_ClearRec18Occupied(&work->rec1);
    Gp_ClearRec18Occupied(&work->rec0);

    task->extra.tmd->coords->coord.t[0] += work->vel.vx / 9;
    task->extra.tmd->coords->coord.t[2] += work->vel.vz / 9;
    task->extra.tmd->coords->flg         = 0;

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// Settling state of the same enemy, after the fall: the step counter drives the whole
/// thing. When the dispatcher flags a state change the horizontal velocity is
/// cut to a ninth, both light modes are reset and the two display nodes drop
/// the draw flags the descent raised. Past x 0x4B65 in room 0x0427 the x
/// velocity is killed outright; for the first eight steps what is left of it
/// moves the model and is halved again each step. Steps 1, 2, 4, 8 and 20 puff
/// a `0x600A5` effect out of the model's coordinate, and 4 and 8 also switch
/// the light mode. After 0x51 steps both nodes are unlinked and the task steps
/// on; until then the two collision-record tables are wiped each step. The
/// model's own `workm` translation is handed to `Gp_UpdateActorColor`.
void func_actor_403200_80136D94(GpEnemy* enemy, Task* task)
{
    Actor403200GrabWork* work = (Actor403200GrabWork*)task->work;
    VECTOR               pos;
    s16                  step;

    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    if (work->field_1A8 != 0) {
        work->field_1AC = 0;
        work->vel.vx   /= 9;
        work->vel.vz   /= 9;
        Gp_SetLightMode(enemy, 0);
        Gp_SetLightMode(enemy, 1);
        work->obj1.flags      &= ~0x4000;
        work->obj0.flags      &= ~0x8000;
        task->extra.tmd->flags = 2;
    }

    work->field_1AC++;

    if ((*(u32*)&gGameSession->at4.loc & 0xFFFF0000) == 0x04270000 &&
        task->extra.tmd->coords->coord.t[0] >= 0x4B65) {
        work->vel.vx = 0;
    }

    if (work->field_1AC < 8) {
        task->extra.tmd->coords->coord.t[0] += work->vel.vx;
        task->extra.tmd->coords->coord.t[2] += work->vel.vz;
        work->vel.vx                       >>= 1;
        work->vel.vz                       >>= 1;
        task->extra.tmd->coords->flg         = 0;
    }

    step = work->field_1AC - 1;
    switch (step) {
        case 3:
        case 7:
            Gp_SpawnEff(0x600A5, task->extra.tmd->coords, 1, NULL);
            Gp_SetLightMode(enemy, 2);
            break;
        case 0:
        case 1:
        case 19:
            Gp_SpawnEff(0x600A5, task->extra.tmd->coords, 1, NULL);
            break;
    }

    if (work->field_1AC >= 0x51) {
        Gp_UnlinkObj(&work->obj0);
        Gp_UnlinkObj(&work->obj1);
        task->state++;
    }

    if (work->field_1AC < 0x51) {
        Gp_ClearRec18Occupied(&work->rec1);
        Gp_ClearRec18Occupied(&work->rec0);
    }

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
}

/// State handlers of the enemy dropped from the host's part 3: spawn, fall,
/// settle, teardown.
const GpEnemyTaskFuncTable4 D_actor_403200_80131F04 = {
    {
        func_actor_403200_8013669C,
        func_actor_403200_80136ACC,
        func_actor_403200_80136D94,
        Gp_DestroyEnemy,
    },
};

/// Spawn state of the enemy dispatched through `D_actor_403200_80131F14`:
/// allocate its work block and pick the point it will be dropped on.
///
/// A spawn with `spawnArg1` 0 rerolls the drop-point group in
/// `D_actor_403200_8015E70C`, mapping the two low bits of the LCG onto group
/// 1, 1, 2 and 0. `work->target` is then the host model's position pushed out
/// by 0x1B58, 0x2710 or 0x32C8 -- whichever ring the host is on, measured
/// against the player -- plus the `[group][spawnArg1]` entry of
/// `D_actor_403200_8015E7C0`, with a 0..0x7F jitter on z. `spawnArg1` 4 drops
/// on the player instead. The model itself is stood up beside the host at the
/// `D_actor_403200_8015E780` offset, its work coordinate is parented to
/// `gGfxViewCoord` with an identity rotation and carries the single display
/// node, and the spawn cue is enqueued at the model's own pan and depth with
/// the owner's id in its high half. The trailing `Gp_SpawnEff` effect becomes
/// this task's parent so it dies with it.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void func_actor_403200_8013709C(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    GpEnemy*             owner;
    Task*                parent;
    Task*                player;
    OverlayMat*          mtx;
    SVECTOR              vec;
    s32                  dist;
    s32                  rnd;
    s32                  snd;
    s32                  pan;

    player = gameGetPtrSlot(3);
    owner  = task->parent->spawnArg2;
    parent = task->parent;

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(Actor403200DropWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->sub = &gGfxViewCoord;
    work->field_1AA              = 0;

    Actor403200_GapToCamera(task->extra.tmd->coords, &vec);

    if ((u16)task->spawnArg1 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        rnd         = ((u32)Gp_LcgState >> 16) & 3;
        switch (rnd) {
            case 0:
            case 1:
                D_actor_403200_8015E70C = 1;
                break;
            case 2:
                D_actor_403200_8015E70C = rnd;
                break;
            case 3:
                D_actor_403200_8015E70C = 0;
                break;
            default:
                D_actor_403200_8015E70C = 0;
                break;
        }
    }

    Actor403200_GapToCamera(parent->extra.tmd->coords, &vec);
    dist  = vec.vx * vec.vx;
    dist += vec.vy * vec.vy;
    dist += vec.vz * vec.vz;
    dist  = SquareRoot0(dist);

    if (dist < 0x1F40) {
        work->target.vx = parent->extra.tmd->coords->coord.t[0] + 0x1B58;
    } else if (dist < 0x2AF8) {
        work->target.vx = parent->extra.tmd->coords->coord.t[0] + 0x2710;
    } else {
        work->target.vx = parent->extra.tmd->coords->coord.t[0] + 0x32C8;
    }

    work->target.vx +=
        D_actor_403200_8015E7C0[D_actor_403200_8015E840[D_actor_403200_8015E70C]
                                                       [(u16)task->spawnArg1]]
            .vz;
    work->target.vy = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->target.vz = D_actor_403200_8015E7C0[D_actor_403200_8015E840[D_actor_403200_8015E70C]
                                                                     [(u16)task->spawnArg1]]
                          .vx -
                      0x189C;
    work->target.vz = (((u32)Gp_LcgState >> 16) & 0x7F) + work->target.vz;

    if ((u16)task->spawnArg1 == 4) {
        work->target.vx = player->extra.tmd->coords->coord.t[0];
        work->target.vy = 0;
        work->target.vz = player->extra.tmd->coords->coord.t[2];
    }

    work->timer     = 0;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_1AE = ((u32)Gp_LcgState >> 16) & 8;

    vec.vx = D_actor_403200_8015E780[(u16)task->spawnArg1].vx;
    vec.vy = D_actor_403200_8015E780[(u16)task->spawnArg1].vy;
    vec.vz = D_actor_403200_8015E780[(u16)task->spawnArg1].vz;

    task->extra.tmd->coords->coord.t[0] = vec.vx + parent->extra.tmd->coords->coord.t[0];
    task->extra.tmd->coords->coord.t[1] = vec.vy;
    task->extra.tmd->coords->coord.t[2] = vec.vz + parent->extra.tmd->coords->coord.t[2];

    work->obj.key = Gp_PackObjPair(owner, 1);

    vec.vx = 0;
    vec.vy = 0;
    vec.vz = 0;

    work->coord.sub    = &gGfxViewCoord;
    mtx                = (OverlayMat*)&work->coord.coord;
    mtx->ident.m00_m01 = 0x1000;
    mtx->ident.m02_m10 = 0;
    mtx->ident.m11_m12 = 0x1000;
    mtx->ident.m20_m21 = 0;
    mtx->ident.m22     = 0x1000;
    Gfx_RotMatrixY(&mtx->mat, 0, 1);

    actorLinkWorkObj(&work->coord, &work->obj, &work->rec, &vec, 0x100, 3, 1);
    work->obj.flags &= 0x7FFF;

    snd = ((owner->placeKey >> 12) << 8) | 0x4020000B;
    pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));

    work->eff = Gp_SpawnEff(0x6019B, task->extra.tmd->coords, 0, NULL);
    if (work->eff != NULL) {
        Task_Reparent(task, work->eff->task);
    }
    task->state++;
}

/// Ascent state that precedes the descent above: lift the model by 0x1F4 plus
/// `field_1AE` a step until it passes -0x4E20, then clamp it there, snap its
/// horizontal position back onto `work->target`, restart the step counter, pick
/// a fresh 0..0x1F bias for the next leg, flag the list object and step the task
/// on. Either way the work block's own coordinate is left tracking the model.
/// Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_403200_80137600(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    s32                  y;

    work = (Actor403200DropWork*)task->work;
    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    y                                   = task->extra.tmd->coords->coord.t[1] - 0x1F4;
    task->extra.tmd->coords->coord.t[1] = y - work->field_1AE;
    if (task->extra.tmd->coords->coord.t[1] < -0x4E20) {
        task->state++;
        task->extra.tmd->coords->coord.t[0] = work->target.vx;
        task->extra.tmd->coords->coord.t[2] = work->target.vz;
        Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
        task->extra.tmd->coords->coord.t[1] = -0x4E20;
        work->timer                         = 0;
        work->field_1AE                     = ((u32)Gp_LcgState >> 16) & 0x1F;
        work->obj.flags                    |= 0x8000;
    }

    task->extra.tmd->coords->flg = 0;
    work->coord.coord.t[0]       = task->extra.tmd->coords->coord.t[0];
    work->coord.coord.t[1]       = task->extra.tmd->coords->coord.t[1];
    work->coord.coord.t[2]       = task->extra.tmd->coords->coord.t[2];
    work->coord.flg              = 0;
    Gp_UpdateCoord(&work->coord);
}

/// Descent state of the enemy dispatched through `D_actor_403200_80131F14`:
/// draw the growing shadow marker on the floor under the model, then after
/// 0x14 steps start pulling the model down by `0x258 + field_1AE` a step. When
/// it reaches floor level, zero the height, restart the step counter, tell the
/// trailing `Gp_SpawnEff` effect to wind down, play the landing cue and step
/// the task on. Either way the work block's own coordinate is left tracking
/// the model. Bails to `Gp_DestroyEnemy` when the overlay is shutting down.
void func_actor_403200_80137788(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork* work;
    Actor403200DropCoord coord;
    MATRIX*              mtx;
    GpEnemy*             owner;
    s32                  snd;
    s32                  pan;

    work = (Actor403200DropWork*)task->work;
    if (D_actor_403200_80141C50 == 1) {
        Gp_UnlinkObj(&work->obj);
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work->timer++;
    coord.c.sub          = &gGfxViewCoord;
    mtx                  = &coord.c.coord;
    coord.ident.m00_m01  = 0x1000;
    coord.ident.m02_m10  = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    coord.ident.m20_m21  = 0;
    mtx->m[2][2]         = 0x1000;
    Gfx_RotMatrixY(mtx, 0, 1);

    coord.c.coord.t[0] = task->extra.tmd->coords->coord.t[0];
    coord.c.coord.t[1] = 0;
    coord.c.coord.t[2] = task->extra.tmd->coords->coord.t[2];
    coord.c.flg        = 0;
    Gp_UpdateCoord(&coord.c);

    Gp_DrawEffGroundQuad((VECTOR3*)coord.c.workm.t, (s16)((s16)work->timer * 8 + 0x80),
                         Gp_State1C->groundShade);

    if ((s16)work->timer >= 0x14) {
        task->extra.tmd->coords->coord.t[1] =
            task->extra.tmd->coords->coord.t[1] + (work->field_1AE + 0x258);
        if (task->extra.tmd->coords->coord.t[1] > 0) {
            owner                               = task->parent->spawnArg2;
            task->extra.tmd->coords->coord.t[1] = 0;
            work->timer                         = 0;
            if (work->eff != NULL) {
                work->eff->task->spawnArg1 = 2;
            }
            task->state++;
            snd = ((owner->placeKey >> 12) << 8) | 0x4020000C;
            pan = (s8)Gp_GetObjPan(task->extra.tmd->coords);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));
        }
    }

    task->extra.tmd->coords->flg = 0;
    Gp_ClearRec18Occupied(&work->rec);
    work->coord.coord.t[0] = task->extra.tmd->coords->coord.t[0];
    work->coord.coord.t[1] = task->extra.tmd->coords->coord.t[1];
    work->coord.coord.t[2] = task->extra.tmd->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}

/// Landing state of the dropped enemy, the one after its descent in
/// `D_actor_403200_80131F14`: each step rebuild the model's rotation about y
/// from its own yaw and flatten it through a 0x34-byte frame borrowed from the
/// scratchpad -- scaled (`0x4000, 0x66, 0x4000`) for the first 0xA steps, then
/// (`0x4C00, 0x199, 0x4C00`). The collision node's radius grows with the step
/// counter over those first steps and is then held at 0x380. After 0xC steps
/// the node is unlinked and the task steps on; either way the shadow
/// coordinate keeps tracking the model.
///
/// The absolute `G_SCRATCH_HEAD` accesses are written out: at `-O2` the
/// expander forces a constant address into a register (`explow.c`
/// `memory_address`), so the `lui $at` assembler-macro form the original
/// carries cannot come from plain C here.
void func_actor_403200_801379EC(GpEnemy* enemy, Task* task)
{
    Actor403200DropWork*  work;
    TmdObject*            extra;
    GsCOORDINATE2*        coord;
    ActorScaleRotScratch* blk;
    u8*                   head;
    s16                   ang;

    work = (Actor403200DropWork*)task->work;
    work->timer++;
    if ((s16)work->timer < 0xA) {
        u8* tail;
        s16 spin;
        u16 m22;

        extra = task->extra.tmd;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->coords;
        __asm__ volatile("addiu %0, %1, -0x34\n\tsw %0, 0x1F8003FC"
                         : "=r"(blk)
                         : "r"(head)
                         : "memory");

        ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x4000;
        blk->scale.vy = 0x66;
        blk->scale.vz = 0x4000;
        ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
        coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
        coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
        coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
        coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
        coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
        coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
        coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
        m22                  = *(u16*)&blk->m.m[2][2];
        coord->flg           = 0;
        coord->coord.m[2][2] = m22;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail = *(u8**)(tail + 0x3FC);
        tail = tail + 0x34;
        spin = (s16)work->timer * 0x40 + 0x100;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
        TOUCH_REG(tail);
        work->obj.radius = spin;
    } else {
        u8* tail;
        u16 m22;

        extra = task->extra.tmd;
        __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
        head  = *(u8**)(head + 0x3FC);
        coord = extra->coords;
        __asm__ volatile("addiu %0, %1, -0x34\n\tsw %0, 0x1F8003FC"
                         : "=r"(blk)
                         : "r"(head)
                         : "memory");

        ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        blk->angle = ang;
        Gfx_RotMatrixY(&blk->m, ang, 1);
        blk->scale.vx = 0x4C00;
        blk->scale.vy = 0x199;
        blk->scale.vz = 0x4C00;
        ScaleMatrix(&blk->m, &((ActorScaleRotScratch*)(head - 0x34))->scale);

        coord->coord.m[0][0] = *(u16*)&((ActorScaleRotScratch*)(head - 0x34))->m.m[0][0];
        coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
        coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
        coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
        coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
        coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
        coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
        coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
        __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
        tail       = *(u8**)(tail + 0x3FC);
        m22        = *(u16*)&blk->m.m[2][2];
        coord->flg = 0;
        tail       = tail + 0x34;
        __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
        coord->coord.m[2][2] = m22;
        work->obj.radius     = 0x380;
    }

    Gp_ClearRec18Occupied(&work->rec);
    if ((s16)work->timer >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        task->state++;
    }

    work->coord.coord.t[0] = task->extra.tmd->coords->coord.t[0];
    work->coord.coord.t[1] = task->extra.tmd->coords->coord.t[1];
    work->coord.coord.t[2] = task->extra.tmd->coords->coord.t[2];
    work->coord.flg        = 0;
    Gp_UpdateCoord(&work->coord);
}

/// State handlers of the enemy that rises out of view and slams back down:
/// spawn, rise, descent, landing, teardown.
const GpEnemyTaskFuncTable5 D_actor_403200_80131F14 = {
    {
        func_actor_403200_8013709C,
        func_actor_403200_80137600,
        func_actor_403200_80137788,
        func_actor_403200_801379EC,
        Gp_DestroyEnemy,
    },
};

/// Spawn state of the spinner enemy dispatched through
/// `D_actor_403200_80131F28`: allocate its `Actor403200SpinnerWork`, parent
/// the model to the view coordinate, give it a random orientation off
/// `Gp_LcgState` and a countdown picked by `spawnArg1`, point it at its own
/// light and colour matrices and step the task on. Bails to `Gp_DestroyEnemy`
/// when the overlay is shutting down or the allocation fails.
void func_actor_403200_80137CCC(GpEnemy* enemy, Task* task)
{
    Actor403200SpinnerWork* work;

    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work       = memCalloc(0xA0, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->sub = &gGfxViewCoord;
    task->extra.tmd->flags       = 0;

    switch ((u16)task->spawnArg1) {
        case 0:
            work->spin = 0x14;
            break;
        case 1:
            work->spin = 0x28;
            break;
        case 2:
            work->spin = 0x50;
            break;
        default:
            work->spin = 0x50;
            break;
    }

    task->msgTable = NULL;
    work->field_98 = 0;
    work->field_96 = 0;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, ((u32)Gp_LcgState >> 16) & 0x4FF, 0);

    task->extra.tmd->lightMtx    = &work->lightMtx;
    task->extra.tmd->colorMtx    = &work->colorMtx;
    task->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(task->extra.tmd->coords);
    func_800D7A9C(task->extra.tmd, (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);
    task->state++;
}

/// Per-tick state of the spinner enemy. While `spin` is counting down the model
/// only yaws in place -- 0x40 on phase 1 and -0x3C on phase 3 of every four
/// frames -- and nothing else happens. Once it reaches zero the enemy homes on
/// `D_actor_403200_8015F8F8`: the offset from the model root to that point is
/// squared against `field_98` in a `VECTOR3` borrowed off `G_SCRATCH_HEAD`, and
/// the task steps on when the enemy is inside that radius. `field_96` then ties
/// the spin rate to the step count (`field_98 += field_96 / 8`), the offset is
/// normalised and scaled by `field_98` through the GTE's `gpf` interpolator, and
/// the result is added to the root translation before the three rotations are
/// rebuilt from `field_98` and `field_96`. Bails to `Gp_DestroyEnemy` while the
/// overlay is shutting down.
void func_actor_403200_80137EB4(GpEnemy* enemy, Task* task)
{
    Actor403200SpinnerWork* work;
    SVECTOR                 step;
    SVECTOR*                stepp;
    VECTOR3*                sq;
    u8*                     head;
    s16                     angle;
    s32                     spin;
    s32                     phase;
    s32                     inside;

    work                         = (Actor403200SpinnerWork*)task->work;
    task->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(task->extra.tmd->coords);
    func_800D7A9C(task->extra.tmd, (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);

    if (D_actor_403200_80141C50 == 1 || D_actor_403200_80141C5A == 0) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->flags = 0;

    spin = work->spin;
    if (spin != 0) {
        spin--;
        work->spin = spin;
        phase      = work->spin;
        if ((phase & 3) == 1) {
            Gfx_RotMatrixY(&task->extra.tmd->coords->coord, 0x40, 0);
        }
        if ((work->spin & 3) == 3) {
            Gfx_RotMatrixY(&task->extra.tmd->coords->coord, -0x3C, 0);
        }
        task->extra.tmd->coords->flg = 0;
        Gp_UpdateCoord(task->extra.tmd->coords);
        return;
    }

    stepp    = &step;
    *stepp   = D_actor_403200_8015F8F8;
    step.vx -= task->extra.tmd->coords->coord.t[0];
    step.vy -= task->extra.tmd->coords->coord.t[1];
    step.vz -= task->extra.tmd->coords->coord.t[2];

    head = actorGetScratchHead();
    sq   = (VECTOR3*)(head - sizeof(VECTOR3));
    actorSetScratchHead(sq);
    angle  = work->field_98;
    sq->vx = step.vx;
    sq->vy = stepp->vz;
    sq->vz = angle;
    sq->vx = sq->vx * sq->vx;
    sq->vy = sq->vy * sq->vy;
    sq->vz = sq->vz * sq->vz;
    actorSetScratchHead(head);
    inside = sq->vx + sq->vy >= sq->vz;
    if (!inside) {
        task->state++;
    }

    work->field_96++;
    work->field_98 += work->field_96 / 8;
    VectorNormalSS(stepp, stepp);

    gte_lddp((u16)work->field_98);
    gte_ldsv(stepp);
    gte_gpf12();
    gte_stsv(stepp);

    task->extra.tmd->coords->coord.t[0] += step.vx;
    task->extra.tmd->coords->coord.t[1] += step.vy;
    task->extra.tmd->coords->coord.t[2] += step.vz;
    task->extra.tmd->coords->flg         = 0;

    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, work->field_98 / 2, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, work->field_98 * 2, 0);
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, work->field_96, 0);
}

/// State handlers of the spinner enemy: spawn, wait, home, teardown.
const GpEnemyTaskFuncTable4 D_actor_403200_80131F28 = {
    {
        func_actor_403200_80137CCC,
        func_actor_403200_80141800,
        func_actor_403200_80137EB4,
        Gp_DestroyEnemy,
    },
};

/// Screen-shake driver for the enemy task: `func_actor_403200_8013FB54` writes a
/// level into `field_EAC`, and a change from the armed level in `field_EAD`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `Display_ClampField126` off the frame counter's
/// low bits, so level 1 alternates 0 / 2, level 2 walks a four-frame 0 / 2 / 3 / 2
/// pattern and level 3 an eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out. Same body as
/// `func_actor_444000_8013A77C`, plus the null test on the work block.
void func_actor_403200_80138284(Task* arg0)
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
        Display_ClampField126(0);
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
            Display_ClampField126(work->field_EAF);
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
            Display_ClampField126(work->field_EAF);
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
            Display_ClampField126(work->field_EAF);
            break;

        case 0:
        default:
            Display_ClampField126(0);
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
            task->extra.tmd->flags = 0x80;
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
            escorts                = (Actor403200Work*)work;
            task->extra.tmd->flags = 0x80;
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
            task->extra.tmd->flags = 0x80;
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
s32 func_actor_403200_80138748(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* rebuilt;
    GpEnemy*         temp_enemy;
    s16              i;
    s16              j;
    s32              sound;
    s32              pan;
    s32              action;

    work       = (Actor403200Work*)task->work;
    temp_enemy = (GpEnemy*)task->spawnArg2;

    work->field_EC4 = msg->from.loc.stage;
    work->field_EC5 = msg->from.loc.area;
    work->field_EC6 = (u8)msg->command;

    if (msg->from.key == 0x2704) {
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
                sound           = ((temp_enemy->placeKey >> 0xC) << 8) | 0x40200002;
                pan             = (s8)Gp_GetObjPan(task->extra.tmd->coords);
                SndEvt_EnqueueType6(sound, pan,
                                    (s8)gpGetObjDepth(task->extra.tmd->coords));
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
                work->field_0           = 0xC;
                work->field_F06         = 4;
                work->field_2           = -1;
                work->field_7B3         = 0x12;
                work->field_F14         = 0x258;
                work->field_7B0         = 2;
                work->field_F04         = 0;
                D_actor_403200_80141C58 = 0x640;
                break;
        }
    }

    if (msg->from.key == 0x2804) {
        switch (msg->command) {
            case 0:
                work->field_0 = 0;
                break;

            case 1:
                work->field_7B3 = 0xA;
                work->field_7B0 = 2;
                work->field_7B6 = 0x7F;
                func_actor_403200_80133DD8(task);
                while (work->slots0[1].flags & 1) {
                    func_actor_403200_80133DD8(task);
                }
                work->field_7B6                     = 0x10;
                task->extra.tmd->coords->coord.t[0] = -0xBB8;
                task->extra.tmd->coords->coord.t[1] = 0;
                task->extra.tmd->coords->coord.t[2] = -0x992;
                task->extra.tmd->coords->flg        = 0;
                work->field_0                       = 6;
                break;
        }
    }
    return 1;
}

/// Spawn state of the arena boss: allocate its work block, wire the host enemy
/// up to the model's root coordinate and its nine collision objects, then spawn
/// the seven escorts that make up the rest of the creature.
void func_actor_403200_80138AFC(GpEnemy* enemy, Task* task)
{
    Actor403200Work* work;
    Actor403200Work* buffers;
    Actor403200Work* escorts;
    OverlayMat*      mtx;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   freeCoord;
    GpEnemy*         esc;
    Task*            escTask;
    GpRec18*         recs2;
    SVECTOR          dir;
    SVECTOR*         gteDir;
    VECTOR           pos;
    s16              i;
    s16              j;

    tmd   = task->extra.tmd;
    coord = tmd->coords;

    work       = memCalloc(0xF24, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    task->exitCallback = func_actor_403200_80141018;

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

    work->anim.animBlock.ptr = NULL;
    work->anim.field_4       = 1;
    work->anim.field_8       = 0;
    work->anim.field_C       = 3;
    work->anim.field_10      = 1;
    work->field_F12          = 0;
    task->msgTable           = &D_actor_403200_8015F770;
    coord->sub               = &gGfxViewCoord;
    coord->flg               = 0;
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

    esc                                                     = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 0, 0, task->spawnArg2);
    work->field_ECC[0]                                      = esc;
    esc->task->extra.tmd->coords->sub                       = task->extra.tmd->coords;
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

    esc                                                     = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 1, 0, task->spawnArg2);
    work->field_ECC[1]                                      = esc;
    esc->task->extra.tmd->coords->sub                       = task->extra.tmd->coords;
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

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 2, 0, task->spawnArg2);
    work->field_ECC[2] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->sub                       = &task->extra.tmd->coords[4];
        work->field_ECC[2]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[2]->task->extra.tmd->coords->coord.t[1] = 0x59;
        work->field_ECC[2]->task->extra.tmd->coords->coord.t[2] = -0x64;
        work->field_ECC[2]->task->extra.tmd->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 3, 0, task->spawnArg2);
    work->field_ECC[3] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->sub                       = &task->extra.tmd->coords[3];
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

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 4, 0, task->spawnArg2);
    work->field_ECC[4] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->sub                       = &task->extra.tmd->coords[4];
        work->field_ECC[4]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[4]->task->extra.tmd->coords->coord.t[1] = 0;
        work->field_ECC[4]->task->extra.tmd->coords->coord.t[2] = 0x14;
        work->field_ECC[4]->task->extra.tmd->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 5, 0, task->spawnArg2);
    work->field_ECC[5] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->sub                       = &task->extra.tmd->coords[2];
        work->field_ECC[5]->task->extra.tmd->coords->coord.t[0] = 0;
        work->field_ECC[5]->task->extra.tmd->coords->coord.t[1] = 0x67C;
        work->field_ECC[5]->task->extra.tmd->coords->coord.t[2] = 0xC8;
        work->field_ECC[5]->task->extra.tmd->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 6, 0, task->spawnArg2);
    work->field_ECC[6] = esc;
    if (esc != NULL) {
        esc->task->extra.tmd->coords->sub                       = &task->extra.tmd->coords[1];
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

    work->field_E3C.c.sub         = task->extra.tmd->coords;
    work->field_E3C.ident.m00_m01 = 0x1000;
    mtx                           = (OverlayMat*)&work->field_E3C.c.coord;
    mtx->ident.m02_m10            = 0;
    mtx->ident.m11_m12            = 0x1000;
    mtx->ident.m20_m21            = 0;
    mtx->ident.m22                = 0x1000;
    work->field_E3C.c.coord.t[0] = work->field_E3C.c.coord.t[1] = work->field_E3C.c.coord.t[2] = 0;
    work->field_E3C.c.flg                                                                      = 0;
    Gp_UpdateCoord(freeCoord);

    work->d4rec.end1.vz    = 0x1B58;
    recs2                  = work->recs2;
    work->d4rec.end0Radius = 0x258;
    work->d4rec.end1Radius = 0x258;
    work->d4rec.end0.vx    = 0;
    work->d4rec.end0.vy    = 0;
    work->d4rec.end0.vz    = 0;
    work->d4rec.end1.vx    = 0;
    work->d4rec.end1.vy    = 0;
    work->d4rec.recs       = recs2;
    work->obj.coord        = freeCoord;
    work->obj.ctx.d4rec    = &work->d4rec;
    work->obj.pos.vx       = 0;
    work->obj.pos.vy       = -0xFA;
    work->obj.pos.vz       = 0x25F;
    work->obj.key          = 0x30000 | 0x20;
    work->obj.radius       = 0;
    work->obj.flags        = 3;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(recs2, 5, 0);
    work->obj.flags &= 0x7FFF;

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
    func_actor_403200_80133DD8(task);

    D_actor_403200_8015F8F4.from.loc.stage = 0;
    D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
    D_actor_403200_8015F8F4.command        = 0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);

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
/// scans the five `GpRec18` records of `hits[0]` for the first whose `key`
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
/// The second arm runs the same tick on the `field_4C` bits 0x2/0x8 hit, which
/// `Gp_TickObjFlag4` turns into damage of its own; that one only comes off the
/// host.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads (the scheduler cannot hoist a load
/// past a store through an unknown pointer). Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
void func_actor_403200_80139A60(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               enemy;
    GpRec18*               recs;
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
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg   = &Player_Status;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor403200Work*)arg0->work;
    sc    = (Actor403200HitScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200HitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
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
        func_actor_403200_80134044(work->hits[0].obj.coord, id);
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
        arg0->extra.tmd->coords->flg = 0;
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

    if (enemy->reactionFlags & 0xC) {
        sc->damage = Gp_TickObjFlag4(enemy);
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
        if (sc->damage != 0) {
            func_800E2C78(enemy, sc->id, sc->damage, 0);
            enemy->hp -= sc->damage;
        }
    }

    SCRATCH_POP_BYTES(sizeof(Actor403200HitScratch));
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
/// The attack kind drives a sub-state change: kinds 4 and 6 roll `Gp_LcgState`
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
void func_actor_403200_80139E94(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
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
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Player_Status;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
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
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
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
    func_actor_403200_80134044(coord, id);
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
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    roll        = ((u32)Gp_LcgState >> 16) % 6;
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
        work->field_F0E                                 -= sc->damage;
        esc3                                             = work->field_ECC[3];
        hp                                               = host->hp;
        esc0                                             = work->field_ECC[0];
        esc1                                             = work->field_ECC[1];
        esc3->hp                                         = hp;
        esc1->hp                                         = hp;
        esc0->hp                                         = hp;
        work->field_ECC[3]->task->extra.tmd->coords->flg = 0;
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
    SCRATCH_POP_BYTES(sizeof(Actor403200HitScratch));
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
/// unless `Gp_StateF0.field_6` is 1.
///
/// `pos` / `pos2` / `pos3` are all `&sc->pos`, and are not spare: each group's
/// scan writes the contact point through its own pointer, which is what keeps
/// the three `sh` pairs in `a3` then `a2` twice. `esc3` / `esc0` / `esc1` and
/// the `hp` load are the sibling's arrangement, but evaluated before
/// `func_800DA6E8` so `host->field_40` is still in a register and the three
/// stores reuse it; the pool subtraction after them carries the same `field_40`
/// value for the same reason.
void func_actor_403200_8013A4A0(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
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
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Player_Status;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        func_actor_403200_80134044(work->hits[3].obj.coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        func_actor_403200_80134044(work->hits[4].obj.coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
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
    func_actor_403200_80134044(work->hits[5].obj.coord, id);
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
        Gp_StateF0.field_6 == 1) {
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
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && Gp_StateF0.field_6 == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &work->field_ECC[0]->task->extra.tmd->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = 0x32;
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    work->field_ECC[0]->task->extra.tmd->coords->flg = 0;
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
    SCRATCH_POP_BYTES(sizeof(Actor403200HitScratch));
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
/// `Gp_StateF0.field_6` is not 1.
///
/// The second escort carries the damage and the effect, but `sc->angle` is the
/// yaw of the contact point relative to the first escort's facing. `pos` /
/// `pos2` / `pos3` are all `&sc->pos` and are not spare: each group's scan
/// writes the contact point through its own pointer. `esc3` / `esc0` / `esc1`
/// and the `hp` load sit after `func_800DA6E8`, unlike the group 3-5 handler.
void func_actor_403200_8013AB70(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
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
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Player_Status;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[6].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
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
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
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
        func_actor_403200_80134044(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[8].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
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
    func_actor_403200_80134044(work->hits[8].obj.coord, id);
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
        Gp_StateF0.field_6 == 1) {
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
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && Gp_StateF0.field_6 == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &work->field_ECC[1]->task->extra.tmd->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0C = 0x3C;
    }

    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
    esc3                                             = work->field_ECC[3];
    hp                                               = host->hp;
    esc0                                             = work->field_ECC[0];
    esc1                                             = work->field_ECC[1];
    esc3->hp                                         = hp;
    esc1->hp                                         = hp;
    esc0->hp                                         = hp;
    work->field_ECC[1]->task->extra.tmd->coords->flg = 0;
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
    SCRATCH_POP_BYTES(sizeof(Actor403200HitScratch));
}

/// Reset handler: pushes the host model's `field_C` onto each of the seven
/// escorts, and once the sub-state counter has reached 2 releases the host's and
/// every escort's model buffers. Same shape as
/// `func_actor_403200_80141B40` with a second arm keyed on `field_6`.
///
/// The `modelFlag` copy is not redundant: the second arm's `0x80` has to reach
/// the store as a 32-bit value of its own, or the two arms merge it into the
/// first arm's constant and the second `li $v0, 0x80` disappears.
void func_actor_403200_8013B23C(Task* arg0)
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
        tmd->flags             = 0x80;
        escorts                = (Actor403200Work*)arg0->work;
        i                      = 0;
        escorts->field_7F3     = 0;
        arg0->extra.tmd->flags = (flag = 0x80);
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
        tmd->flags             = 0x80;
        escorts                = (Actor403200Work*)arg0->work;
        modelFlag              = 0x80;
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
void func_actor_403200_8013B3C8(Task* arg0)
{
    Actor403200Work*        work;
    Actor403200Work*        escorts;
    Actor403200Work*        dying;
    GsCOORDINATE2*          model;
    GsCOORDINATE2*          facing;
    Actor403200TurnScratch* sc;
    s16                     i;
    s16                     j;
    s16                     state;
    s16                     ang;

    sc   = (Actor403200TurnScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200TurnScratch));
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
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 0, arg0->spawnArg2)->workType = 0x900;
            break;
        case 7:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 1, arg0->spawnArg2)->workType = 0x900;
            break;
        case 9:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 2, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x10:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 3, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x1F:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 4, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x37:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 5, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x3B:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 6, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x3F:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 7, arg0->spawnArg2)->workType = 0x900;
            break;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->slots0[1].flags & 1) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 1;
    }
    model      = arg0->extra.tmd->coords;
    sc->dir.vx = Player_Status.coordMtx->t[0] - model->coord.t[0];
    sc->dir.vy = Player_Status.coordMtx->t[1] - model->coord.t[1];
    sc->dir.vz = Player_Status.coordMtx->t[2] - model->coord.t[2];
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
    SCRATCH_POP_BYTES(sizeof(Actor403200TurnScratch));
}

/// Spawns up to nine enemies in a randomly selected formation, stopping when
/// a spawn fails. Each member's index becomes the high nibble of its place key.
void func_actor_403200_8013B740(Task* arg0)
{
    Actor403200Work*     work;
    TaskDesc*            desc;
    Actor403200SpawnRec* entries;
    SVECTOR(*positions)
    [9];
    SVECTOR* row;
    s32      offset;
    s32      rowOffset;
    GpEnemy* enemy;
    s16      i;
    s16      formation;

    work = (Actor403200Work*)arg0->work;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    formation   = ((u32)Gp_LcgState >> 16) & 3;
    if (formation == 3) {
        formation = 0;
    }

    i         = 0;
    desc      = &D_actor_403200_8015E858;
    entries   = D_actor_403200_8015F888;
    positions = D_actor_403200_8015F7B0;

spawnNext:
    offset = i * 8;
    SOFT_USE_REG(offset);
    desc[4].arg.model = ((Actor403200SpawnRec*)(offset + (u32)entries))->model;
    enemy             = Gp_SpawnEnemyFromTable(desc, 4, ((Actor403200SpawnRec*)(offset + (u32)entries))->spawnArg, NULL);
    work->field_EF0   = enemy;
    if (enemy != NULL) {
        rowOffset                                            = ((formation * 8) | formation) * 8;
        row                                                  = (SVECTOR*)((offset + rowOffset) + (u32)positions);
        enemy->task->extra.tmd->coords->coord.t[0]           = row->vx;
        work->field_EF0->task->extra.tmd->coords->coord.t[1] = row->vy;
        work->field_EF0->task->extra.tmd->coords->coord.t[2] = row->vz;
        work->field_EF0->workType                            = 0x900;
        work->field_EF0->placeKey                           |= i << 12;
        i++;
        if (i < 9) {
            goto spawnNext;
        }
    }
}

/// Per-frame body of the launch state's pull. A state change re-arms the
/// escorts and tells the scene (message 0x7DA, action 0x2C). Each tick yaws
/// the enemy toward the player and scales a pull from the animation frame;
/// inside the swipe window, once the player accepts message 0x3F8, it places
/// them (0x3E9) and hands over an animation (0x3F4).
void func_actor_403200_8013B8C4(Task* arg0)
{
    Actor403200Work*        work;
    Actor403200Work*        escorts;
    Actor403200Work*        dying;
    GpEnemy*                enemy;
    Task*                   task;
    PlayerStatus*           cfg;
    Actor403200DragScratch* sc;
    SVECTOR*                posp;
    s16                     i;
    s16                     j;

    work  = (Actor403200Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    task  = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    sc    = (Actor403200DragScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200DragScratch));

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
        {
            SVECTOR* p = &D_actor_403200_8015F8F8;
            SOFT_USE_REG(p);
            posp = p;
            SOFT_USE_REG(posp);
        }
        work->field_EFE = 0;
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_F04 = 0;
        work->field_EFA = 0;
        posp->vz        = 0;
        posp->vy        = 0;
        posp->vx        = 0;
        actorLocalToView(&arg0->extra.tmd->coords[3], posp);
        D_actor_403200_8015F8F4.from.loc.stage = 0;
        D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command        = 2;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        {
            s16 armed               = 1;
            work->field_E96         = 0xC80;
            D_actor_403200_80141C5A = armed;
        }
    }

    func_actor_403200_80133DD8(arg0);

    {
        GsCOORDINATE2* facing;
        GsCOORDINATE2* yawCoord;
        SVECTOR*       dirp;
        s16            dz;
        s16            ang;

        facing     = arg0->extra.tmd->coords;
        dirp       = &sc->dir;
        sc->dir.vx = *(u16*)&Player_Status.coordMtx->t[0] - *(u16*)&facing->coord.t[0];
        dirp->vy   = *(u16*)&Player_Status.coordMtx->t[1] - *(u16*)&facing->coord.t[1];
        dz         = *(u16*)&Player_Status.coordMtx->t[2] - *(u16*)&facing->coord.t[2];
        dirp->vz   = dz;
        yawCoord   = arg0->extra.tmd->coords;
        ang        = ratan2(sc->dir.vx, dz) - ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
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
    }

    sc->dir.vz = 0;
    sc->dir.vy = 0;
    sc->dir.vx = 0;
    actorLocalToView(&arg0->extra.tmd->coords[4], &sc->dir);

    sc->dir.vx = *(u16*)&task->extra.tmd->coords->coord.t[0] - (u16)sc->dir.vx;
    sc->dir.vy = *(u16*)&task->extra.tmd->coords->coord.t[1] - (u16)sc->dir.vy;
    sc->dir.vz = *(u16*)&task->extra.tmd->coords->coord.t[2] - (u16)sc->dir.vz;
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
    if (((u32)((work->slots0[1].curRec & 0x3FF) - 0xA) < 9U) && ((work->field_6 % sc->period) == 0)) {
        Gp_SpawnScript18((s32)&D_actor_403200_80141C7C, (s32)&D_actor_403200_80141C88);
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

        sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_6 == 0x3C) {
        s32 sfx;
        s32 pan;

        sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x4020000A;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_6 == 0xE8) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
    }

    work->field_EFA = 1;
    switch (work->slots0[1].curRec & 0x3FF) {
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

    if (((u32)((work->slots0[1].curRec & 0x3FF) - 0xB) < 5U) && (sc->dist < 0x4B0) && (enemy->hp > 0) &&
        (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_403200_8015FA00, 0) == 0)) {
        GsCOORDINATE2* yawCoord;
        GsCOORDINATE2* facing;
        SVECTOR*       dirp;
        s16            ang;

        work->field_0   = 0xD;
        work->field_EC8 = 1;
        sc->pos.vz      = 0;
        sc->pos.vy      = 0;
        sc->pos.vx      = 0;
        actorLocalToView(&arg0->extra.tmd->coords[4], &sc->pos);

        sc->dir.vx = *(u16*)&task->extra.tmd->coords->coord.t[0] - (u16)sc->pos.vx;
        sc->dir.vy = 0;
        sc->dir.vz = *(u16*)&task->extra.tmd->coords->coord.t[2] - (u16)sc->pos.vz;
        __asm__("" : "+m"(sc->dir.vz));
        yawCoord = arg0->extra.tmd->coords;
        ang      = ratan2(sc->dir.vx, sc->dir.vz) -
              ratan2(-yawCoord->coord.m[2][0], yawCoord->coord.m[2][2]);
        if (ang < 0) {
        wrapUp2:
            if (ang < -0x800) {
                ang += 0x1000;
                goto wrapUp2;
            }
        } else {
        wrapDown2:
            if (ang > 0x800) {
                ang -= 0x1000;
                goto wrapDown2;
            }
        }
        dirp            = &sc->dir;
        work->field_7C4 = ang;
        VectorNormalSS(dirp, dirp);
        gte_lddp(0x384);
        gte_ldsv(dirp);
        gte_gpf12();
        gte_stsv(dirp);

        D_actor_403200_8015F9C0.pos.vx = sc->pos.vx + sc->dir.vx;
        D_actor_403200_8015F9C0.pos.vy = task->extra.tmd->coords->coord.t[1];
        {
            s32 pz = sc->pos.vz;
            s32 dz = sc->dir.vz;

            D_actor_403200_8015F9C0.rot.vx = 0;
            D_actor_403200_8015F9C0.rot.vz = 0;
            D_actor_403200_8015F9C0.pos.vz = pz + dz;
        }
        {
            u16 px = (u16)sc->pos.vx;
            u16 mx = (u16)D_actor_403200_8015F9C0.pos.vx;

            sc->dir.vy = 0;
            sc->dir.vx = px - mx;
        }
        sc->dir.vz = (u16)sc->pos.vz - (u16)D_actor_403200_8015F9C0.pos.vz;
        facing     = task->extra.tmd->coords;
        ang        = ratan2(sc->dir.vx, dirp->vz) -
              ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        if (ang < 0) {
        wrapUp3:
            if (ang < -0x800) {
                ang += 0x1000;
                goto wrapUp3;
            }
        } else {
        wrapDown3:
            if (ang > 0x800) {
                ang -= 0x1000;
                goto wrapDown3;
            }
        }
        {
            s32 ext = ang;

            sc->angle = ext;
            if (abs(ext) < 0x400) {
                D_actor_403200_8015F9C0.rot.vy = ratan2((s32)sc->dir.vx, (s32)sc->dir.vz);
                work->anim.animBlock.ptr       = D_actor_403200_8015E6AC;
            } else {
                D_actor_403200_8015F9C0.rot.vy = ratan2((s32)sc->dir.vx, (s32)sc->dir.vz) + 0x800;
                work->anim.animBlock.ptr       = D_actor_403200_8015E6CC;
            }
        }
        if (cfg->hp > 0) {
            Gp_DispatchMsg(task, 0x3E9, (s32)&D_actor_403200_8015F9C0, 0);
        }
        work->anim.field_4 = 1;
        work->anim.field_8 = 0;
        work->anim.field_C = 0;
        work->field_F02    = 1;
        Gp_DispatchMsg(task, 0x3F4, (s32)&work->anim, 0);
        work->field_7CA = 0;
    }

    if (sc->dir.vx != 0 || sc->dir.vz != 0) {
        sc->push.vx = sc->dir.vx;
        sc->push.vy = 0;
        sc->push.vz = sc->dir.vz;
        func_80105B74(&sc->push);
    }

    if (work->slots0[1].flags & 1) {
        D_actor_403200_8015F8F4.from.loc.stage = 0;
        D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command        = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        D_actor_403200_80141C5A = 0;
        work->field_7F2         = 0;
        work->field_0           = 0xA;
        for (sc->i = 0; sc->i < 2; sc->i++) {
            work->field_EE8[sc->i] = NULL;
        }
        work->field_F1C = 0;
    }
    if (work->field_6 == 0x14) {
        work->field_F06 = 2;
    }

    SCRATCH_POP_BYTES(sizeof(Actor403200DragScratch));
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
/// (0x3F9) with `Player_Status.hp` as its gate: the two arms either side
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
/// block `D_actor_403200_8015F8E0` is cleared before the `field_7C4` store, so
/// its address is the one computed first.
void func_actor_403200_8013C84C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    Task*            task;
    PlayerStatus*    cfg;
    SVECTOR          view;
    SVECTOR*         posp;
    GsCOORDINATE2*   coord;
    s16              i;
    s16              j;
    s16              yaw;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    task  = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    if (work->field_4 != 0) {
        D_actor_403200_8015F8F4.from.loc.stage = 0;
        D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command        = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        D_actor_403200_80141C5A = 0;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
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
        D_actor_403200_8015F8E0                = 0;
        work->field_7C4                        = yaw;
        D_actor_403200_8015F8F4.from.loc.stage = 0;
        D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
        D_actor_403200_8015F8F4.command        = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        work->field_E96         = 0x9C4;
        D_actor_403200_80141C5A = 0;
        Gp_StateC08.field_6    |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        return;
    }

    SCRATCH_PUSH_BYTES(0x3C);
    func_actor_403200_80133DD8(arg0);
    if ((work->slots0[1].flags & 1) && (work->field_7B3 == 0xF)) {
        work->field_7B0 = 2;
        work->field_7B3 = 0xE;
    }
    if (work->field_7B3 == 0xF) {
        if (cfg->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 3), 0);
            if (cfg->hp <= 0) {
                ((GameActor*)task->work)->field_956 = 0xA;
                gGameSession->areaBgmCountdown      = 0x1E;
                gGameSession->field_12E             = 0x36;
                gGameSession->deathRestartDelay     = 0x5A;
            }
        }
        if (((work->slots0[3].curRec & 0x3FF) == 0x19) && (work->field_7A8 != (work->slots0[3].curRec & 0x3FF))) {
            s32 sfx;
            s32 pan;
            s32 depth;

            sfx   = (((u16)enemy->placeKey >> 12) << 8) | 0x40200011;
            pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            depth = (s8)gpGetObjDepth(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sfx, pan, depth);
        }
        work->field_7A8 = work->slots0[3].curRec & 0x3FF;
    }
    if (work->field_7B3 == 0xE) {
        if (((work->slots0[3].curRec & 0x3FF) == 0x1E) && (work->field_7A8 != (work->slots0[3].curRec & 0x3FF))) {
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->slots0[3].curRec & 0x3FF) == 0x23) && (work->field_7A8 != (work->slots0[3].curRec & 0x3FF))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200012;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->slots0[3].curRec & 0x3FF) == 0x27) && (work->field_7A8 != (work->slots0[3].curRec & 0x3FF))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200012;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        work->field_7A8 = work->slots0[3].curRec & 0x3FF;
    }
    if ((Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) && (cfg->hp > 0)) {
        D_actor_403200_8015F9C0.pos.vx = arg0->extra.tmd->coords[0].coord.t[0];
        D_actor_403200_8015F9C0.pos.vy = arg0->extra.tmd->coords[0].coord.t[1];
        D_actor_403200_8015F9C0.pos.vz = arg0->extra.tmd->coords[0].coord.t[2];
        Gp_DispatchMsg(task, 0x3E9, (s32)&D_actor_403200_8015F9C0, 0);
        D_actor_403200_8015F8E0 = 1;
    }
    if (work->field_6 < 0x18) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
        Gp_DispatchMsg(task, 0x3FF, (s32)&work->anim, 0);
        work->field_7CA = 0;
    }
    SCRATCH_POP_BYTES(0x3C);
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
/// `D_actor_403200_80141C58` and re-arming `field_0`, and the 0x29..0x2E window
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
void func_actor_403200_8013D028(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpRec18*         recs;
    GpEnemy*         enemy;
    Task*            task;
    Task*            target;
    s16              i;
    s16              j;
    s16              k;
    s16              frame;
    s16              frame2;
    s16              reply;
    s32              found;
    s32              resetId;
    s32              resetPan;
    s32              swipeId;
    s32              swipePan;
    s32              swipe2Id;
    s32              swipe2Pan;
    s32              hitId;
    s32              hitPan;
    s32              cueId;
    s32              cuePan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    task  = gameGetPtrSlot(3);
    SCRATCH_PUSH_BYTES(0x30);

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
        Gfx_RotMatrixY(&work->field_E3C.c.coord, work->field_7C8, 1);
        work->field_E3C.c.flg = 0;
        Gp_UpdateCoord(&work->field_E3C.c);
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        resetPan        = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }

    work->field_E3C.c.flg = 0;
    Gp_UpdateCoord(&work->field_E3C.c);

    if (work->field_7B3 == 4 && (frame = work->slots0[1].curRec & 0x3FF) == 0xC &&
        work->field_7AC != frame) {
        work->field_EAC  = 3;
        work->obj.flags |= 0x8000;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);
        swipeId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200019;
        swipePan = (s8)Gp_GetObjPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(gpGetObjDepth(
                     &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                 2));
        swipe2Id  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020001A;
        swipe2Pan = (s8)Gp_GetObjPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(gpGetObjDepth(
                     &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                 2));
    } else {
        work->obj.flags &= 0x7FFF;
    }

    if (work->field_7B3 == 5 && (frame2 = work->slots0[2].curRec & 0x3FF) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EFA = 0;
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x7F, 8);
        hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020001B;
        hitPan = (s8)Gp_GetObjPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(gpGetObjDepth(
                     &work->field_ECC[0]->task->extra.tmd->coords[1]) /
                 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->slots0[1].curRec & 0x3FF;
    } else {
        work->field_7AC = work->slots0[2].curRec & 0x3FF;
    }

    switch (work->field_6) {
        case 0x14:
            D_actor_403200_80141C58 = 0x640;
            work->field_7A4         = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->placeKey >> 12) << 8) | 0x40200018;
            cuePan          = (s8)Gp_GetObjPan(
                &work->field_ECC[0]->task->extra.tmd->coords[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(gpGetObjDepth(
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

    if ((u32)((u16)work->field_6 - 0x29) < 6 && D_actor_403200_80141C58 < 0x1770) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 + 0x258;
    }

    func_actor_403200_80133DD8(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
        if (recs[k].key == 0) {
            goto missed;
        }
        if ((recs[k].key & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && enemy->hp > 0 &&
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_403200_8015FA00, 0) == 0) {
        target          = gameGetPtrSlot(3);
        reply           = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair(enemy, 4), 0);
        work->field_ECA = reply;
        if (reply == 1) {
            ((GameActor*)task->work)->field_956 = 0xA;
        }
        work->anim.animBlock.ptr = D_actor_403200_8015E6AC;
        work->field_EC8          = 1;
        work->anim.field_4       = 2;
        work->anim.field_8       = 0;
        work->anim.field_C       = 0;
        Gp_DispatchMsg(task, 0x3FF, (s32)&work->anim, 0);
        work->field_7CA = 0;
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
        if (D_actor_403200_80141C58 >= 0xBB9) {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        } else {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0x1E;
        }
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 4;
    }

    SCRATCH_POP_BYTES(0x30);
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
void func_actor_403200_8013D78C(Task* arg0)
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
    func_actor_403200_80133DD8(arg0);
    if (work->slots0[1].flags & 1) {
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
    arg0->extra.tmd->coords->flg = 0;
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
void func_actor_403200_8013D9EC(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    s16              i;
    s16              j;
    s16              state;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
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
        sfx             = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        pan             = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sfx, pan,
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
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
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 0, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
            break;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->slots0[1].flags & 1) {
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
void func_actor_403200_8013DC3C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
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
    enemy = arg0->spawnArg2;
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
        resetId         = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        resetPan        = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200016;
        cuePan = (s8)Gp_GetObjPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            cueId, cuePan,
            (s8)gpGetObjDepth(
                &work->field_ECC[0]->task->extra.tmd->coords[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020000D;
        hitPan = (s8)Gp_GetObjPan(
            &work->field_ECC[0]->task->extra.tmd->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)gpGetObjDepth(
                &work->field_ECC[0]->task->extra.tmd->coords[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 3) == 0) {
            actorAccumulateToView(
                &work->field_ECC[0]->task->extra.tmd->coords[1],
                &D_actor_403200_8015F924);

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            actorLocalToView(&work->field_ECC[0]->task->extra.tmd->coords[1],
                             &pos);

            D_actor_403200_8015F920.sub        = &gGfxViewCoord;
            D_actor_403200_8015F920.coord.t[0] = pos.vx;
            D_actor_403200_8015F920.coord.t[1] = pos.vy;
            D_actor_403200_8015F920.coord.t[2] = pos.vz;
            Gfx_RotMatrixY(&D_actor_403200_8015F920.coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_403200_8015F920.coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_403200_8015F920.coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12();
            gte_stsv(posp);

            D_actor_403200_8015F920.coord.t[0] += pos.vx;
            D_actor_403200_8015F920.coord.t[1] += pos.vy;
            D_actor_403200_8015F920.coord.t[2] += pos.vz;
            D_actor_403200_8015F920.flg         = 0;
            Gp_UpdateCoord(&D_actor_403200_8015F920);
            Gp_SpawnEff(0x60199, &D_actor_403200_8015F920, 0x97A0D680, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 2, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
        }
    }

    func_actor_403200_80133DD8(arg0);

    if (work->slots0[1].flags & 1) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000D, 1);
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 5;
    }
}

void func_actor_403200_8013E2FC(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    OverlayMat*      mtx;
    GsCOORDINATE2*   coords;
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
        frame = work->slots0[2].curRec & 0x3FF;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_403200_80141C6C, (s32)&D_actor_403200_80141C74);
        }
        work->field_7D8 = work->slots0[2].curRec & 0x3FF;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                = arg0->extra.tmd->coords;
        D_actor_403200_8015F970.ident.m00_m01 = 0x1000;
        mtx                                   = (OverlayMat*)&D_actor_403200_8015F970.c.coord;
        mtx->ident.m02_m10                    = 0;
        mtx->ident.m11_m12                    = 0x1000;
        mtx->ident.m20_m21                    = 0;
        mtx->ident.m22                        = 0x1000;
        D_actor_403200_8015F970.c.coord.t[1]  = -0x64;
        D_actor_403200_8015F970.c.coord.t[0]  = 0;
        D_actor_403200_8015F970.c.coord.t[2]  = 0x64;
        D_actor_403200_8015F970.c.flg         = 0;
        D_actor_403200_8015F970.c.sub         = &coords[4];
        Gp_UpdateCoord(&D_actor_403200_8015F970.c);
    }
    state = work->field_7B3;
    if (state == 0x14) {
        if (work->slots0[1].flags & 1) {
            work->field_7B3 = 0xD;
            work->field_7B0 = 1;
        }
        if (work->field_7B3 == state && work->field_7B0 == 2) {
            work->field_7B6 = 0x60;
            func_actor_403200_80133DD8(arg0);
            while ((u32)(work->slots0[1].curRec & 0x3FF) < 0x34) {
                func_actor_403200_80133DD8(arg0);
            }
            work->field_7B6 = 0x10;
        }
    }
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
    }
    func_actor_403200_80133DD8(arg0);
    arg0->extra.tmd->coords->flg = 0;
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
/// The rest of the tick winds the shared `D_actor_403200_80141C58` counter down
/// by 0xC8 once it has passed 0x1F4, runs the per-frame body, clears the host
/// coordinate's rebuild flag, and on frame 0x1C of `slots0[2]` arms the screen
/// shake at level 3 and spawns the `D_actor_403200_80141C5C` script pair. While
/// `field_7B3` is still 0x12 four one-shot cues fire on frames 0x33, 0x3D, 0x4E
/// and 0x71 of `slots0[3]`, each latching the frame it saw in `field_7A8`.
void func_actor_403200_8013E5A8(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         obj;
    s16              i;
    s16              j;
    s32              state;
    s32              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                    = arg0->spawnArg2;
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
            func_actor_403200_80133DD8(arg0);
            j++;
            if (work->slots0[1].flags & 1) {
                break;
            }
        }
        work->field_F06 = 7;
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x4020000A, 1);
        work->field_7A4 = 0;
        work->field_E96 = 0xFA0;
    }
    if (D_actor_403200_80141C58 >= 0x1F5) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
    }
    func_actor_403200_80133DD8(arg0);
    arg0->extra.tmd->coords->flg = 0;
    state                        = work->slots0[2].curRec & 0x3FF;
    if (state == 0x1C && work->field_7D8 != state) {
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
    }
    work->field_7D8 = work->slots0[2].curRec & 0x3FF;
    if (work->field_7B3 == 0x12) {
        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200013;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200003;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200014;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        frame = work->slots0[3].curRec & 0x3FF;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200015;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        work->field_7A8 = work->slots0[3].curRec & 0x3FF;
    }
}

/// The enemy's attack-launch body: when the dispatcher has flagged the state
/// change it re-arms the work block (`field_7A4` at 3, `field_E96` at 0xC80) and
/// plays the two launch cues -- a type-6 with the object's pan and depth, then
/// type-7s for ids 0x0D and 0x09 -- and otherwise runs the per-frame body,
/// winding the shared `D_actor_403200_80141C58` counter down by 0xC8 once it has
/// passed 0x190 and clearing `field_F06` once `field_6` has passed 0x14.
void func_actor_403200_8013E9C0(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         obj;
    s32              state;
    s32              id;
    s32              pan;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        obj             = arg0->spawnArg2;
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
        id  = (((u16)obj->placeKey >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_7A4 = 3;
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x4020000D, 1);
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x40200009, 1);
        return;
    }
    SCRATCH_PUSH_BYTES(0xC);
    func_actor_403200_80133DD8(arg0);
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        work->field_7A4         = 0;
    }
    if (work->slots0[1].flags & 1) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 0;
    }
    SCRATCH_POP_BYTES(0xC);
}

/// State-selecting tick of the enemy's approach: on the tick the dispatcher has
/// flagged a state change it re-arms the work block -- the two flags, the
/// stagger countdown at 0x28, the yaw target at 0xE10 and the animation slot at
/// 0x10 -- and winds the shared `D_actor_403200_80141C58` counter down by 0xC8
/// once it has passed 0x190.
///
/// It then runs the per-frame body and aims the enemy at the camera: the
/// camera's translation minus the part's own translation gives the pair
/// `ratan2` turns into a yaw, taken relative to the part's facing the same way
/// the group-0 hit handler does it, and the result is wrapped to +/-0x800 into
/// `field_7C4`. `D_actor_403200_80141C50` holding `field_6` at zero makes the
/// per-frame body's animation re-arm win the next tick.
///
/// Once the `field_F10` stagger countdown has run out it walks the three
/// `field_F08` sub-states, in which the player-relative range and the enemy's
/// remaining HP pick the next state, and a roll of `Gp_LcgState` breaks the tie
/// between the two strafing states; the state already in `field_F1D` is never
/// re-selected twice in a row. A positive heal counter in `field_F1A` overrides
/// all of it with the heal state 0xF.
///
/// The x range that sub-state 0 tests is the player-relative offset read back
/// out of the frame, not `dist`: the two share only the frame, and the y test
/// carries the -0xFA the z one carries +0x25F, the offsets the hit handler puts
/// on the same pair.
void func_actor_403200_8013EB64(Task* arg0)
{
    Actor403200ApproachScratch* sc;
    Actor403200Work*            work;
    GpEnemy*                    enemy;
    Task*                       player;
    GsCOORDINATE2*              coord;
    GsCOORDINATE2*              facing;
    SVECTOR*                    view;
    s16                         angle;

    work   = (Actor403200Work*)arg0->work;
    enemy  = arg0->spawnArg2;
    player = gameGetPtrSlot(3);

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
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        work->field_7A4         = 0;
    }
    sc = (Actor403200ApproachScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200ApproachScratch));
    func_actor_403200_80133DD8(arg0);

    coord    = arg0->extra.tmd->coords;
    view     = &sc->view;
    view->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    view->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    view->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
    if (D_actor_403200_80141C50 == 1) {
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
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
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
    SCRATCH_POP_BYTES(sizeof(Actor403200ApproachScratch));
}

/// Escort-spawn tick of the arena fight. While `D_actor_403200_80141C50` is 1
/// the whole body is skipped; otherwise it carves an
/// `Actor403200SpawnScratch` off the scratch-pad stack.
///
/// On the dispatcher's re-arm tick it tops the two `field_EE8` slots back up
/// to two live escorts (`field_F1C` < 2 and `field_F1B` < 8), dresses each
/// model from the current area record's fourth placement, stamps the slot
/// index into `GpEnemy::placeKey`, and plays the two type-7 launch cues.
/// Every later tick yaws the host at the player, and at `field_6` 0x46 / 0x78
/// it sends escort 0 or 1 a 0x7DB order whose action is picked from
/// `field_F08` and a coin flip of `Gp_LcgState`.
void func_actor_403200_8013EF6C(Task* arg0)
{
    Actor403200SpawnScratch* sc;
    Actor403200Work*         work;
    GpEnemy*                 host;
    GpEnemy*                 escort;
    PlayerStatus*            cfg;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    TmdObject*               model;
    GpAreaPlace*             entry;
    GpAreaKey                key;
    GpAreaKey*               sessionKey;
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
    host = arg0->spawnArg2;
    if (D_actor_403200_80141C50 != 1) {
        sc = (Actor403200SpawnScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200SpawnScratch));
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
                        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                        key.stage  = sessionKey->stage;
                        key.area   = sessionKey->area;
                        key.room   = sessionKey->room;
                        key.view   = sessionKey->view;
                        Gp_SyncAreaKeyIndex(&key);
                        entry        = (GpAreaPlace*)((s32)Gp_GetNestedAreaRec(&key)->field_0 + 0x30);
                        model->tpage = entry->tpage;
                        model->clut  = entry->clut;
                        if (model->buffer != NULL) {
                            tmdProcessStream(model);
                            tmdProcessStream(model);
                        }
                        work->field_EE8[sc->i]->workType = 0x900;
                        escort                           = work->field_EE8[sc->i];
                        escort->placeKey                |= sc->i << 12;
                        work->field_F1C++;
                    }
                }
            }
            work->field_E96 = 0xC80;
            SndEvt_EnqueueType7((((u16)host->placeKey >> 12) << 8) | 0x4020000D, 1);
            SndEvt_EnqueueType7((((u16)host->placeKey >> 12) << 8) | 0x40200009, 1);
        }
        if (D_actor_403200_80141C58 >= 0x191) {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        }
        func_actor_403200_80133DD8(arg0);
        if (work->field_7B3 == 0x13 && (frame = work->slots0[1].curRec & 0x3FF) >= 4 && frame < 0xD) {
            work->field_EFA = 1;
        } else {
            work->field_EFA = 0;
        }
        cfg          = &Player_Status;
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
        if ((work->slots0[1].flags & 1) && work->field_7B3 == 0x13) {
            work->field_7B3 = 1;
            work->field_7B0 = 1;
        }
        if (work->field_6 >= 0x14B || (work->field_7B3 == 1 && Gp_StateF0.field_6 == 1)) {
            work->field_0 = 3;
        }
        if (work->field_6 == 6) {
            cueId  = (((u16)host->placeKey >> 12) << 8) | 0x40200004;
            cuePan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(cueId, cuePan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        }
        if (work->field_6 == 0x3B) {
            blastId  = (((u16)host->placeKey >> 12) << 8) | 0x40200010;
            blastPan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(blastId, blastPan, (s8)(gpGetObjDepth(arg0->extra.tmd->coords) / 2));
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
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
                D_actor_403200_8015F8F4.from.loc.stage = 0;
                D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
                sel                                    = work->field_F08;
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
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 3;
                    } else {
                        D_actor_403200_8015F8F4.command = 4;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 9;
                    } else {
                        D_actor_403200_8015F8F4.command = 0xA;
                    }
                }
                goto L_join;
            L_case1:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.command = 0xA;
                    } else {
                        D_actor_403200_8015F8F4.command = 0xB;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
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
                rnd                               = (Gp_LcgState * 5) + 0x71357911;
                D_actor_403200_8015F8F4.command  |= (s16)(((((u32)rnd >> 16) % 3) * 0x10) | 1);
                Gp_LcgState                       = rnd;
                Gp_DispatchMsg(work->field_EE8[sc->i]->task, 0x7DB, (s32)&D_actor_403200_8015F8F4, 0);
            }
        }
    out:
        SCRATCH_POP_BYTES(sizeof(Actor403200SpawnScratch));
    }
}

void func_actor_403200_8013F700(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyp;
    u8               areaByte0;
    SVECTOR          vec;
    SVECTOR*         v;
    GpAreaKey        key;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   rot;
    GpAreaPlace*     entry;
    TmdObject*       model;
    s16              i;
    s16              j;
    s16              angle;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
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
            sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
            pan = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(
                sfx, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
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
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 3, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
            if (spawned != NULL) {
                model      = spawned->task->extra.tmd;
                sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                key.stage  = sessionKey->stage;
                key.area   = sessionKey->area;
                key.room   = sessionKey->room;
                areaByte0  = sessionKey->view;
                SOFT_BARRIER();
                keyp = &key;
                TOUCH_REG(keyp);
                key.view = areaByte0;
                Gp_SyncAreaKeyIndex(keyp);
                entry        = (GpAreaPlace*)(0x20 + (s32)Gp_GetNestedAreaRec(&key)->field_0);
                model->tpage = entry->tpage;
                model->clut  = entry->clut;
                if (model->buffer != NULL) {
                    tmdProcessStream(model);
                    tmdProcessStream(model);
                }
                work->field_EFE = 0;
            }
            break;
    }
    coord = arg0->extra.tmd->coords;
    v     = &vec;
    v->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    v->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    v->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
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
    func_actor_403200_80133DD8(arg0);
    if (work->field_7B3 == 0x10 && (work->slots0[1].flags & 1)) {
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
const Actor403200StateTable D_actor_403200_80132154 = {
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
const GpEnemyTaskFuncTable3 D_actor_403200_801321B8 = {
    {
        func_actor_403200_80138AFC,
        func_actor_403200_8013FB54,
        Gp_DestroyEnemy,
    },
};

/// Per-frame tick for the enemy task. Updates the host coordinate, hides or
/// shows the escorts, and either returns on the cinematic mode byte or runs
/// the hit handlers, the death handoff and the state in `field_0`.
void func_actor_403200_8013FB54(GpEnemy* arg0, Task* arg1)
{
    VECTOR                  pos;
    Actor403200StateTable   states;
    Actor403200Work*        work;
    Actor403200Work*        dying;
    Actor403200Work*        vis;
    Actor403200TickScratch* scratch;
    Actor403200PendingPos*  pending;
    TmdObject*              tmd;
    TmdObject*              escortTmd;
    Task*                   player;
    Task*                   slot3;
    GpEnemy*                colorEnemy;
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
    player = gameGetPtrSlot(3);
    states = D_actor_403200_80132154;

    arg1->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);

    dying = (Actor403200Work*)arg1->work;
    if (dying->field_7F3 != 0) {
        arg1->extra.tmd->flags = 0x80;
        dying->field_7F3--;
        if (dying->field_7F3 == 0) {
            tmd         = arg1->extra.tmd;
            tmd->flags |= 4;
            Tmd_FreeBuffers(arg1->extra.tmd);
            for (i = 0; i < 7; i++) {
                if (dying->field_ECC[i] != NULL) {
                    escortTmd         = dying->field_ECC[i]->task->extra.tmd;
                    escortTmd->flags |= 4;
                    Tmd_FreeBuffers(dying->field_ECC[i]->task->extra.tmd);
                }
            }
        }
    }

    pos.vx = arg1->extra.tmd->coords[3].workm.t[0];
    pos.vy = arg1->extra.tmd->coords[3].workm.t[1];
    pos.vz = arg1->extra.tmd->coords[3].workm.t[2];

    slot3 = gameGetPtrSlot(3);
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

    if (work->field_0 == 0xB && D_actor_403200_80141C58 >= 0x7D1) {
        work->field_ECC[4]->task->extra.tmd->otOffset = -8;
    } else {
        work->field_ECC[4]->task->extra.tmd->otOffset = 0;
    }

    if (((Gp_GetViewIndex() & 0xFF) == 0x1E) || ((Gp_GetViewIndex() & 0xFF) == 0x1D)) {
        vis                    = (Actor403200Work*)arg1->work;
        vis->field_7F3         = 0;
        arg1->extra.tmd->flags = 0x80;
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

    d801153f4 = Gp_StateF0.field_4;
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

    scratch = (Actor403200TickScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor403200TickScratch));

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
        if (D_80073BA0 <= 0) {
            arg0->hp                = 1;
            D_actor_403200_80141C50 = 0;
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
        if (arg0->hp <= 0 && D_actor_403200_80141C50 == 0) {
            s32 slot;
            s16 armed;
            slot = 4;
            TOUCH_REG(slot);
            armed = 1;
            SOFT_BARRIER();
            D_actor_403200_8015F8F4.from.loc.stage = 0;
            D_actor_403200_80141C50                = armed;
            D_actor_403200_8015F8F4.from.loc.area  = 0x2C;
            D_actor_403200_8015F8F4.command        = 3;
            Gp_DispatchMsg(gameGetPtrSlot(slot), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
            D_actor_403200_80141C5A = 0;
        }
    }

    if (D_actor_403200_80141C50 == 1 && work->field_F16 <= 0 && work->field_EC8 == 0 &&
        work->field_F12 < 0x100) {
        work->field_F12 = (s16)((u16)work->field_F12 + 1);
    }

    if (work->field_F12 == 8) {
        work->field_0      = 0x12;
        Gp_StateF0.field_6 = 1;
        pending            = (Actor403200PendingPos*)Gp_PendingObj4C;
        pending->pos.vx    = arg1->extra.tmd->coords->coord.t[0] + 0xFA0;
        pendingPos         = &pending->pos;
        pendingPos->vy     = arg1->extra.tmd->coords->coord.t[1] - 0x64;
        pendingPos->vz     = arg1->extra.tmd->coords->coord.t[2];
        SndEvt_EnqueueType7((((u16)arg0->placeKey >> 12) << 8) | 0x4020000A, 1);
        SndEvt_EnqueueType7((((u16)arg0->placeKey >> 12) << 8) | 0x4020000D, 1);
    }

    if (arg0->hp <= 0) {
        stateF0 = Gp_StateF0.field_0;
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
        nodeFlags                              = 1;
        arg0->node.state.b.flags               = 1;
        work->field_ECC[3]->node.state.b.flags = 1;
        work->field_ECC[0]->node.state.b.flags = nodeFlags;
        work->field_ECC[1]->node.state.b.flags = nodeFlags;
    } else if (work->field_EFA != 0) {
        if (Gp_NodeSlotMask(&work->field_ECC[3]->node) != 0) {
            Gp_AssignNodeSlot0(&arg0->node);
        }
        arg0->node.state.b.flags               = 8;
        work->field_ECC[3]->node.state.b.flags = 5;
        work->field_ECC[0]->node.state.b.flags = 5;
        work->field_ECC[1]->node.state.b.flags = 5;
    } else {
        if (Gp_NodeSlotMask(&arg0->node) != 0) {
            Gp_AssignNodeSlot0(&work->field_ECC[3]->node);
        }
        arg0->node.state.b.flags               = 1;
        nodeFlags                              = 8;
        work->field_ECC[3]->node.state.b.flags = 8;
        work->field_ECC[0]->node.state.b.flags = nodeFlags;
        work->field_ECC[1]->node.state.b.flags = nodeFlags;
    }

    if (work->field_0 != 0 && work->field_EFA == 1) {
        work->hits[0].obj.flags |= 0x8000;
    } else {
        work->hits[0].obj.flags &= 0x7FFF;
    }
    {
        u32 a;
        u32 b;
        if (work->field_0 != 0 && work->field_EFA != 1) {
            a  = work->hits[1].obj.flags;
            b  = work->hits[2].obj.flags;
            a |= 0x8000;
        } else {
            a  = work->hits[1].obj.flags;
            b  = work->hits[2].obj.flags;
            a &= 0x7FFF;
        }
        b |= 0x8000;
        SOFT_BARRIER();
        work->hits[1].obj.flags = a;
        work->hits[2].obj.flags = b;
    }
    SOFT_BARRIER();
    {
        u32 a;
        u32 b;
        if (work->field_0 != 0) {
            a                       = work->hits[3].obj.flags;
            b                       = work->hits[5].obj.flags;
            a                      |= 0x8000;
            work->hits[3].obj.flags = a;
            SOFT_BARRIER();
            a                       = work->hits[4].obj.flags;
            b                      |= 0x8000;
            work->hits[5].obj.flags = b;
            a                      |= 0x8000;
            work->hits[4].obj.flags = a;
        } else {
            a                       = work->hits[3].obj.flags;
            b                       = work->hits[5].obj.flags;
            a                      &= 0x7FFF;
            work->hits[3].obj.flags = a;
            SOFT_BARRIER();
            a                       = work->hits[4].obj.flags;
            b                      &= 0x7FFF;
            work->hits[5].obj.flags = b;
            a                      &= 0x7FFF;
            work->hits[4].obj.flags = a;
        }
    }
    SOFT_BARRIER();
    {
        u32 a;
        u32 b;
        if (work->field_0 != 0) {
            a                       = work->hits[6].obj.flags;
            b                       = work->hits[8].obj.flags;
            a                      |= 0x8000;
            work->hits[6].obj.flags = a;
            SOFT_BARRIER();
            a                       = work->hits[7].obj.flags;
            b                      |= 0x8000;
            work->hits[8].obj.flags = b;
            a                      |= 0x8000;
            work->hits[7].obj.flags = a;
        } else {
            a                       = work->hits[6].obj.flags;
            b                       = work->hits[8].obj.flags;
            a                      &= 0x7FFF;
            work->hits[6].obj.flags = a;
            SOFT_BARRIER();
            a                       = work->hits[7].obj.flags;
            b                      &= 0x7FFF;
            work->hits[8].obj.flags = b;
            a                      &= 0x7FFF;
            work->hits[7].obj.flags = a;
        }
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
            (arg1->spawnArg1 >> 16) == 0) {
            Mc_SaveData.at4.loc.view = scratch->view;
        }
    }

    mode = work->field_EC8;
    if (mode == 1 && work->field_0 != 0xD) {
        count           = work->field_7CA + 1;
        work->field_7CA = count;
        if (work->field_ECA == mode) {
            if (work->anim.field_4 == 2) {
                work->anim.animBlock.ptr = D_actor_403200_8015E6AC;
                work->anim.field_8       = 0;
                work->anim.field_C       = 0;
                Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                work->field_7CA = 0;
            }
        } else if (work->anim.field_4 == 2 && (s16)count < 0x28) {
            work->anim.animBlock.ptr = D_actor_403200_8015E6AC;
            work->anim.field_8       = 0;
            work->anim.field_C       = 0;
            Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
        }

        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            switch (work->anim.field_4) {
                case 2:
                    if (work->field_ECA != 1 && (s16)work->field_7CA >= 0x17) {
                        work->anim.animBlock.ptr = D_actor_403200_8015E6AC;
                        D_actor_403200_8015E6AC[4] =
                            ((Actor403200AnimTable*)Gp_PlayerAnimBlkTbl
                                 [Gp_WeaponIdBase[D_8007218A[0] - 1] + D_80073BA9])
                                ->sets[7];
                        work->anim.field_4 = 4;
                        work->anim.field_8 = 1;
                        work->anim.field_C = 3;
                        Gp_DispatchMsg(player, 0x3FF, (s32)&work->anim, 0);
                        work->field_7CA = 0;
                    }
                    break;
                case 4:
                    if (work->field_ECA != 1) {
                        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
                        work->field_EC8 = 0;
                    }
                    break;
            }
        }
    }

    if (D_actor_403200_80141C50 == 1) {
        if (work->field_EC8 == D_actor_403200_80141C50) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 2, 0);
            work->field_EC8 = 0;
        }
    }

    SCRATCH_POP_BYTES(sizeof(Actor403200TickScratch));
}

void func_actor_403200_801408D8(Task* task, s16 scale, s16 drop, s16 index)
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

    face.field_A     = 3;
    face2.field_A    = 3;
    faces[index]     = face;
    faces[index + 1] = face2;
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
    GpEnemy*              enemy;
    s32                   diff;
    s32                   y;
    GsCOORDINATE2*        playerCoord;
    GsCOORDINATE2*        selfCoord;

    sp     = D_actor_403200_801321B8;
    player = gameGetPtrSlot(3);
    work   = (Actor403200Work*)arg0->work;
    enemy  = arg0->spawnArg2;
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
void func_actor_403200_80140FD4(s32 arg0, s16 arg1)
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

/// Exit callback of the boss task: when its work block exists, send each of the
/// seven escorts to state 2, unlink every collision group but the third, and
/// detach the enemy's contact records, then tear the enemy down.
void func_actor_403200_80141018(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;
    s16              i;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work != NULL) {
        for (i = 0; i < 7; i++) {
            if (work->field_ECC[i] != NULL) {
                work->field_ECC[i]->task->state = 2;
            }
        }
        Gp_UnlinkObj(&work->hits[0].obj);
        Gp_UnlinkObj(&work->hits[1].obj);
        Gp_UnlinkObj(&work->hits[3].obj);
        Gp_UnlinkObj(&work->hits[4].obj);
        Gp_UnlinkObj(&work->hits[5].obj);
        Gp_UnlinkObj(&work->hits[6].obj);
        Gp_UnlinkObj(&work->hits[7].obj);
        Gp_UnlinkObj(&work->hits[8].obj);
        enemy->recs = 0;
    }
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_403200_801410F0(s8 arg0)
{
    ((Actor403200Work*)D_actor_403200_8015F8F0->work)->field_EAC = arg0;
}

void func_actor_403200_80141108(s16 arg0)
{
    D_actor_403200_80141C5A = arg0;
}

s16 func_actor_403200_80141114(void)
{
    return D_actor_403200_80141C5A;
}

s32 func_actor_403200_80141124(Actor403200Obj* arg0, s16 arg1)
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

s16 func_actor_403200_80141180(Task* arg0, s16 arg1)
{
    return func_actor_403200_801344C4(arg0, arg1);
}

/// Returns 0x25 for the current view, or 0x1E when the slot-3 model's X
/// translation is at or above a threshold that depends on the view index:
/// 0x3A98 for view 0x1E, 0x3E80 otherwise.
s32 func_actor_403200_801411A8(void)
{
    Task* task;
    s32   flag;
    s32   value;
    s32   view;

    view = Gp_GetViewIndex() & 0xFF;
    task = gameGetPtrSlot(3);
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

void func_actor_403200_8014122C(Task* arg0)
{
}

void func_actor_403200_80141234(Task* arg0)
{
}

/// The state handler `D_actor_403200_80132154` lists for state 8. Re-arms the
/// sub-state counter if the dispatcher saw a state change this tick, runs the
/// per-frame body, and on the tick the counter reaches 8 tells the player's
/// task (message 0x13F4) and plays the actor's cue.
void func_actor_403200_8014123C(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         enemy;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_6 = 0;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_6 == 8) {
        Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F4, 0, 0);
        SndEvt_EnqueueType7(((enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
    }
}

/// Setup state of the handler table `D_actor_403200_80131E84`: look up the
/// area placement the parent's spawn record names (its top nibble) under the
/// current session location, give the model that placement's texture page and
/// CLUT, run its stream twice when it has one, and step the task on.
void func_actor_403200_801412D0(GpEnemy* enemy, Task* task)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    TmdObject*   model;
    s32          idx;
    u32          raw;

    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    raw        = ((GpWorkObj*)task->parent->spawnArg2)->field_8.as_u16;
    model      = task->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    task->state++;
}

/// Per-frame state of the same table: refresh the model's root coordinate. The
/// world position it then copies into a local is never used.
void func_actor_403200_8014139C(GpEnemy* enemy, Task* arg1)
{
    VECTOR sp10;

    arg1->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    sp10.vx = arg1->extra.tmd->coords->workm.t[0];
    sp10.vy = arg1->extra.tmd->coords->workm.t[1];
    sp10.vz = arg1->extra.tmd->coords->workm.t[2];
}

void func_actor_403200_80141430(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403200_8014148C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E84;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Dispatcher of the enemy `D_actor_403200_80131E90` drives: run the handler for
/// the task's state, skipped while the global game mode is 2.
void func_actor_403200_801414E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403200_80131E90;
    switch (Gp_StateF0.field_4) {
        default:
        case 0:
        case 1:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 2:
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

    switch (Gp_StateF0.field_4) {
        case 0:
            arg0->extra.tmd->flags = 0;
            break;
        case 1:
            arg0->extra.tmd->flags = 0;
            return;
        case 2:
            arg0->extra.tmd->flags = 0x80;
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
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
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

    switch (Gp_StateF0.field_4) {
        case 0:
            arg0->extra.tmd->flags = 2;
            break;
        case 1:
            arg0->extra.tmd->flags = 2;
            return;
        case 2:
            arg0->extra.tmd->flags = 0x80;
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
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Dispatcher of the dropped enemy (`D_actor_403200_80131F14`): run the handler
/// for the task's state, skipped while the global game mode is 1 or 2.
void func_actor_403200_80141778(Task* arg0)
{
    GpEnemyTaskFuncTable5 sp;

    sp = D_actor_403200_80131F14;
    switch (Gp_StateF0.field_4) {
        case 0:
        default:
            sp.funcs[arg0->state](arg0->spawnArg2, arg0);
            break;
        case 1:
        case 2:
            break;
    }
}

/// Waiting state of the spinner enemy (`D_actor_403200_80131F28`): hand the
/// enemy back to `Gp_DestroyEnemy` once `D_actor_403200_80141C50` is set;
/// otherwise keep the model's flag word cleared, so it is not drawn, and step
/// the task on once `D_actor_403200_80141C5A` is 1.
void func_actor_403200_80141800(GpEnemy* arg0, Task* arg1)
{
    if (D_actor_403200_80141C50 == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }

    if (D_actor_403200_80141C5A == 1) {
        arg1->state++;
    }

    arg1->extra.tmd->flags = 0;
}

/// Dispatcher of the spinner enemy (`D_actor_403200_80131F28`): park the model
/// object while the global game mode is 1 or 2, otherwise note in the work
/// block whether the state changed since the last step and run the handler for
/// it.
void func_actor_403200_80141868(Task* arg0)
{
    GpEnemyTaskFuncTable4   sp;
    Actor403200SpinnerWork* work;

    sp = D_actor_403200_80131F28;

    switch (Gp_StateF0.field_4) {
        case 0:
            arg0->extra.tmd->flags = 0;
            break;
        case 1:
            arg0->extra.tmd->flags = 0;
            return;
        case 2:
            arg0->extra.tmd->flags = 0x80;
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
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403200_8014196C(void)
{
}

/// Whether the actor should keep acting: 1 while its enemy still has HP. Once
/// the enemy is down, 0 if the model carries flag 0x80 or flag 2, and 1
/// otherwise.
s32 func_actor_403200_80141974(Task* task)
{
    s32 ret;
    u16 flags;
    s32 mask2;
    s32 mask80;

    if (((GpEnemy*)task->spawnArg2)->hp > 0) {
        return 1;
    }

    flags   = task->extra.tmd->flags;
    mask80  = flags;
    mask80 &= 0x80;
    mask2   = flags & 2;
    if (mask80 != 0) {
        return 0;
    }

    ret = 0;
    if (mask2 == 0) {
        ret = 1;
        SOFT_BARRIER();
    }
    return ret;
}

/// Place the task's model from `placement`: the three longs become the root
/// coordinate's translation, then the X, Y and Z rotations are applied in that
/// order and the coordinate is marked dirty. Returns 1.
s32 func_actor_403200_801419C4(Task* task, s32 arg1, GpXformArg* placement)
{
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->flg = 0;
    return 1;
}

/// Per-frame upkeep for the enemy, dispatched by `arg2`: state 0 bumps the
/// heal counter, files a negative "damage" with `func_800DA6E8` so the HUD
/// shows it as a heal, and tops the enemy's HP back up by 0x64; state 1 ticks
/// the countdown at 0xF1C down and, once it has run out, re-arms the enemy's
/// `field_F16`. Same body as `func_actor_444000_80143E68` without its tracked
/// escort slots.
s32 func_actor_403200_80141A94(Task* arg0, s32 arg1, s32 arg2)
{
    Actor403200Work* work  = (Actor403200Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;

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
    D_actor_403200_80141C54 = 0;
    return 1;
}

/// State-change reset: once the dispatcher has flagged the change in
/// `field_4`, drop the re-arm marker and push the host model's `field_C` onto
/// every live escort's own model object. Same body as
/// `func_actor_444000_80143F4C`.
void func_actor_403200_80141B40(Task* arg0)
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
        func_actor_403200_80133DD8(arg0);
    }
}
