#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
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
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// 0xF0-byte body block `Actor05600_Fn031B0` parks at `Task::work`.
/// The two leading matrices are the light/colour pair published on the model
/// root's `TmdObject`; the three `GpObj` bodies collide against `rec60`
/// (shared by the first two) and, through the `GpActorD4Rec` between them,
/// `recD0`. `field_EE` mirrors the placement table's variant flag.
typedef struct Actor105600FxWork {
    /* 0x00 */ MATRIX       colorMtx;
    /* 0x20 */ MATRIX       lightMtx;
    /* 0x40 */ GpObj        obj40;
    /* 0x60 */ GpRec18      rec60[1];
    /* 0x78 */ GpObj        obj78;
    /* 0x98 */ GpObj        obj98;
    /* 0xB8 */ GpActorD4Rec d4rec;
    /* 0xD0 */ GpRec18      recD0[1];
    /* 0xE8 */ s16          field_E8;
    /* 0xEA */ s16          field_EA;
    /// Teardown step: 0 unlinks the three bodies, 1 counts `field_E8` up to
    /// the frame the task is destroyed on.
    /* 0xEC */ s16 field_EC;
    /* 0xEE */ s16 field_EE;
} Actor105600FxWork;
STATIC_ASSERT_SIZEOF(Actor105600FxWork, 0xF0);

/// 0x14-byte placement descriptor in the overlay's `.data`, handed to
/// `Gp_PackPair` as the source of the body objects' `GpObj.key`.
/// `field_E` is the variant flag `Actor05600_Fn031B0` latches into its
/// work block: it is 1 when the actor is placed normally, and anything else
/// puts the body in the other pose.
typedef struct Actor105600PlaceSrc {
    /* 0x00 */ GpU16Pair pair;
    /* 0x04 */ u16       field_4;
    /* 0x06 */ u16       field_6;
    /* 0x08 */ u16       field_8;
    /* 0x0A */ u16       field_A;
    /* 0x0C */ u16       field_C;
    /* 0x0E */ u16       field_E;
    /* 0x10 */ u16       field_10;
    /* 0x12 */ u16       field_12;
} Actor105600PlaceSrc;
STATIC_ASSERT_SIZEOF(Actor105600PlaceSrc, 0x14);

/// 0x38-byte scratch carved off `G_SCRATCH_HEAD` by
/// `Actor05600_Fn031B0`. `rot` first holds the local offset the root
/// coordinate is translated by (through `gte_rtv0` into `pos`), then the
/// placement angles `RotMatrix` turns into `mtx` for the three `rtir` column
/// transforms that overwrite the root coordinate's matrix.
typedef struct Actor105600PlaceScratch {
    /* 0x00 */ SVECTOR rot;
    /* 0x08 */ VECTOR  pos;
    /* 0x18 */ MATRIX  mtx;
} Actor105600PlaceScratch;
STATIC_ASSERT_SIZEOF(Actor105600PlaceScratch, 0x38);

