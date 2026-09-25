#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include <psyq/libgs.h>

/// 0x6E4-byte work block hung off `Task::work`, allocated by
/// `Actor05700_Fn03CC4` in both actor_105700 and actor_205700. It opens with the animation context and its
/// nineteen 0x28-byte slots, exactly like the `Actor02000Work` block of
/// `actor_102000`; the animation/state halfwords around 0x694-0x6E0 keep that
/// block's offsets and meaning.
typedef struct Actor105700Work {
    /* 0x000 */ GpAnimCtx  ctx;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C;
    /* 0x45C */ MATRIX     field_45C;
    /// First body object, handed to `Gp_UnlinkObj` by the teardown of
    /// `Actor05700_Fn01A58`; its `pos.vz` is the pose the state-0
    /// branch of `Actor05700_Fn01318` parks (-0xA7 or 0x109) and its
    /// `radius` the frame count parked alongside it.
    /* 0x47C */ GpObj        field_47C;
    /* 0x49C */ GpActorD4Rec field_49C;
    /* 0x4B4 */ GpRec18      field_4B4[1];
    /// Second body object; `pos.vz` is the pose the state-0 branch parks
    /// (0x15E) and `flags` the bits whose 0x4000 it raises.
    /* 0x4CC */ GpObj   field_4CC;
    /* 0x4EC */ GpRec18 field_4EC[5];
    /// Third body object; `flags` is the field whose bit 0x4000 the state-0
    /// branch clears.
    /* 0x564 */ GpObj   field_564;
    /* 0x584 */ GpRec18 field_584[4];
    /// Fourth body object: `key` is the object `Gp_PackPair` hands it when
    /// `field_698` first reaches the animation's 0x1C mark and `flags` the
    /// bits whose 0x8000 is raised with it and dropped at the 0x28 mark
    /// (`Actor05700_Fn023AC`).
    /* 0x5E4 */ GpObj   field_5E4;
    /* 0x604 */ GpRec18 field_604[1];
    /// Fifth body object, unlinked with the others by `Actor05700_Fn01A58`.
    /* 0x61C */ GpObj        field_61C;
    /* 0x63C */ GpActorD4Rec field_63C;
    /* 0x654 */ GpRec18      field_654[1];
    /* 0x66C */ TaskDesc*    field_66C;
    /* 0x670 */ GpEffArg     field_670;
    /* 0x678 */ s32          field_678;
    /* 0x67C */ s32          field_67C;
    /* 0x680 */ s32          field_680;
    /* 0x684 */ byte         pad_684[4];
    /// Tilt angles decayed toward zero by `Actor05700_Fn016D0`.
    /* 0x688 */ SVECTOR           field_688;
    /* 0x690 */ struct GpEffWork* field_690;
    /// Animation index selected by the state machine; 4 is the "handover"
    /// clip of `Actor05700_Fn04CC0`'s state 0.
    /* 0x694 */ s16 field_694;
    /// Animation the playing clip was started from; when it differs from
    /// `field_694` the frame counter is reset and the slots reseeded.
    /* 0x696 */ s16 field_696;
    /* 0x698 */ s16 field_698; ///< current frame of the playing clip
    /* 0x69A */ s16 field_69A;
    /* 0x69C */ s16 field_69C; ///< dwell counter, cleared on state 0 entry
    /* 0x69E */ s16 field_69E; ///< dwell counter, cleared on state 0 entry
    /* 0x6A0 */ u16 field_6A0; ///< sound flags; bit 5/4 gate the two cues
                               /// Current yaw, walked toward `field_6A4` by
                               /// `Actor05700_Fn01544`, using `field_69E` as the per-frame step.
    /* 0x6A2 */ s16 field_6A2;
    /* 0x6A4 */ s16 field_6A4; ///< yaw the actor wants to face
    /* 0x6A6 */ s16 field_6A6; ///< parked animation for the state-F0 path
    /* 0x6A8 */ s16 field_6A8; ///< state-machine step
    /* 0x6AA */ s16 field_6AA; ///< animation the state-0 branch picks
    /* 0x6AC */ s16 field_6AC;
    /* 0x6AE */ s16 field_6AE; ///< state-0 frame budget
    /* 0x6B0 */ s16 field_6B0;
    /* 0x6B2 */ s16 field_6B2; ///< non-zero forces the state-F0 path
    /* 0x6B4 */ s16 field_6B4; ///< cleared once the tilt has settled
    /* 0x6B6 */ s16 field_6B6;
    /// State-0 branch selector: 1 picks the short dwell and animation 1,
    /// 2 the long dwell and animation 2.
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ s16  field_6BA;
    /* 0x6BC */ s16  field_6BC;
    /* 0x6BE */ s16  field_6BE;
    /* 0x6C0 */ s16  field_6C0;
    /* 0x6C2 */ s16  field_6C2;
    /* 0x6C4 */ s16  field_6C4;
    /* 0x6C6 */ byte pad_6C6[4];
    /// Body variant select: `Actor05700_Fn01A58` drops the fifth body
    /// object for the two values 0x38 / 0x39 and hands the halfword to
    /// `Gp_ReleaseStateF0Add`.
    /* 0x6CA */ s16 field_6CA;
    /* 0x6CC */ s16 field_6CC;
    /* 0x6CE */ s16 field_6CE;
    /* 0x6D0 */ s16 field_6D0;
    /// Spawn state driven by `Actor05700_Fn05310`: 0 clears the
    /// coordinate, 1 fires the effect burst and sound cue, 2 is idle.
    /* 0x6D2 */ s16 field_6D2;
    /// Latched on state-0 entry, cleared when the frame budget runs out.
    /* 0x6D4 */ s16  field_6D4;
    /* 0x6D6 */ s16  field_6D6; ///< animation index, used as a table row
    /* 0x6D8 */ byte pad_6D8[2];
    /* 0x6DA */ s16  field_6DA; ///< state-0 frame budget, drained by `field_69C`
    /* 0x6DC */ s16  field_6DC;
    /// State-1 step gate: 1 while the state-0 exit is still to be seen, 2
    /// once it has been.
    /* 0x6DE */ s16 field_6DE;
    /// State-1 branch selector: zero picks the short dwell and animation 2,
    /// non-zero the long dwell and animation 0x14.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor105700Work;
STATIC_ASSERT_SIZEOF(Actor105700Work, 0x6E4);

extern u8 D_801153F2;

/// 0x40-byte scratch carved off `G_SCRATCH_HEAD` by `Actor05700_Fn000B0`.
typedef struct Actor105700HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         normal;
    /* 0x20 */ VECTOR         push;
    /* 0x30 */ SVECTOR        effOfs;
    /* 0x38 */ SVECTOR        target;
} Actor105700HitScratch;
STATIC_ASSERT_SIZEOF(Actor105700HitScratch, 0x40);

/// 0x14-byte placement descriptor in the overlay's `.data`, handed to
/// `Gp_PackPair` as the source of the body objects' `GpObj.key`.
/// `field_E` is the variant flag `Actor05700_Fn031BC` latches into its
/// work block: it is 1 (the table's own value is 2) when the actor is placed
/// normally, and anything else puts the body in the other pose.
typedef struct Actor105700PlaceSrc {
    /* 0x00 */ GpU16Pair pair;
    /* 0x04 */ u16       field_4;
    /* 0x06 */ u16       field_6;
    /* 0x08 */ u16       field_8;
    /* 0x0A */ u16       field_A;
    /* 0x0C */ u16       field_C;
    /* 0x0E */ u16       field_E;
    /* 0x10 */ u16       field_10;
    /* 0x12 */ u16       field_12;
} Actor105700PlaceSrc;
STATIC_ASSERT_SIZEOF(Actor105700PlaceSrc, 0x14);

/// `G_SCRATCH_HEAD` viewed as a struct. The member access, not a plain `u32`
/// dereference, is what `Actor05700_Fn016D0` needs to schedule its
/// argument setup around `RotMatrix`.
typedef struct Actor105700ScratchStack {
    u32 sp;
} Actor105700ScratchStack;

/// 0x38-byte scratch carved off `G_SCRATCH_HEAD` by
/// `Actor05700_Fn031BC`. `rot` first holds the local offset the root
/// coordinate is translated by (through `gte_rtv0` into `pos`), then the
/// placement angles `RotMatrix` turns into `mtx` for the three `rtir` column
/// transforms that overwrite the root coordinate's matrix.
typedef struct Actor105700PlaceScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ VECTOR  pos;
    /* 0x18 */ MATRIX  mtx;
} Actor105700PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor105700PlaceScratch, 0x38);

/// 0x40-byte scratch carved off `G_SCRATCH_HEAD` by
/// `Actor05700_Fn02554`: the converted matrix, the `gte_rtv0` output
/// and the two vectors fed through it (`rot` and `vec` are also the pair
/// handed to `Actor05700_Fn0295C`).
typedef struct Actor105700AimScratch {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ VECTOR  pos;
    /* 0x30 */ SVECTOR rot;
    /* 0x38 */ SVECTOR vec;
} Actor105700AimScratch;
STATIC_ASSERT_SIZEOF(Actor105700AimScratch, 0x40);

/// 0x48-byte scratch carved off `G_SCRATCH_HEAD` by
/// `Actor05700_Fn0295C`: the beam is walked in eight steps from `vec`
/// to `rot`, each step projected into `cur` (packed screen xy) and `curZ`
/// (OTZ). `xs`/`ys` hold the two projected ends followed by the four
/// offset corners the ribbon polygons are cut from.
typedef struct Actor105700BeamScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR pt;
    /* 0x18 */ SVECTOR step;
    /* 0x20 */ s32     prev;
    /* 0x24 */ s32     cur;
    /* 0x28 */ s32     prevZ;
    /* 0x2C */ s32     curZ;
    /* 0x30 */ s16     xs[6];
    /* 0x3C */ s16     ys[6];
} Actor105700BeamScratch;
STATIC_ASSERT_SIZEOF(Actor105700BeamScratch, 0x48);

/// 0xF0-byte body block `Actor05700_Fn031BC` parks at `Task::work`.
/// The two leading matrices are the light/colour pair published on the model
/// root's `TmdObject`; the three `GpObj` bodies collide against `rec60`
/// (shared by the first two) and, through the `GpActorD4Rec` between them,
/// `recD0`. `field_EE` mirrors the placement table's variant flag.
typedef struct Actor105700FxWork {
    /* 0x00 */ MATRIX       colorMtx;
    /* 0x20 */ MATRIX       lightMtx;
    /* 0x40 */ GpObj        obj40;
    /* 0x60 */ GpRec18      rec60[1];
    /* 0x78 */ GpObj        obj78;
    /* 0x98 */ GpObj        obj98;
    /* 0xB8 */ GpActorD4Rec d4rec;
    /* 0xD0 */ GpRec18      recD0[1];
    /// Frame counter: paces the effect puffs while ticking (wraps at 4), then
    /// counts the teardown's wait before the child is destroyed.
    /* 0xE8 */ s16 field_E8;
    /// Frame count; the burst ends the cycle at 0x5A.
    /* 0xEA */ s16 field_EA;
    /// Teardown step of `Actor05700_Fn051D8`: 0 unlinks the bodies, 1 waits.
    /* 0xEC */ s16 field_EC;
    /* 0xEE */ s16 field_EE;
} Actor105700FxWork;
STATIC_ASSERT_SIZEOF(Actor105700FxWork, 0xF0);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void Actor05700_Fn00D08(Task* arg0);
void Actor05700_Fn02554(Task* arg0);
void Actor05700_Fn0295C(Task* arg0, SVECTOR* arg1, SVECTOR* arg2);

