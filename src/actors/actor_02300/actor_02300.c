#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "decomp/common.h"

#include "actors/actor.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// Per-instance work block this overlay's task holds in `Task::work`, the
/// same shape the other enemy overlays give theirs:
/// `field_694` is the current animation id, `field_698` the frame counter the
/// state handlers compare against the per-animation start frame table
/// `Actor02300_D03F44`, and `field_6A8` the state the frame dispatcher
/// switches on.
typedef struct Actor102300Work {
    /// Animation context `func_800B3F84` fills in, followed by the nineteen
    /// slots and the pose buffer it is handed.
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x13];
    /* 0x30C */ byte       poses[0x130];
    /// Light and colour matrices the model object is pointed at, in place of
    /// the defaults `Tmd_SetupDraw` would otherwise load.
    /* 0x43C */ MATRIX field_43C;
    /* 0x45C */ MATRIX field_45C;
    /// The four list nodes the spawn handler links, each with its own
    /// `GpRec18` table. `field_47C` goes on list 3 through the bounding-box
    /// record at `field_49C`; the other three point straight at their tables.
    /* 0x47C */ GpObj        field_47C;
    /* 0x49C */ GpActorD4Rec field_49C;
    /* 0x4B4 */ GpRec18      field_4B4[1];
    /* 0x4CC */ GpObj        field_4CC;
    /* 0x4EC */ GpRec18      field_4EC[5];
    /* 0x564 */ GpObj        field_564;
    /* 0x584 */ GpRec18      field_584[4];
    /// Collision/proximity list node, the slot the lunge raises bit 0x8000 of
    /// and parks its `Gp_PackPair` entry in.
    /* 0x5E4 */ GpObj   field_5E4;
    /* 0x604 */ GpRec18 field_604[1];
    /// Fifth list node, unlinked with the others by the teardown state when
    /// `field_6CA` is 0x38 or 0x39.
    /* 0x61C */ GpObj field_61C;
    /* 0x63C */ byte  pad_63C[0x30];
    /// The spawn table this overlay's enemies come from, kept for the state
    /// handlers to respawn through.
    /* 0x66C */ TaskDesc* field_66C;
    /* 0x670 */ GpEffArg  field_670;
    /// Root coordinate the collision tick restores when a push-out resolves
    /// back to the recorded spawn position.
    /* 0x678 */ s32  field_678;
    /* 0x67C */ s32  field_67C;
    /* 0x680 */ s32  field_680;
    /* 0x684 */ byte pad_684[4];
    /// Hit tilt, seeded from the LCG when a hit lands with no reaction state.
    /* 0x688 */ SVECTOR           field_688;
    /* 0x690 */ struct GpEffWork* field_690;
    /* 0x694 */ s16               field_694;
    /// Animation the slots were last seeded for; when `field_694` differs the
    /// frame counter restarts and the slots are reseeded.
    /* 0x696 */ s16 field_696;
    /* 0x698 */ s16 field_698;
    /// Hit cooldown in frames, seeded from `Gp_GetIdParam2`; while non-zero the
    /// weapon-hit records are ignored.
    /* 0x69A */ s16 field_69A;
    /* 0x69C */ s16 field_69C;
    /* 0x69E */ s16 field_69E;
    /// Cue bits 0x20 / 0x10 of the playing animation record, kept from the
    /// previous frame so each cue plays once on its falling edge.
    /* 0x6A0 */ u16 field_6A0;
    /// Facing angle the lunge steers `field_6A4` towards; the tick compares the
    /// two and only commits once they are within 0x100.
    /* 0x6A2 */ s16 field_6A2;
    /* 0x6A4 */ s16 field_6A4;
    /* 0x6A6 */ s16 field_6A6;
    /* 0x6A8 */ s16 field_6A8;
    /// 1 when the attacker is in front of this enemy, 0 behind; taken from the
    /// sign of the hit vector against the root's forward axis.
    /* 0x6AA */ s16 field_6AA;
    /// Awake variant this enemy starts in, taken from bit 0 of the placement
    /// record's `mode`.
    /* 0x6AC */ s16 field_6AC;
    /* 0x6AE */ s16 field_6AE;
    /// Frame counter of the per-frame tick's dust puffs: one every third frame.
    /* 0x6B0 */ s16 field_6B0;
    /// Raised by the collision node once the lunge connects; the state
    /// handlers check it to break out of the approach cycle.
    /* 0x6B2 */ s16 field_6B2;
    /// Raised with `field_688` when a hit lands and no reaction animation is
    /// selected; the tilt decay clears it.
    /* 0x6B4 */ s16 field_6B4;
    /// Frames spent in the current lunge cycle; at 0x4C the tick gives up and
    /// falls back to animation 8.
    /* 0x6B6 */ s16 field_6B6;
    /// Non-zero selects the second reaction family (animations 4/6/0xC/0xE).
    /* 0x6B8 */ s16  field_6B8;
    /* 0x6BA */ byte pad_6BA[6];
    /// Shift count for the idle-to-lunge draw: each cycle widens the LCG mask
    /// by one bit, so the enemy grows less likely to lunge again.
    /* 0x6C0 */ s16 field_6C0;
    /* 0x6C2 */ s16 field_6C2;
    /// 1 or 2, picked from bit 16 of the next LCG draw.
    /* 0x6C4 */ s16  field_6C4;
    /* 0x6C6 */ byte pad_6C6[4];
    /* 0x6CA */ s16  field_6CA;
    /* 0x6CC */ s16  field_6CC;
    /* 0x6CE */ s16  field_6CE;
    /* 0x6D0 */ s16  field_6D0;
    /* 0x6D2 */ s16  field_6D2;
    /// Non-zero suppresses the two reaction animations that would otherwise
    /// interrupt the one already playing.
    /* 0x6D4 */ s16 field_6D4;
    /// Sector id of the enemy's voice stream, looked up per room from
    /// `Actor02300_D15C80` and queued with `CdCmd_Enqueue(0x21, ...)`.
    /* 0x6D6 */ s16 field_6D6;
    /// Countdown the part-7 child's spawn state seeds and its tick drains; the
    /// effect spawns on the frame it reaches zero.
    /* 0x6D8 */ s16 field_6D8;
    /// Dwell budget in thousandths, scaled by the placement record's `variant`.
    /* 0x6DA */ s16 field_6DA;
    /* 0x6DC */ s16 field_6DC;
    /// Step gate of the approach-cycle states: set to 1 on state-0 entry and
    /// to 2 once state 1 has run; below 2 the per-frame tick adds 0x80 to the
    /// root coordinate's y.
    /* 0x6DE */ s16 field_6DE;
    /// Non-zero suppresses the crit and knock-back reactions.
    /* 0x6E0 */ s16  field_6E0;
    /* 0x6E2 */ byte pad_6E2[2];
} Actor102300Work;
STATIC_ASSERT_SIZEOF(Actor102300Work, 0x6E4);