/// 0x6E4-byte animation/state work block hung off the actor task's
/// `Task::work`, allocated by `Actor05600_Fn03924`. It opens with the
/// animation context and its nineteen 0x28-byte slots, carries the light and
/// colour matrices the model is drawn under and the five body objects, then
/// the halfwords the state handlers drive.
typedef struct Actor105600Work {
    /* 0x000 */ GpAnimCtx  ctx;
    /* 0x014 */ GpAnimSlot slots[19];
    /* 0x30C */ byte       field_30C[0x130];
    /// Colour and light matrices the model object's `colorMtx` / `lightMtx`
    /// are pointed at.
    /* 0x43C */ MATRIX field_43C;
    /* 0x45C */ MATRIX field_45C;
    /// The five collision/proximity list nodes the spawn handler links, each
    /// followed by the `GpRec18` table it walks. The first and last reach
    /// their tables through a `GpActorD4Rec`.
    /* 0x47C */ GpObj        field_47C;
    /* 0x49C */ GpActorD4Rec field_49C;
    /* 0x4B4 */ GpRec18      field_4B4[1];
    /* 0x4CC */ GpObj        field_4CC;
    /* 0x4EC */ GpRec18      field_4EC[5];
    /* 0x564 */ GpObj        field_564;
    /* 0x584 */ GpRec18      field_584[4];
    /* 0x5E4 */ GpObj        field_5E4;
    /* 0x604 */ GpRec18      field_604[1];
    /* 0x61C */ GpObj        field_61C;
    /* 0x63C */ GpActorD4Rec field_63C;
    /* 0x654 */ GpRec18      field_654[1];
    /// The spawn table the companion enemy comes from.
    /* 0x66C */ TaskDesc* field_66C;
    /// Record the hit effects are spawned with, placed under model part 3.
    /* 0x670 */ GpEffArg field_670;
    /// World position of the root coordinate as of the previous frame, saved
    /// before the per-frame drift below is applied.
    /* 0x678 */ s32  field_678;
    /* 0x67C */ s32  field_67C;
    /* 0x680 */ s32  field_680;
    /* 0x684 */ byte pad_684[4];
    /// Tilt angles a landed hit sets and `Actor05600_Fn016C4` decays toward
    /// zero while `field_6B4` is raised.
    /* 0x688 */ SVECTOR field_688;
    /// Effect whose task a landed hit sends to state 3 before dropping the
    /// pointer; nothing in this actor sets it.
    /* 0x690 */ struct GpEffWork* field_690;
    /// Animation index selected by the state machine.
    /* 0x694 */ s16 field_694;
    /// Animation the playing clip was started from; when it differs from
    /// `field_694` the frame counter is reset and the slots reseeded.
    /* 0x696 */ s16 field_696;
    /// Current frame of the playing clip.
    /* 0x698 */ s16 field_698;
    /// Frames before another weapon hit is taken, loaded from the hit's
    /// parameters and counted down every frame.
    /* 0x69A */ s16 field_69A;
    /* 0x69C */ s16 field_69C; ///< forward speed, applied along the root Z axis
                               /// Non-zero runs the turn helper, which turns by this much per frame.
    /* 0x69E */ s16 field_69E;
    /// Bits 4 and 5 of the animation record's flags as of the last frame;
    /// `Actor05600_Fn018D0` plays a cue when either drops.
    /* 0x6A0 */ u16 field_6A0;
    /* 0x6A2 */ s16 field_6A2; ///< yaw the root coordinate currently faces
    /* 0x6A4 */ s16 field_6A4; ///< yaw the actor wants to face
    /* 0x6A6 */ s16 field_6A6; ///< state-machine step, indexes the handler table
    /* 0x6A8 */ s16 field_6A8;
    /// Picks the clip `Actor05600_Fn0485C` plays: 1 starts animation
    /// 0x12, anything else 0x13.
    /* 0x6AA */ s16 field_6AA;
    /// Awake variant the actor was placed in (bit 0 of the placement record's
    /// `mode`); non-zero starts it on the longer approach.
    /* 0x6AC */ s16  field_6AC;
    /* 0x6AE */ s16  field_6AE;
    /* 0x6B0 */ byte pad_6B0[2];
    /* 0x6B2 */ s16  field_6B2; ///< non-zero forces the state-F0 path
    /* 0x6B4 */ s16  field_6B4; ///< non-zero runs the tilt helper
    /* 0x6B6 */ s16  field_6B6;
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ s16  field_6BA;
    /* 0x6BC */ s16  field_6BC;
    /* 0x6BE */ s16  field_6BE;
    /* 0x6C0 */ byte pad_6C0[2];
    /* 0x6C2 */ s16  field_6C2;
    /// Non-zero quarters the damage of a hit with key bit 0x8000.
    /* 0x6C4 */ s16  field_6C4;
    /* 0x6C6 */ byte pad_6C6[4];
    /* 0x6CA */ s16  field_6CA;
    /* 0x6CC */ s16  field_6CC;
    /* 0x6CE */ s16  field_6CE;
    /* 0x6D0 */ s16  field_6D0;
    /* 0x6D2 */ s16  field_6D2;
    /// Raised while the pose-change handlers play; while it is set a hit in
    /// the second pose does not switch to handler 0xC or 0xE.
    /* 0x6D4 */ s16 field_6D4;
    /// Streaming cue id for this room, looked up in `Actor05600_D16478`.
    /* 0x6D6 */ s16  field_6D6;
    /* 0x6D8 */ byte pad_6D8[2];
    /// Approach budget, 1000 per unit of the placement record's `variant`.
    /* 0x6DA */ s16  field_6DA;
    /* 0x6DC */ s16  field_6DC;
    /* 0x6DE */ s16  field_6DE; ///< below 2 the actor also drifts upward
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor105600Work;
STATIC_ASSERT_SIZEOF(Actor105600Work, 0x6E4);

/// Placement descriptor for this actor.
extern Actor105600PlaceSrc Actor05600_D161BC;

/// Pair source the approach cycle parks at `GpEnemy::param`; its `hpMax`
/// becomes the enemy's `field_40`.
extern GpPairSrcE Actor05600_D161D0[];

/// Per-stage tables of streaming cue ids, indexed by `GameSession::at4.loc.stage`
/// and then `GameSession::at4.loc.area`.
extern u16* Actor05600_D16478[];

/// Spawn table the approach cycle starts its companion enemy from, index 1.
extern TaskDesc Actor05600_D164A0[];

/// Animation stream set bound into the work block's animation context.
extern void Actor05600_D164C4;

/// Sound id of the burst cue, with the spawn context's room/channel bits packed
/// in.
extern s32 Actor05600_D162F0;

/// 0x40-byte scratch carved off `G_SCRATCH_HEAD` by `Actor05600_Fn000A4`.
typedef struct Actor105600HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         normal;
    /* 0x20 */ VECTOR         push;
    /* 0x30 */ SVECTOR        effOfs;
    /* 0x38 */ SVECTOR        target;
} Actor105600HitScratch;
STATIC_ASSERT_SIZEOF(Actor105600HitScratch, 0x40);

