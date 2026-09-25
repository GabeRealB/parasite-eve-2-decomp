#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/session.h"
#include "main/sound.h"

#include "gameplay/gameplay.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

/* The controller task this actor is reparented to is the Mist shooting
 * gallery's, so the counter at +0xE of its work block is that room's. */
#include "rooms/mist_shooting_gallery.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

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
/// `rec18` is the collision table `obj.ctx.recs` points at and
/// `func_actor_107600_80134958` hands to `Gp_InitRec18Table` with count 8, so
/// it really runs to +0x140 and `field_13E` sits inside its last record.
/// `field_162` is the spawn variant `func_actor_107600_80132ED0` takes from
/// the low nibble of the task's own `Task::spawnArg1` high halfword
/// (`lhu 0x36` then `andi 0xF`): `func_actor_107600_80134958` picks the
/// display node's `field_1C` from it and `func_actor_107600_80134C54`
/// switches on it.
typedef struct Actor107600Work {
    /// Colour / light matrix pair `func_actor_107600_80132ED0` hangs off the
    /// display object's `TmdObject.colorMtx` / `field_1C` so the actor draws
    /// with its own light instead of `Gp_BindDefaultMtx`'s.
    /* 0x000 */ MATRIX  matrix_0;  // color matrix for the child models
    /* 0x020 */ MATRIX  matrix_20; // light matrix for the child models
    /* 0x040 */ u16     pitch;     // fed to RotMatrixX
    /* 0x042 */ s16     yaw;       // fed to func_8004BFF8
    /* 0x044 */ u16     roll;      // fed to RotMatrixZ
    /* 0x046 */ byte    pad_46[0x2];
    /* 0x048 */ u16     field_48;  // spawn position x
    /* 0x04A */ u16     field_4A;  // spawn position y
    /* 0x04C */ u16     field_4C;  // spawn position z
    /* 0x04E */ byte    pad_4E[0x2];
    /* 0x050 */ u16     field_50;  // fed to Gfx_RotMatrixX
    /* 0x052 */ u16     field_52;  // fed to Gfx_RotMatrixY
    /* 0x054 */ u16     field_54;  // fed to Gfx_RotMatrixZ
    /* 0x056 */ byte    pad_56[0x2];
    /* 0x058 */ s16     field_58;  // spin velocity added to field_50 while tumbling
    /* 0x05A */ s16     field_5A;  // spin velocity added to field_52
    /* 0x05C */ s16     field_5C;  // spin velocity added to field_54
    /* 0x05E */ byte    pad_5E[0x2];
    /* 0x060 */ GpObj   obj;
    /* 0x080 */ GpRec18 rec18[1];  // collision table; count 8 passed to Gp_InitRec18Table
    /* 0x098 */ byte    pad_98[0xA2];
    /* 0x13A */ u16     field_13A; // frame counter / countdown of func_actor_107600_80132160
    /* 0x13C */ byte    pad_13C[0x2];
    /* 0x13E */ u16     field_13E;
    /// The spawn state stores the model root here as a word, while
    /// `func_actor_107600_80132D54` counts its sub-phase in the low halfword.
    /* 0x140 */ union {
        GsCOORDINATE2* coord; // model root, stored by the spawn state
        s16            step;  // sub-phase of func_actor_107600_80132D54
    } field_140;
    /* 0x144 */ s16  field_144;
    /* 0x146 */ s16  field_146; // written 2 beside field_144 by the spawn state
    /* 0x148 */ s16  field_148; // waypoint index into the D_actor_107600_80135624 path
    /* 0x14A */ u8   field_14A; // rotating flag: gates the yaw advance in func_actor_107600_80132CD4
    /* 0x14B */ s8   field_14B; // scale percent applied to the model root coord.m[1][1]
    /* 0x14C */ s32  field_14C; // XZ distance to the Gp_ActorSlots[0] actor's coord
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
/// `G_SCRATCH_HEAD` to draw one `POLY_FT4`. `v` holds the four corners
/// (the offset table plus the coordinate's translation and the caller's
/// position), projected through `workm` by one `RTPS` and one `RTPT` into
/// `sxy` (each a packed `gte_stsxy` word, x low and y high). `otz` is the
/// `gte_stszotz` less 0x40, which picks the OT bucket.
typedef struct Actor107600QuadScratch {
    /* 0x00 */ s32     sxy[4];
    /* 0x10 */ s32     otz;
    /* 0x14 */ SVECTOR v[4];
} Actor107600QuadScratch;

/// Global scene mode the actor updates switch on: 0 runs the full update, 1 only
/// refreshes the colour, 2 hides the model (`TmdObject.flags` bit 0x80).
extern u8 D_801153F4;

/// Psy-Q `RotMatrixY` (it sits right after `RotMatrixX`).
void func_8004BFF8(s16 angle, MATRIX* matrix);

void func_actor_107600_801328CC(Task* arg0);
void func_actor_107600_80132A7C(Task* arg0);
void func_actor_107600_80132AC0(Task* arg0);
void func_actor_107600_80132B0C(Task* arg0);
void func_actor_107600_80132B7C(Task* arg0);
void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst);
void func_actor_107600_80132CB8(Task* arg0);
void func_actor_107600_80132CD4(Task* arg0);
void func_actor_107600_80132D54(Task* arg0);
void func_actor_107600_80132DF0(GpEnemy* arg0, s32 arg1, s32 arg2);
void func_actor_107600_80132ED0(Task* arg0);
void func_actor_107600_80133024(Task* arg0);
void func_actor_107600_801332D4(Task* arg0);
void func_actor_107600_80133668(Task* arg0);
void func_actor_107600_801337FC(Task* arg0);
void func_actor_107600_801339A4(Task* arg0);
void func_actor_107600_80133FA8(GsCOORDINATE2* arg0, SVECTOR* arg1);
void func_actor_107600_80134248(GsCOORDINATE2* arg0, SVECTOR* arg1);
void func_actor_107600_80134608(struct GpEnemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_actor_107600_801348A0(Task* arg0);
void func_actor_107600_80134904(Task* arg0);
void func_actor_107600_80134920(Task* arg0);
void func_actor_107600_80134958(Task* arg0);
void func_actor_107600_801349E0(Task* arg0);
void func_actor_107600_80134A50(Task* arg0);
void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst);
void func_actor_107600_80134B98(Task* arg0, s16 arg1);
s32  func_actor_107600_80134BAC(Task* arg0);
void func_actor_107600_80134C54(Task* arg0);
void func_actor_107600_80134D10(Task* arg0);
void func_actor_107600_80134D30(Task* arg0);
void func_actor_107600_80134D50(Task* arg0);
void func_actor_107600_80134D70(Task* arg0);
void func_actor_107600_80134D9C(Task* arg0);
void func_actor_107600_80134E5C(GsCOORDINATE2* arg0);
void func_actor_107600_80134EF4(Task* arg0);