/// 0x40-byte scratch the hit tick claims from `G_SCRATCH_HEAD`: the collision
/// delta `func_800E0C10` fills in, the normalised push-out derived from it, and
/// the two points `Actor02300_Fn0371C` tests the player's sight line over.
typedef struct Actor102300HitScratch {
    /* 0x00 */ GpDeltaScratch delta;
    /* 0x10 */ VECTOR         normal;
    /* 0x20 */ VECTOR         push;
    /* 0x30 */ SVECTOR        effOfs;
    /* 0x38 */ SVECTOR        target;
} Actor102300HitScratch;
STATIC_ASSERT_SIZEOF(Actor102300HitScratch, 0x40);

/// First frame of each animation, indexed by `Actor102300Work::field_694`;
/// the state handlers offset it to get the frames their cues fire on.
extern s16 Actor02300_D03F44[];
/// The `Gp_PackPair` entry the lunge parks in the work block's 0x5E4 node.
extern GpU16Pair Actor02300_D159C4;
/// Base sound id of the lunge cue, ORed with the enemy's id nibble.
extern s32 Actor02300_D15AF8;

/// The `GpPairSrcE` the enemy parks in its own `field_50` slot.
extern GpPairSrcE Actor02300_D159D8;
/// Per-room voice-stream sector tables, indexed by `GameSession::at4.loc.stage` then
/// `field_6`; a NULL row means this room has no cue.
extern u16* Actor02300_D15C80[];
/// The overlay's own spawn table: entry 0 is this enemy, 1 and 2 the two
/// companions the setup state spawns.
extern TaskDesc Actor02300_D15C98;
/// Animation bank `func_800B3F84` binds to the work block.
extern u8 Actor02300_D15CBC[];

/// Per-weapon-id weak-point flags (`id & 0x7F`) for the two hit families,
/// picked by the id's 0x8000 bit.
extern s16 Actor02300_D159E8[];
extern s16 Actor02300_D15A44[];

/// Voice-cue sound ids, indexed from `Actor102300Work::field_6D6`.
extern s32 Actor02300_D15AB4[];
/// Sound ids of the burst state's two cues.
extern s32 Actor02300_D15B00;
extern s32 Actor02300_D15B04;
/// Effect id the burst state spawns at part 4.
extern s32 D_80115758;

/// Handlers of the `Actor102300Work::field_6A6` states, one per entry.
extern TaskFunc Actor02300_D15D38[];

/// Flag bits the proximity check tests: one group raises contact within
/// 0x5DC, the other two start the approach.
extern u8 D_801153F2;
/// Raised by the teardown state; the approach states drop out of their cycle
/// while it is set.
extern s8 D_80115419;