void Actor05700_Fn0509C(GpEnemy* arg0, Task* task);
void Actor05700_Fn050E0(GpEnemy* enemy, Task* task);
void Actor05700_Fn051D8(GpEnemy* arg0, Task* arg1);
void Actor05700_Fn052CC(GpEnemy* arg0, Task* task);
void Actor05700_Fn05310(GpEnemy* arg0, Task* arg1);

/// Sound ids this actor's cues play, indexed by `Actor105700Work.field_6D6`
/// (row `field_6D6` starts at the second word, the `- 1` in the body).
extern s32 Actor05700_D171E4[];

/// Per-animation frame marks: row `field_694` holds the frame the 0x1C, 0x28
/// and 0x7A marks of `Actor05700_Fn023AC` are measured from.
extern s16 Actor05700_D054CC[];

/// The body objects' variant flag comes from `Actor05700_D170F4`.
extern Actor105700PlaceSrc Actor05700_D170F4;

/// Set while the player is being grabbed; forces this actor's approach cycle
/// into its handover animation.
extern s8 D_80115419;

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 Actor05700_D17118[];
extern s16 Actor05700_D17174[];

s32 Actor05700_Fn04BB4(SVECTOR* arg0, SVECTOR* arg1);

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void Actor05700_Fn000B0(Task* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor105700Work*       work;
    GpDeltaScratch*        head;
    Actor105700HitScratch* scratch;
    GpEnemy*               enemy;
    GsCOORDINATE2*         self;
    GsCOORDINATE2*         other;
    GsCOORDINATE2*         part;
    s32                    i;
    s32                    x, y, z;
    s32                    damage;
    s32                    kind;
    s32                    dz;
    s32                    clamped;
    s32                    val;
    s32                    push;
    s16                    cooldown;
    u32                    rng;
    s32                    tilt;
    s32                    byte1;

    result                                    = 0;
    maxPush                                   = 0;
    hit                                       = 0;
    lastId                                    = 0;
    work                                      = arg0->work;
    head                                      = *(GpDeltaScratch**)G_SCRATCH_HEAD;
    self                                      = ((TmdObject*)arg0->extra)->coords;
    *(Actor105700HitScratch**)G_SCRATCH_HEAD -= 1;
    scratch                                   = *(Actor105700HitScratch**)G_SCRATCH_HEAD;
    enemy                                     = (GpEnemy*)arg0->spawnArg2;

    switch (func_800E0C10(work->field_584, head - 4, 4, NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-4].vx.h.hi;
            self->coord.t[1] += scratch->delta.vy.h.hi;
            self->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            self->coord.t[0] = work->field_678;
            self->coord.t[1] = work->field_67C;
            self->coord.t[2] = work->field_680;
            break;
    }
    Gp_ClearRec18Occupied(work->field_584);

    if (work->field_4CC.flags & 0x4000) {
        switch (func_800E0C10(work->field_4EC, &scratch->delta, 5, NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.vx.h.hi;
                self->coord.t[2] += scratch->delta.vz.h.hi;
                break;
            case 2:
                self->coord.t[0] = work->field_678;
                self->coord.t[2] = work->field_680;
                break;
        }
    }

    if (work->field_69A != 0) {
        if (--work->field_69A <= 0) {
            work->field_69A = 0;
        }
    }

    for (i = 0; i < 5; i++) {
        switch ((u32)work->field_4EC[i].key >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_69A != 0) {
                    break;
                }
                other               = ((TmdObject*)Gp_ActorSlots[((u32)work->field_4EC[i].key >> 7) & 1]->extra)->coords;
                scratch->delta.vx.w = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vy.w = other->coord.t[1] - self->coord.t[1];
                dz                  = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vz.w = dz;
                val                 = (scratch->delta.vx.w * self->coord.m[0][2]) + (scratch->delta.vy.w * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->field_6AA     = val >= 0;
                damage              = Gp_ComputeDamage(work->field_4EC[i].key,
                                                       SquareRoot0((scratch->delta.vx.w * scratch->delta.vx.w) + (scratch->delta.vy.w * scratch->delta.vy.w) + (scratch->delta.vz.w * scratch->delta.vz.w)),
                                                       0, 0);
                kind                = Gp_GetIdParam0(work->field_4EC[i].key);
                if (work->field_6CE != 0 && work->field_6AA == 1 && work->field_6B8 == 0) {
                    if (work->field_4EC[i].key & 0x8000) {
                        if (Actor05700_D17174[work->field_4EC[i].key & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (Actor05700_D17118[work->field_4EC[i].key & 0x7F] != 0) {
                        hit              = 1;
                        work->field_6D0 -= damage;
                    }
                    if (hit == 1) {
                        if (work->field_6D0 <= 0) {
                            work->field_6A6        = 9;
                            work->field_6CE        = 0;
                            work->field_6D2        = 1;
                            work->field_6A8        = 0;
                            work->field_5E4.flags &= 0x7FFF;
                            if (work->field_690 != NULL) {
                                work->field_690->task->state = 3;
                                work->field_690              = NULL;
                            }
                        }
                        func_800DA6E8(&enemy->node, 0, 0);
                        cooldown = Gp_GetIdParam2(work->field_4EC[i].key);
                        if (cooldown > 0) {
                            work->field_69A = cooldown;
                        }
                        break;
                    }
                } else {
                    work->field_6CE = 0;
                    if ((kind & 0xFFFF) == 5) {
                        damage *= 2;
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 2, NULL);
                    }
                }
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(0x6009C, &((TmdObject*)arg0->extra)->coords[3], 0, NULL);
                    }
                    if (work->field_6E0 == 0) {
                        result = 1;
                    }
                }
                if (work->field_6C4 != 0 && (work->field_4EC[i].key & 0x8000)) {
                    damage >>= 2;
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->field_4EC[i].key, damage, 0);
                enemy->hp -= damage;
                if (enemy->hp <= 0) {
                    if (work->field_6B8 == 0) {
                        result = 5;
                    } else {
                        result = 6;
                    }
                } else if (enemy->hp < enemy->param->hpMax * 15 / 100) {
                    if (work->field_6B8 == 0) {
                        result = 3;
                    } else {
                        result = 4;
                    }
                }
                if (work->field_6CC != 0 || work->field_6C2 != 0) {
                    work->field_6B6 += damage;
                }
                switch (kind & 0xFFFF) {
                    case 1:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3 && work->field_6E0 == 0) {
                            result = 2;
                        }
                        break;
                    case 2:
                        if (work->field_6C4 == 0 && work->field_6B8 == 0 && result < 3) {
                            Gp_SetObjFlag2(enemy, work->field_4EC[i].key, 0);
                            result = 1;
                        }
                        break;
                    case 0:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        break;
                }
                if (lastId != work->field_4EC[i].key) {
                    lastId             = work->field_4EC[i].key;
                    scratch->effOfs.vx = 0;
                    scratch->effOfs.vy = 0;
                    scratch->effOfs.vz = (work->field_6AA == 1) ? 0x12C : -0x96;
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key) & 0xFFFF, &((TmdObject*)arg0->extra)->coords[3],
                                  &scratch->effOfs, &work->field_670);
                }
                cooldown = Gp_GetIdParam2(work->field_4EC[i].key);
                if (cooldown > 0) {
                    work->field_69A = cooldown;
                }
                switch (result) {
                    case 0:
                        if (work->field_6A6 < 2) {
                            work->field_694 = 2;
                            work->field_6A6 = 2;
                            work->field_6A8 = 0;
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        rng         = Gp_LcgState >> 16;
                        tilt        = (rng & 0x7F) + 0x40;
                        if (!(rng & 1)) {
                            tilt = -tilt;
                        }
                        work->field_688.vx = tilt;
                        byte1              = (s16)rng >> 8;
                        val                = (byte1 & 0x7F) + 0x40;
                        if (!(byte1 & 1)) {
                            val = -val;
                        }
                        work->field_688.vy = val;
                        work->field_6B4    = 1;
                        break;
                    case 1:
                        work->field_6A6        = 8;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 2:
                        work->field_6A6        = 9;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 3:
                        work->field_6A6        = 0xB;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 4:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xC;
                            work->field_6A8 = 0;
                        }
                        break;
                    case 5:
                        work->field_6A6        = 0xD;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= 0x7FFF;
                        break;
                    case 6:
                        if (work->field_6D4 == 0) {
                            work->field_6A6 = 0xE;
                            work->field_6A8 = 0;
                        }
                        break;
                }
                if (result != 0 && work->field_690 != NULL) {
                    work->field_690->task->state = 3;
                    work->field_690              = NULL;
                }
                break;
            case 3:
                part                = &((TmdObject*)arg0->extra)->coords[3];
                x                   = part->workm.t[0] - work->field_4EC[i].point.vx;
                scratch->delta.vx.w = x;
                y                   = part->workm.t[1] - work->field_4EC[i].point.vy;
                scratch->delta.vy.w = y;
                z                   = part->workm.t[2] - work->field_4EC[i].point.vz;
                scratch->delta.vz.w = z;
                push                = work->field_4EC[i].depth - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped             = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, &scratch->push);
                }
                break;
        }
    }

    if (maxPush > 0) {
        self->coord.t[0] += (maxPush * scratch->push.vx) >> 12;
        self->coord.t[2] += (maxPush * scratch->push.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->field_4EC);
    if (work->field_604[0].flags & 1) {
        work->field_5E4.flags &= 0x7FFF;
        Gp_ClearRec18Occupied(work->field_604);
    }
    work->field_6B2 = 0;
    if (Gp_CountRec18Hi(work->field_4B4, 0x10000) != 0) {
        part               = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[4];
        scratch->effOfs.vx = part->workm.t[0];
        scratch->effOfs.vy = part->workm.t[1];
        scratch->effOfs.vz = part->workm.t[2];
        scratch->target.vx = self->workm.t[0];
        scratch->target.vy = self->workm.t[1];
        scratch->target.vz = self->workm.t[2];
        if (Actor05700_Fn04BB4(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x40;
}

/// Per-frame tick of the approach cycle, sharing the `field_6A8` state with
/// `Actor05700_Fn023AC` and `Actor05700_Fn04CC0`; the same body
/// as `Actor02000_Fn00AEC` of `actor_102000` (see `overlay_dup_index.py find
/// Actor05700_Fn00B24`). State 0 drains the `field_6DA` budget by
/// `field_69C` (0 while `field_698` is under the per-animation entry of
/// `Actor05700_D054CC`, 0x14 once past it) and runs the proximity cue
/// every frame; when the budget runs out it switches to animation 4 and state
/// 1. State 1 waits for `field_698` to reach 0x60, then either falls back to
/// animation 2 (budget left) or starts the lunge: animation 3, state 2, a fresh
/// budget of 1000 per unit of the placement record's `variant`, and `field_6A2` /
/// `field_6A4` set to the actor's current yaw and its opposite. State 2 holds
/// `field_69E` at 0x3B until `field_698` reaches 0x23, then returns to animation
/// 2 and state 0. A set `field_6B2` or `D_80115419` overrides everything with
/// animation 2 and the shared state-F0 slot.
void Actor05700_Fn00B24(Task* arg0)
{
    GpEnemy*         spawn;
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    u8*              head;
    s16              state;
    s16              delta;
    s32              ang;
    s32              param;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;

    self  = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    spawn = arg0->spawnArg2;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= Actor05700_D054CC[work->field_694]) {
                delta = 0x14;
            }
            work->field_69C  = delta;
            work->field_69E  = 0;
            work->field_6DA -= work->field_69C;
            if (work->field_6DA <= 0) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
                work->field_69C = 0;
            }
            Actor05700_Fn00D08(arg0);
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x60) {
                if (work->field_6DA <= 0) {
                    param           = spawn->place->variant;
                    work->field_694 = 3;
                    work->field_6A8 = 2;
                    work->field_6DA = param * 1000;
                    ang             = ratan2(self->coord.m[0][2], self->coord.m[2][2]) & 0xFFF;
                    work->field_6A2 = ang;
                    work->field_6A4 = (ang + 0x800) & 0xFFF;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }

    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Proximity cue; the same body as `Actor02000_Fn00CD0` of `actor_102000`
/// (see `overlay_dup_index.py find Actor05700_Fn00D08`). Carves a
/// 0x10-byte direction vector off the scratch head, aims it from the player
/// at the actor's root coordinate, and takes its length through
/// `SquareRoot0`: under 0x5DC one of `D_801153F2`'s bit groups raises
/// `field_6B2`; past it the other two (the second only within 0xBB8) put the
/// actor into animation 4 and state 1.
void Actor05700_Fn00D08(Task* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    s32              dx;
    s32              distance;
    s32              dz;
    s32              trigger;
    VECTOR*          head;
    VECTOR*          delta;

    self                      = ((TmdObject*)arg0->extra)->coords;
    work                      = arg0->work;
    head                      = *(VECTOR**)G_SCRATCH_HEAD;
    delta                     = head - 1;
    head[-1].vx               = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
    delta->vy                 = 0;
    dz                        = Player_Status.coordMtx->t[2] - self->coord.t[2];
    delta->vz                 = dz;
    dx                        = head[-1].vx;
    trigger                   = 0;
    *(VECTOR**)G_SCRATCH_HEAD = delta;
    distance                  = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x5DC) {
        if (D_801153F2 & 0x17) {
            work->field_6B2 = 1;
        }
    } else {
        if (D_801153F2 & 5) {
            trigger = 1;
        }
        if ((D_801153F2 & 0x12) && (distance < 0xBB8)) {
            trigger = 1;
        }
        if (trigger != 0) {
            work->field_694 = 4;
            work->field_69C = 0;
            work->field_69E = 0;
            work->field_6AE = 0;
            work->field_6A8 = 1;
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// The approach-cycle driver: state 0 picks the side from `field_6AA`, states
/// 1 and 2 cue sounds at fixed frames before rolling a dwell, and states 3 and
/// 4 alternate idles until it runs out. The same body as `Actor02000_Fn00E0C`.
void Actor05700_Fn00E44(Task* arg0)
{
    s16              state;
    s16              nextAnim;
    s16              nextAnim2;
    s32              snd;
    s32              random3;
    s32              pan;
    s32              pan2;
    s32              pan3;
    u16              timer;
    u16              timer2;
    u32              random;
    u32              random2;
    Actor105700Work* work;
    GsCOORDINATE2*   self;

    work  = arg0->work;
    self  = ((TmdObject*)arg0->extra)->coords;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694        = 0x16;
                work->field_6A8        = 1;
                work->field_6B8        = 1;
                work->field_4CC.pos.vz = -0xA7;
            } else {
                work->field_694        = 0x1A;
                work->field_6A8        = 2;
                work->field_6B8        = 2;
                work->field_4CC.pos.vz = 0x109;
            }
            work->field_4CC.radius                     = 0x15E;
            work->field_69C                            = 0;
            work->field_69E                            = 0;
            work->field_6DE                            = 1;
            work->field_4CC.flags                      = (u16)(work->field_4CC.flags | 0x4000);
            work->field_564.flags                      = (u16)(work->field_564.flags & 0xBFFF);
            ((GpEnemy*)arg0->spawnArg2)->reactionFlags = 0;
            work->field_6D4                            = 1;
            break;
        case 1:
            if (work->field_698 == 0x14) {
                snd = Actor05700_D171E4[work->field_6D6 + 0xC] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = Actor05700_D171E4[work->field_6D6 + 8] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan2, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 >= 0x42) {
                work->field_694 = 0x19;
                work->field_6D4 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_6AE = (u16)((random >> 0x10) & 0x3F);
                Gp_LcgState     = (s32)random;
                if (((GpEnemy*)arg0->spawnArg2)->hp > 0) {
                    work->field_6A8 = 3;
                } else {
                    arg0->state     = 2;
                    work->field_6A8 = 0;
                }
            }
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
                break;
            }
            break;
        case 2:
            if (work->field_698 == 0x19) {
                snd  = Actor05700_D171E4[work->field_6D6 + 8] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan3 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan3, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 >= 0x31) {
                work->field_694 = 0x1D;
                work->field_6D4 = 0;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_6AE = (u16)((random2 >> 0x10) & 0x3F);
                Gp_LcgState     = (s32)random2;
                if (((GpEnemy*)arg0->spawnArg2)->hp > 0) {
                    work->field_6A8 = 3;
                } else {
                    arg0->state     = 2;
                    work->field_6A8 = 0;
                }
            }
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            break;
        case 3:
            timer           = work->field_6AE - 1;
            work->field_6AE = timer;
            if ((s16)timer <= 0) {
                nextAnim = 0x1C;
                if (work->field_6B8 == 1) {
                    nextAnim = 0x18;
                }
                work->field_6AE = 0xAU;
                work->field_694 = nextAnim;
                work->field_6A8 = 4;
                break;
            }
            break;
        case 4:
            timer2          = work->field_6AE - 1;
            work->field_6AE = timer2;
            if ((s16)timer2 <= 0) {
                nextAnim2 = 0x1D;
                if (work->field_6B8 == 1) {
                    nextAnim2 = 0x19;
                }
                work->field_694 = nextAnim2;
                work->field_6A8 = 3;
                random3         = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random3;
                work->field_6AE = (u16)(((u32)random3 >> 0x10) & 0x3F);
            }
            break;
    }
}

/// The `field_6A8` state shared by the approach-cycle ticks; state 0 arms the
/// dwell the `field_6B8` selector picks, and states 1 and 2 wait for
/// `field_698` to reach 0x10 / 0x16 before parking animation 0x19 / 0x1D and
/// rolling `Gp_LcgState` into the `field_6AE` budget. The same body as
/// `Actor02000_Fn011E8` of `actor_102000`.
void Actor05700_Fn01220(Task* arg0)
{
    Actor105700Work* work;
    s16              state;
    s32              next;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                work->field_694 = 0x19;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                work->field_694 = 0x1D;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_6AE = ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            break;
    }
}