/// `G_SCRATCH_HEAD` viewed as a struct. The member access, not a plain `u32`
/// dereference, is what `Actor05600_Fn016C4` needs to schedule its
/// argument setup around `RotMatrix`.
typedef struct Actor105600ScratchStack {
    u32 sp;
} Actor105600ScratchStack;

/// 0x40-byte scratch carved off `G_SCRATCH_HEAD` by `Actor05600_Fn02548`:
/// the converted matrix, the `gte_rtv0` output and the two vectors fed through
/// it (`rot` and `vec` are also the pair handed to `Actor05600_Fn02950`).
typedef struct Actor105600AimScratch {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ VECTOR  pos;
    /* 0x30 */ SVECTOR rot;
    /* 0x38 */ SVECTOR vec;
} Actor105600AimScratch;
STATIC_ASSERT_SIZEOF(Actor105600AimScratch, 0x40);

/// 0x48-byte scratch carved off `G_SCRATCH_HEAD` by `Actor05600_Fn02950`:
/// the beam is walked in eight steps from `vec` to `rot`, each step projected
/// into `cur` (packed screen xy) and `curZ` (OTZ). `xs`/`ys` hold the two
/// projected ends followed by the four offset corners the ribbon polygons are
/// cut from.
typedef struct Actor105600BeamScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR pt;
    /* 0x18 */ SVECTOR step;
    /* 0x20 */ s32     prev;
    /* 0x24 */ s32     cur;
    /* 0x28 */ s32     prevZ;
    /* 0x2C */ s32     curZ;
    /* 0x30 */ s16     xs[6];
    /* 0x3C */ s16     ys[6];
} Actor105600BeamScratch;
STATIC_ASSERT_SIZEOF(Actor105600BeamScratch, 0x48);

/// Flag byte whose writer lies outside this actor; the proximity check reacts
/// to three groups of its bits.
extern u8 D_801153F2;

/// Nonzero sends the idle handlers to handler 2 with animation 2; the
/// teardown raises it.
extern s8 D_80115419;

/// Nonzero parks the actor instead of running its state machine: 1 draws the
/// body where it stands and 2 hides it.
extern u8 D_801153F4;

/// Frame counts of the actor's animations, indexed by `Actor105600Work.field_694`.
extern s16 Actor05600_D04CFC[];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 Actor05600_D161E0[];
extern s16 Actor05600_D1623C[];

/// Sound ids of the actor's cues, indexed from `Actor105600Work.field_6D6`.
extern s32 Actor05600_D162AC[];

/// The approach cycle's per-state handlers, indexed by `Actor105600Work.field_6A6`.
extern TaskFunc Actor05600_D16540[];

void Gp_AnimTickIndex(GpAnimCtx* arg0, s32 arg1);
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