/* Per-variant waypoint paths `func_actor_107600_80132160` walks, indexed by
 * `Actor107600Work.field_146`; trailing-blob data. */
extern Actor107600Waypoint* D_actor_107600_80135624[];

/* Eight effect offsets `func_actor_107600_80133024` cycles through from
 * `Actor107600Work.field_16A`. */
extern Actor107600Pair D_actor_107600_80135730[];

/* Table `func_actor_107600_80132DF0` spawns from, indexed with `arg1 + 1`; it
 * is the trailing animation/data blob, not the leading rodata. */
extern TaskDesc D_actor_107600_80134F94;

/* The pair-source record the spawn state hangs off the enemy's `GpEnemy.param`
 * (a zeroed pointer to `D_actor_107600_8013571C`, 0x32 and 0xFF000000) and the
 * 16-entry HP table it indexes with the spawn variant. Both are trailing-blob
 * data, after the collision tables. */
/* Pair-source record the spawn state hangs off `GpEnemy.param`. */
extern GpPairSrcE D_actor_107600_80134F84;
extern GpPairSrcE D_actor_107600_80135720;
extern u16        D_actor_107600_80135750[];

/* Remaining-enemy count, and the gallery controller task the room overlay
 * publishes (its `Task::work` is the `MistShootingGalleryWork`). */
extern s16   D_80073BA0;
extern Task* D_8018E0C4;

void func_actor_107600_80131F10(Task* arg0);
void func_actor_107600_80132160(Task* arg0);
void func_actor_107600_80132514(Task* arg0);
void func_actor_107600_80132930(Task* arg0);
void func_actor_107600_80133DC4(Task* arg0);

/// The actor's top-level task states, which `func_actor_107600_801328CC` runs:
/// spawn, update, drop and destroy.
const TaskFuncTable4 D_actor_107600_80131E24 = { {
    func_actor_107600_80131F10,
    func_actor_107600_80132930,
    func_actor_107600_80132A7C,
    func_actor_107600_80132AC0,
} };

/// One entry per `Actor107600Work.field_144` phase, run by
/// `func_actor_107600_80132CD4`.
const TaskFuncTable3 D_actor_107600_80131E34 = { {
    func_actor_107600_80132160,
    func_actor_107600_80132514,
    func_actor_107600_80132D54,
} };