/// Per-frame tick of the actor's approach cycle, the verbatim counterpart of
/// `Actor02000_Fn012E0` of `actor_102000` (see `overlay_dup_index.py find
/// Actor05700_Fn01318`, which also lists five more actors carrying it).
/// State 0 arms the cycle: `field_6AA` picks the dwell and animation, and the
/// pose `field_4CC.field_14`, the flags `field_4CC.flags` / `field_564.flags`
/// and the step gate are all set before state 1 takes over. State 1 gates the
/// handover once through `field_6DE`, plays the cue of the animation `field_6B8`
/// selects at its 0x14 / 0x2C frame mark, and drops back to state 0 when the
/// `field_6AE` frame budget runs out.
void Actor05700_Fn01318(Task* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    s32              snd;
    s16              state;

    work  = arg0->work;
    self  = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    state = work->field_6A8;

    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694        = 0x16;
                work->field_6A8        = 1;
                work->field_6B8        = 1;
                work->field_6AE        = 0x42;
                work->field_4CC.pos.vz = -0xA7;
            } else {
                work->field_694        = 0x1A;
                work->field_6A8        = 1;
                work->field_6B8        = 2;
                work->field_6AE        = 0x31;
                work->field_4CC.pos.vz = 0x109;
            }
            work->field_4CC.radius                     = 0x15E;
            work->field_69C                            = 0;
            work->field_69E                            = 0;
            work->field_6DE                            = 1;
            work->field_4CC.flags                     |= 0x4000;
            work->field_564.flags                     &= 0xBFFF;
            ((GpEnemy*)arg0->spawnArg2)->reactionFlags = 0;
            work->field_6D4                            = 1;
            break;
        case 1:
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            if (work->field_6B8 == 1) {
                if (work->field_698 == 0x14) {
                    s32 pan;

                    snd = Actor05700_D171E4[work->field_6D6 + 0xC] |
                          ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = Actor05700_D171E4[work->field_6D6 + 8] |
                          ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = Actor05700_D171E4[work->field_6D6 + 8] |
                      ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);

                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
            }
            work->field_6AE--;
            if (work->field_6AE <= 0) {
                arg0->state     = 2;
                work->field_6A8 = 0;
                work->field_6D4 = 0;
            }
            break;
    }
}