void Actor05600_Fn00CFC(Task* arg0);
void Actor05600_Fn02548(Task* arg0);
void Actor05600_Fn02950(Task* arg0, SVECTOR* arg1, SVECTOR* arg2);
s32  Actor05600_Fn045E4(SVECTOR* arg0, SVECTOR* arg1);
void Actor05600_Fn04ACC(GpEnemy* arg0, Task* task);
void Actor05600_Fn04B10(GpEnemy* enemy, Task* task);
void Actor05600_Fn04C08(GpEnemy* arg0, Task* arg1);

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void Actor05600_Fn000A4(Task* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor105600Work*       work;
    GpDeltaScratch*        head;
    Actor105600HitScratch* scratch;
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
    work                                      = (Actor105600Work*)arg0->work;
    head                                      = *(GpDeltaScratch**)G_SCRATCH_HEAD;
    self                                      = ((TmdObject*)arg0->extra)->coords;
    *(Actor105600HitScratch**)G_SCRATCH_HEAD -= 1;
    scratch                                   = *(Actor105600HitScratch**)G_SCRATCH_HEAD;
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
                        if (Actor05600_D1623C[work->field_4EC[i].key & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (Actor05600_D161E0[work->field_4EC[i].key & 0x7F] != 0) {
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
        if (Actor05600_Fn045E4(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x40;
}

/// Idle approach handler, entry 1 of `Actor05600_D16540`. State 0 drains the
/// `field_6DA` budget by `field_69C` (0 while `field_698` is under the
/// clip's frame count in `Actor05600_D04CFC`, 0x14 once past it) and runs
/// the proximity check every frame; when the budget runs out it switches to
/// animation 4 and state 1. State 1 waits for frame 0x60, then either falls
/// back to animation 2 (budget left) or turns round: animation 3, state 2, a
/// fresh budget of 1000 per unit of the placement record's `variant`, and
/// `field_6A2` / `field_6A4` set to the current yaw and its opposite. State 2
/// turns at 0x3B per frame until frame 0x23, then returns to animation 2 and
/// state 0. A set `field_6B2` or `D_80115419` overrides everything with
/// animation 2, handler 2 and the shared state-F0 slot.
void Actor05600_Fn00B18(Task* arg0)
{
    GpEnemy*         spawn;
    Actor105600Work* work;
    GsCOORDINATE2*   self;
    u8*              head;
    s16              state;
    s16              delta;
    s32              ang;
    s32              param;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;

    self  = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor105600Work*)arg0->work;
    spawn = (GpEnemy*)arg0->spawnArg2;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= Actor05600_D04CFC[work->field_694]) {
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
            Actor05600_Fn00CFC(arg0);
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

/// Proximity check: measures the player's horizontal distance from the root
/// coordinate in a 0x10-byte block carved off the scratch head. Under 0x5DC,
/// one group of `D_801153F2` bits raises `field_6B2`; past it the other two
/// groups (the second only within 0xBB8) switch to animation 4 and state 1.
void Actor05600_Fn00CFC(Task* arg0)
{
    Actor105600Work* work;
    GsCOORDINATE2*   self;
    s32              dx;
    s32              distance;
    s32              dz;
    s32              trigger;
    VECTOR*          head;
    VECTOR*          delta;

    self                      = ((TmdObject*)arg0->extra)->coords;
    work                      = (Actor105600Work*)arg0->work;
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

/// Entry 0xB of `Actor05600_D16540`: state 0 picks the pose from `field_6AA`
/// and arms the body objects; states 1 and 2 play cues at fixed frames and,
/// once their clip is done, either roll an idle length or, with the enemy's
/// hit points gone, advance the task to state 2; states 3 and 4 alternate
/// the two idle clips until each length runs out.
void Actor05600_Fn00E38(Task* arg0)
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
    Actor105600Work* work;
    GsCOORDINATE2*   self;

    work  = (Actor105600Work*)arg0->work;
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
                snd = Actor05600_D162AC[work->field_6D6 + 0xC] | (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = Actor05600_D162AC[work->field_6D6 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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
                snd  = Actor05600_D162AC[work->field_6D6 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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

/// Entry 0xC of `Actor05600_D16540`: state 0 starts the clip the `field_6B8`
/// pose picks, and states 1 and 2 wait for frame 0x10 / 0x16 before handing
/// over to handler 0xB in its idle state 3 with animation 0x19 / 0x1D and an
/// idle length rolled from `Gp_LcgState`.
void Actor05600_Fn01214(Task* arg0)
{
    Actor105600Work* work;
    s16              state;
    s32              next;

    work  = (Actor105600Work*)arg0->work;
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

/// Entry 0xD of `Actor05600_D16540`: state 0 picks the pose from `field_6AA`,
/// with its clip, frame budget and body-object setup; state 1 plays the pose's
/// cues at their frame marks and, once the budget runs out, advances the task
/// to state 2.
void Actor05600_Fn0130C(Task* arg0)
{
    Actor105600Work* work;
    GsCOORDINATE2*   self;
    s32              snd;
    s16              state;

    work  = (Actor105600Work*)arg0->work;
    self  = ((TmdObject*)arg0->extra)->coords;
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

                    snd = Actor05600_D162AC[work->field_6D6 + 0xC] |
                          (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = Actor05600_D162AC[work->field_6D6 + 8] |
                          (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = Actor05600_D162AC[work->field_6D6 + 8] |
                      (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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

/// Turns the root coordinate toward `field_6A4` by `field_69E` per frame:
/// within half a turn it closes on the target directly (or, for animation 3,
/// always steps down by the turn rate), past that it goes the long way round,
/// snapping onto the target once a step would overshoot. The resulting yaw,
/// kept in `field_6A2`, rebuilds the coordinate's matrix.
void Actor05600_Fn01538(Task* arg0)
{
    Actor105600Work* work;
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
    work  = (Actor105600Work*)arg0->work;
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
void Actor05600_Fn016C4(Task* arg0)
{
    Actor105600Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                         = (MATRIX*)(((Actor105600ScratchStack*)G_SCRATCH_HEAD)->sp - 0x20);
    ((Actor105600ScratchStack*)G_SCRATCH_HEAD)->sp = (u32)matrix;
    active                                         = 0;
    work                                           = (Actor105600Work*)arg0->work;
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
void Actor05600_Fn018D0(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105600Work* work;
    GsCOORDINATE2*   self;
    GpAnimRec*       rec;

    work = (Actor105600Work*)arg0->work;
    self = ((TmdObject*)arg0->extra)->coords;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(&work->ctx, (GpAnimSlot*)&work->slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & 0x20) && (work->field_6A0 & 0x20)) {
                snd = Actor05600_D162AC[work->field_6D6 * 2 - 1] |
                      (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
            }
            if (!(rec->flags & 0x10) && (work->field_6A0 & 0x10)) {
                snd = Actor05600_D162AC[work->field_6D6 * 2] |
                      (((u16)((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
            }
            work->field_6A0 = (u16)(rec->flags & 0x30);
        }
    }
}

/// Teardown state of the actor, entry 2 of `Actor05600_D00098`.
/// `D_801153F4` overrides it: 0 shows the body and lets the tick run, 1 only
/// refreshes the colour and ground shadow, 2 hides the body. State 0 unlinks
/// the body objects (the fifth only for the 0x38 / 0x39 variants), hands the
/// variant to `Gp_ReleaseStateF0Add`, picks the collapse clip for the pose,
/// saves the enemy pose and raises `D_80115419`; state 1 spawns a ground
/// effect every fourth frame. The tail advances the animation slots and
/// redraws the colour and ground shadow.
void Actor05600_Fn01A4C(GpEnemy* arg0, Task* arg1)
{
    Actor105600Work* work;
    Actor105600Work* animWork;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    GsCOORDINATE2*   part;
    SVECTOR*         scratch;
    VECTOR3          pos;
    s16              anim;
    s16              duration;
    s32              i;
    u32              random;

    work    = (Actor105600Work*)arg1->work;
    coord   = ((TmdObject*)arg1->extra)->coords;
    scratch = (SVECTOR*)(*(u8**)G_SCRATCH_HEAD -= 8);
    switch (D_801153F4) {
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
            Gp_UpdateActorColor((GpEnemy*)arg1->spawnArg2, (VECTOR*)&pos, 0, 0);
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
    animWork = (Actor105600Work*)arg1->work;
    i        = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0U;
        duration            = Actor05600_D04CFC[animWork->field_694];
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
    Gp_UpdateActorColor((GpEnemy*)arg1->spawnArg2, (VECTOR*)&pos, 0, 0);
    root   = ((TmdObject*)arg1->extra)->coords;
    part   = &root[3];
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 8;
}

/// Approach-cycle state machine, entry 6 of `Actor05600_D16540` for the
/// second half of the fight. State 0 waits out the opening clip; state 1 backs
/// away while tracking the player and running the aim helper, for 0x1E frames;
/// state 2 measures the distance and yaw error to the companion in slot 3 and
/// either breaks off (too close) or commits to the lunge; state 3 keeps facing
/// the companion, counts the strikes in `field_6BC` / `field_6BE` and picks the
/// follow-up clip from them; state 4 fires the effect burst; states 5 and 6
/// hand back to the other handlers. The delta vector and its normal are carved
/// off `G_SCRATCH_HEAD` and released on the way out.
void Actor05600_Fn01E1C(Task* arg0)
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
    Actor105600Work* work;
    GsCOORDINATE2*   coord;

    delta = (VECTOR*)(*(u8**)G_SCRATCH_HEAD -= 0x20);
    work  = (Actor105600Work*)arg0->work;
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
            Actor05600_Fn02548(arg0);
            work->field_6AE++;
            if (work->field_6AE >= 0x1E) {
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
            if (work->field_6BE != 0 && work->field_698 == Actor05600_D04CFC[13] - 1) {
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
                if (work->field_698 >= Actor05600_D04CFC[13] + 0x16) {
                    work->field_6A8       = 4;
                    work->field_694       = 0xF;
                    work->field_6BC       = 0;
                    work->field_6BE       = 0;
                    Actor05600_D04CFC[13] = 0;
                    work->field_6CC       = 0;
                }
            } else if (work->field_6BE < 6) {
                if (work->field_698 >= Actor05600_D04CFC[13] + 3) {
                    Actor05600_D04CFC[13] = 3;
                    work->field_694       = 0xD;
                    work->field_696       = 0x1E;
                }
            } else if (work->field_698 >= Actor05600_D04CFC[13] + 0x16) {
                work->field_6A8       = 1;
                work->field_6BE       = 0;
                work->field_694       = 0x1E;
                Actor05600_D04CFC[13] = 0;
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

/// Runs the animation's mark events: measures `field_698` against the three
/// frames `Actor05600_D04CFC[field_694]` marks out. At the 0x1C mark the
/// body object is packed from `Actor05600_D161BC` and bit 0x8000 raised,
/// at 0x28 dropped; inside the 0x1C..0x1E window the player's distance decides
/// whether `field_69C` parks at 0x64; and past 0x7A the actor hands over to
/// animation 4. The delta the distance is taken from is left in the scratch
/// vector it is accumulated in.
void Actor05600_Fn023A0(Task* arg0)
{
    Actor105600Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;
    s16              anim;
    s32              dx;
    s32              dz;
    s32              distance;

    *(VECTOR**)G_SCRATCH_HEAD -= 1;
    delta                      = *(VECTOR**)G_SCRATCH_HEAD;
    work                       = (Actor105600Work*)arg0->work;
    anim                       = Actor05600_D04CFC[work->field_694];
    self                       = ((TmdObject*)arg0->extra)->coords;
    if (work->field_698 == anim + 0x1C) {
        work->field_5E4.key    = Gp_PackPair(&Actor05600_D161BC.pair, 4);
        work->field_5E4.flags |= 0x8000;
    } else if (work->field_698 == anim + 0x28) {
        work->field_5E4.flags &= 0x7FFF;
    }
    anim = Actor05600_D04CFC[work->field_694];
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
    if (work->field_698 >= Actor05600_D04CFC[work->field_694] + 0x7A) {
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
/// the rotated result and the parked point go to `Actor05600_Fn02950`.
void Actor05600_Fn02548(Task* arg0)
{
    Actor105600AimScratch* scratch;
    Actor105600Work*       work;
    GsCOORDINATE2*         self;

    scratch     = (Actor105600AimScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x40);
    self        = ((TmdObject*)arg0->extra)->coords;
    work        = (Actor105600Work*)arg0->work;
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
    Actor05600_Fn02950(arg0, &scratch->rot, &scratch->vec);
    *(u8**)G_SCRATCH_HEAD += 0x40;
}

/// Corner indices of the two ribbon polygons in the beam scratch's
/// projected-point arrays.
extern s16 Actor05600_D16490[][4];

/// Draws the aim beam from `arg2` to `arg1` in eight projected steps. Each
/// step nearer than OTZ 30 is skipped; otherwise the segment's screen normal
/// (`VectorNormalS`) offsets the ends by a depth-scaled width into two
/// semi-transparent red-to-black `POLY_G4`s (corner order from
/// `Actor05600_D16490`), a red `LINE_F2` core and a blend `DR_TPAGE`.
void Actor05600_Fn02950(Task* arg0, SVECTOR* arg1, SVECTOR* arg2)
{
    Actor105600BeamScratch* s;
    GsCOORDINATE2*          self;
    POLY_G4*                poly;
    LINE_F2*                line;
    DR_TPAGE*               page;
    s32                     i;
    s32                     j;
    s32                     depth;

    s          = (Actor105600BeamScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x48);
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
            poly->x0 = s->xs[Actor05600_D16490[j][0]];
            poly->y0 = s->ys[Actor05600_D16490[j][0]];
            poly->x1 = s->xs[Actor05600_D16490[j][1]];
            poly->y1 = s->ys[Actor05600_D16490[j][1]];
            poly->x2 = s->xs[Actor05600_D16490[j][2]];
            poly->y2 = s->ys[Actor05600_D16490[j][2]];
            poly->x3 = s->xs[Actor05600_D16490[j][3]];
            poly->y3 = s->ys[Actor05600_D16490[j][3]];
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
/// packs the room/channel bits of the spawn context into
/// `Actor05600_D162F0`.
void Actor05600_Fn031B0(GpEnemy* arg0, Task* arg1)
{
    Actor105600FxWork*       work;
    Actor105600PlaceScratch* scratch;
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
    scratch       = (Actor105600PlaceScratch*)(*(u8**)G_SCRATCH_HEAD -= 0x38);
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

    work->field_EE = (Actor05600_D161BC.field_E != 1);

    work->obj40.coord    = coord;
    work->obj40.ctx.recs = work->rec60;
    work->obj40.pos.vx   = 0;
    work->obj40.pos.vy   = 0;
    work->obj40.pos.vz   = 0;
    work->obj40.key      = Gp_PackPair(&Actor05600_D161BC.pair, 3);
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
    sound = Actor05600_D162F0 | (((u16)ctx->placeKey >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));

    *(u8**)G_SCRATCH_HEAD += 0x38;
}

/// Effect id the burst at the end of the child's flight is spawned with.
extern s32 D_80115750;

/// Sound id of the burst cue, with the spawn context's room/channel bits
/// packed in.
extern s32 Actor05600_D162F4;

/// Per-frame state of the effect child set up by `Actor05600_Fn031B0`, entry
/// 1 of `Actor05600_D0008C`. `D_801153F4` overrides it: 0 shows the child and
/// runs the tick, 1 only refreshes its colour, 2 hides it. The tick moves the
/// child along its own Y axis, spawns a puff every fourth frame and ends the
/// flight on a body contact, a room face without the `field_1` flag, or after
/// 0x5A frames: the burst effect and cue play, the child hides and advances
/// to state 2, and a kind-1 body contact also starts a pad rumble.
void Actor05600_Fn035F0(GpEnemy* arg0, Task* arg1)
{
    Actor105600FxWork* work;
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
    work  = (Actor105600FxWork*)arg1->work;
    found = 0;
    switch (D_801153F4) {
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
        sound                            = Actor05600_D162F4 | (((u16)ctx->placeKey >> 0xC) << 8);
        pan                              = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
        arg1->state = 2;
        if ((work->rec60[0].key & 0xFFFF0080) == 0x10000) {
            Gp_SpawnPadLerp(0xA, 0xFF, 8);
        }
    }
    *(u8**)G_SCRATCH_HEAD += 0x28;
}

/// Spawn handler of the approach cycle: allocates the 0x6E4-byte work block,
/// binds the animation set and reseeds the nineteen slots, then starts the
/// companion enemy whose model takes its texture page and CLUT row from the
/// current room's area record. `GpEnemy::spawnState` picks how much of that is
/// kept: 0 also links the list node, the five `Gp_LinkObj` collision nodes with
/// their `GpRec18` tables and the room's streaming cue, while 1 and 2 only
/// prime the animation state. Entry 0 of `Actor05600_D00098`.
void Actor05600_Fn03924(GpEnemy* ctx, Task* actor)
{
    Actor105600Work* work;
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
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    GpAreaKey        key;
    s32              i;
    s32              one;
    s32              kind;
    s32              idx;
    s32              param;

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
    work->field_6CA            = 0x38;
    work->field_66C            = Actor05600_D164A0;
    work->field_670.coord      = &((TmdObject*)actor->extra)->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->ctx, &Actor05600_D164C4, obj, work->field_30C, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->ctx, i, 1);
    }
    eff        = Gp_SpawnEnemyFromTable(Actor05600_D164A0, 1, 0, ctx);
    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    model      = eff->task->extra;
    idx        = ctx->placeKey >> 12;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
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
    ctx->param      = Actor05600_D161D0;
    ctx->recs       = work->field_4EC;
    ctx->coord      = &parts[3];
    ctx->hp         = Actor05600_D161D0->hpMax;
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

    tbl = Actor05600_D16478[gGameSession->at4.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->at4.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x38;
        param2[3] = 0;
        param2[2] = 0;
        param2[1] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }

    work->field_49C.end0.vz    = 0x1F40;
    work->field_49C.end0Radius = 0x3E8;
    work->field_49C.end0.vx    = 0;
    work->field_49C.end0.vy    = 0;
    work->field_49C.end1.vx    = 0;
    work->field_49C.end1.vy    = 0;
    work->field_49C.end1.vz    = 0;
    work->field_49C.end1Radius = 0x5DC;
    work->field_49C.recs       = work->field_4B4;
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
    work->field_4CC.key      = 0x30038;
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
    work->field_5E4.flags &= 0x7FFF;

    work->field_63C.end0.vx    = 0;
    work->field_63C.end0.vy    = 0;
    work->field_63C.end0.vz    = 0;
    work->field_63C.end1.vx    = 0;
    work->field_63C.end1.vy    = 0;
    work->field_63C.end1.vz    = 0;
    work->field_63C.end0Radius = 1;
    work->field_63C.end1Radius = 1;
    work->field_63C.recs       = work->field_654;
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

/// Per-frame tick of the approach cycle: runs the collision and state handlers,
/// drifts the root coordinate forward along its Z axis (and upward while
/// `field_6DE` is below 2), reseeds or ticks the nineteen animation slots, then
/// publishes the body's colour and its ground shadow. `D_801153F4` overrides
/// the whole state machine - 1 draws the body without advancing it and 2
/// hides it. Entry 1 of `Actor05600_D00098`.
void Actor05600_Fn03EBC(GpEnemy* ctx, Task* actor)
{
    VECTOR3          pos;
    GpEnemy*         spawn;
    TmdObject*       model;
    Actor105600Work* moveWork;
    Actor105600Work* animWork;
    Actor105600Work* work;
    Actor105600Work* flagWork;
    GsCOORDINATE2*   moveCoord;
    GsCOORDINATE2*   part;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   root;
    s16              duration;
    s32              i;
    u8               flags;

    work  = (Actor105600Work*)actor->work;
    model = (TmdObject*)actor->extra;
    coord = model->coords;
    switch (D_801153F4) {
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
        spawn    = (GpEnemy*)actor->spawnArg2;
        flags    = spawn->reactionFlags;
        flagWork = (Actor105600Work*)actor->work;
        if ((flags & 2) && (flagWork->field_6B8 == 0)) {
            spawn->reactionFlags = flags & 0xFD;
            flagWork->field_6A6  = 0xA;
            flagWork->field_694  = 0x14;
            flagWork->field_6A8  = 0;
            flagWork->field_6E0  = 1;
        }
    }
    Actor05600_Fn000A4(actor);
    Actor05600_D16540[work->field_6A6](actor);
    if (work->field_69E != 0) {
        Actor05600_Fn01538(actor);
    }
    moveCoord              = ((TmdObject*)actor->extra)->coords;
    moveWork               = (Actor105600Work*)actor->work;
    moveWork->field_678    = moveCoord->coord.t[0];
    moveWork->field_67C    = moveCoord->coord.t[1];
    moveWork->field_680    = moveCoord->coord.t[2];
    moveCoord->coord.t[0] += (s32)(moveCoord->coord.m[0][2] * moveWork->field_69C) >> 0xC;
    if (moveWork->field_6DE < 2) {
        moveCoord->coord.t[1] += 0x80;
    }
    moveCoord->coord.t[2] += (s32)(moveCoord->coord.m[2][2] * moveWork->field_69C) >> 0xC;
    animWork               = (Actor105600Work*)actor->work;
    i                      = 1;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0;
        duration            = Actor05600_D04CFC[animWork->field_694];
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
        Actor05600_Fn016C4(actor);
    }
    Actor05600_Fn018D0(actor);
    coord->flg                                = 0;
    ((TmdObject*)actor->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(coord);
draw:
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)actor->spawnArg2, (VECTOR*)&pos, 0, 0);
    root   = ((TmdObject*)actor->extra)->coords;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}

/// Approach-cycle state machine, entry 6 of `Actor05600_D16540`. State 0
/// turns the actor toward the player, handing over to the charge animation once
/// the clip has run and the yaw error is wide (or to the recovery animation when
/// `field_6DC` is set); state 1 picks the close or far attack from the distance
/// to the player; state 2 waits out its clip before turning again; state 3 arms
/// `field_6DC` and drops back to state 0.
void Actor05600_Fn041E4(Task* arg0)
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
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor105600Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = (Actor105600Work*)arg0->work;
    state                 = work->field_6A8;
    coord                 = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= Actor05600_D04CFC[work->field_694]) {
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
                work->field_6A6 = 6;
                work->field_6A8 = 0;
                work->field_694 = 0xC;
                work->field_6AE = 0;
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
s32 Actor05600_Fn045E4(SVECTOR* arg0, SVECTOR* arg1)
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

/// Entry 0 of `Actor05600_D16540`: state 0 counts `field_6AE` up to 0x5B
/// frames, running the proximity check meanwhile, then switches to animation
/// 4 and state 1; state 1 waits for frame 0x5E and returns to animation 1 and
/// state 0. A set `field_6B2` or `D_80115419` overrides everything with
/// animation 2, handler 2 and the shared state-F0 slot.
void Actor05600_Fn046F0(Task* arg0)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_6AE++;
            if (work->field_6AE >= 0x5B) {
                work->field_694 = 4;
                work->field_6AE = 0;
                work->field_6A8 = 1;
            }
            Actor05600_Fn00CFC(arg0);
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

/// Hit-reaction state, entry 8 of `Actor05600_D16540`: step 0 starts
/// animation 0x11 and stops the actor; step 1 waits for frame 0x37, then parks
/// on animation 2 (entry 2) or, with `field_6E0` set, on animation 0x14
/// (entry 0xA).
void Actor05600_Fn047D0(Task* task)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)task->work;
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

/// Entry 9 of `Actor05600_D16540`: step 0 starts animation 0x12 when
/// `field_6AA` is 1 (step 1, waits for frame 0x50) and animation 0x13
/// otherwise (step 2, waits for frame 0x3B); either way the actor stops, and
/// it parks on animation 2 (entry 2) when done.
void Actor05600_Fn0485C(Task* task)
{
    Actor105600Work* work;
    s32              state;
    s32              next;

    work  = (Actor105600Work*)task->work;
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

/// Entry 0xA of `Actor05600_D16540`: step 0 waits for `Gp_TickObjFlag2` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `field_6E0`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void Actor05600_Fn04924(Task* task)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(task->spawnArg2) != 0) {
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

/// Entry 0xE of `Actor05600_D16540`: step 0 starts animation 0x17 when
/// `field_6B8` is 1 (step 1, waits for frame 0x10) and animation 0x1B
/// otherwise (step 2, waits for frame 0x16); when done the task advances to
/// state 2.
void Actor05600_Fn049B0(Task* task)
{
    Actor105600Work* work;
    s32              sel;
    s16              state;

    work  = (Actor105600Work*)task->work;
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
                task->state     = 2;
                work->field_6A8 = 0;
            }
            return;
        case 2:
            if (work->field_698 >= 0x16) {
                task->state     = state;
                work->field_6A8 = 0;
            }
            return;
    }
}

void Actor05600_Fn04A68(void)
{
}

/// State handlers of the model child hung off the actor's part 7 - spawn,
/// per-frame tick and teardown - dispatched through by `Actor05600_Fn04A70`.
const GpEnemyTaskFuncTable3 Actor05600_D00080 = {
    Actor05600_Fn04ACC,
    Actor05600_Fn04B10,
    Gp_DestroyEnemy,
};

void Actor05600_Fn04A70(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D00080;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Spawn state of the model child hung off the actor: parents the child's
/// root coordinate to part 7 of the actor's model, points the child's model
/// at the actor's light and colour matrices and advances to state 1.
void Actor05600_Fn04ACC(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105600Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor105600Work*)parent->work;

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
void Actor05600_Fn04B10(GpEnemy* enemy, Task* task)
{
    Actor105600Work* work;
    GpEnemy*         spawned;
    TmdObject*       src;
    TmdObject*       dst;

    work                             = (Actor105600Work*)task->parent->work;
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

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor05600_D0008C = {
    Actor05600_Fn031B0,
    Actor05600_Fn035F0,
    Actor05600_Fn04C08,
};

void Actor05600_Fn04BAC(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D0008C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Teardown state of the effect child set up by `Actor05600_Fn031B0`: step 0
/// unlinks its three collision bodies and restarts the frame counter, step 1
/// destroys the child once 0x3D frames have passed.
void Actor05600_Fn04C08(GpEnemy* arg0, Task* arg1)
{
    Actor105600FxWork* work;
    u16                temp_v0;

    work = (Actor105600FxWork*)arg1->work;
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

/// The enemy's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
const GpEnemyTaskFuncTable3 Actor05600_D00098 = {
    Actor05600_Fn03924,
    Actor05600_Fn03EBC,
    Actor05600_Fn01A4C,
};

void Actor05600_Fn04CA0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor05600_D00098;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