/// Sound id of the part-11 child's cue, ORed with the enemy's id nibble.
extern s32 Actor02300_D15B08;
/// Effect id the part-7 child spawns when its countdown runs out.
extern s32 D_8011572C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

s32  Actor02300_Fn0371C(SVECTOR* arg0, SVECTOR* arg1);
void Actor02300_Fn00CD0(Task* arg0);
void Actor02300_Fn03C04(GpEnemy* arg0, Task* task);
void Actor02300_Fn03C50(GpEnemy* arg0, Task* task);
void Actor02300_Fn03D44(GpEnemy* arg0, Task* task);
void Actor02300_Fn03D88(GpEnemy* arg0, Task* arg1);

/// Dust puff of the per-frame state: every third call spawns a spark effect at
/// part 3 with a random upward offset, out of an 8-byte `G_SCRATCH_HEAD` block.
static __inline__ void Actor02300_SpawnDust(Task* actor)
{
    Actor102300Work* work;
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

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void Actor02300_Fn00084(Task* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor102300Work*       work;
    GpDeltaScratch*        head;
    Actor102300HitScratch* scratch;
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
    s32                    max;

    result                                    = 0;
    maxPush                                   = 0;
    hit                                       = 0;
    lastId                                    = 0;
    work                                      = arg0->work;
    head                                      = *(GpDeltaScratch**)G_SCRATCH_HEAD;
    self                                      = ((TmdObject*)arg0->extra)->coords;
    *(Actor102300HitScratch**)G_SCRATCH_HEAD -= 1;
    scratch                                   = *(Actor102300HitScratch**)G_SCRATCH_HEAD;
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
                        if (Actor02300_D15A44[work->field_4EC[i].key & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (Actor02300_D159E8[work->field_4EC[i].key & 0x7F] != 0) {
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
                } else if (max = enemy->param->hpMax, enemy->hp < max / 4) {
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
        if (Actor02300_Fn0371C(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x40;
}

/// Approach-cycle state (entry 1 of `Actor02300_D15D38`). State 0 drains the
/// `field_6DA` budget by `field_69C` (0 while `field_698` is under the
/// animation's start frame in `Actor02300_D03F44`, 0x14 once past it) and runs
/// the proximity check `Actor02300_Fn00CD0` every frame; when the budget runs
/// out it switches to animation 4 and state 1. State 1 waits for `field_698`
/// to reach 0x60, then either falls back to animation 2 (budget left) or
/// turns about: animation 3, state 2, a fresh budget of 1000 per unit of the
/// placement record's `variant`, and `field_6A2` / `field_6A4` set to the
/// current yaw and its opposite. State 2 turns at `field_69E` = 0x3B until
/// `field_698` reaches 0x23, then returns to animation 2 and state 0. A set
/// `field_6B2` or `D_80115419` overrides everything with animation 2, entry 2
/// and the shared state-F0 slot.
void Actor02300_Fn00AEC(Task* arg0)
{
    GpEnemy*         spawn;
    Actor102300Work* work;
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
            if (work->field_698 >= Actor02300_D03F44[work->field_694]) {
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
            Actor02300_Fn00CD0(arg0);
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

/// Proximity check of the approach states. Measures the player's horizontal
/// distance from the root coordinate through a 0x10-byte `G_SCRATCH_HEAD`
/// block: under 0x5DC one of `D_801153F2`'s bit groups raises `field_6B2`;
/// past it the other two (the second only within 0xBB8) put the enemy into
/// animation 4 and state 1.
void Actor02300_Fn00CD0(Task* arg0)
{
    Actor102300Work* work;
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

/// Entry 0xB of the `field_6A6` state table `Actor02300_D15D38`. State 0 picks
/// the reaction from `field_6AA` (animation 0x16 into state 1, or 0x1A into
/// state 2), parks the second body node's pose and radius, raises its 0x4000
/// flag, drops the third node's, and clears the enemy's `reactionFlags`.
/// States 1 and 2 play the voice cues at their frame marks and, at the end of
/// the animation, settle on an idle (0x19 / 0x1D) with a fresh 6-bit dwell
/// from `Gp_LcgState`: into state 3 while the enemy has hit points left,
/// otherwise handing the task over to state 2. States 3 and 4 alternate
/// between the two idles until the dwell runs out.
void Actor02300_Fn00E0C(Task* arg0)
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
    Actor102300Work* work;
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
                snd = Actor02300_D15AB4[work->field_6D6 + 0xC] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = Actor02300_D15AB4[work->field_6D6 + 8] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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
                snd  = Actor02300_D15AB4[work->field_6D6 + 8] | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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

/// Entry 0xC of the `field_6A6` table. State 0 picks the animation from
/// `field_6B8`: 1 selects 0x17 into state 1, anything else 0x1B into state 2.
/// States 1 and 2 wait for `field_698` to reach 0x10 / 0x16, then settle on
/// the matching idle (0x19 / 0x1D), move to entry 0xB's state 3 and roll a
/// fresh 6-bit dwell into `field_6AE`.
void Actor02300_Fn011E8(Task* arg0)
{
    Actor102300Work* work;
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

/// Entry 0xD of the `field_6A6` table. State 0 arms the reaction: `field_6AA`
/// picks animation 0x16 with a 0x42-frame budget or 0x1A with 0x31 frames,
/// the second body node's pose and radius are parked and its 0x4000 flag
/// raised, the third node's dropped, and the enemy's `reactionFlags` cleared.
/// State 1 plays the voice cues of the animation `field_6B8` selects at its
/// frame marks and, when the `field_6AE` budget runs out, hands the task over
/// to state 2.
void Actor02300_Fn012E0(Task* arg0)
{
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    s32              snd;
    s16              state;

    work  = arg0->work;
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

                    snd = Actor02300_D15AB4[work->field_6D6 + 0xC] |
                          ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = Actor02300_D15AB4[work->field_6D6 + 8] |
                          ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = Actor02300_D15AB4[work->field_6D6 + 8] |
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

/// Steps the yaw `field_6A2` from the root coordinate's own heading toward the
/// target `field_6A4` by `field_69E` per frame: within half a turn it closes on
/// the target directly (or, on animation 3, turns the other way by the step),
/// past that it goes round the long way, snapping onto the target once the
/// step would overshoot. The result rebuilds the root coordinate's matrix.
void Actor02300_Fn0150C(Task* arg0)
{
    Actor102300Work* work;
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

/// Applies the decaying hit tilt `field_688` to part 3 of the model: the tilt's
/// rotation matrix is multiplied column by column into that part's matrix,
/// then X and Y each step 0x20 toward zero, snapping to zero within 0x20.
/// Once both have settled, `field_6B4` is cleared.
void Actor02300_Fn01698(Task* arg0)
{
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix                                   = (MATRIX*)(((ActorScratchStack*)G_SCRATCH_HEAD)->sp - 0x20);
    ((ActorScratchStack*)G_SCRATCH_HEAD)->sp = (u32)matrix;
    active                                   = 0;
    work                                     = arg0->work;
    coord                                    = ((TmdObject*)arg0->extra)->coords;
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

/// Animation sound cues. While the enemy has a voice row (`field_6D6`), reads the
/// current record of animation slot 1 and plays the row's two cues on the
/// falling edge of its 0x20 and 0x10 bits, remembering them in `field_6A0`.
void Actor02300_Fn018A4(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    GpAnimRec*       rec;

    work = arg0->work;
    self = ((TmdObject*)arg0->extra)->coords;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(&work->anim, &work->slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & 0x20) && (work->field_6A0 & 0x20)) {
                snd = Actor02300_D15AB4[work->field_6D6 * 2 - 1] |
                      ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
            }
            if (!(rec->flags & 0x10) && (work->field_6A0 & 0x10)) {
                snd = Actor02300_D15AB4[work->field_6D6 * 2] |
                      ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
            }
            work->field_6A0 = (u16)(rec->flags & 0x30);
        }
    }
}

/// Teardown state of the enemy (entry 2 of `Actor02300_D00078`).
/// `Gp_StateF0.field_4` gates it: 1 only redraws and 2 hides the model and its lock-on
/// node, both returning; 0 shows them and runs the states. State 0 unlinks the enemy's lock-on
/// node and collision bodies, releases its state-F0 slot, settles on the idle
/// `field_6B8` selects, files the pose with `Gp_SaveEnemyPose` so the enemy is
/// restored in that pose, and raises `D_80115419`. State 1 spawns a spark
/// every fourth frame. Either way the animation slots advance or are reseeded
/// and the model is drawn with its ground shadow.
void Actor02300_Fn01A20(GpEnemy* arg0, Task* arg1)
{
    Actor102300Work* work;
    Actor102300Work* animWork;
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
        duration            = Actor02300_D03F44[animWork->field_694];
        do {
            func_800B4114(&animWork->anim, i, animWork->field_694, 0, (s32)duration);
            i += 1;
        } while (i < 0x13);
        coord->flg = 0;
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)(animWork->field_698 + i);
        do {
            Gp_AnimTickIndex(&animWork->anim, i);
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

/// Per-frame tick for the enemy's charge, sharing `field_6A8` with the rest of
/// the overlay and measuring the offset to the player through a 0x10-byte
/// `G_SCRATCH_HEAD` block. State 0 waits out the wind-up: from frame 0x47 it
/// mirrors `field_6D0` into `field_6CE` and commits to the charge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, zeroes
/// the cycle counter `field_6B6` on frame 0x46, and hands over to state 1 on
/// animation 6 once the animation is past its start frame plus 0x52. State 1
/// drives the charge, aiming `field_6A4` at the player each frame, switching to
/// state 2 on animation 7 within 1500 units and on animation 4 once the heading
/// has drifted more than 0x100 from `field_6A2`. State 2 raises the 0x5E4
/// node's 0x8000 flag and queues the cue on frame 0xD, drops the flag on frame
/// 0x1E, and from frame 0x3B picks animation 8 (back to state 0) within 3000
/// units or animation 4 otherwise. A `field_6B6` of 0x4C at any point aborts
/// the whole cycle back to animation 8.
void Actor02300_Fn01DF0(Task* arg0)
{
    s16              state;
    s16              diff;
    s16              turn;
    s32*             scratch;
    s32              dx;
    s32              dz;
    s32              dxAim;
    s32              dzAim;
    s32              dxHold;
    s32              dzHold;
    s32              dist;
    s32              sound;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->work;
    state                 = work->field_6A8;
    self                  = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            if ((work->field_698 >= 0x47) && (work->field_6CE              = (s16)(work->field_6D0 > 0),
                                              ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]),
                                              dz                           = Player_Status.coordMtx->t[2] - self->coord.t[2],
                                              delta->vz                    = dz,
                                              dx                           = ((VECTOR*)(head - 0x10))->vx,
                                              ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0;
            if (work->field_698 == 0x46) {
                work->field_6B6 = 0;
                work->field_6CC = 1;
            }
            if ((work->field_698 >= 0x47) && (work->field_6B6 >= 0x4C)) {
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
                break;
            }
            if (work->field_698 >= (Actor02300_D03F44[work->field_694] + 0x52)) {
                work->field_6A8 = 1;
                work->field_694 = 6;
            }
            break;
        case 1:
            work->field_69C              = 0x84;
            work->field_69E              = 0;
            ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(Player_Status.coordMtx->t[2] - self->coord.t[2]);
            work->field_6A4              = (s16)(ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF);
            dxAim                        = ((VECTOR*)(head - 0x10))->vx;
            dzAim                        = delta->vz;
            dist                         = SquareRoot0((dxAim * dxAim) + (dzAim * dzAim));
            if (work->field_6B6 >= 0x4C) {
                goto reset;
            }
            if (dist < 0x5DC) {
                work->field_6A8 = 2;
                work->field_694 = 7;
                work->field_69C = 0;
            } else {
                diff = (ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF) - work->field_6A2;
                turn = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (turn > 0x100) {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                    work->field_6CC = 0;
                    work->field_6CE = 0;
                }
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0;
            if ((work->field_698 < 0xD) && (work->field_6B6 >= 0x4C)) {
            reset:
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_6CE       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
                break;
            }
            if (work->field_698 == 0xD) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.key   = Gp_PackPair(&Actor02300_D159C4, 1);
                sound                 = Actor02300_D15AF8 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan                   = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(self));
            }
            if (work->field_698 == 0x1E) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            if (work->field_698 >= 0x3B) {
                work->field_6CE = 0;
                dxHold          = Player_Status.coordMtx->t[0] - self->coord.t[0];
                delta->vx       = dxHold;
                dzHold          = Player_Status.coordMtx->t[2] - self->coord.t[2];
                delta->vz       = dzHold;
                if (SquareRoot0((dxHold * dxHold) + (dzHold * dzHold)) < 0xBB8) {
                    work->field_6A6 = 4;
                    work->field_6A8 = 0;
                    work->field_694 = 8;
                } else {
                    work->field_6A6 = 2;
                    work->field_6A8 = 2;
                    work->field_694 = 4;
                }
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

/// Per-frame tick for the enemy's lunge cycle, sharing the `field_6A8` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte `G_SCRATCH_HEAD` block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, aims
/// `field_6A4` at them every frame, raises the 0x5E4 node's 0x8000 flag on
/// frame 0x20 and queues the cue on frame 0x21, then hands over to state 1 on
/// animation 9 once the animation is past frame 0x27. State 1 waits for frame
/// 0x5E and moves on to state 2 on animation 4.
void Actor02300_Fn02290(Task* arg0)
{
    s16              startFrame;
    s16              state;
    s16              frame;
    s32*             scratch;
    s32              dz;
    s32              sound;
    s32              dx;
    s32              pan;
    u8*              head;
    Actor102300Work* work;
    GsCOORDINATE2*   self;
    VECTOR*          delta;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x10;
    delta                 = (VECTOR*)(head - 0x10);
    work                  = arg0->work;
    state                 = work->field_6A8;
    self                  = ((TmdObject*)arg0->extra)->coords;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
            dz                           = Player_Status.coordMtx->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = Actor02300_D03F44[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (Actor02300_D03F44[work->field_694] + 0x20)) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.key   = Gp_PackPair(&Actor02300_D159C4, 0);
            }
            if (work->field_698 == (Actor02300_D03F44[work->field_694] + 0x21)) {
                sound = Actor02300_D15AF8 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
                pan   = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(self));
            }
            if (work->field_698 >= (Actor02300_D03F44[work->field_694] + 0x27)) {
                work->field_6A8       = 1;
                work->field_694       = 9;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x5E) {
                work->field_6A6 = 2;
                work->field_6A8 = 2;
                work->field_694 = 4;
            }
            break;
    }
    scratch   = (s32*)G_SCRATCH_HEAD;
    *scratch += 0x10;
}

/// Entry 5 of the `field_6A6` table, a four-step burst. State 0 spawns the
/// effect at part 4 and plays the first cue on entry, rumbles every tenth
/// frame, and either ends once `field_6B6` passes 0x28 (entry 9 when
/// `field_6C4` runs out, else animation 0x15 into state 3) or times out at
/// 0x96 frames into state 1. State 1 counts `field_6AE` down into state 2,
/// which triggers the PE state, returns to entry 2 and plays the second cue.
/// State 3 spawns a spark every fourth frame until `field_6AE` runs out.
void Actor02300_Fn02518(Task* arg0)
{
    Actor102300Work* work;
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
                sound           = Actor02300_D15B00 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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
            sound           = Actor02300_D15B04 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
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

/// Spawn/setup state for this enemy. Allocates the 0x6E4 work block, wires the
/// model object to the block's own light/colour matrices, primes the nineteen
/// animation slots, then spawns the two companion enemies from the overlay's
/// table (entries 2 and 1) and points each one's model at the texture page and
/// CLUT row its room's `GpAreaPlace` names.
///
/// `GpEnemy::spawnState` then picks how the enemy starts: 0 builds the full
/// object set -- the four `GpObj` nodes with their `GpRec18` tables, the voice
/// cue looked up per room in `Actor02300_D15C80`, and the coin-flip in
/// `field_6C4` drawn from `Gp_LcgState` -- while 1 and 2 only prime the
/// animation state and hand straight on to the next task state.
void Actor02300_Fn028AC(GpEnemy* enemy, Task* actor)
{
    Actor102300Work* work;
    TmdObject*       obj;
    TmdObject*       model;
    TmdObject*       model2;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parts;
    GsCOORDINATE2*   partsA;
    GsCOORDINATE2*   partsB;
    GsCOORDINATE2*   partsC;
    GsCOORDINATE2*   effParts;
    GpAreaKey*       sessionKey;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       keyPtr;
    GpAreaKey*       keyPtr2;
    u8               areaByte0;
    u8               areaByte02;
    GpAreaRec*       rec;
    GpAreaRec*       rec2;
    GpAreaPlace*     entry;
    GpAreaPlace*     entry2;
    GpEnemy*         eff;
    GpEnemy*         eff2;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    GpAreaKey        key;
    s32              i;
    s32              one;
    s32              kind;
    s32              idx;
    s32              idx2;
    s32              param;
    u32              lcg;

    obj   = actor->extra;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->flg                 = 0;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x17;
    work->field_66C            = &Actor02300_D15C98;
    work->field_670.coord      = &((TmdObject*)actor->extra)->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->anim, Actor02300_D15CBC, obj, work->poses, work->slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }

    eff        = Gp_SpawnEnemyFromTable(&Actor02300_D15C98, 2, 0, enemy);
    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    model      = eff->task->extra;
    idx        = enemy->placeKey >> 12;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    /* Both calls take `&key`. Left alone, GCC 2.8.1 CSEs that address into one
       pseudo that is live across the first call, costing a callee-saved
       register; the ROM rematerializes `addiu a0, sp, key` for each call. The
       barrier keeps the address materialization next to the call and the
       `+r` touch makes the second one a fresh computation. */
    keyPtr   = &key;
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    SOFT_DEF_REG(keyPtr);
    keyPtr       = &key;
    rec          = Gp_GetNestedAreaRec(keyPtr);
    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    SOFT_DEF_REG(keyPtr);

    eff2        = Gp_SpawnEnemyFromTable(&Actor02300_D15C98, 1, 0, enemy);
    sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
    model2      = eff2->task->extra;
    idx2        = enemy->placeKey >> 12;
    key.stage   = sessionKey2->stage;
    key.area    = sessionKey2->area;
    key.room    = sessionKey2->room;
    areaByte02  = sessionKey2->view;
    keyPtr2     = &key;
    key.view    = areaByte02;
    Gp_SyncAreaKeyIndex(keyPtr2);
    SOFT_DEF_REG(keyPtr2);
    keyPtr2       = &key;
    rec2          = Gp_GetNestedAreaRec(keyPtr2);
    entry2        = (GpAreaPlace*)((idx2 << 4) + (s32)rec2->field_0);
    model2->tpage = entry2->tpage;
    model2->clut  = entry2->clut;
    if (model2->buffer != NULL) {
        tmdProcessStream(model2);
        tmdProcessStream(model2);
    }

    one  = 1;
    kind = enemy->spawnState;
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
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    Gp_LinkNode(&enemy->node);
    parts             = ((TmdObject*)actor->extra)->coords;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    enemy->param      = &Actor02300_D159D8;
    enemy->recs       = work->field_4EC;
    enemy->coord      = &parts[3];
    enemy->hp         = Actor02300_D159D8.hpMax;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_6AC = enemy->place->mode & 1;
    if (work->field_6AC == 0) {
        work->field_694 = one;
        work->field_6A6 = 0;
    } else {
        work->field_694 = 2;
        work->field_6A6 = one;
        param           = enemy->place->variant;
        work->field_6DA = param * 1000;
    }

    tbl = Actor02300_D15C80[gGameSession->at4.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->at4.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x17;
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
    lcg                        = (Gp_LcgState * 5) + 0x71357911;
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
    work->field_4CC.key      = 0x30017;
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

    effParts                 = ((TmdObject*)eff2->task->extra)->coords;
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
    actor->state           = 1;
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

/// Per-frame state of the enemy (entry 1 of `Actor02300_D00078`). Unless
/// `Gp_StateF0.field_4` holds the scene (1 only redraws, 2 hides the model and its
/// lock-on node), it turns a pending knock-back into entry 0xA, runs the hit
/// tick and the `field_6A6` state handler, turns toward `field_6A4`, steps the
/// root coordinate forward by `field_69C`, advances or reseeds the animation
/// slots, applies the hit tilt and the sound cues, and every third frame while
/// `field_6C4` is 0 puffs dust at part 3. It then draws the model with its
/// ground shadow.
void Actor02300_Fn02EA0(GpEnemy* ctx, Task* actor)
{
    VECTOR3          pos;
    GpEnemy*         spawn;
    TmdObject*       model;
    Actor102300Work* moveWork;
    Actor102300Work* animWork;
    Actor102300Work* work;
    Actor102300Work* flagWork;
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
    Actor02300_Fn00084(actor);
    Actor02300_D15D38[work->field_6A6](actor);
    if (work->field_69E != 0) {
        Actor02300_Fn0150C(actor);
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
        duration            = Actor02300_D03F44[animWork->field_694];
        do {
            func_800B4114(&animWork->anim, i, animWork->field_694, 0, duration);
            i += 1;
        } while (i < 0x13);
    } else {
        TOUCH_REG(i);
        animWork->field_698 = (u16)animWork->field_698 + i;
        do {
            Gp_AnimTickIndex(&animWork->anim, i);
            i += 1;
        } while (i < 0x13);
    }
    if (work->field_6B4 != 0) {
        Actor02300_Fn01698(actor);
    }
    Actor02300_Fn018A4(actor);
    coord->flg                                = 0;
    ((TmdObject*)actor->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(coord);
    if (work->field_6C4 == 0) {
        Actor02300_SpawnDust(actor);
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

/// The lunge's own tick, run out of a 0x10-byte `G_SCRATCH_HEAD` block. State
/// 0 is the wind-up: it holds `field_69C` at 0 until the animation reaches its
/// start frame, aims `field_6A4` at the player and compares it with the
/// enemy's own facing `field_6A2` - past 0x581 apart it gives up and turns
/// (animation 3, or animation 4 when `field_6DC` says it has already turned
/// once), within 0x80 it raises the body node's 0xC000 flags and commits as
/// soon as `field_6B2` reports contact. State 1 picks what to do next: inside
/// 0x8CA of the player it lunges (animation 8), otherwise it draws from
/// `Gp_LcgState` through a mask that widens by a bit each cycle and either
/// circles (animation 0xA) or walks in (animation 5). State 2 waits out the
/// recovery and state 3 the turn.
void Actor02300_Fn0327C(Task* actor)
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
    Actor102300Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = actor->work;
    state                 = work->field_6A8;
    coord                 = ((TmdObject*)actor->extra)->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= Actor02300_D03F44[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = (u16)work->field_6A4 - yaw;
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
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
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
                    work->field_6A6 = 3;
                    work->field_6A8 = 0;
                    work->field_694 = 5;
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
                deltaYaw2       = (u16)work->field_6A4 - yaw2;
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
s32 Actor02300_Fn0371C(SVECTOR* arg0, SVECTOR* arg1)
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

/// Entry 0 of the `field_6A6` state table, the idle. State 0 counts
/// `field_6AE` up to 0x5B frames and then switches to animation 4 and state 1,
/// running the proximity check `Actor02300_Fn00CD0` every frame meanwhile;
/// state 1 waits for `field_698` to reach 0x5E and drops back to state 0 with
/// animation 1. A set `field_6B2` or `D_80115419` overrides both with
/// animation 2, entry 2 and the shared state-F0 slot.
void Actor02300_Fn03828(Task* arg0)
{
    Actor102300Work* work;
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
            Actor02300_Fn00CD0(arg0);
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

/// Entry 8 of the `field_6A6` state table `Actor02300_D15D38`: step 0 starts
/// animation 0x11 and clears both dwell counters; step 1 waits for frame 0x37,
/// then parks on animation 2 (entry 2) or, with `field_6E0` set, on animation
/// 0x14 (entry 0xA).
void Actor02300_Fn03908(Task* arg0)
{
    Actor102300Work* work;
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
/// cleared and the enemy parks on animation 2 (entry 2) when done.
void Actor02300_Fn03994(Task* arg0)
{
    Actor102300Work* work;
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
void Actor02300_Fn03A5C(Task* arg0)
{
    Actor102300Work* work;
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
/// otherwise (step 2, waits for frame 0x16); when done the enemy's handler
/// chain advances to state 2.
void Actor02300_Fn03AE8(Task* arg0)
{
    Actor102300Work* work;
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

void Actor02300_Fn03BA0(void)
{
}

/// State handlers of the child task hung off part 7 of the enemy's model -
/// spawn/setup, per-frame tick and teardown - dispatched through by
/// `Actor02300_Fn03BA8`.
const GpEnemyTaskFuncTable3 Actor02300_D00060 = {
    Actor02300_Fn03C04,
    Actor02300_Fn03C50,
    Gp_DestroyEnemy,
};

void Actor02300_Fn03BA8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00060;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Spawn state of the child task driven by `Actor02300_Fn03BA8`: parents the
/// child's root coordinate to part 7 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and seeds the enemy's
/// `field_6D8` countdown the child's tick drains, then advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
void Actor02300_Fn03C04(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor102300Work*)parent->work;

    coord->flg      = 0;
    coord->sub      = &parentCoords[7];
    obj->lightMtx   = &work->field_45C;
    obj->colorMtx   = &work->field_43C;
    obj->flags      = 0;
    task->state     = 1;
    work->field_6D8 = 0xA;
}

/// Per-frame state of the child task `Actor02300_Fn03C04` sets up. It mirrors
/// the enemy's model flags onto its own model and drains the enemy's
/// `field_6D8` countdown; on the frame it reaches zero it spawns a
/// `Gp_SpawnEff` effect at part 7 of the enemy's coordinate array and
/// reparents the effect's task to this child. `arg0` is the spawn context
/// every state handler takes and is unused here.
void Actor02300_Fn03C50(GpEnemy* arg0, Task* task)
{
    GpEffWork*       effect;
    Task*            parent;
    Actor102300Work* work;
    s16              count;

    parent                           = task->parent;
    work                             = (Actor102300Work*)parent->work;
    ((TmdObject*)task->extra)->flags = (u16)((TmdObject*)parent->extra)->flags;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &((TmdObject*)task->parent->extra)->coords[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->task);
            }
        }
    }
}

/// State handlers of the child task hung off part 11 of the enemy's model -
/// spawn/setup, per-frame tick and teardown - dispatched through by
/// `Actor02300_Fn03CE8`.
const GpEnemyTaskFuncTable3 Actor02300_D0006C = {
    Actor02300_Fn03D44,
    Actor02300_Fn03D88,
    Gp_DestroyEnemy,
};

void Actor02300_Fn03CE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D0006C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Spawn state of the child task driven by `Actor02300_Fn03CE8`: parents the
/// child's root coordinate to part 11 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
void Actor02300_Fn03D44(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor102300Work*)parent->work;

    coord->flg    = 0;
    coord->sub    = &parentCoords[11];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

/// The enemy's own state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by `Actor02300_Fn03EE8`. Each takes the task
/// as the enemy view it is.
const GpEnemyTaskFuncTable3 Actor02300_D00078 = {
    Actor02300_Fn028AC,
    Actor02300_Fn02EA0,
    Actor02300_Fn01A20,
};

/// Per-frame state of the child task `Actor02300_Fn03D44` sets up at part 11
/// of the enemy's model. It mirrors the enemy's model flags onto its own and
/// runs on the enemy's `field_6D2`: 0 marks its coordinate for recomputation, 1 spawns
/// four effects on it, plays the cue and moves to 2, and 2 hands the task over
/// to state 2.
void Actor02300_Fn03D88(GpEnemy* arg0, Task* arg1)
{
    Task*            owner;
    TmdObject*       obj;
    TmdObject*       ownerObj;
    Actor102300Work* work;
    GsCOORDINATE2*   coord;
    s16              state;
    s32              snd;
    s32              pan;

    owner      = arg1->parent;
    obj        = (TmdObject*)arg1->extra;
    ownerObj   = owner->extra;
    work       = owner->work;
    coord      = obj->coords;
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
            snd             = Actor02300_D15B08 | ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 0xC) << 8);
            pan             = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(coord));
            return;
        case 2:
            arg1->state = state;
            return;
    }
}

void Actor02300_Fn03EE8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02300_D00078;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