/// Verbatim port of `Actor02000_Fn0150C` of `actor_102000` - the two bodies are
/// byte-identical (see `overlay_dup_index.py find Actor05700_Fn01544`,
/// which also lists five more actors carrying it). Takes the root coordinate's
/// own heading through `ratan2` and steps the yaw `field_6A2` toward the parked
/// `field_6A4` by the dwell counter `field_69E` per frame: within half a turn
/// of the target it closes on it directly (or, for `field_694 == 3`, spins past
/// it by the unsigned counter), past that it unwinds the long way, snapping
/// straight onto the target once `field_69E` would overshoot. The resulting yaw
/// rebuilds the coordinate's matrix.
void Actor05700_Fn01544(Task* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         rot;
    s32              ang;
    u16              want;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              ustep;
    s32              wstep;
    s32              cur;
    s32              next;
    s32              wrapStep;

    rot   = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    coord = ((TmdObject*)arg0->extra)->coords;
    work  = arg0->work;
    ang   = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
    want  = work->field_6A4;
    diff  = want - ang;
    adiff = diff >= 0 ? diff : -diff;

    work->field_6A2 = ang;
    if (adiff < 0x800) {
        step  = work->field_69E;
        ustep = (u16)work->field_69E;
        if (step >= adiff) {
            work->field_6A2 = want;
        } else {
            if (work->field_694 == 3) {
                next = ang - ustep;
            } else {
                next = work->field_6A2;
                if (diff <= 0) {
                    next -= step;
                } else {
                    next += step;
                }
            }
            work->field_6A2 = next;
        }
    } else {
        wstep = work->field_69E;
        if (diff > 0) {
            if (wstep >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (wstep >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_6A2 = work->field_6A4;
        goto done;
    turn:
        if (work->field_694 == 3) {
            work->field_6A2 = (u16)work->field_6A2 - (u16)work->field_69E;
        } else {
            wrapStep = work->field_69E;
            cur      = work->field_6A2;
            if (diff > 0) {
                work->field_6A2 = cur - wrapStep;
            } else {
                work->field_6A2 = cur + wrapStep;
            }
        }
    }
done:
    rot->vx = 0;
    rot->vy = work->field_6A2;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

/// Applies the work block's decaying tilt (`field_688`) to the root
/// coordinate: the tilt's rotation matrix is multiplied column by column into
/// the fourth coordinate's matrix, then X and Y each step 0x20 toward zero,
/// snapping once within 0x20. `field_6B4` is cleared when both have settled.
void Actor05700_Fn016D0(Task* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                         = (MATRIX*)(((Actor105700ScratchStack*)G_SCRATCH_HEAD)->sp - 0x20);
    ((Actor105700ScratchStack*)G_SCRATCH_HEAD)->sp = (u32)matrix;
    active                                         = 0;
    work                                           = arg0->work;
    coord                                          = ((TmdObject*)arg0->extra)->coords;
    RotMatrix(&work->field_688, matrix);
    USE_REG(matrix);
    gte_SetRotMatrix(&coord[3].coord);
    gte_ldclmv(matrix);
    gte_rtir();
    gte_stclmv(&coord[3].coord);
    gte_ldclmv((char*)matrix + 2);
    gte_rtir();
    gte_stclmv((char*)&coord[3].coord + 2);
    gte_ldclmv((char*)matrix + 4);
    gte_rtir();
    gte_stclmv((char*)&coord[3].coord + 4);
    angleX = work->field_688.vx;
    if (angleX != 0) {
        absX = __builtin_abs(angleX);
        if (absX < 0x21) {
            work->field_688.vx = 0;
        } else {
            nextX = angleX - 0x20;
            if (angleX <= 0) {
                nextX = angleX + 0x20;
            }
            work->field_688.vx = nextX;
            active             = 1;
        }
    }
    angleY = work->field_688.vy;
    if (angleY != 0) {
        absY = __builtin_abs(angleY);
        if (absY < 0x21) {
            work->field_688.vy = 0;
        } else {
            nextY = angleY - 0x20;
            if (angleY <= 0) {
                nextY = angleY + 0x20;
            }
            work->field_688.vy = nextY;
            active             = 1;
        }
    }
    if (active == 0) {
        work->field_6B4 = 0;
    }
    *(u32*)G_SCRATCH_HEAD += 0x20;
}

/// Plays the actor's "appear"/"disappear" cue when the animation record's
/// flags gain bit 5 or bit 4, then mirrors those two bits back into the work
/// block's sound flags so each transition fires once. The pan and depth come
/// from the model's root coordinate.
void Actor05700_Fn018DC(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    GpAnimRec*       rec;

    work = arg0->work;
    self = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(&work->ctx, (GpAnimSlot*)&work->slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & 0x20) && (work->field_6A0 & 0x20)) {
                snd = Actor05700_D171E4[work->field_6D6 * 2 - 1] |
                      ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
            }
            if (!(rec->flags & 0x10) && (work->field_6A0 & 0x10)) {
                snd = Actor05700_D171E4[work->field_6D6 * 2] |
                      ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
            }
            work->field_6A0 = (u16)(rec->flags & 0x30);
        }
    }
}

/// Teardown / effect tail of the approach cycle, the same body as
/// `Actor02000_Fn01A20` of `actor_102000`. `Gp_StateF0.field_4` overrides the state
/// machine: 0 clears the body position, 1 draws the ground quad and returns,
/// 2 parks the body behind the actor. Otherwise state 0 unlinks all five body
/// objects (the fifth only for the 0x38 / 0x39 variants), hands the variant
/// halfword to `Gp_ReleaseStateF0Add`, selects animation 0x1D (0x19 for
/// variant 1), saves the enemy pose and switches to state 1; state 1 spawns the
/// ground effect every fourth frame. The tail then reseeds or ticks the
/// nineteen animation slots and redraws the quad.
void Actor05700_Fn01A58(GpEnemy* arg0, Task* arg1)
{
    Actor105700Work* work;
    Actor105700Work* animWork;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    GsCOORDINATE2*   part;
    SVECTOR*         scratch;
    VECTOR3          pos;
    s16              anim;
    s16              duration;
    s32              i;
    u32              random;

    work    = arg1->work;
    coord   = ((TmdObject*)arg1->extra)->coords;
    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    switch (Gp_StateF0.field_4) {
        case 0:
            ((TmdObject*)arg1->extra)->flags = 0;
            arg0->node.flags                 = 0;
            break;
        case 1:
            coord->flg                               = 0;
            ((TmdObject*)arg1->extra)->coords[3].flg = 0;
            Gp_UpdateCoord(coord);
            root   = ((TmdObject*)arg1->extra)->coords;
            pos.vx = root->workm.t[0];
            pos.vy = root->workm.t[1];
            pos.vz = root->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, (VECTOR*)&pos, 0, 0);
            root   = ((TmdObject*)arg1->extra)->coords;
            part   = &root[3];
            pos.vx = part->workm.t[0];
            pos.vy = root->workm.t[1];
            pos.vz = part->workm.t[2];
            Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
            return;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->node.flags                 = 1;
            return;
    }
    switch (work->field_6A8) {
        case 0:
            arg0->recs = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->field_47C);
            Gp_UnlinkObj(&work->field_564);
            Gp_UnlinkObj(&work->field_4CC);
            Gp_UnlinkObj(&work->field_5E4);
            if ((u32)((u16)work->field_6CA - 0x38) < 2U) {
                Gp_UnlinkObj(&work->field_61C);
            }
            Gp_ReleaseStateF0Add(arg1, work->field_6CA);
            anim = 0x1D;
            if (work->field_6B8 == 1) {
                anim = 0x19;
            }
            work->field_694  = anim;
            work->field_6A8  = 1;
            arg0->spawnState = (u8)work->field_6B8;
            Gp_SaveEnemyPose(arg0);
            D_80115419 = 1;
            break;
        case 1:
            if (!(work->field_698 & 3)) {
                scratch->vx = 0;
                scratch->vz = 0;
                random      = (Gp_LcgState * 5) + 0x71357911;
                scratch->vy = -((random >> 0x10) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &((TmdObject*)arg1->extra)->coords[3], 0x400, scratch);
            }
            break;
    }
    animWork = arg1->work;
    i        = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0U;
        duration            = Actor05700_D054CC[animWork->field_694];
        do {
            func_800B4114(&animWork->ctx, i, animWork->field_694, 0, (s32)duration);
            i += 1;
        } while (i < 0x13);
        coord->flg = 0;
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)(animWork->field_698 + i);
        do {
            Gp_AnimTickIndex(&animWork->ctx, i);
            i += 1;
        } while (i < 0x13);
        coord->flg = 0;
    }
    ((TmdObject*)arg1->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(coord);
    root   = ((TmdObject*)arg1->extra)->coords;
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, (VECTOR*)&pos, 0, 0);
    root   = ((TmdObject*)arg1->extra)->coords;
    part   = &root[3];
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