/// Spawn state of the `D_actor_107600_80131E24` table. The target is dropped
/// (and the gallery's live count given back) when the player is within 0x400 on
/// XZ unless `Task::spawnArg1` bit 0x40000000 forces it, when byte 0 of
/// `spawnArg1` is the 0xFF marker, or when the work block cannot be allocated.
/// Otherwise binds the work block's matrices, records the spawn position, and
/// spawns the child from `func_actor_107600_80132DF0`.
void func_actor_107600_80131F10(Task* arg0)
{
    TmdObject*       obj;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   target;
    void**           scratch;
    u8*              head;
    VECTOR*          block;
    Actor107600Work* work;

    obj       = arg0->extra;
    enemy     = arg0->spawnArg2;
    coord     = obj->coords;
    target    = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = target->coord.t[0] - coord->coord.t[0];
    *scratch  = block;
    block->vz = target->coord.t[2] - coord->coord.t[2];
    if ((!(arg0->spawnArg1 & 0x40000000) && func_80103D8C(block->vx, block->vz) < 0x401) || (u8)arg0->spawnArg1 == 0xFF) {
    fail:
        if ((arg0->spawnArg1 & 0xF000) != 0x2000) {
            ((MistShootingGalleryWork*)arg0->parent->work)->field_0E--;
        }
        *(u8**)G_SCRATCH_HEAD += 0x10;
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    work       = memCalloc(0x14C, false);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        goto fail;
    }
    arg0->exitCallback = func_actor_107600_80132AC0;
    work->field_146    = (u8)((u32)arg0->spawnArg1 >> 16);
    work->field_144    = (s32)(arg0->spawnArg1 & 0xF000) >> 12;
    obj->lightMtx      = &work->matrix_20;
    obj->colorMtx      = &work->matrix_0;
    enemy->param       = &D_actor_107600_80134F84;
    coord->sub         = &gGfxViewCoord;
    enemy->field_4     = &((TmdObject*)arg0->extra)->coords->coord;
    enemy->field_48    = 0;
    if (work->field_144 != 2) {
        /* retail passes a 0 the resident definition ignores */
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    work->field_48 = coord->coord.t[0];
    work->field_4A = coord->coord.t[1];
    work->field_4C = coord->coord.t[2];
    work->yaw      = -0x400;
    if (work->field_144 == 1) {
        work->roll += 0x800;
    }
    arg0->state++;
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    func_actor_107600_80132DF0(enemy, arg0->spawnArg1 & 0xF,
                               work->field_144 | (((u32)arg0->spawnArg1 >> 16) & 0x2000));
    *scratch = (u8*)*scratch + 0x10;
}

/// Path-following phase: grows the
/// `field_14B` scale to 100, bobs the model root for four frames, then once the
/// first child raises bit 0x20 steps the root along the `field_146` path of
/// `D_actor_107600_80135624` one waypoint at a time. A waypoint with no step
/// holds for `30 *` the spawn nibble instead; the -1 terminator, the countdown
/// or the child's bit 0x40 stops the path, and bit 0x80 then shrinks the scale
/// back to 0 and advances the task state.
void func_actor_107600_80132160(Task* arg0)
{
    Actor107600Work*     work  = (Actor107600Work*)arg0->work;
    GpEnemy*             enemy = arg0->spawnArg2;
    GsCOORDINATE2*       coord = ((TmdObject*)arg0->extra)->coords;
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
                enemy->task->firstChild->spawnArg1 |= 0x10;
            }
            return;
        case 2:
            if (!(enemy->task->firstChild->spawnArg1 & 0x20)) {
                return;
            }
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            if (arg0->spawnArg1 & 0x10000000) {
                work->field_14A = 1;
            }
            if (wp->step == 0) {
                work->field_140.step = 4;
                work->field_13A      = (((u8*)&arg0->spawnArg1)[3] & 0xF) * 30;
                return;
            }
            work->field_140.step++;
        case 3:
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            x   = wp->x;
            if (x == -1) {
            stop:
                work->field_140.step                = 5;
                work->field_14A                     = 0;
                enemy->task->firstChild->spawnArg1 |= 0x40;
                return;
            }
            d = (s16)(coord->coord.t[0] - x);
            if (d != 0) {
                step = wp->step;
                if (step >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1 & 0x40) {
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
                    if (enemy->task->firstChild->spawnArg1 & 0x40) {
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
            if (enemy->task->firstChild->spawnArg1 & 0x80) {
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
void func_actor_107600_80132514(Task* arg0)
{
    Actor107600Work*     work  = (Actor107600Work*)arg0->work;
    GpEnemy*             enemy = arg0->spawnArg2;
    GsCOORDINATE2*       coord = ((TmdObject*)arg0->extra)->coords;
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
                enemy->task->firstChild->spawnArg1 |= 0x10;
            }
            return;
        case 2:
            if (!(enemy->task->firstChild->spawnArg1 & 0x20)) {
                return;
            }
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            if (arg0->spawnArg1 & 0x10000000) {
                work->field_14A = 1;
            }
            if (wp->step == 0) {
                work->field_140.step = 4;
                work->field_13A      = (((u8*)&arg0->spawnArg1)[3] & 0xF) * 30;
                return;
            }
            work->field_140.step++;
        case 3:
            wp  = D_actor_107600_80135624[work->field_146];
            wp += work->field_148;
            x   = wp->x;
            if (x == -1) {
            stop:
                work->field_140.step                = 5;
                work->field_14A                     = 0;
                enemy->task->firstChild->spawnArg1 |= 0x40;
                return;
            }
            d = (s16)(coord->coord.t[0] - x);
            if (d != 0) {
                step = wp->step;
                if (step >= abs(d)) {
                    if (enemy->task->firstChild->spawnArg1 & 0x40) {
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
                    if (enemy->task->firstChild->spawnArg1 & 0x40) {
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
            if (enemy->task->firstChild->spawnArg1 & 0x80) {
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
const TaskFuncTable4 D_actor_107600_80131E74 = { {
    func_actor_107600_80132ED0,
    func_actor_107600_80133024,
    func_actor_107600_80134904,
    func_actor_107600_80134920,
} };

/// Behaviour states indexed by `Actor107600Work.field_158`, run by
/// `func_actor_107600_80133024`.
const TaskFuncTable10 D_actor_107600_80131E84 = { {
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
/// mode `D_801153F4`. Mode 0 runs the `field_13E` sub-state, copies the yaw and
/// roll onto the model root, rebuilds its rotation and scales `coord.m[1][1]`
/// by the `field_14B` percent; modes 0 and 1 then refresh the colour and show
/// the model, and mode 2 hides it.
void func_actor_107600_80132930(Task* arg0)
{
    TmdObject*       ext      = arg0->extra;
    GpCoordPose*     coord    = (GpCoordPose*)ext->coords;
    Actor107600Work* work     = (Actor107600Work*)arg0->work;
    TaskFunc         funcs[2] = { (TaskFunc)func_actor_107600_80132CB8, func_actor_107600_80132CD4 };
    TmdObject*       obj;

    obj = ext;
    switch (D_801153F4) {
        case 0:
            funcs[(s16)work->field_13E](arg0);
            coord->field_46 = work->yaw;
            coord->field_48 = work->roll;
            coord->flg      = 0;
            func_actor_107600_80132B7C(arg0);
            coord->coord.m[1][1] = work->field_14B * (coord->coord.m[1][1] / 100);
        case 1:
            func_actor_107600_80132B0C(arg0);
            obj->flags &= ~0x80;
            break;
        case 2:
            ext->flags |= 0x80;
            break;
    }
}

/// State 2 of the `D_actor_107600_80131E24` table: drops this instance from the
/// spawning gallery's live-target count unless it was spawned already counted
/// (phase 2, the `0x200D` cursor target), then advances to the exit state.
void func_actor_107600_80132A7C(Task* arg0)
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
void func_actor_107600_80132AC0(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->work;

    if (work->field_144 != 2) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    }
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off `G_SCRATCH_HEAD` and hands it to
/// `Gp_UpdateActorColor` for the enemy in `Task::spawnArg2` with no blend
/// parameters. Same shape as `func_actor_107600_801349E0`, a different callee.
void func_actor_107600_80132B0C(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = arg0->spawnArg2;
    coord     = ((TmdObject*)arg0->extra)->coords;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Rebuilds the model root's rotation from the work block's three angles: wrap
/// each to 12 bits, build the rotation in a scratch matrix carved off
/// `G_SCRATCH_HEAD`, then copy its 3x3 into the part's `GsCOORDINATE2::coord`.
/// The copy is a call to `func_actor_107600_80132C4C`.
void func_actor_107600_80132B7C(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    MATRIX*          m;

    work->pitch              &= 0xFFF;
    work->yaw                &= 0xFFF;
    work->roll               &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->roll, m);
    RotMatrixX((s16)work->pitch, m);
    func_8004BFF8((s16)work->yaw, m);
    func_actor_107600_80132C4C(m, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
}

/// Copies the 3x3 rotation of the scratch matrix `func_actor_107600_80132B7C`
/// just built into the part's `GsCOORDINATE2::coord`, leaving the translation
/// row of the destination alone.
void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst)
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

void func_actor_107600_80132CB8(Task* arg0)
{
    Actor107600Work* work = arg0->work;

    work->field_13E++;
}

/// Runs the `D_actor_107600_80131E34` entry for the work block's `field_144`
/// phase through the same stack-copied table idiom as
/// `func_actor_107600_801328CC`, then advances the model's yaw by 0x20 once the
/// spawn flag at `field_14A` says this instance is rotating.
void func_actor_107600_80132CD4(Task* arg0)
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
void func_actor_107600_80132D54(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    GpEnemy*         enemy = arg0->spawnArg2;

    switch (work->field_140.step) {
        case 0:
            work->field_140.step++;
            work->field_14B   = 100;
            coord->coord.t[1] = 0;
        case 1:
            if (enemy->task->firstChild->spawnArg1 & 0x80) {
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
/// task is reparented and its root coordinate's `sub` link is pointed at
/// `arg0`'s own root coordinate, `Task::spawnArg1` is packed from the two
/// arguments, and the spawned model takes `arg1`'s texture page - dropping the
/// CLUT row to 0 once `arg1` reaches 10 - before the stream is processed twice
/// (one half-buffer per call) and the enemy's light is set to 0x900.
void func_actor_107600_80132DF0(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    GpEnemy*    enemy;
    GpCoordExt* coord;
    TmdObject*  obj;

    enemy = Gp_SpawnEnemyFromTable(&D_actor_107600_80134F94, arg1 + 1, arg2, arg0);
    if (enemy != NULL) {
        Task_Reparent(arg0->task, enemy->task);
        coord                  = (GpCoordExt*)((TmdObject*)enemy->task->extra)->coords;
        coord->sub             = ((TmdObject*)arg0->task->extra)->coords;
        enemy->task->spawnArg1 = arg1 | (arg2 << 16);
        obj                    = (TmdObject*)enemy->task->extra;
        obj->tpage             = 0;
        if (arg1 < 10) {
            obj->clut = 2;
        } else {
            obj->clut = 0;
        }
        tmdProcessStream(obj);
        tmdProcessStream(obj);
        enemy->workType = 0x900;
    }
}

/// Spawn state: allocates the work block, hangs the two matrices off the
/// display object's `field_1C` / `field_20`, puts the actor's own light on the
/// enemy and links its node in. Both failure paths - the 0xFF "already dead"
/// marker in byte 0 of `Task::spawnArg1` and a failed allocation - destroy the
/// enemy and return before the exit callback is installed. The variant is the
/// low nibble of `spawnArg1`'s high halfword: it selects the HP from the
/// 16-entry table and its model root later, in `func_actor_107600_80134958`.
void func_actor_107600_80132ED0(Task* arg0)
{
    Actor107600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    u16              hp;
    u32              variant;

    obj     = arg0->extra;
    variant = *(u8*)&arg0->spawnArg1;
    enemy   = arg0->spawnArg2;
    coord   = obj->coords;
    if (variant == 0xFF || (work = (Actor107600Work*)memCalloc(0x16C, false), arg0->work = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    arg0->exitCallback    = func_actor_107600_80134920;
    work->field_162       = ((u32)arg0->spawnArg1 >> 16) & 0xF;
    obj->lightMtx         = &work->matrix_20;
    obj->colorMtx         = &work->matrix_0;
    enemy->param          = &D_actor_107600_80135720;
    enemy->recs           = work->rec18;
    work->field_140.coord = ((TmdObject*)arg0->extra)->coords;
    work->field_144       = 0x140;
    work->field_146       = 2;
    hp                    = D_actor_107600_80135750[arg0->spawnArg1 & 0xF];
    enemy->hpMax          = hp;
    enemy->hp             = hp;
    func_actor_107600_80134958(arg0);
    Gp_LinkNode(&enemy->node);
    enemy->field_4    = &coord->workm;
    enemy->bodyPos.vy = -0x244;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vz = 0;
    enemy->coord      = coord;
    enemy->node.flags = 1;
    func_actor_107600_80134E5C(coord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    arg0->state += 1;
}

/// Per-frame update switched on the scene mode `D_801153F4`, like
/// `func_actor_107600_80132930`. Mode 0 runs the `field_158` state out of
/// `D_actor_107600_80131E84`, then (below state 7) takes hits, clears the collision records and enters state 9 once the enemy's
/// HP is gone. Afterwards publishes the enemy's slot mask to the gallery and
/// emits one effect per `field_16B` hit, the last one upward when dead.
void func_actor_107600_80133024(Task* arg0)
{
    TaskFuncTable10  sp;
    GpEnemy*         enemy;
    TmdObject*       ext;
    TmdObject*       obj;
    Actor107600Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         v;
    s32              i;

    enemy                  = arg0->spawnArg2;
    ext                    = arg0->extra;
    work                   = (Actor107600Work*)arg0->work;
    coord                  = ext->coords;
    obj                    = ext;
    sp                     = D_actor_107600_80131E84;
    *(s32*)G_SCRATCH_HEAD -= 8;
    v                      = *(SVECTOR**)G_SCRATCH_HEAD;
    switch (D_801153F4) {
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
            obj->flags &= ~0x80;
            break;
        case 2:
            obj->flags |= 0x80;
            break;
    }
    if (work->field_162 != 2) {
        ((MistShootingGalleryWork*)D_8018E0C4->work)->field_1D = Gp_NodeSlotMask(&enemy->node);
    }
    coord->flg = 0;
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
    *(s32*)G_SCRATCH_HEAD += 8;
}

/// Sub-state machine in `field_15A`: once `Task::spawnArg1` bit 0x10 is set,
/// grows the `field_168`/`field_169` scales by 0x20 up to 100, plays a cue and
/// eases `field_50` down, alternates `field_50` for four frames and raises bit
/// 0x20. From then on, while bit 0x20000000 is set, `field_166` counts frames:
/// at 120 it switches the light mode, at 210 it spawns an effect on the
/// `gameGetPtrSlot(3)` actor's fifth coordinate and updates that actor.
void func_actor_107600_801332D4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    GpActorWork*     player;
    GameActor*       actor;
    s32              pan;
    s32              flags;
    s16              v;

    if ((s16)func_actor_107600_80134BAC(arg0) != 0) {
        return;
    }
    switch (work->field_15A) {
        case 0:
            if (!(arg0->spawnArg1 & 0x10)) {
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
                GsCOORDINATE2* o = ((TmdObject*)arg0->extra)->coords;
                s32            p;
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
                    arg0->spawnArg1 |= 0x20;
                    Gp_SetLightMode((GpObj4C*)enemy, 0);
                    enemy->node.flags = 4;
                    work->obj.flags  |= 0x8000;
                }
            }
        case 5:
            flags = arg0->spawnArg1;
            if (flags & 0x40) {
                func_actor_107600_80134B98(arg0, 7);
                return;
            }
            if (!(flags & 0x20000000)) {
                return;
            }
            work->field_166++;
            if ((s16)work->field_166 == 120) {
                GsCOORDINATE2* o = ((TmdObject*)arg0->extra)->coords;
                s32            p;
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                p = (s8)Gp_GetObjPan(o);
                SndEvt_EnqueueType6(0x51140013, p, (s8)gpGetObjDepth(o));
            } else if ((s16)work->field_166 == 210) {
                GsCOORDINATE2* c;
                s32            p;
                player          = (GpActorWork*)gameGetPtrSlot(3);
                c               = &player->extra->coords[4];
                actor           = player->actor;
                work->field_166 = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 0);
                Gp_SpawnEff(0x601BD, (GsCOORDINATE2*)c, 0, NULL);
                p = (s8)Gp_GetObjPan(c);
                SndEvt_EnqueueType6(0x5114000E, p, (s8)gpGetObjDepth(c));
                if (actor->field_954 != 1) {
                    if (D_80073BA0 < 11) {
                        ((MistShootingGalleryWork*)D_8018E0C4->work)->field_22 = 1;
                        actor->field_96E                                       = 0;
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
void func_actor_107600_80133668(Task* arg0)
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
void func_actor_107600_801337FC(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   obj;
    s32              pan;

    switch (work->field_15A) {
        case 0:
            work->field_15A++;
            arg0->spawnArg1 |= 0x40;
            work->field_154  = 7;
            Gp_UnlinkNode(&enemy->node);
            enemy->recs      = 0;
            work->obj.flags &= 0x7FFF;
        case 1:
            work->field_154--;
            if ((s16)work->field_154 <= 0) {
                obj = ((TmdObject*)arg0->extra)->coords;
                work->field_15A++;
                work->field_16B = 0;
                work->field_15C = 0;
                Gp_SetLightMode((GpObj4C*)enemy, 2);
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
            arg0->spawnArg1 |= 0x80;
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
void func_actor_107600_801339A4(Task* arg0)
{
    Actor107600Work*         work  = (Actor107600Work*)arg0->work;
    GpEnemy*                 enemy = arg0->spawnArg2;
    TmdObject*               tmd   = arg0->extra;
    GsCOORDINATE2*           obj   = tmd->coords;
    MistShootingGalleryWork* gal   = (MistShootingGalleryWork*)D_8018E0C4->work;
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
            tmd->flags      |= 2;
            arg0->spawnArg1 |= 0x40;
            work->field_16B  = 0;
            work->field_15C  = 0;
            Gp_SetLightMode((GpObj4C*)enemy, 2);
            work->field_154 = 0;
            id              = arg0->spawnArg1 & 0xF;
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
            work->field_52 = ((GpCoordExt*)obj->sub)->field_46;
            work->field_54 = ((GpCoordExt*)obj->sub)->field_48;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            x              = (Gp_LcgState >> 16) & 0x7F;
            work->field_58 = x;
            if (!((Gp_LcgState >> 16) & 1)) {
                x = -x;
            }
            work->field_58 = x;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            y              = (Gp_LcgState >> 16) & 0x7F;
            work->field_5A = y;
            if (!((Gp_LcgState >> 16) & 1)) {
                y = -y;
            }
            work->field_5A = y;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            z              = (Gp_LcgState >> 16) & 0x7F;
            work->field_5C = z;
            if (!((Gp_LcgState >> 16) & 1)) {
                z = -z;
            }
            work->field_5C = z;
            func_actor_107600_80134B2C(&obj->sub->coord, &obj->coord);
            obj->coord.t[0] += obj->sub->coord.t[0];
            obj->coord.t[1] += obj->sub->coord.t[1];
            obj->coord.t[2] += obj->sub->coord.t[2];
            obj->sub         = &gGfxViewCoord;
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
            work->obj.flags &= 0xBFFF;
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
                arg0->spawnArg1 |= 0x80;
            }
            break;
        case 2:
            break;
    }
}

/// Hit handler: for each collision record tagged 0x2xxxx, stores the hit
/// position, applies `Gp_ComputeDamage` to the enemy's HP, plays the hit sound
/// for the first eight hits and picks a light/heavy reaction in `field_15E`.
void func_actor_107600_80133DC4(Task* arg0)
{
    Actor107600Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   obj;
    s32              i;
    s16              damage;
    s32              pan;

    work                   = (Actor107600Work*)arg0->work;
    enemy                  = arg0->spawnArg2;
    *(s32*)G_SCRATCH_HEAD -= 8;
    work->field_156        = 0;
    if (Gp_FindRec18(work->obj.ctx.recs, 0) != 0) {
        for (i = 0; i < 8; i++) {
            if ((work->rec18[i].key & 0xFFFF0000) == 0x20000) {
                work->field_156                        = 1;
                ((Actor107600HitPos*)&work->pitch)->vx = work->rec18[i].at10.normal.vx;
                ((Actor107600HitPos*)&work->pitch)->vy = work->rec18[i].at10.normal.vy;
                ((Actor107600HitPos*)&work->pitch)->vz = work->rec18[i].at10.normal.vz;
                func_actor_107600_80134D9C(arg0);
                damage          = Gp_ComputeDamage(work->rec18[i].key, work->field_14C, 0, 0);
                work->field_150 = Gp_GetIdParam2(work->rec18[i].key);
                work->field_160 = damage;
                func_800DA6E8(&enemy->node, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    enemy->hp = 0;
                }
                if (damage > 0) {
                    if (work->field_16B < 8) {
                        obj = ((TmdObject*)arg0->extra)->coords;
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
    *(s32*)G_SCRATCH_HEAD += 8;
}

/// Corner offsets of the quad `func_actor_107600_80133FA8` draws.
const DVECTOR D_actor_107600_80131ED8[] = {
    { -0x100, -0x100 },
    { -0x100, 0x100 },
    { 0x100, -0x100 },
    { 0x100, 0x100 },
};

/// Same as `func_actor_107600_80134248` with a 0x200-wide square, UVs
/// 0x40..0x67 x 0..0x27 and a 0xA0 depth bias.
void func_actor_107600_80133FA8(GsCOORDINATE2* coord, SVECTOR* pos)
{
    Actor107600QuadScratch* s;
    POLY_FT4*               p;
    s32                     i;

    *(s32*)G_SCRATCH_HEAD -= sizeof(Actor107600QuadScratch);
    s                      = *(Actor107600QuadScratch**)G_SCRATCH_HEAD;
    for (i = 0; i < 4; i++) {
        s->v[i].vx = pos->vx + (D_actor_107600_80131ED8[i].vx + coord->coord.t[0]);
        s->v[i].vy = pos->vy + (D_actor_107600_80131ED8[i].vy + coord->coord.t[1]);
        s->v[i].vz = coord->coord.t[2] + pos->vz;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&s->v[0]);
    gte_rtps();
    p              = (POLY_FT4*)gGpuPrimCursor;
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
        *(s32*)G_SCRATCH_HEAD += sizeof(Actor107600QuadScratch);
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
    *(s32*)G_SCRATCH_HEAD += sizeof(Actor107600QuadScratch);
}

/// Corner offsets of the quad `func_actor_107600_80134248` draws; the
/// zero fifth entry is never read.
const DVECTOR D_actor_107600_80131EE8[] = {
    { -0x60, -0x60 },
    { -0x60, 0x60 },
    { 0x60, -0x60 },
    { 0x60, 0x60 },
    { 0, 0 },
};

/// Projects a 0xC0-wide square centred on `pos` (relative to `coord`'s
/// translation) and links it as an unshaded `POLY_FT4` on tpage 0x99,
/// dropping it when its OT depth lands too close.
void func_actor_107600_80134248(GsCOORDINATE2* coord, SVECTOR* pos)
{
    Actor107600QuadScratch* s;
    POLY_FT4*               p;
    s32                     i;

    *(s32*)G_SCRATCH_HEAD -= sizeof(Actor107600QuadScratch);
    s                      = *(Actor107600QuadScratch**)G_SCRATCH_HEAD;
    for (i = 0; i < 4; i++) {
        s->v[i].vx = pos->vx + (D_actor_107600_80131EE8[i].vx + coord->coord.t[0]);
        s->v[i].vy = pos->vy + (D_actor_107600_80131EE8[i].vy + coord->coord.t[1]);
        s->v[i].vz = coord->coord.t[2] + pos->vz;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(&s->v[0]);
    gte_rtps();
    p              = (POLY_FT4*)gGpuPrimCursor;
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
        *(s32*)G_SCRATCH_HEAD += sizeof(Actor107600QuadScratch);
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
    *(s32*)G_SCRATCH_HEAD += sizeof(Actor107600QuadScratch);
}

/// Mode 1 collapses each column of `m` to one weighted value plus a
/// `gDisplayState.loopCount`-driven sine pulse; mode 2 clears the 3x3 part.
void func_actor_107600_801344E8(void* arg0, MATRIX* m, s32 mode)
{
    s32 i;
    s16 v;

    switch (mode) {
        case 0:
            break;
        case 1:
            for (i = 0; i < 3; i++) {
                v          = (m->m[0][i] * 7 + m->m[1][i] * 6 + m->m[2][i] * 3) / 33;
                v         += (s16)(rsin(gDisplayState.loopCount * 198) + 0x1000);
                m->m[0][i] = v;
                m->m[1][i] = v;
                m->m[2][i] = v;
            }
            break;
        case 2:
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

/// Recolours the model's light matrix from the `field_4E` mode pair, blending
/// the two remaps by `field_4F` while it counts down; a copy of
/// `Gp_UpdateActorColor`.
void func_actor_107600_80134608(GpEnemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3)
{
    TmdObject*      extra;
    MATRIX*         colorMtx;
    s32             mode;
    u8*             head;
    GpColorScratch* block;
    SVECTOR*        col0;
    SVECTOR*        col1;
    GpMtxCol*       src;
    GpMtxCol*       dst;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = (TmdObject*)arg0->task->extra;
    colorMtx = extra->colorMtx;
    mode     = arg0->colorMode & 3;
    if ((!(extra->flags & 0x80) && (extra->buffer != NULL)) || (gGameSession->field_65 != 1)) {
        {
            void**                   scratch;
            register GpColorScratch* tmp asm("v0");

            scratch  = (void**)G_SCRATCH_HEAD;
            head     = *scratch;
            tmp      = (GpColorScratch*)(head - 0x30);
            block    = tmp;
            *scratch = tmp;
        }
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
            func_actor_107600_801344E8(arg0, &block->mtx, (arg0->colorMode >> 2) & 3);
            i    = 0;
            col0 = (SVECTOR*)(head - 0x10);
            col1 = (SVECTOR*)(head - 8);
            src  = (GpMtxCol*)colorMtx;
            w0   = (s8)arg0->colorBlend << 8;
            dst  = (GpMtxCol*)block;
            w1   = 0x1000 - w0;
            do {
                block->col0.vx = src->x;
                TOUCH_REG(src);
                block->col0.vy = src->y;
                TOUCH_REG(src);
                block->col0.vz = src->z;
                block->col1.vx = dst->x;
                TOUCH_REG(dst);
                block->col1.vy = dst->y;
                TOUCH_REG(dst);
                block->col1.vz = dst->z;
                gte_lddp(w1);
                gte_ldsv(col0);
                gte_gpf12();
                gte_lddp(w0);
                gte_ldsv(col1);
                gte_gpl12();
                gte_stsv(col0);
                src->x = block->col0.vx;
                dst    = (GpMtxCol*)&dst->_0;
                src->y = block->col0.vy;
                i++;
                src->z = block->col0.vz;
                src    = (GpMtxCol*)&src->_0;
            } while (i < 3);
            if (D_801153F4 == 0) {
                arg0->colorBlend--;
            }
        }
        *(u8**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
    }
}

void func_actor_107600_801348A0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E74;
    sp.funcs[arg0->state](arg0);
}

void func_actor_107600_80134904(Task* arg0)
{
    TmdObject* obj = arg0->extra;

    obj->flags |= 0x80;
}

void func_actor_107600_80134920(Task* arg0)
{
    Gp_UnlinkObj(&((Actor107600Work*)arg0->work)->obj);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Links this actor's display node the way `func_8010C980` does for the
/// gameplay objects: the node's collision table is the `GpRec18` run at
/// `work->rec18` (count 8), and its `field_1C` payload is the spawn variant's
/// height, 0x220 for the `field_162 == 1` variant and 0x190 otherwise.
void func_actor_107600_80134958(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    GpRec18*         rec   = work->rec18;

    work->obj.coord    = coord;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = -0x250;
    work->obj.pos.vz   = 0;
    work->obj.key      = 0x3004C;
    work->obj.radius   = (work->field_162 == 1) ? 0x220 : 0x190;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off `G_SCRATCH_HEAD` and hands it to
/// `func_actor_107600_80134608` with no blend parameters.
void func_actor_107600_801349E0(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = arg0->spawnArg2;
    coord     = ((TmdObject*)arg0->extra)->coords;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    func_actor_107600_80134608(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Builds a second rotation from the work block's angle trio at +0x50: wrap
/// each to 12 bits, lay an unscaled `MATRIX` down at the scratchpad head, apply
/// `Gfx_RotMatrixZ`/`X`/`Y` to it in that order, copy its 3x3 into the model's
/// `GsCOORDINATE2::coord` through `func_actor_107600_80134B2C`, and hand the
/// scratch block back.
void func_actor_107600_80134A50(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    MATRIX*          m;

    work->field_50           &= 0xFFF;
    work->field_52           &= 0xFFF;
    work->field_54           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    Gfx_RotMatrixZ(m, (s16)work->field_54, 0);
    Gfx_RotMatrixX(m, (s16)work->field_50, 0);
    Gfx_RotMatrixY(m, (s16)work->field_52, 0);
    func_actor_107600_80134B2C(m, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
}

/// Copies the 3x3 rotation of `src` into `dst`, leaving `dst`'s translation row
/// alone. The actor carries this body twice; the other copy is
/// `func_actor_107600_80132C4C`.
void func_actor_107600_80134B2C(MATRIX* src, MATRIX* dst)
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

void func_actor_107600_80134B98(Task* arg0, s16 arg1)
{
    Actor107600Work* work = arg0->work;

    work->field_158 = arg1;
    work->field_15A = 0;
}

/// Applies the transition `work->field_15E` queues once `field_156` is 1:
/// requests 1..5 open states 2, 3, 4, 6 and 5 through the same stores as
/// `func_actor_107600_80134B98` (written out, since the setter is not
/// inlined). The request is always consumed; returns whether one was pending.
s32 func_actor_107600_80134BAC(Task* arg0)
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
/// roll `Gp_LcgState` into `field_16A` beside the 10 percent scale pair
/// `func_actor_107600_80134EF4` divides the model root's rotation by, rebuild
/// that rotation through `func_actor_107600_80134A50`, and put the spawned
/// object's light into mode 2 with its blend timer cleared. Variant 2 only
/// starts the state at 8 and leaves the scale pair at 100 percent.
void func_actor_107600_80134C54(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->work;
    GpObj4C*         obj  = arg0->spawnArg2;

    switch (work->field_162) {
        case 0:
        case 1:
            work->field_158 = 1;
            work->field_50  = 0x400;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_16A = (Gp_LcgState >> 16) & 7;
            work->field_168 = 10;
            work->field_169 = 10;
            func_actor_107600_80134A50(arg0);
            Gp_SetLightMode(obj, 2);
            obj->field_4F = 0;
            break;
        case 2:
            work->field_158 = 8;
            work->field_168 = 100;
            work->field_169 = 100;
            break;
    }
}

void func_actor_107600_80134D10(Task* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D30(Task* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D50(Task* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D70(Task* arg0)
{
    ((Actor107600Work*)arg0->work)->field_16B = 3;
    func_actor_107600_80134B98(arg0, 7);
}

/// Measures the XZ offset from this model's own attach coordinate to the one on
/// the `Gp_ActorSlots[0]` actor's model, in a 0x10-byte `VECTOR` carved off
/// `G_SCRATCH_HEAD` the way `func_actor_107600_80134E5C` carves its block, and
/// leaves the distance in `Actor107600Work.field_14C`. With no slot-0 actor the
/// carve is undone and nothing is measured. The distance is only stored once the
/// scratch block has been handed back, which is the order the original compiled
/// in - moving the store up costs a nop after the reload.
void func_actor_107600_80134D9C(Task* arg0)
{
    Actor107600Work* work;
    GsCOORDINATE2*   self;
    GsCOORDINATE2*   target;
    void**           scratch;
    u8*              head;
    VECTOR*          block;
    s32              dist;

    work     = (Actor107600Work*)arg0->work;
    self     = ((TmdObject*)arg0->extra)->coords;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (VECTOR*)(head - 0x10);
    *scratch = block;
    if (Gp_ActorSlots[0] == NULL) {
        *scratch = head;
        return;
    }
    target          = ((TmdObject*)Gp_ActorSlots[0]->extra)->coords;
    block->vx       = target->coord.t[0] - self->coord.t[0];
    block->vy       = target->coord.t[1] - self->coord.t[1];
    block->vz       = target->coord.t[2] - self->coord.t[2];
    dist            = func_80103D8C(block->vx, block->vz);
    *scratch        = (u8*)*scratch + 0x10;
    work->field_14C = dist;
}

/// Rotates a fixed 0x10-byte offset by the coordinate's own `coord` matrix and
/// leaves the result in that matrix's translation row. The offset is carved off
/// `G_SCRATCH_HEAD` the way `func_actor_107600_80132B0C` carves its VECTOR, but
/// is filled with (0, -0x180, 0) and rotated in place by `ApplyMatrixLV`, which
/// also folds in the matrix's existing translation. `func_actor_107600_80132ED0`
/// calls this on the coordinate it then hands to `Gp_UpdateCoord`.
void func_actor_107600_80134E5C(GsCOORDINATE2* arg0)
{
    void**  scratch;
    u8*     head;
    VECTOR* block;

    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    *scratch  = block;
    block->vx = 0;
    block->vy = -0x180;
    block->vz = 0;
    ApplyMatrixLV(&arg0->coord, block, block);
    arg0->coord.t[0] = block->vx;
    arg0->coord.t[1] = block->vy;
    *scratch         = (u8*)*scratch + 0x10;
    arg0->coord.t[2] = block->vz;
}

/// Scales the model root's rotation by the two percent factors
/// `func_actor_107600_80134C54` rolls into the work block: the diagonal
/// `coord.m[0][0]` and `coord.m[2][1]` halves, each read as a raw 16-bit value
/// and re-signed before the divide so the scale stays signed.
void func_actor_107600_80134EF4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->coords;
    u16              x     = coord->coord.m[0][0];
    u16              y     = coord->coord.m[2][1];

    coord->coord.m[0][0] = (s16)x / 100 * work->field_168;
    coord->coord.m[2][1] = (s16)y / 100 * work->field_169;
}