/// `field_6A8` state machine that aims at the player: states 2 and 3 measure the
/// player's root `workm` against this actor's in grid space, state 2 backs off
/// inside 0x7D0 and turns (state 6) when the heading is off by more than 0x100,
/// and state 4 spawns effect 0x6006E on frame 0x1A.
void Actor05700_Fn01E28(Task* arg0)
{
    s16              diff;
    s32              mag;
    s16              angle;
    s32              dx;
    s32              dz;
    VECTOR*          delta;
    VECTOR*          normal;
    VECTOR*          normal2;
    GsCOORDINATE2*   target;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;

    delta = (VECTOR*)(*(u8**)G_SCRATCH_HEAD -= 0x20);
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_6A8) {
        case 0:
            if (work->field_698 >= 0x14) {
                work->field_6A8 = 1;
                work->field_694 = 0x1E;
                work->field_6AE = 0;
            }
            break;
        case 1:
            work->field_69C = -0x16;
            work->field_69E = 0x1E;
            work->field_6CE = work->field_6D0 > 0;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
            Actor05700_Fn02554(arg0);
            work->field_6AE++;
            if (work->field_6AE >= 0x3C) {
                work->field_6A8        = 2;
                work->field_6AE        = 0;
                work->field_61C.flags &= 0x3FFF;
            }
            break;
        case 2:
            work->field_6CE = work->field_6D0 > 0;
            target          = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[2];
            delta->vx       = target->workm.t[0] - coord->workm.t[0];
            normal          = delta + 1;
            delta->vy       = target->workm.t[1] - coord->workm.t[1];
            delta->vz       = target->workm.t[2] - coord->workm.t[2];
            VectorNormal(delta, normal);
            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal, delta);
            dx = delta->vx;
            dz = delta->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
                work->field_6CE = 0;
                break;
            }
            diff = (ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF) - work->field_6A2;
            mag  = __builtin_abs(diff);
            if (mag < 0x800) {
                angle = mag;
            } else if (diff > 0) {
                angle = 0x1000 - diff;
            } else {
                angle = diff + 0x1000;
            }
            if (angle >= 0x101) {
                work->field_6A8 = 6;
                work->field_694 = 0xE;
                work->field_6CE = 0;
            } else {
                work->field_6A8 = 3;
                work->field_694 = 0xD;
                work->field_6CC = 1;
                work->field_6BA = 1;
                work->field_6B6 = 0;
                work->field_6BC++;
                work->field_6BE++;
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_6CE = work->field_6D0 > 0;
            if (work->field_698 < 3) {
                work->field_69E = 0;
            } else {
                target    = &((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[2];
                delta->vx = target->workm.t[0] - coord->workm.t[0];
                normal2   = delta + 1;
                delta->vy = target->workm.t[1] - coord->workm.t[1];
                delta->vz = target->workm.t[2] - coord->workm.t[2];
                VectorNormal(delta, normal2);
                ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, normal2, delta);
                work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
                work->field_69E = 7;
            }
            if (work->field_6BE != 0 && work->field_698 == Actor05700_D054CC[13] - 1) {
                work->field_6BA = 1;
                work->field_6BC++;
                work->field_6BE++;
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6A6        = 8;
                work->field_6A8        = 0;
                work->field_69C        = 0;
                work->field_69E        = 0;
                work->field_6CC        = 0;
                work->field_6CE        = 0;
                work->field_5E4.flags &= 0x7FFF;
            } else if (work->field_6BC >= 6) {
                if (work->field_698 >= Actor05700_D054CC[13] + 0x16) {
                    work->field_6A8       = 4;
                    work->field_694       = 0xF;
                    work->field_6BC       = 0;
                    work->field_6BE       = 0;
                    Actor05700_D054CC[13] = 0;
                    work->field_6CC       = 0;
                }
            } else if (work->field_6BE < 3) {
                if (work->field_698 >= Actor05700_D054CC[13] + 3) {
                    Actor05700_D054CC[13] = 3;
                    work->field_694       = 0xD;
                    work->field_696       = 0x1E;
                }
            } else if (work->field_698 >= Actor05700_D054CC[13] + 0x16) {
                work->field_6A8       = 1;
                work->field_6BE       = 0;
                work->field_694       = 0x1E;
                Actor05700_D054CC[13] = 0;
                work->field_6CC       = 0;
            }
            break;
        case 4:
            if (work->field_698 == 0x1A) {
                Gp_SpawnEff(0x6006E, &((TmdObject*)arg0->extra)->coords[7], 0x6000C, NULL);
            }
            work->field_6CE = 0;
            if (work->field_698 >= 0x87) {
                work->field_6A8 = 5;
            }
            break;
        case 5:
            work->field_6A6 = 2;
            work->field_6A8 = 2;
            work->field_694 = 4;
            break;
        case 6:
            if (work->field_698 >= 0x19) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 0x20;
}

/// State handlers of the model child hung off the actor's part 7 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05700_Fn05040`.
const GpEnemyTaskFuncTable3 Actor05700_D00080 = {
    Actor05700_Fn0509C,
    Actor05700_Fn050E0,
    Gp_DestroyEnemy,
};

/// Runs the animation's mark events: measures `field_698` against the three
/// frames `Actor05700_D054CC[field_694]` marks out. At the 0x1C mark the
/// body object is packed from `Actor05700_D170F4` and bit 0x8000 raised,
/// at 0x28 dropped; inside the 0x1C..0x1E window the player's distance decides
/// whether `field_69C` parks at 0x64; and past 0x7A the actor hands over to
/// animation 4. The delta the distance is taken from is left in the scratch
/// vector it is accumulated in.
void Actor05700_Fn023AC(Task* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;
    s16              anim;
    s32              dx;
    s32              dz;
    s32              distance;

    *(VECTOR**)G_SCRATCH_HEAD -= 1;
    delta                      = *(VECTOR**)G_SCRATCH_HEAD;
    work                       = arg0->work;
    anim                       = Actor05700_D054CC[work->field_694];
    self                       = ((TmdObject*)arg0->extra)->coords;
    if (work->field_698 == anim + 0x1C) {
        work->field_5E4.key    = Gp_PackPair(&Actor05700_D170F4.pair, 4);
        work->field_5E4.flags |= 0x8000;
    } else if (work->field_698 == anim + 0x28) {
        work->field_5E4.flags &= 0x7FFF;
    }
    anim = Actor05700_D054CC[work->field_694];
    if ((work->field_698 >= anim + 0x1C) && (anim + 0x1E >= work->field_698)) {
        dx        = Player_Status.coordMtx->t[0] - self->coord.t[0];
        delta->vx = dx;
        dz        = Player_Status.coordMtx->t[2] - self->coord.t[2];
        delta->vz = dz;
        distance  = SquareRoot0((delta->vx * delta->vx) + (delta->vz * delta->vz));
        if (distance < 0x3E8) {
            work->field_69C = 0;
        } else {
            work->field_69C = 0x64;
        }
    } else {
        work->field_69C = 0;
    }
    if (work->field_698 >= Actor05700_D054CC[work->field_694] + 0x7A) {
        work->field_6A6 = 2;
        work->field_6A8 = 2;
        work->field_694 = 4;
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Converts the root coordinate's world matrix into the frame of part 7 and
/// parks the (0, 100, -100) offset rotated through it, plus its translation,
/// in `field_63C.end1`; then stores the (0, -0x514, 10000) vector rotated by
/// the (-5, -5, 0) matrix in `field_63C.end0` and raises the fifth body
/// object's 0xC000 flags. While `field_6AE` is non-zero, the parked point is
/// taken back to world space, the distance to the first `field_654` hit (10000
/// with none, plus 1000 for a kind-0x1 hit) replaces the vector's depth, and
/// the rotated result and the parked point go to `Actor05700_Fn0295C`.
void Actor05700_Fn02554(Task* arg0)
{
    Actor105700AimScratch* scratch;
    Actor105700Work*       work;
    GsCOORDINATE2*         self;

    scratch     = (Actor105700AimScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x40);
    self        = ((TmdObject*)arg0->extra)->coords;
    work        = arg0->work;
    self[0].flg = 0;
    self[7].flg = 0;
    Gp_UpdateCoord(&self[7]);
    Gp_WorldToLocal(&self->workm, &self[7].workm, &scratch->mtx);
    scratch->vec.vy = 100;
    scratch->vec.vx = 0;
    scratch->vec.vz = -100;
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->vec);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    work->field_63C.end1.vx = scratch->mtx.t[0] + scratch->pos.vx;
    work->field_63C.end1.vy = scratch->mtx.t[1] + scratch->pos.vy;
    work->field_63C.end1.vz = scratch->mtx.t[2] + scratch->pos.vz;
    scratch->rot.vx         = -5;
    scratch->rot.vy         = -5;
    scratch->rot.vz         = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    scratch->rot.vx = 0;
    scratch->rot.vy = -0x514;
    scratch->rot.vz = 10000;
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    work->field_63C.end0.vx = scratch->pos.vx;
    work->field_63C.end0.vy = scratch->pos.vy;
    work->field_63C.end0.vz = scratch->pos.vz;
    work->field_61C.flags  |= 0xC000;
    if (work->field_6AE == 0) {
        *(u8**)G_SCRATCH_HEAD += 0x40;
        return;
    }
    gte_SetRotMatrix(&self->workm);
    scratch->vec.vx = work->field_63C.end1.vx;
    scratch->vec.vy = work->field_63C.end1.vy;
    scratch->vec.vz = work->field_63C.end1.vz;
    gte_ldv0(&scratch->vec);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    scratch->vec.vx = scratch->pos.vx + self->workm.t[0];
    scratch->vec.vy = scratch->pos.vy + self->workm.t[1];
    scratch->vec.vz = scratch->pos.vz + self->workm.t[2];
    scratch->rot.vx = work->field_63C.end0.vx;
    scratch->rot.vy = work->field_63C.end0.vy;
    if (Gp_FindRec18(work->field_654, 0) != 0) {
        scratch->pos.vx = work->field_654[0].point.vx - scratch->vec.vx;
        scratch->pos.vy = work->field_654[0].point.vy - scratch->vec.vy;
        scratch->pos.vz = work->field_654[0].point.vz - scratch->vec.vz;
        scratch->rot.vz = SquareRoot0(scratch->pos.vx * scratch->pos.vx + scratch->pos.vy * scratch->pos.vy +
                                      scratch->pos.vz * scratch->pos.vz);
        if ((work->field_654[0].key & 0xFFFF0000) == 0x10000) {
            scratch->rot.vz += 1000;
        }
    } else {
        scratch->rot.vz = 10000;
    }
    Gp_ClearRec18Occupied(work->field_654);
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    scratch->rot.vx = scratch->pos.vx;
    scratch->rot.vy = scratch->pos.vy;
    scratch->rot.vz = scratch->pos.vz;
    scratch->vec.vx = work->field_63C.end1.vx;
    scratch->vec.vy = work->field_63C.end1.vy;
    scratch->vec.vz = work->field_63C.end1.vz;
    Actor05700_Fn0295C(arg0, &scratch->rot, &scratch->vec);
    *(u8**)G_SCRATCH_HEAD += 0x40;
}

extern s16 Actor05700_D173C8[][4];

/// Draws the aim beam from `arg2` to `arg1` in eight projected steps. Each
/// step nearer than OTZ 30 is skipped; otherwise the segment's screen normal
/// (`VectorNormalS`) offsets the ends by a depth-scaled width into two
/// semi-transparent red-to-black `POLY_G4`s (corner order from
/// `Actor05700_D173C8`), a red `LINE_F2` core and a blend `DR_TPAGE`.
void Actor05700_Fn0295C(Task* arg0, SVECTOR* arg1, SVECTOR* arg2)
{
    Actor105700BeamScratch* s;
    GsCOORDINATE2*          self;
    POLY_G4*                poly;
    LINE_F2*                line;
    DR_TPAGE*               page;
    s32                     i;
    s32                     j;
    s32                     depth;

    s          = (Actor105700BeamScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x48);
    self       = ((TmdObject*)arg0->extra)->coords;
    s->step.vx = (arg1->vx - arg2->vx) / 8;
    s->step.vy = (arg1->vy - arg2->vy) / 8;
    s->step.vz = (arg1->vz - arg2->vz) / 8;
    gte_SetRotMatrix(&self->workm);
    gte_SetTransMatrix(&self->workm);
    gte_ldv0(arg2);
    gte_rtps();
    gte_stsxy(&s->prev);
    gte_stszotz(&s->prevZ);
    for (i = 1; i < 8; i++) {
        s->pt.vx = arg2->vx + s->step.vx * i;
        s->pt.vy = arg2->vy + s->step.vy * i;
        s->pt.vz = arg2->vz + s->step.vz * i;
        gte_SetRotMatrix(&self->workm);
        gte_SetTransMatrix(&self->workm);
        gte_ldv0(&s->pt);
        gte_rtps();
        gte_stsxy(&s->cur);
        gte_stszotz(&s->curZ);
        depth = (s->prevZ + s->curZ) / 2;
        if (depth < 30) {
            s->prev  = s->cur;
            s->prevZ = s->curZ;
            continue;
        }
        s->vec.vz = 0;
        s->xs[0]  = s->prev;
        s->ys[0]  = s->prev >> 16;
        s->xs[1]  = s->cur;
        s->ys[1]  = s->cur >> 16;
        s->vec.vx = s->xs[1] - s->xs[0];
        s->vec.vy = s->ys[1] - s->ys[0];
        VectorNormalS(&s->vec, &s->pt);
        s->pt.vy *= -1;
        s->xs[2]  = s->xs[0] + (-(s->pt.vy * 0x900) >> 12) / depth;
        s->xs[3]  = s->xs[1] + (-(s->pt.vy * 0x900) >> 12) / depth;
        s->xs[4]  = s->xs[0] + ((s->pt.vy * 9) >> 4) / depth;
        s->ys[2]  = s->ys[0] + (-(s->pt.vx * 0x900) >> 12) / depth;
        s->ys[3]  = s->ys[1] + (-(s->pt.vx * 0x900) >> 12) / depth;
        s->ys[4]  = s->ys[0] + ((s->pt.vx * 9) >> 4) / depth;
        s->ys[5]  = s->ys[1] + ((s->pt.vx * 9) >> 4) / depth;
        s->xs[5]  = s->xs[1] + ((s->pt.vy * 9) >> 4) / depth;
        for (j = 0; j < 2; j++) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + sizeof(POLY_G4);
            setPolyG4(poly);
            setSemiTrans(poly, 1);
            poly->x0 = s->xs[Actor05700_D173C8[j][0]];
            poly->y0 = s->ys[Actor05700_D173C8[j][0]];
            poly->x1 = s->xs[Actor05700_D173C8[j][1]];
            poly->y1 = s->ys[Actor05700_D173C8[j][1]];
            poly->x2 = s->xs[Actor05700_D173C8[j][2]];
            poly->y2 = s->ys[Actor05700_D173C8[j][2]];
            poly->x3 = s->xs[Actor05700_D173C8[j][3]];
            poly->y3 = s->ys[Actor05700_D173C8[j][3]];
            poly->r0 = 0xFF;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0xFF;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            addPrim((u32*)((((u32)(depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        line           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)line + sizeof(LINE_F2);
        setLineF2(line);
        setSemiTrans(line, 1);
        line->x0 = s->prev;
        line->y0 = s->prev >> 16;
        line->x1 = s->cur;
        line->y1 = s->cur >> 16;
        line->r0 = 0xFF;
        line->g0 = 0;
        line->b0 = 0;
        addPrim((u32*)((((u32)(depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), line);
        page           = gGpuPrimCursor;
        gGpuPrimCursor = (u8*)page + sizeof(DR_TPAGE);
        setlen(page, 1);
        page->code[0] = 0xE1000620;
        addPrim((u32*)((((u32)(depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), page);
        s->prev  = s->cur;
        s->prevZ = s->curZ;
    }
    *(u8**)G_SCRATCH_HEAD += 0x48;
}

/// Places a fresh body block for the actor: allocates the 0xF0-byte work
/// block, builds the root coordinate by rotating the local spawn offset through
/// the parent coordinate and re-aiming it, then links the three collision
/// bodies and their `GpRec18` tables onto the model root and hands the light /
/// colour matrices to its `TmdObject`. The sound cue that marks the placement
/// packs the room/channel bits of the spawn context into `Actor05700_D17228`.
extern s32 Actor05700_D17228;

void Actor05700_Fn031BC(GpEnemy* arg0, Task* arg1)
{
    Actor105700FxWork*       work;
    Actor105700PlaceScratch* scratch;
    GpEnemy*                 ctx;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           parentCoord;
    TmdObject*               tmd;
    Task*                    parent;
    s32                      sound;
    s32                      pan;

    tmd         = arg1->extra;
    coord       = tmd->coords;
    parent      = arg1->parent;
    parentCoord = ((TmdObject*)parent->extra)->coords;
    work        = memCalloc(0xF0, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work    = (TaskIdMap*)work;
    tmd->flags    = 0;
    scratch       = (Actor105700PlaceScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x38);
    tmd->lightMtx = &work->lightMtx;
    tmd->colorMtx = &work->colorMtx;

    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->flg = 0;
    Gp_UpdateCoord(parentCoord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);

    scratch->rot.vx = 0;
    scratch->rot.vy = 0x1F4;
    scratch->rot.vz = 0x64;
    gte_SetRotMatrix(&coord->coord);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    coord->sub         = &gGfxViewCoord;
    coord->coord.t[0] += scratch->pos.vx;
    coord->coord.t[1] += scratch->pos.vy;
    coord->coord.t[2] += scratch->pos.vz;

    scratch->rot.vx = 0x80;
    scratch->rot.vy = 0;
    scratch->rot.vz = 0x10;
    RotMatrix(&scratch->rot, &scratch->mtx);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&scratch->mtx);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv(&scratch->mtx.m[0][1]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][1]);
    gte_ldclmv(&scratch->mtx.m[0][2]);
    gte_rtir();
    gte_stclmv(&coord->coord.m[0][2]);

    work->field_EE = (Actor05700_D170F4.field_E != 1);

    work->obj40.coord    = coord;
    work->obj40.ctx.recs = work->rec60;
    work->obj40.pos.vx   = 0;
    work->obj40.pos.vy   = 0;
    work->obj40.pos.vz   = 0;
    work->obj40.key      = Gp_PackPair(&Actor05700_D170F4.pair, 3);
    work->obj40.radius   = 0x64;
    work->obj40.flags    = 1;
    Gp_LinkObj(3, &work->obj40);
    Gp_InitRec18Table(work->rec60, 1, 0);
    work->obj40.flags |= 0x8000;

    work->obj78.coord    = coord;
    work->obj78.ctx.recs = work->rec60;
    work->obj78.pos.vx   = 0;
    work->obj78.pos.vy   = 0;
    work->obj78.pos.vz   = 0;
    work->obj78.key      = 0x22B2B;
    work->obj78.radius   = 0x64;
    work->obj78.flags    = 1;
    Gp_LinkObj(1, &work->obj78);
    work->obj78.flags |= 0x8000;

    work->d4rec.end0.vx    = 0;
    work->d4rec.end0.vy    = 0;
    work->d4rec.end0.vz    = 0;
    work->d4rec.end1.vx    = 0;
    work->d4rec.end1.vy    = -0x1F4;
    work->d4rec.end1.vz    = 0;
    work->d4rec.end0Radius = 1;
    work->d4rec.end1Radius = 1;
    work->d4rec.recs       = work->recD0;
    work->obj98.ctx.d4rec  = &work->d4rec;
    work->obj98.coord      = coord;
    work->obj98.pos.vx     = 0;
    work->obj98.pos.vy     = 0;
    work->obj98.pos.vz     = 0;
    work->obj98.key        = 0;
    work->obj98.radius     = 0;
    work->obj98.flags      = 3;
    Gp_LinkObj(3, &work->obj98);
    Gp_InitRec18Table(work->recD0, 1, 0);
    work->obj98.flags |= 0x4400;

    arg1->state = 1;
    Task_DetachFromParent(arg1);

    coord->flg = 0;
    Gp_UpdateCoord(coord);

    ctx   = arg1->spawnArg2;
    sound = Actor05700_D17228 | ((ctx->placeKey >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));

    *(u8**)G_SCRATCH_HEAD += 0x38;
}

extern s32 D_80115750;
/// Sound id of the burst cue, with the spawn context's room/channel bits
/// packed in like `Actor05700_D17228`.
extern s32 Actor05700_D1722C;

/// Per-frame tick of the placed effect body from `Actor05700_Fn031BC`.
/// Mode 0 of `Gp_StateF0.field_4` drifts the root coordinate along its Y axis, puffs
/// an effect every fourth frame and ends the cycle - burst, sound cue and
/// state 2 - on a hit, an empty room-parameter slot, or after 0x5A frames.

void Actor05700_Fn035FC(GpEnemy* arg0, Task* arg1)
{
    Actor105700FxWork* work;
    GsCOORDINATE2*     coord;
    TmdObject*         tmd;
    SVECTOR*           scratch;
    GpEnemy*           ctx;
    s32                found;
    s32                idx;
    s32                sound;
    s32                pan;
    VECTOR             pos;

    tmd   = arg1->extra;
    coord = tmd->coords;
    work  = (Actor105700FxWork*)arg1->work;
    found = 0;
    switch (Gp_StateF0.field_4) {
        case 0:
            tmd->flags = 0;
            break;
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);
            return;
        case 2:
            tmd->flags = 0x80;
            return;
    }

    coord->flg         = 0;
    coord->coord.t[0] += (coord->coord.m[0][1] * 75) >> 11;
    coord->coord.t[1] += (coord->coord.m[1][1] * 75) >> 11;
    coord->coord.t[2] += (coord->coord.m[2][1] * 75) >> 11;

    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 0x28);
    if (++work->field_E8 >= 4) {
        scratch->vx = 0;
        scratch->vy = 0x64;
        scratch->vz = 0;
        Gp_SpawnEff(0x60070, coord, 0x01001600, scratch);
        work->field_E8 = 0;
    }
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2, &pos, 0, 0);

    if (work->recD0[0].key != 0) {
        idx = func_800E1B24(work->recD0[0].key);
        if (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1][idx]->field_1 == 0) {
            found = 1;
        }
        Gp_ClearRec18Occupied(work->recD0);
    }
    if (work->rec60[0].key != 0 || found || ++work->field_EA >= 0x5A) {
        Gp_SpawnEff(D_80115750, coord, work->field_EE, NULL);
        ((TmdObject*)arg1->extra)->flags = 0x80;
        ctx                              = arg1->spawnArg2;
        sound                            = Actor05700_D1722C | ((ctx->placeKey >> 0xC) << 8);
        pan                              = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        arg1->state = 2;
        if ((work->rec60[0].key & 0xFFFF0080) == 0x10000) {
            Gp_SpawnPadLerp(0xA, 0xFF, 8);
        }
    }
    *(u8**)G_SCRATCH_HEAD += 0x28;
}

extern s32 D_80115758;
extern s32 Actor05700_D17230;
extern s32 Actor05700_D17234;

/// Four-step burst sequence driven by `field_6A8`: 0 spawns the effect and cue
/// on entry, rumbles every tenth frame and either ends after `field_6B6` passes
/// 0x28 or times out at 0x96 frames; 1 counts `field_6AE` down into 2; 2
/// triggers the PE state and plays the second cue; 3 spawns random-offset
/// sparks every fourth frame until `field_6AE` runs out.
void Actor05700_Fn03930(Task* arg0)
{
    Actor105700Work* work;
    GsCOORDINATE2*   self;
    SVECTOR*         scratch;
    s32              sound;
    u32              random;

    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    work    = arg0->work;
    self    = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_6A8) {
        case 0:
            if (work->field_6AE == 0) {
                scratch->vx     = 0;
                scratch->vy     = 0;
                scratch->vz     = 0;
                work->field_690 = Gp_SpawnEff(D_80115758, &((TmdObject*)arg0->extra)->coords[4], 0x96, scratch);
                sound           = Actor05700_D17230 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            }
            work->field_6CE = work->field_6D0 > 0;
            if ((s16)(work->field_6AE % 10) == 0) {
                Gp_SpawnPadLerp(5, 0x80, 8);
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6C2 = 0;
                if (--work->field_6C4 <= 0) {
                    work->field_6A6 = 9;
                    work->field_6A8 = 0;
                } else {
                    work->field_6A6 = 5;
                    work->field_6A8 = 3;
                    work->field_694 = 0x15;
                    work->field_6AE = 0x4F;
                }
                work->field_6CE = 0;
                if (work->field_690 != NULL) {
                    work->field_690->task->state = 3;
                }
                work->field_690 = NULL;
            } else if (++work->field_6AE >= 0x96) {
                work->field_6AE = 8;
                work->field_6A8 = 1;
                work->field_694 = 0xB;
                work->field_6C2 = 0;
                work->field_6CE = 0;
                work->field_690 = NULL;
                Gp_SpawnPadLerp(0xF, 0xFF, 8);
            }
            break;
        case 1:
            if (--work->field_6AE <= 0) {
                work->field_6A8 = 2;
                work->field_6AE = 0;
            }
            break;
        case 2:
            Gp_TriggerPeState(0, 0x10);
            work->field_6A6 = 2;
            work->field_6A8 = 0;
            work->field_694 = 2;
            sound           = Actor05700_D17234 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            break;
        case 3:
            if (!(work->field_698 & 3)) {
                scratch->vx = 0;
                scratch->vz = 0;
                random      = Gp_LcgState * 5 + 0x71357911;
                scratch->vy = -((random >> 16) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &((TmdObject*)arg0->extra)->coords[3], 0x100, scratch);
            }
            if (--work->field_6AE <= 0) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    *(u8**)G_SCRATCH_HEAD += 8;
}

/// Animation bank the work block's animation context is started on.
extern void Actor05700_D17408;

/// The actor's spawn table: entry 3 is the model child re-skinned with the
/// placement's texture page, entry 1 the effect child, and the whole table
/// is kept in `field_66C` for later spawns.
extern TaskDesc Actor05700_D173D8[];

/// Per-stage tables of per-area CD cue ids; a NULL stage has no cue.
extern u16* Actor05700_D173B0[];

/// Enemy parameter record the spawn hands to its `GpEnemy`.
extern GpPairSrcE Actor05700_D17108[];

/// Spawn state handler: allocates the 0x6E4-byte work block, starts its
/// animation, spawns the model and effect children, then by the enemy's spawn
/// state either links the enemy and sets up its five collision bodies (state
/// 0) or parks it on one of the two resume animations (states 1 and 2).
void Actor05700_Fn03CC4(GpEnemy* ctx, Task* actor)
{
    Actor105700Work* work;
    TmdObject*       obj;
    TmdObject*       model;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parts;
    GsCOORDINATE2*   partsA;
    GsCOORDINATE2*   partsB;
    GsCOORDINATE2*   partsC;
    GsCOORDINATE2*   partsD;
    GsCOORDINATE2*   effParts;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     entry;
    GpEnemy*         eff;
    GpEnemy*         spawned;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    GpAreaKey        key;
    s32              i;
    s32              one;
    s32              kind;
    u32              idx;
    u8               areaByte3;
    GameSession*     session;
    s32              param;
    u32              lcg;

    obj   = actor->extra;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = (TaskIdMap*)work;
    obj->flags                 = 0;
    coord->flg                 = 0;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x39;
    work->field_66C            = Actor05700_D173D8;
    work->field_670.coord      = &((TmdObject*)actor->extra)->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->ctx, &Actor05700_D17408, obj, work->field_30C, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->ctx, i, 1);
    }

    spawned    = Gp_SpawnEnemyFromTable(Actor05700_D173D8, 3, 0, ctx);
    session    = gGameSession;
    spawned    = (GpEnemy*)spawned->task;
    sessionKey = (GpAreaKey*)&session->at4.loc.view;
    idx        = ctx->placeKey;
    areaByte3  = sessionKey->stage;
    model      = ((Task*)spawned)->extra;
    SOFT_USE_REG(spawned);
    key.stage = areaByte3;
    key.area  = sessionKey->area;
    key.room  = sessionKey->room;
    areaByte0 = session->at4.loc.view;
    idx       = idx >> 12;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    keyPtr       = &key;
    rec          = Gp_GetNestedAreaRec(keyPtr);
    idx        <<= 4;
    idx         += (s32)rec->field_0;
    model->tpage = ((GpAreaPlace*)idx)->tpage;
    model->clut  = ((GpAreaPlace*)idx)->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }

    eff     = Gp_SpawnEnemyFromTable(Actor05700_D173D8, 1, 0, ctx);
    session = gGameSession;
    spawned = (GpEnemy*)eff->task;

    sessionKey = (GpAreaKey*)&session->at4.loc.view;
    idx        = ctx->placeKey;
    areaByte3  = sessionKey->stage;
    model      = ((Task*)spawned)->extra;
    SOFT_USE_REG(spawned);
    key.stage = areaByte3;
    key.area  = sessionKey->area;
    key.room  = sessionKey->room;
    areaByte0 = session->at4.loc.view;
    idx       = idx >> 12;
    SOFT_BARRIER();
    DEF_REG(keyPtr);
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    idx        <<= 4;
    idx         += (s32)rec->field_0;
    model->tpage = ((GpAreaPlace*)idx)->tpage;
    model->clut  = ((GpAreaPlace*)idx)->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }

    one  = 1;
    kind = ctx->spawnState;
    if (kind == one) {
        goto case1;
    }
    if (kind >= 2) {
        goto ge2;
    }
    if (kind == 0) {
        goto case0;
    }
    return;
ge2:
    if (kind == 2) {
        goto case2;
    }
    return;

case0:
    ctx->field_4  = &coord->coord;
    ctx->field_48 = 0;
    Gp_LinkNode(&ctx->node);
    parts           = ((TmdObject*)actor->extra)->coords;
    ctx->bodyPos.vx = 0;
    ctx->bodyPos.vy = 0;
    ctx->bodyPos.vz = 0;
    ctx->param      = Actor05700_D17108;
    ctx->recs       = work->field_4EC;
    ctx->coord      = &parts[3];
    ctx->hp         = Actor05700_D17108->hpMax;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_6AC = ctx->place->mode & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = ctx->place->variant;
        work->field_6DA = param * 1000;
    }

    tbl = Actor05700_D173B0[gGameSession->at4.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->at4.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x39;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_6D0            = 0xFA;
    work->field_49C.end0.vz    = 0x1F40;
    work->field_49C.end0Radius = 0x3E8;
    work->field_49C.end1Radius = 0x5DC;
    work->field_49C.end0.vx    = 0;
    work->field_49C.end0.vy    = 0;
    work->field_49C.end1.vx    = 0;
    work->field_49C.end1.vy    = 0;
    work->field_49C.end1.vz    = 0;
    work->field_49C.recs       = work->field_4B4;
    lcg                        = Gp_LcgState * 5 + 0x71357911;
    work->field_6C4            = ((lcg >> 16) & 1) + 1;
    Gp_LcgState                = lcg;
    partsA                     = ((TmdObject*)actor->extra)->coords;
    work->field_47C.ctx.d4rec  = &work->field_49C;
    work->field_47C.pos.vx     = 0;
    work->field_47C.pos.vy     = 0;
    work->field_47C.pos.vz     = 0;
    work->field_47C.key        = 0;
    work->field_47C.radius     = 0;
    work->field_47C.flags      = 3;
    work->field_47C.coord      = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= 0xCC00;

    partsB                   = ((TmdObject*)actor->extra)->coords;
    work->field_4CC.ctx.recs = work->field_4EC;
    work->field_4CC.pos.vx   = 0;
    work->field_4CC.pos.vy   = 0;
    work->field_4CC.pos.vz   = 0;
    work->field_4CC.key      = 0x30039;
    work->field_4CC.radius   = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.coord    = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = ((TmdObject*)actor->extra)->coords;
    work->field_564.pos.vy   = -0x226;
    work->field_564.ctx.recs = work->field_584;
    work->field_564.pos.vx   = 0;
    work->field_564.pos.vz   = 0;
    work->field_564.key      = 0;
    work->field_564.radius   = 0x226;
    work->field_564.flags    = 1;
    work->field_564.coord    = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= 0x4200;

    effParts                 = ((TmdObject*)eff->task->extra)->coords;
    work->field_5E4.ctx.recs = work->field_604;
    work->field_5E4.pos.vx   = 0;
    work->field_5E4.pos.vy   = 0x1F4;
    work->field_5E4.pos.vz   = 0;
    work->field_5E4.key      = 0;
    work->field_5E4.radius   = 0x1F4;
    work->field_5E4.flags    = 1;
    work->field_5E4.coord    = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);

    work->field_63C.end0.vx    = 0;
    work->field_63C.end0.vy    = 0;
    work->field_63C.end0.vz    = 0;
    work->field_63C.end1.vx    = 0;
    work->field_63C.end1.vy    = 0;
    work->field_63C.end1.vz    = 0;
    work->field_63C.end0Radius = 1;
    work->field_63C.end1Radius = 1;
    work->field_63C.recs       = work->field_654;
    work->field_5E4.flags     &= 0x7FFF;
    partsD                     = ((TmdObject*)actor->extra)->coords;
    work->field_61C.ctx.d4rec  = &work->field_63C;
    work->field_61C.pos.vx     = 0;
    work->field_61C.pos.vy     = 0;
    work->field_61C.pos.vz     = 0;
    work->field_61C.key        = 0;
    work->field_61C.radius     = 0;
    work->field_61C.flags      = 3;
    work->field_61C.coord      = partsD;
    Gp_LinkObj(3, &work->field_61C);
    Gp_InitRec18Table(work->field_654, 1, 0);
    work->field_61C.flags = (work->field_61C.flags & 0x3FFF) | 0xC00;
    actor->state          = 1;
    return;

case1:
    work->field_694 = 0x19;
    work->field_6A8 = 2;
    actor->state    = 2;
    return;

case2:
    work->field_694 = 0x1D;
    work->field_6A8 = kind;
    actor->state    = kind;
}

/// Per-state handlers of the approach cycle, indexed by `field_6A6`.
extern TaskFunc Actor05700_D17484[];

/// Every third frame while `field_6C4` is clear, kicks a dust effect off the
/// fourth body coordinate with a random upward velocity.
static __inline__ void Actor105700_SpawnDust(Task* actor)
{
    Actor105700Work* work;
    SVECTOR*         head;
    SVECTOR*         rot;

    work                       = actor->work;
    head                       = *(SVECTOR**)G_SCRATCH_HEAD;
    rot                        = head - 1;
    *(SVECTOR**)G_SCRATCH_HEAD = rot;
    if (++work->field_6B0 >= 3) {
        work->field_6B0 = 0;
        head[-1].vx     = 0;
        rot->vz         = 0;
        rot->vy         = -(((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x1FF);
        Gp_SpawnEff(0x600E0, &((TmdObject*)actor->extra)->coords[3], 0x100, rot);
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

/// Per-frame tick: runs the state handler, integrates the forward step,
/// advances or reseeds the animation slots, then draws. The same body as
/// `Actor02000_Fn02A34` plus the dust effect.
void Actor05700_Fn04338(GpEnemy* ctx, Task* actor)
{
    VECTOR3          pos;
    GpEnemy*         spawn;
    TmdObject*       model;
    Actor105700Work* moveWork;
    Actor105700Work* animWork;
    Actor105700Work* work;
    Actor105700Work* flagWork;
    GsCOORDINATE2*   moveCoord;
    GsCOORDINATE2*   part;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    s16              duration;
    s32              i;
    u8               flags;

    work  = actor->work;
    model = actor->extra;
    coord = model->coords;
    switch (Gp_StateF0.field_4) {
        case 0:
            model->flags    = 0;
            ctx->node.flags = 0;
            break;
        case 1:
            goto draw;
        case 2:
            model->flags    = 0x80;
            ctx->node.flags = 1;
            return;
    }

    if (ctx->reactionFlags != 0) {
        spawn    = actor->spawnArg2;
        flags    = spawn->reactionFlags;
        flagWork = actor->work;
        if ((flags & 2) && (flagWork->field_6B8 == 0)) {
            spawn->reactionFlags = flags & 0xFD;
            flagWork->field_6A6  = 0xA;
            flagWork->field_694  = 0x14;
            flagWork->field_6A8  = 0;
            flagWork->field_6E0  = 1;
        }
    }
    Actor05700_Fn000B0(actor);
    Actor05700_D17484[work->field_6A6](actor);
    if (work->field_69E != 0) {
        Actor05700_Fn01544(actor);
    }
    moveCoord              = ((TmdObject*)actor->extra)->coords;
    moveWork               = actor->work;
    moveWork->field_678    = moveCoord->coord.t[0];
    moveWork->field_67C    = moveCoord->coord.t[1];
    moveWork->field_680    = moveCoord->coord.t[2];
    moveCoord->coord.t[0] += (s32)(moveCoord->coord.m[0][2] * moveWork->field_69C) >> 0xC;
    if (moveWork->field_6DE < 2) {
        moveCoord->coord.t[1] += 0x80;
    }
    moveCoord->coord.t[2] += (s32)(moveCoord->coord.m[2][2] * moveWork->field_69C) >> 0xC;
    animWork               = actor->work;
    i                      = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0;
        duration            = Actor05700_D054CC[animWork->field_694];
        do {
            func_800B4114(&animWork->ctx, i, animWork->field_694, 0, duration);
            i += 1;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)animWork->field_698 + i;
        do {
            Gp_AnimTickIndex(&animWork->ctx, i);
            i += 1;
        } while (i < 0x13);
    }
    if (work->field_6B4 != 0) {
        Actor05700_Fn016D0(actor);
    }
    Actor05700_Fn018DC(actor);
    coord->flg                                = 0;
    ((TmdObject*)actor->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(coord);
    if (work->field_6C4 == 0) {
        Actor105700_SpawnDust(actor);
    }
draw:
    USE_REG(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(actor->spawnArg2, (VECTOR*)&pos, 0, 0);
    root   = ((TmdObject*)actor->extra)->coords;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}

void Actor05700_Fn04714(Task* arg0)
{
    s16              yaw;
    s16              yaw2;
    s16              state;
    s16              deltaYaw;
    s16              deltaYaw2;
    s16              speed;
    s32              magnitude;
    s32              magnitude2;
    s16              wrapped;
    s16              wrapped2;
    s16              angle;
    s32              dx;
    s32              dz;
    u32              random;
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = arg0->work;
    state                 = work->field_6A8;
    coord                 = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= Actor05700_D054CC[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x1E;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | 0xC000;
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & 0x3FFF);
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            dz              = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
            } else {
                random      = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = random;
                if (!((random >> 0x10) & ((1 << (work->field_6C0 + 1)) - 1)) && !(Player_Status.peStateFlags & 0x10) &&
                    work->field_6C4 != 0) {
                    work->field_6A6 = 5;
                    work->field_6A8 = 0;
                    work->field_694 = 0xA;
                    work->field_6AE = 0;
                    work->field_6C2 = 1;
                    work->field_6B6 = 0;
                    work->field_6C0++;
                } else {
                    work->field_6A6 = 6;
                    work->field_6A8 = 0;
                    work->field_694 = 0xC;
                    work->field_6AE = 0;
                }
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | 0xC000;
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & 0x3FFF);
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = work->field_6A4 - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    *(s32*)G_SCRATCH_HEAD += 0x10;
}

/// Tests the segment from `arg0` to `arg1` against the collision faces on the
/// `D_80115550` list: the segment's direction is normalised in a 0x10-byte
/// block carved off `G_SCRATCH_HEAD`, and every face with bit 0x40 of
/// `field_3A` set is tested until one reports a hit. Returns 1 on a hit and
/// the last test's result otherwise.
s32 Actor05700_Fn04BB4(SVECTOR* arg0, SVECTOR* arg1)
{
    void**   scratch;
    u8*      head;
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret                          = 0;
    scratch                      = (void**)G_SCRATCH_HEAD;
    node                         = D_80115550;
    head                         = *scratch;
    ((VECTOR*)(head - 0x10))->vx = arg1->vx - arg0->vx;
    head                         = head - 0x10;
    TOUCH_REG_USE(head, node);
    vec      = (VECTOR*)head;
    vec->vy  = arg1->vy - arg0->vy;
    *scratch = vec;
    vec->vz  = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
    return ret;
}

/// Per-frame tick, the same body as `Actor02000_Fn03268` of `actor_102000`.
/// State 0 counts `field_6AE` up to 0x5B frames and then hands over to state
/// 1 with animation 4, running `Actor05700_Fn00D08` every frame
/// meanwhile; state 1 waits for `field_698` to reach 0x5E and drops back to
/// state 0 with animation 1. Either way, once `field_6B2` or the global
/// `D_80115419` is set the actor switches to animation 2 and arms the shared
/// state-F0 slot.
void Actor05700_Fn04CC0(Task* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            Actor05700_Fn00D08(arg0);
            break;
        case 1:
            if (work->field_698 >= 0x5E) {
                work->field_694 = 1;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (D_80115419 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }
}

/// Hit-reaction state, entry 8 of the `field_6A6` table: step 0 starts
/// animation 0x11 and clears both dwell counters; step 1 waits for frame 0x37,
/// then parks on animation 2 (entry 2) or, with `field_6E0` set, on animation
/// 0x14 (entry 0xA).
void Actor05700_Fn04DA0(Task* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}

/// Entry 9 of the `field_6A6` table: step 0 starts animation 0x12 when
/// `field_6AA` is 1 (step 1, waits for frame 0x50) and animation 0x13
/// otherwise (step 2, waits for frame 0x3B); either way the dwell counters are
/// cleared and the actor parks on animation 2 (entry 2) when done.
void Actor05700_Fn04E2C(Task* arg0)
{
    Actor105700Work* work;
    s32              state;
    s32              next;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6AA;
            if (next == 1) {
                work->field_694 = 0x12;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x13;
                work->field_6A8 = 2;
            }
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x50) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x3B) {
                work->field_694 = state;
                work->field_6A6 = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Entry 0xA of the `field_6A6` table: step 0 waits for `Gp_TickObjFlag2` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `field_6E0`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void Actor05700_Fn04EF4(Task* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Entry 0xE of the `field_6A6` table: step 0 starts animation 0x17 when
/// `field_6B8` is 1 (step 1, waits for frame 0x10) and animation 0x1B
/// otherwise (step 2, waits for frame 0x16); when done the actor's handler
/// chain advances to state 2.
void Actor05700_Fn04F80(Task* arg0)
{
    Actor105700Work* work;
    s32              sel;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            /* The 32-bit local is load-bearing: an s16 one makes combine fold
             * the sign-extension into a second `lh` of field_6B8. */
            sel = work->field_6B8;
            if (sel == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = sel;
                return;
            }
            work->field_694 = 0x1B;
            work->field_6A8 = 2;
            return;
        case 1:
            if (work->field_698 >= 0x10) {
                arg0->state     = 2;
                work->field_6A8 = 0;
            }
            return;
        case 2:
            if (work->field_698 >= 0x16) {
                arg0->state     = state;
                work->field_6A8 = 0;
            }
            return;
    }
}

void Actor05700_Fn05038(void)
{
}

void Actor05700_Fn05040(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D00080;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Spawn state of the model child hung off the actor: parents the child's
/// root coordinate to part 7 of the actor's model, points the child's model
/// at the actor's light and colour matrices and advances to state 1.
void Actor05700_Fn0509C(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor105700Work*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[7];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

/// Per-frame state of the model child: mirrors the actor's model flags onto
/// its own, and when the actor raises `field_6BA` spawns entry 2 of the
/// actor's spawn table and hands it this child's texture page and CLUT row,
/// reprocessing its model stream for both half-buffers.
void Actor05700_Fn050E0(GpEnemy* enemy, Task* task)
{
    Actor105700Work* work;
    GpEnemy*         spawned;
    TmdObject*       src;
    TmdObject*       dst;

    work                             = (Actor105700Work*)task->parent->work;
    ((TmdObject*)task->extra)->flags = ((TmdObject*)task->parent->extra)->flags;
    if (work->field_6BA != 0) {
        work->field_6BA = 0;
        spawned         = Gp_SpawnEnemyFromTable(work->field_66C, 2, 0, enemy);
        src             = (TmdObject*)task->extra;
        dst             = (TmdObject*)spawned->task->extra;
        dst->tpage      = src->tpage;
        dst->clut       = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
}

/// State handlers of the effect child - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor05700_Fn0517C`.
const GpEnemyTaskFuncTable3 Actor05700_D0008C = {
    Actor05700_Fn031BC,
    Actor05700_Fn035FC,
    Actor05700_Fn051D8,
};

void Actor05700_Fn0517C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Teardown state of the effect child set up by `Actor05700_Fn031BC`: step 0
/// unlinks its three collision bodies and restarts the frame counter, step 1
/// destroys the child once 0x3D frames have passed.
void Actor05700_Fn051D8(GpEnemy* arg0, Task* arg1)
{
    Actor105700FxWork* work;
    u16                temp_v0;

    work = (Actor105700FxWork*)arg1->work;
    switch (work->field_EC) {
        case 0:
            Gp_UnlinkObj(&work->obj40);
            Gp_UnlinkObj(&work->obj78);
            Gp_UnlinkObj(&work->obj98);
            work->field_E8 = 0;
            work->field_EC = 1;
            return;
        case 1:
            temp_v0        = work->field_E8 + 1;
            work->field_E8 = temp_v0;
            if ((s16)temp_v0 >= 0x3D) {
                Gp_DestroyEnemy(arg0, arg1);
            }
            return;
    }
}

/// State handlers of the burst child parented to the actor's part 11 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05700_Fn05270`.
const GpEnemyTaskFuncTable3 Actor05700_D00098 = {
    Actor05700_Fn052CC,
    Actor05700_Fn05310,
    Gp_DestroyEnemy,
};

void Actor05700_Fn05270(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Spawn state of the burst child driven by `Actor05700_Fn05310`: parents the
/// child's root coordinate to part 11 of the actor's model, points the
/// child's model at the actor's light and colour matrices and advances to
/// state 1.
void Actor05700_Fn052CC(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor105700Work*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[11];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

/// The actor's own state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor05700_Fn05470`. The tick and
/// teardown take the task as the actor view it is.
const GpEnemyTaskFuncTable3 Actor05700_D000A4 = {
    Actor05700_Fn03CC4,
    Actor05700_Fn04338,
    Actor05700_Fn01A58,
};

/// Sound id the spawn cue is played against; the low byte comes from the
/// context block's room/channel bits.
extern s32 Actor05700_D17238;

/// Spawns the effect burst for the owner's coordinate, hands that coordinate
/// to the pan/depth sound cue, then parks the work block in state 2.
void Actor05700_Fn05310(GpEnemy* arg0, Task* arg1)
{
    Task*            owner;
    TmdObject*       obj;
    TmdObject*       ownerObj;
    Actor105700Work* work;
    GsCOORDINATE2*   coord;
    s16              state;
    s32              snd;
    s32              pan;

    owner      = arg1->parent;
    obj        = (TmdObject*)arg1->extra;
    ownerObj   = owner->extra;
    work       = owner->work;
    coord      = (GsCOORDINATE2*)obj->coords;
    obj->flags = ownerObj->flags;
    state      = work->field_6D2;

    switch (state) {
        case 0:
            coord->flg = 0;
            return;
        case 1:
            Gp_SpawnEff(0x6005C, coord, 0x10002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x01002600, NULL);
            Gp_SpawnEff(0x6005C, coord, 0x02002600, NULL);
            work->field_6D2 = 2;
            snd             = Actor05700_D17238 |
                  ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8);
            pan = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            return;
        case 2:
            arg1->state = state;
            return;
    }
}

void Actor05700_Fn05470(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05700_D000A4;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
