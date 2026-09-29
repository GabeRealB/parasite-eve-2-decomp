#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/gtemac.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/object_fields.h"
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/gamemain.h"
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

static const GpEnemyTaskFuncTable3 Actor02000_D00060;
static const GpEnemyTaskFuncTable3 Actor02000_D0006C;

static s32 Actor02000_Fn0315C(SVECTOR* start, SVECTOR* end);

extern GpU16Pair Actor02000_D15CFC[];
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s32 value;
    u8  retained[16];
} Actor102000TextStorage7C50;
STATIC_ASSERT_SIZEOF(Actor102000TextStorage7C50, 20);

extern Actor102000TextStorage7C50 Actor02000_D15E30;
void                              Actor02000_Fn02294(Task* actor);

static void Actor02000_Fn00CD0(Task* arg0);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern s16 Actor02000_D03784[];
extern s16 Actor02000_D15D20[];
extern s16 Actor02000_D15D7C[];
extern s32 Actor02000_D15DEC[];

extern GpAnimSet Actor02000_D09C20;

extern GpAnimSet Actor02000_D0A588;

extern GpAnimSet Actor02000_D0ABB0;

extern GpAnimSet Actor02000_D0B3B0;

extern GpAnimSet Actor02000_D0CAA4;

extern GpAnimSet Actor02000_D0D1E8;

extern GpAnimSet Actor02000_D0E7FC;

extern GpAnimSet Actor02000_D0F534;

extern GpAnimSet Actor02000_D0FC6C;

extern GpAnimSet Actor02000_D10190;

extern GpAnimSet Actor02000_D10BA0;

extern GpAnimSet Actor02000_D11648;

extern GpAnimSet Actor02000_D11C28;

extern GpAnimSet Actor02000_D1291C;

extern GpAnimSet Actor02000_D13994;

extern GpAnimSet Actor02000_D13E68;

extern GpAnimSet Actor02000_D14178;

extern GpAnimSet Actor02000_D14354;

extern GpAnimSet Actor02000_D1529C;

extern GpAnimSet Actor02000_D15830;

extern GpAnimSet Actor02000_D15AF8;

extern GpAnimSet Actor02000_D15CD4;

extern TmdSource Actor02000_D08AA8;

extern TmdSource Actor02000_D08FEC;

void Actor02000_Fn00AEC(Task*);

void Actor02000_Fn00E0C(Task*);

void Actor02000_Fn011E8(Task*);

void Actor02000_Fn012E0(Task*);

void Actor02000_Fn01DF0(Task*);

void Actor02000_Fn02294(Task*);

void Actor02000_Fn02D5C(Task*);

void Actor02000_Fn03268(Task*);

void Actor02000_Fn03348(Task*);

void Actor02000_Fn033D4(Task*);

void Actor02000_Fn0349C(Task*);

void Actor02000_Fn03528(Task*);

void Actor02000_Fn035E0(Task*);

void Actor02000_Fn035E8(Task*);

void Actor02000_Fn03728(Task*);

extern GpAnimSet* Actor02000_D15FE8[31];

extern TaskDesc Actor02000_D15FD0[];

extern u16* Actor02000_D15FB8[];

extern GpPairSrcE Actor02000_D15D10;

static void Actor02000_Fn00078(Task*);

static void Actor02000_Fn01698(Task*);

extern TaskFunc Actor02000_D16064[];

static void        Actor02000_Fn0150C(Task* arg0);
static void        Actor02000_Fn018A4(Task* arg0);
static void        Actor02000_Fn01A20(GpEnemy* ctx, Task* actor);
static void        Actor02000_Fn0251C(GpEnemy* ctx, Task* actor);
static inline void _actor02000ApplyReaction(Task* actor);
static inline void _actor02000StepRoot(Task* actor);
static inline void _actor02000TickAnim(Task* actor);
static inline void _actor02000Draw(Task* actor, GpCoord* coord);
static void        Actor02000_Fn02A34(GpEnemy* ctx, Task* actor);
static void        Actor02000_Fn03644(GpEnemy* arg0, Task* task);
static void        Actor02000_Fn03690(GpEnemy* arg0, Task* task);

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
static void Actor02000_Fn00078(Task* arg0)
{
    s32                    result;
    s32                    maxPush;
    s32                    hit;
    u32                    lastId;
    Actor105600Work*       work;
    GpDeltaScratch*        head;
    Actor105600HitScratch* scratch;
    GpEnemy*               enemy;
    GpCoord*               self;
    GpCoord*               other;
    GpCoord*               part;
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

    result  = 0;
    maxPush = 0;
    hit     = 0;
    lastId  = 0;
    work    = arg0->work;
    head    = SCRATCH_HEAD(GpDeltaScratch);
    self    = arg0->extra.tmd->coords;
    SCRATCH_PUSH(Actor105600HitScratch);
    scratch = SCRATCH_HEAD(Actor105600HitScratch);
    enemy   = arg0->spawnArg2.pointer;

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
                other               = Gp_ActorSlots[((u32)work->field_4EC[i].key >> 7) & 1]->extra.tmd->coords;
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
                        if (Actor02000_D15D7C[work->field_4EC[i].key & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (Actor02000_D15D20[work->field_4EC[i].key & 0x7F] != 0) {
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
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 2, NULL);
                    }
                }
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
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
                } else if (max = enemy->param->hpMax, enemy->hp < max * 15 / 100) {
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
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key) & 0xFFFF, &arg0->extra.tmd->coords[3],
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
                part                = &arg0->extra.tmd->coords[3];
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
        part               = &(gameGetPtrSlot(3))->extra.tmd->coords[4];
        scratch->effOfs.vx = part->workm.t[0];
        scratch->effOfs.vy = part->workm.t[1];
        scratch->effOfs.vz = part->workm.t[2];
        scratch->target.vx = self->workm.t[0];
        scratch->target.vy = self->workm.t[1];
        scratch->target.vz = self->workm.t[2];
        if (Actor02000_Fn0315C(&scratch->effOfs, &scratch->target) == 0) {
            work->field_6B2 = 1;
        }
    }
    Gp_ClearRec18Occupied(work->field_4B4);
    SCRATCH_POP_BYTES(0x40);
}

s16 Actor02000_D03784[32] = {
    0,
    8,
    8,
    0,
    8,
    8,
    0,
    0,
    8,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    8,
    4,
    4,
    4,
    4,
    4,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
};

TmdBone Actor02000_D037C4[19] = {
#include "assets/actor_102000_model_08AA8_skeleton.inc"
};

u32 Actor02000_D03A70[19] = {
#include "assets/actor_102000_model_08AA8_partVerts.inc"
};

SVECTOR Actor02000_D03ABC[339] = {
#include "assets/actor_102000_model_08AA8_verts.inc"
};

SVECTOR Actor02000_D04554[346] = {
#include "assets/actor_102000_model_08AA8_normals.inc"
};

u32 Actor02000_D05024[3745] = {
#include "assets/actor_102000_model_08AA8_stream.inc"
};

TmdSource Actor02000_D08AA8 = {
    0,
    20476,
    5672,
    19,
    Actor02000_D03A70,
    Actor02000_D03ABC,
    Actor02000_D04554,
    Actor02000_D037C4,
    Actor02000_D05024,
};

TmdBone Actor02000_D08ACC[1] = {
#include "assets/actor_102000_model_08FEC_skeleton.inc"
};

u32 Actor02000_D08AF0[1] = {
#include "assets/actor_102000_model_08FEC_partVerts.inc"
};

SVECTOR Actor02000_D08AF4[29] = {
#include "assets/actor_102000_model_08FEC_verts.inc"
};

SVECTOR Actor02000_D08BDC[24] = {
#include "assets/actor_102000_model_08FEC_normals.inc"
};

u32 Actor02000_D08C9C[212] = {
#include "assets/actor_102000_model_08FEC_stream.inc"
};

TmdSource Actor02000_D08FEC = {
    0,
    1436,
    0,
    1,
    Actor02000_D08AF0,
    Actor02000_D08AF4,
    Actor02000_D08BDC,
    Actor02000_D08ACC,
    Actor02000_D08C9C,
};

AnimationPackedPose Actor02000_D09010[21] = {
#include "assets/actor_102000_animation_09C20_bank1.inc"
};

AnimationPackedRotation Actor02000_D0910C[317] = {
#include "assets/actor_102000_animation_09C20_bank4.inc"
};

GpAnimRec Actor02000_D09600[382] = {
#include "assets/actor_102000_animation_09C20_records.inc"
};

u16 Actor02000_D09BF8[20] = {
#include "assets/actor_102000_animation_09C20_indices.inc"
};

GpAnimSet Actor02000_D09C20 = {
    Actor02000_D09600,
    Actor02000_D09BF8,
    { NULL, Actor02000_D09010, NULL, NULL, Actor02000_D0910C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D09C48[16] = {
#include "assets/actor_102000_animation_0A588_bank1.inc"
};

AnimationPackedRotation Actor02000_D09D08[237] = {
#include "assets/actor_102000_animation_0A588_bank4.inc"
};

GpAnimRec Actor02000_D0A0BC[297] = {
#include "assets/actor_102000_animation_0A588_records.inc"
};

u16 Actor02000_D0A560[20] = {
#include "assets/actor_102000_animation_0A588_indices.inc"
};

GpAnimSet Actor02000_D0A588 = {
    Actor02000_D0A0BC,
    Actor02000_D0A560,
    { NULL, Actor02000_D09C48, NULL, NULL, Actor02000_D09D08, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0A5B0[12] = {
#include "assets/actor_102000_animation_0ABB0_bank1.inc"
};

AnimationPackedRotation Actor02000_D0A640[142] = {
#include "assets/actor_102000_animation_0ABB0_bank4.inc"
};

GpAnimRec Actor02000_D0A878[196] = {
#include "assets/actor_102000_animation_0ABB0_records.inc"
};

u16 Actor02000_D0AB88[20] = {
#include "assets/actor_102000_animation_0ABB0_indices.inc"
};

GpAnimSet Actor02000_D0ABB0 = {
    Actor02000_D0A878,
    Actor02000_D0AB88,
    { NULL, Actor02000_D0A5B0, NULL, NULL, Actor02000_D0A640, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0ABD8[14] = {
#include "assets/actor_102000_animation_0B3B0_bank1.inc"
};

AnimationPackedRotation Actor02000_D0AC80[186] = {
#include "assets/actor_102000_animation_0B3B0_bank4.inc"
};

GpAnimRec Actor02000_D0AF68[264] = {
#include "assets/actor_102000_animation_0B3B0_records.inc"
};

u16 Actor02000_D0B388[20] = {
#include "assets/actor_102000_animation_0B3B0_indices.inc"
};

GpAnimSet Actor02000_D0B3B0 = {
    Actor02000_D0AF68,
    Actor02000_D0B388,
    { NULL, Actor02000_D0ABD8, NULL, NULL, Actor02000_D0AC80, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0B3D8[47] = {
#include "assets/actor_102000_animation_0CAA4_bank1.inc"
};

AnimationPackedRotation Actor02000_D0B60C[613] = {
#include "assets/actor_102000_animation_0CAA4_bank4.inc"
};

GpAnimRec Actor02000_D0BFA0[695] = {
#include "assets/actor_102000_animation_0CAA4_records.inc"
};

u16 Actor02000_D0CA7C[20] = {
#include "assets/actor_102000_animation_0CAA4_indices.inc"
};

GpAnimSet Actor02000_D0CAA4 = {
    Actor02000_D0BFA0,
    Actor02000_D0CA7C,
    { NULL, Actor02000_D0B3D8, NULL, NULL, Actor02000_D0B60C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0CACC[19] = {
#include "assets/actor_102000_animation_0D1E8_bank1.inc"
};

AnimationPackedRotation Actor02000_D0CBB0[160] = {
#include "assets/actor_102000_animation_0D1E8_bank4.inc"
};

GpAnimRec Actor02000_D0CE30[228] = {
#include "assets/actor_102000_animation_0D1E8_records.inc"
};

u16 Actor02000_D0D1C0[20] = {
#include "assets/actor_102000_animation_0D1E8_indices.inc"
};

GpAnimSet Actor02000_D0D1E8 = {
    Actor02000_D0CE30,
    Actor02000_D0D1C0,
    { NULL, Actor02000_D0CACC, NULL, NULL, Actor02000_D0CBB0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0D210[66] = {
#include "assets/actor_102000_animation_0E7FC_bank1.inc"
};

AnimationPackedRotation Actor02000_D0D528[549] = {
#include "assets/actor_102000_animation_0E7FC_bank4.inc"
};

GpAnimRec Actor02000_D0DDBC[646] = {
#include "assets/actor_102000_animation_0E7FC_records.inc"
};

u16 Actor02000_D0E7D4[20] = {
#include "assets/actor_102000_animation_0E7FC_indices.inc"
};

GpAnimSet Actor02000_D0E7FC = {
    Actor02000_D0DDBC,
    Actor02000_D0E7D4,
    { NULL, Actor02000_D0D210, NULL, NULL, Actor02000_D0D528, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0E824[21] = {
#include "assets/actor_102000_animation_0F534_bank1.inc"
};

AnimationPackedRotation Actor02000_D0E920[354] = {
#include "assets/actor_102000_animation_0F534_bank4.inc"
};

GpAnimRec Actor02000_D0EEA8[409] = {
#include "assets/actor_102000_animation_0F534_records.inc"
};

u16 Actor02000_D0F50C[20] = {
#include "assets/actor_102000_animation_0F534_indices.inc"
};

GpAnimSet Actor02000_D0F534 = {
    Actor02000_D0EEA8,
    Actor02000_D0F50C,
    { NULL, Actor02000_D0E824, NULL, NULL, Actor02000_D0E920, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0F55C[16] = {
#include "assets/actor_102000_animation_0FC6C_bank1.inc"
};

AnimationPackedRotation Actor02000_D0F61C[177] = {
#include "assets/actor_102000_animation_0FC6C_bank4.inc"
};

GpAnimRec Actor02000_D0F8E0[217] = {
#include "assets/actor_102000_animation_0FC6C_records.inc"
};

u16 Actor02000_D0FC44[20] = {
#include "assets/actor_102000_animation_0FC6C_indices.inc"
};

GpAnimSet Actor02000_D0FC6C = {
    Actor02000_D0F8E0,
    Actor02000_D0FC44,
    { NULL, Actor02000_D0F55C, NULL, NULL, Actor02000_D0F61C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D0FC94[8] = {
#include "assets/actor_102000_animation_10190_bank1.inc"
};

AnimationPackedRotation Actor02000_D0FCF4[116] = {
#include "assets/actor_102000_animation_10190_bank4.inc"
};

GpAnimRec Actor02000_D0FEC4[169] = {
#include "assets/actor_102000_animation_10190_records.inc"
};

u16 Actor02000_D10168[20] = {
#include "assets/actor_102000_animation_10190_indices.inc"
};

GpAnimSet Actor02000_D10190 = {
    Actor02000_D0FEC4,
    Actor02000_D10168,
    { NULL, Actor02000_D0FC94, NULL, NULL, Actor02000_D0FCF4, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D101B8[19] = {
#include "assets/actor_102000_animation_10BA0_bank1.inc"
};

AnimationPackedRotation Actor02000_D1029C[255] = {
#include "assets/actor_102000_animation_10BA0_bank4.inc"
};

GpAnimRec Actor02000_D10698[312] = {
#include "assets/actor_102000_animation_10BA0_records.inc"
};

u16 Actor02000_D10B78[20] = {
#include "assets/actor_102000_animation_10BA0_indices.inc"
};

GpAnimSet Actor02000_D10BA0 = {
    Actor02000_D10698,
    Actor02000_D10B78,
    { NULL, Actor02000_D101B8, NULL, NULL, Actor02000_D1029C, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D10BC8[18] = {
#include "assets/actor_102000_animation_11648_bank1.inc"
};

AnimationPackedRotation Actor02000_D10CA0[281] = {
#include "assets/actor_102000_animation_11648_bank4.inc"
};

GpAnimRec Actor02000_D11104[327] = {
#include "assets/actor_102000_animation_11648_records.inc"
};

u16 Actor02000_D11620[20] = {
#include "assets/actor_102000_animation_11648_indices.inc"
};

GpAnimSet Actor02000_D11648 = {
    Actor02000_D11104,
    Actor02000_D11620,
    { NULL, Actor02000_D10BC8, NULL, NULL, Actor02000_D10CA0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D11670[8] = {
#include "assets/actor_102000_animation_11C28_bank1.inc"
};

AnimationPackedRotation Actor02000_D116D0[130] = {
#include "assets/actor_102000_animation_11C28_bank4.inc"
};

GpAnimRec Actor02000_D118D8[202] = {
#include "assets/actor_102000_animation_11C28_records.inc"
};

u16 Actor02000_D11C00[20] = {
#include "assets/actor_102000_animation_11C28_indices.inc"
};

GpAnimSet Actor02000_D11C28 = {
    Actor02000_D118D8,
    Actor02000_D11C00,
    { NULL, Actor02000_D11670, NULL, NULL, Actor02000_D116D0, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D11C50[23] = {
#include "assets/actor_102000_animation_1291C_bank1.inc"
};

AnimationPackedRotation Actor02000_D11D64[326] = {
#include "assets/actor_102000_animation_1291C_bank4.inc"
};

GpAnimRec Actor02000_D1227C[414] = {
#include "assets/actor_102000_animation_1291C_records.inc"
};

u16 Actor02000_D128F4[20] = {
#include "assets/actor_102000_animation_1291C_indices.inc"
};

GpAnimSet Actor02000_D1291C = {
    Actor02000_D1227C,
    Actor02000_D128F4,
    { NULL, Actor02000_D11C50, NULL, NULL, Actor02000_D11D64, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D12944[31] = {
#include "assets/actor_102000_animation_13994_bank1.inc"
};

AnimationPackedRotation Actor02000_D12AB8[429] = {
#include "assets/actor_102000_animation_13994_bank4.inc"
};

GpAnimRec Actor02000_D1316C[512] = {
#include "assets/actor_102000_animation_13994_records.inc"
};

u16 Actor02000_D1396C[20] = {
#include "assets/actor_102000_animation_13994_indices.inc"
};

GpAnimSet Actor02000_D13994 = {
    Actor02000_D1316C,
    Actor02000_D1396C,
    { NULL, Actor02000_D12944, NULL, NULL, Actor02000_D12AB8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D139BC[9] = {
#include "assets/actor_102000_animation_13E68_bank1.inc"
};

AnimationPackedRotation Actor02000_D13A28[113] = {
#include "assets/actor_102000_animation_13E68_bank4.inc"
};

GpAnimRec Actor02000_D13BEC[149] = {
#include "assets/actor_102000_animation_13E68_records.inc"
};

u16 Actor02000_D13E40[20] = {
#include "assets/actor_102000_animation_13E68_indices.inc"
};

GpAnimSet Actor02000_D13E68 = {
    Actor02000_D13BEC,
    Actor02000_D13E40,
    { NULL, Actor02000_D139BC, NULL, NULL, Actor02000_D13A28, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D13E90[5] = {
#include "assets/actor_102000_animation_14178_bank1.inc"
};

AnimationPackedRotation Actor02000_D13ECC[65] = {
#include "assets/actor_102000_animation_14178_bank4.inc"
};

GpAnimRec Actor02000_D13FD0[96] = {
#include "assets/actor_102000_animation_14178_records.inc"
};

u16 Actor02000_D14150[20] = {
#include "assets/actor_102000_animation_14178_indices.inc"
};

GpAnimSet Actor02000_D14178 = {
    Actor02000_D13FD0,
    Actor02000_D14150,
    { NULL, Actor02000_D13E90, NULL, NULL, Actor02000_D13ECC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D141A0[2] = {
#include "assets/actor_102000_animation_14354_bank1.inc"
};

AnimationPackedRotation Actor02000_D141B8[17] = {
#include "assets/actor_102000_animation_14354_bank4.inc"
};

GpAnimRec Actor02000_D141FC[76] = {
#include "assets/actor_102000_animation_14354_records.inc"
};

u16 Actor02000_D1432C[20] = {
#include "assets/actor_102000_animation_14354_indices.inc"
};

GpAnimSet Actor02000_D14354 = {
    Actor02000_D141FC,
    Actor02000_D1432C,
    { NULL, Actor02000_D141A0, NULL, NULL, Actor02000_D141B8, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D1437C[28] = {
#include "assets/actor_102000_animation_1529C_bank1.inc"
};

AnimationPackedRotation Actor02000_D144CC[394] = {
#include "assets/actor_102000_animation_1529C_bank4.inc"
};

GpAnimRec Actor02000_D14AF4[480] = {
#include "assets/actor_102000_animation_1529C_records.inc"
};

u16 Actor02000_D15274[20] = {
#include "assets/actor_102000_animation_1529C_indices.inc"
};

GpAnimSet Actor02000_D1529C = {
    Actor02000_D14AF4,
    Actor02000_D15274,
    { NULL, Actor02000_D1437C, NULL, NULL, Actor02000_D144CC, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D152C4[9] = {
#include "assets/actor_102000_animation_15830_bank1.inc"
};

AnimationPackedRotation Actor02000_D15330[127] = {
#include "assets/actor_102000_animation_15830_bank4.inc"
};

GpAnimRec Actor02000_D1552C[183] = {
#include "assets/actor_102000_animation_15830_records.inc"
};

u16 Actor02000_D15808[20] = {
#include "assets/actor_102000_animation_15830_indices.inc"
};

GpAnimSet Actor02000_D15830 = {
    Actor02000_D1552C,
    Actor02000_D15808,
    { NULL, Actor02000_D152C4, NULL, NULL, Actor02000_D15330, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D15858[5] = {
#include "assets/actor_102000_animation_15AF8_bank1.inc"
};

AnimationPackedRotation Actor02000_D15894[56] = {
#include "assets/actor_102000_animation_15AF8_bank4.inc"
};

GpAnimRec Actor02000_D15974[87] = {
#include "assets/actor_102000_animation_15AF8_records.inc"
};

u16 Actor02000_D15AD0[20] = {
#include "assets/actor_102000_animation_15AF8_indices.inc"
};

GpAnimSet Actor02000_D15AF8 = {
    Actor02000_D15974,
    Actor02000_D15AD0,
    { NULL, Actor02000_D15858, NULL, NULL, Actor02000_D15894, NULL, NULL, NULL },
};

AnimationPackedPose Actor02000_D15B20[2] = {
#include "assets/actor_102000_animation_15CD4_bank1.inc"
};

AnimationPackedRotation Actor02000_D15B38[17] = {
#include "assets/actor_102000_animation_15CD4_bank4.inc"
};

GpAnimRec Actor02000_D15B7C[76] = {
#include "assets/actor_102000_animation_15CD4_records.inc"
};

u16 Actor02000_D15CAC[20] = {
#include "assets/actor_102000_animation_15CD4_indices.inc"
};

GpAnimSet Actor02000_D15CD4 = {
    Actor02000_D15B7C,
    Actor02000_D15CAC,
    { NULL, Actor02000_D15B20, NULL, NULL, Actor02000_D15B38, NULL, NULL, NULL },
};

GpU16Pair Actor02000_D15CFC[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, 8 },
    { 15, 2 },
    { 5, 0 },
};

GpPairSrcE Actor02000_D15D10 = { Actor02000_D15CFC, 425, 125, 100, 5, 50, 6, 0, 0, 0 };

s16 Actor02000_D15D20[46] = {
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
};

s16 Actor02000_D15D7C[56] = {
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

s32 Actor02000_D15DEC[17] = {
    0,
    0x40140001,
    0x40140002,
    0x40140003,
    0x40140004,
    0x40140005,
    0x40140006,
    0x4014000C,
    0x4014000D,
    0x40140009,
    0x4014000A,
    0x4014000B,
    0x4014000E,
    0x4014000F,
    0x40140010,
    0x40140011,
    0x40140012,
};

Actor102000TextStorage7C50 Actor02000_D15E30 = { 0x40140007, { 0 } };

u16 Actor02000_D15E44[22] = {
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u16 Actor02000_D15E70[40] = {
    0,
    0,
    4,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

u16 Actor02000_D15EC0[40] = {
    0,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
};

u16 Actor02000_D15F10[50] = {
    0,
    4,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    1,
    2,
    1,
    0,
    3,
    2,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    3,
    1,
    0,
    1,
    0,
    2,
    2,
    2,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
};

u16 Actor02000_D15F74[34] = {
    0,
    0,
    2,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    4,
    0,
    2,
    2,
    0,
    2,
    0,
    4,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    4,
    0,
};

u16* Actor02000_D15FB8[6] = {
    NULL,
    Actor02000_D15E44,
    Actor02000_D15E70,
    Actor02000_D15EC0,
    Actor02000_D15F10,
    Actor02000_D15F74,
};

TaskDesc Actor02000_D15FD0[2] = {
    { 1, 96, Actor02000_Fn03728, { .model = &Actor02000_D08AA8 } },
    { 1, 96, Actor02000_Fn035E8, { .model = &Actor02000_D08FEC } },
};

GpAnimSet* Actor02000_D15FE8[31] = {
    NULL,
    &Actor02000_D09C20,
    &Actor02000_D0A588,
    &Actor02000_D0ABB0,
    &Actor02000_D0B3B0,
    &Actor02000_D0CAA4,
    &Actor02000_D0D1E8,
    &Actor02000_D0E7FC,
    &Actor02000_D0FC6C,
    &Actor02000_D0F534,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &Actor02000_D10190,
    &Actor02000_D10BA0,
    &Actor02000_D11648,
    &Actor02000_D11C28,
    &Actor02000_D1291C,
    &Actor02000_D13994,
    &Actor02000_D13E68,
    &Actor02000_D14178,
    &Actor02000_D14354,
    &Actor02000_D1529C,
    &Actor02000_D15830,
    &Actor02000_D15AF8,
    &Actor02000_D15CD4,
    NULL,
};

TaskFunc Actor02000_D16064[15] = {
    Actor02000_Fn03268,
    Actor02000_Fn00AEC,
    Actor02000_Fn02D5C,
    Actor02000_Fn01DF0,
    Actor02000_Fn02294,
    Actor02000_Fn035E0,
    Actor02000_Fn035E0,
    Actor02000_Fn035E0,
    Actor02000_Fn03348,
    Actor02000_Fn033D4,
    Actor02000_Fn0349C,
    Actor02000_Fn00E0C,
    Actor02000_Fn011E8,
    Actor02000_Fn012E0,
    Actor02000_Fn03528,
};

/// Per-frame tick for the actor's approach cycle, sharing the `field_6A8`
/// state with `Actor02000_Fn03268`. State 0 drains the `field_6DA` budget by
/// `field_69C` (0 while `field_698` is still under the per-animation entry of
/// `Actor02000_D03784`, 0x14 once it is past it) and runs
/// `Actor02000_Fn00CD0` every frame; when the budget runs out it switches to
/// animation 4 and state 1. State 1 waits for `field_698` to reach 0x60, then
/// either falls back to animation 2 (budget left) or starts the lunge:
/// animation 3, state 2, a fresh budget of 1000 per unit of the spawn record's
/// byte 1, and `field_6A2` / `field_6A4` set to the actor's current yaw and its
/// opposite. State 2 holds `field_69E` at 0x3B until `field_698` reaches 0x23,
/// then returns to animation 2 and state 0. As in `Actor02000_Fn03268`, a set
/// `field_6B2` or `Gp_StateF0.field_29` overrides everything with animation 2 and the
/// shared state-F0 slot.
void Actor02000_Fn00AEC(Task* arg0)
{
    Actor105600Work* work;
    GpEnemy*         spawn;
    GpCoord*         self;
    u8*              head;
    s16              state;
    s16              delta;
    s32              ang;
    s32              param;

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - 0x10;

    self  = arg0->extra.tmd->coords;
    work  = arg0->work;
    spawn = arg0->spawnArg2.pointer;
    state = work->field_6A8;

    switch (state) {
        case 0:
            delta = 0;
            if (work->field_698 >= Actor02000_D03784[work->field_694]) {
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
            Actor02000_Fn00CD0(arg0);
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

    if ((work->field_6B2 != 0) || (Gp_StateF0.field_29 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }

    SCRATCH_POP_BYTES(0x10);
}

static void Actor02000_Fn00CD0(Task* arg0)
{
    Actor105600Work* work;
    GpCoord*         self;
    s32              dx;
    s32              distance;
    s32              dz;
    s32              trigger;
    VECTOR*          head;
    VECTOR*          delta;

    self                 = arg0->extra.tmd->coords;
    work                 = arg0->work;
    head                 = SCRATCH_HEAD(VECTOR);
    delta                = head - 1;
    head[-1].vx          = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
    delta->vy            = 0;
    dz                   = Player_Status.coordMtx->t[2] - self->coord.t[2];
    delta->vz            = dz;
    dx                   = head[-1].vx;
    trigger              = 0;
    SCRATCH_HEAD(VECTOR) = delta;
    distance             = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x5DC) {
        if (Gp_StateF0.prefix.bytes.field_2 & 0x17) {
            work->field_6B2 = 1;
        }
    } else {
        if (Gp_StateF0.prefix.bytes.field_2 & 5) {
            trigger = 1;
        }
        if ((Gp_StateF0.prefix.bytes.field_2 & 0x12) && (distance < 0xBB8)) {
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
    SCRATCH_POP_BYTES(0x10);
}

void Actor02000_Fn00E0C(Task* arg0)
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
    GpCoord*         self;

    work  = arg0->work;
    self  = arg0->extra.tmd->coords;
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
            work->field_4CC.radius                             = 0x15E;
            work->field_69C                                    = 0;
            work->field_69E                                    = 0;
            work->field_6DE                                    = 1;
            work->field_4CC.flags                              = (u16)(work->field_4CC.flags | 0x4000);
            work->field_564.flags                              = (u16)(work->field_564.flags & 0xBFFF);
            ((GpEnemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->field_6D4                                    = 1;
            break;
        case 1:
            if (work->field_698 == 0x14) {
                snd = Actor02000_D15DEC[work->field_6D6 + 0xC] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = Actor02000_D15DEC[work->field_6D6 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan2, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 >= 0x42) {
                work->field_694 = 0x19;
                work->field_6D4 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                work->field_6AE = (u16)((random >> 0x10) & 0x3F);
                Gp_LcgState     = (s32)random;
                if (((GpEnemy*)arg0->spawnArg2.pointer)->hp > 0) {
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
                snd  = Actor02000_D15DEC[work->field_6D6 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan3 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan3, (s8)gpGetObjDepth(self));
            }
            if (work->field_698 >= 0x31) {
                work->field_694 = 0x1D;
                work->field_6D4 = 0;
                random2         = (Gp_LcgState * 5) + 0x71357911;
                work->field_6AE = (u16)((random2 >> 0x10) & 0x3F);
                Gp_LcgState     = (s32)random2;
                if (((GpEnemy*)arg0->spawnArg2.pointer)->hp > 0) {
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

/// Per-frame tick. State 0 picks the animation from `field_6B8`: 1 selects
/// animation 0x17 and hands over to state 1, anything else selects 0x1B and
/// hands over to state 2. State 1 waits for `field_698` to reach 0x10 and
/// state 2 waits for it to reach 0x16; both then play the matching idle
/// (0x19 / 0x1D), park `field_6A6` at 0xB, move to state 3 and roll a fresh
/// 6-bit dwell into `field_6AE`.
void Actor02000_Fn011E8(Task* arg0)
{
    Actor105600Work* work;
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

void Actor02000_Fn012E0(Task* arg0)
{
    Actor105600Work* work;
    GpCoord*         self;
    s32              snd;
    s16              state;

    work  = arg0->work;
    self  = arg0->extra.tmd->coords;
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
            work->field_4CC.radius                             = 0x15E;
            work->field_69C                                    = 0;
            work->field_69E                                    = 0;
            work->field_6DE                                    = 1;
            work->field_4CC.flags                             |= 0x4000;
            work->field_564.flags                             &= 0xBFFF;
            ((GpEnemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->field_6D4                                    = 1;
            break;
        case 1:
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            if (work->field_6B8 == 1) {
                if (work->field_698 == 0x14) {
                    s32 pan;

                    snd = Actor02000_D15DEC[work->field_6D6 + 0xC] | ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = Actor02000_D15DEC[work->field_6D6 + 8] | ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = Actor02000_D15DEC[work->field_6D6 + 8] | ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 12) << 8);
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

static void Actor02000_Fn0150C(Task* arg0)
{
    Actor105600Work* work;
    GpCoord*         coord;
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

    rot   = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    coord = arg0->extra.tmd->coords;
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
    SCRATCH_POP_BYTES(8);
}

/// Turns the model's fourth coordinate by the angles in `field_688`, then
/// eases the x and y angles back toward zero by 0x20 a call; once both have
/// settled, clears `field_6B4`.
static void Actor02000_Fn01698(Task* arg0)
{
    Actor105600Work* work;
    GpCoord*         coord;
    MATRIX*          matrix;
    s32              angleX;
    s32              angleY;
    s32              absX;
    s32              nextX;
    s32              absY;
    s32              nextY;
    s32              active;

    matrix = SCRATCH_PUSH(MATRIX);
    active = 0;
    work   = arg0->work;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(&work->field_688, matrix);
    gte_MulMatrix0(&coord[3].coord, matrix, &coord[3].coord);
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
    SCRATCH_POP_BYTES(0x20);
}

static void Actor02000_Fn018A4(Task* arg0)
{
    s32              snd;
    s32              pan;
    s32              pan2;
    Actor105600Work* work;
    GpCoord*         self;
    GpAnimRec*       rec;

    work = arg0->work;
    self = arg0->extra.tmd->coords;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & 0x20) && (work->field_6A0 & 0x20)) {
                snd = Actor02000_D15DEC[work->field_6D6 * 2 - 1] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
            }
            if (!(rec->flags & 0x10) && (work->field_6A0 & 0x10)) {
                snd  = Actor02000_D15DEC[work->field_6D6 * 2] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan2 = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)gpGetObjDepth(self));
            }
            work->field_6A0 = (u16)(rec->flags & 0x30);
        }
    }
}

static void Actor02000_Fn01A20(GpEnemy* ctx, Task* actor)
{
    VECTOR3          pos;
    SVECTOR*         scratch;
    s16              duration;
    s16              anim;
    GpCoord*         partA;
    GpCoord*         partB;
    GpCoord*         coord;
    GpCoord*         rootA;
    GpCoord*         rootB;
    GpCoord*         rootC;
    GpCoord*         rootD;
    s32              i;
    u32              random;
    Actor105600Work* work;
    Actor105600Work* animWork;

    work    = actor->work;
    coord   = actor->extra.tmd->coords;
    scratch = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    switch (Gp_StateF0.field_4) {
        case 0:
            actor->extra.tmd->flags = 0;
            ctx->node.state.b.flags = 0;
            break;
        case 1:
            coord->flg                      = 0;
            actor->extra.tmd->coords[3].flg = 0;
            Gp_UpdateCoord(coord);
            rootC  = actor->extra.tmd->coords;
            pos.vx = rootC->workm.t[0];
            pos.vy = rootC->workm.t[1];
            pos.vz = rootC->workm.t[2];
            Gp_UpdateActorColor(actor->spawnArg2.pointer, (VECTOR*)&pos, 0, 0);
            rootD  = actor->extra.tmd->coords;
            partB  = &rootD[3];
            pos.vx = partB->workm.t[0];
            pos.vy = rootD->workm.t[1];
            pos.vz = partB->workm.t[2];
            Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
            return;
        case 2:
            actor->extra.tmd->flags = 0x80;
            ctx->node.state.b.flags = 1;
            return;
    }
    switch (work->field_6A8) {
        case 0:
            ctx->recs = 0;
            Gp_UnlinkNode(&ctx->node);
            Gp_UnlinkObj(&work->field_47C);
            Gp_UnlinkObj(&work->field_564);
            Gp_UnlinkObj(&work->field_4CC);
            Gp_UnlinkObj(&work->field_5E4);
            if ((u32)((u16)work->field_6CA - 0x38) < 2U) {
                Gp_UnlinkObj(&work->field_61C);
            }
            Gp_ReleaseStateF0Add(actor, work->field_6CA);
            anim = 0x1D;
            if (work->field_6B8 == 1) {
                anim = 0x19;
            }
            work->field_694 = anim;
            work->field_6A8 = 1;
            ctx->spawnState = (u8)work->field_6B8;
            Gp_SaveEnemyPose(ctx);
            Gp_StateF0.field_29 = 1;
            break;
        case 1:
            if (!(work->field_698 & 3)) {
                scratch->vx = 0;
                scratch->vz = 0;
                random      = (Gp_LcgState * 5) + 0x71357911;
                scratch->vy = -((random >> 0x10) & 0x1FF);
                Gp_LcgState = random;
                Gp_SpawnEff(0x600E0, &actor->extra.tmd->coords[3], 0x400, scratch);
            }
            break;
    }
    animWork = actor->work;
    if (animWork->field_694 != animWork->field_696) {
        animWork->field_696 = (s16)(u16)animWork->field_694;
        animWork->field_698 = 0U;
        duration            = Actor02000_D03784[animWork->field_694];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&animWork->rig.anim, i, animWork->field_694, 0, duration);
        }
        coord->flg = 0;
    } else {
        animWork->field_698++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&animWork->rig.anim, i);
        }
        coord->flg = 0;
    }
    actor->extra.tmd->coords[3].flg = 0;
    Gp_UpdateCoord(coord);
    rootA  = actor->extra.tmd->coords;
    pos.vx = rootA->workm.t[0];
    pos.vy = rootA->workm.t[1];
    pos.vz = rootA->workm.t[2];
    Gp_UpdateActorColor(actor->spawnArg2.pointer, (VECTOR*)&pos, 0, 0);
    rootB  = actor->extra.tmd->coords;
    partA  = &rootB[3];
    pos.vx = partA->workm.t[0];
    pos.vy = rootB->workm.t[1];
    pos.vz = partA->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
    SCRATCH_POP_BYTES(8);
    return;
}

void Actor02000_Fn01DF0(Task* arg0)
{
    s16              state;
    s16              yawDiff;
    s32              magnitude;
    s16              angle;
    s16              wrapped;
    void**           scratch;
    s32              sound;
    s32              dx0;
    s32              dx2;
    s32              dz2;
    s32              dx1;
    s32              dz1;
    s32              dist;
    s32              dz0;
    s32              pan;
    u8*              head;
    Actor105600Work* work;
    GpCoord*         self;
    VECTOR*          delta;

    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 0x10;
    delta              = (VECTOR*)(head - 0x10);
    work               = arg0->work;
    state              = work->field_6A8;
    self               = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if ((work->field_698 >= 0x47) && (work->field_6CE = (s16)(work->field_6D0 > 0), ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]), dz0 = Player_Status.coordMtx->t[2] - self->coord.t[2], delta->vz = dz0, dx0 = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx0 * dx0) + (dz0 * dz0)) < 0x3E8) == 0))) {
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
                work->field_6A6        = 8;
                work->field_6A8        = 0;
                work->field_69C        = 0;
                work->field_69E        = 0;
                work->field_6CC        = 0;
                work->field_5E4.flags &= 0x7FFF;
                break;
            }
            if (work->field_698 >= (Actor02000_D03784[work->field_694] + 0x52)) {
                work->field_6A8 = 1;
                work->field_694 = 6;
            }
            break;
        case 1:
            work->field_69C              = 0x84;
            work->field_69E              = 0xF;
            ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(Player_Status.coordMtx->t[2] - self->coord.t[2]);
            work->field_6A4              = (s16)(ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF);
            dx1                          = ((VECTOR*)(head - 0x10))->vx;
            dz1                          = delta->vz;
            dist                         = SquareRoot0((dx1 * dx1) + (dz1 * dz1));
            if (work->field_6B6 < 0x4C) {
                if (dist < 0x5DC) {
                    work->field_6A8 = 2;
                    work->field_694 = 7;
                    work->field_69C = 0;
                } else {
                    yawDiff   = (ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF) - work->field_6A2;
                    magnitude = __builtin_abs(yawDiff);
                    if (magnitude < 0x800) {
                        angle = magnitude;
                    } else {
                        if (yawDiff > 0) {
                            wrapped = 0x1000 - yawDiff;
                        } else {
                            wrapped = yawDiff + 0x1000;
                        }
                        angle = wrapped;
                    }
                    if ((s16)angle >= 0x101) {
                        work->field_6A6 = 2;
                        work->field_6A8 = 2;
                        work->field_694 = 4;
                        work->field_6CC = 0;
                        work->field_6CE = 0;
                    }
                }
            } else {
                work->field_6A6       = 8;
                work->field_6A8       = 0;
                work->field_69C       = 0;
                work->field_69E       = 0;
                work->field_6CC       = 0;
                work->field_6CE       = 0;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            break;
        case 2:
            work->field_69C = 0;
            work->field_69E = 0;
            if ((work->field_698 < 0xD) && (work->field_6B6 >= 0x4C)) {
                work->field_6A6        = 8;
                work->field_6A8        = 0;
                work->field_69C        = 0;
                work->field_69E        = 0;
                work->field_6CC        = 0;
                work->field_6CE        = 0;
                work->field_5E4.flags &= 0x7FFF;
                break;
            }
            if (work->field_698 == 0xD) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.key   = Gp_PackPair(Actor02000_D15CFC, 1);
                sound                 = Actor02000_D15E30.value | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan                   = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(self));
            }
            if (work->field_698 == 0x1E) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags & 0x7FFF);
            }
            if (work->field_698 >= 0x3B) {
                work->field_6CE = 0;
                dx2             = Player_Status.coordMtx->t[0] - self->coord.t[0];
                delta->vx       = dx2;
                dz2             = Player_Status.coordMtx->t[2] - self->coord.t[2];
                delta->vz       = dz2;
                if (SquareRoot0((dx2 * dx2) + (dz2 * dz2)) < 0xBB8) {
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
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

void Actor02000_Fn02294(Task* arg0)
{
    s16              startFrame;
    s16              state;
    s16              frame;
    void**           scratch;
    s32              dz;
    s32              sound;
    s32              dx;
    s32              pan;
    u8*              head;
    Actor105600Work* work;
    GpCoord*         self;
    VECTOR*          delta;

    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = head - 0x10;
    delta              = (VECTOR*)(head - 0x10);
    work               = arg0->work;
    state              = work->field_6A8;
    self               = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(Player_Status.coordMtx->t[0] - self->coord.t[0]);
            dz                           = Player_Status.coordMtx->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = Actor02000_D03784[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (Actor02000_D03784[work->field_694] + 0x20)) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | 0x8000);
                work->field_5E4.key   = Gp_PackPair(Actor02000_D15CFC, 0);
            }
            if (work->field_698 == (Actor02000_D03784[work->field_694] + 0x21)) {
                sound = Actor02000_D15E30.value | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan   = (s8)Gp_GetObjPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(self));
            }
            if (work->field_698 >= (Actor02000_D03784[work->field_694] + 0x27)) {
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
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Enemy init. Allocates the 0x6E4-byte work block, points the model object at
/// the light / color matrices inside it, runs the animation context over its
/// nineteen slots, and spawns the companion enemy from `Actor02000_D15FD0`,
/// copying that model's texture page and CLUT row out of the current area
/// record. `GpEnemy.spawnState` then selects the variant: 0 builds the
/// full object set (list node, the four `Gp_LinkObj` nodes and their
/// `GpRec18` tables, and the optional CD prefetch of `field_6D6`),
/// while 1 and 2 only prime the animation state and hand the task to state 2.
static void Actor02000_Fn0251C(GpEnemy* ctx, Task* actor)
{
    Actor105600Work* work;
    TmdObject*       obj;
    GpCoord*         coord;
    GpCoord*         parts;
    GpCoord*         partsA;
    GpCoord*         partsB;
    GpCoord*         partsC;
    GpCoord*         effParts;
    GpEnemy*         eff;
    u16*             tbl;
    u8               param1[8];
    u8               param2[8];
    s32              i;
    s32              one;
    s32              kind;
    s32              param;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->flg                 = 0;
    obj->lightMtx              = &work->field_45C;
    obj->colorMtx              = &work->field_43C;
    work->field_6CA            = 0x14;
    work->field_66C            = Actor02000_D15FD0;
    work->field_670.coord      = &actor->extra.tmd->coords[3];
    work->field_670.spawnArgLo = 0x500;
    work->field_670.spawnArgHi = 2;
    func_800B3F84(&work->rig.anim, Actor02000_D15FE8, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 0x13; i++) {
        Gp_AnimResetSlot(&work->rig.anim, i, 1);
    }
    eff = Gp_SpawnEnemyFromTable(Actor02000_D15FD0, 1, 0, ctx);
    actorTintTask(eff->task, ctx);

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
    parts           = actor->extra.tmd->coords;
    ctx->bodyPos.vx = 0;
    ctx->bodyPos.vy = 0;
    ctx->bodyPos.vz = 0;
    ctx->param      = &Actor02000_D15D10;
    ctx->recs       = work->field_4EC;
    ctx->coord      = &parts[3];
    ctx->hp         = Actor02000_D15D10.hpMax;
    Gp_IncStateF0Ref(0);
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

    tbl = Actor02000_D15FB8[gGameSession->at4.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->at4.loc.area];
    }
    if (work->field_6D6 != 0) {
        param1[3] = 0;
        param1[2] = 0xA;
        param1[0] = work->field_6D6;
        param2[0] = 0x14;
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
    partsA                     = actor->extra.tmd->coords;
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

    partsB                   = actor->extra.tmd->coords;
    work->field_4CC.ctx.recs = work->field_4EC;
    work->field_4CC.pos.vx   = 0;
    work->field_4CC.pos.vy   = 0;
    work->field_4CC.pos.vz   = 0;
    work->field_4CC.key      = 0x30014;
    work->field_4CC.radius   = 0x190;
    work->field_4CC.flags    = 1;
    work->field_4CC.coord    = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= 0x8000;

    partsC                   = actor->extra.tmd->coords;
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

    effParts                 = eff->task->extra.tmd->coords;
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

/// Takes a pending reaction: while `field_6B8` is 0, bit 1 of the spawn
/// context's `reactionFlags` is cleared and the enemy switches to entry 0xA of
/// the `field_6A6` table with animation 0x14.
static inline void _actor02000ApplyReaction(Task* actor)
{
    GpEnemy*         spawn;
    Actor105600Work* work;
    u8               flags;

    spawn = actor->spawnArg2.pointer;
    flags = spawn->reactionFlags;
    work  = actor->work;
    if ((flags & 2) && (work->field_6B8 == 0)) {
        spawn->reactionFlags = flags & 0xFD;
        work->field_6A6      = 0xA;
        work->field_694      = 0x14;
        work->field_6A8      = 0;
        work->field_6E0      = 1;
    }
}

/// Saves the root coordinate's translation in `field_678`..`field_680`, then
/// moves it `field_69C` along its facing, raising it by 0x80 while `field_6DE`
/// is below 2.
static inline void _actor02000StepRoot(Task* actor)
{
    GpCoord*         coord;
    Actor105600Work* work;

    coord              = actor->extra.tmd->coords;
    work               = actor->work;
    work->field_678    = coord->coord.t[0];
    work->field_67C    = coord->coord.t[1];
    work->field_680    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_69C) >> 0xC;
    if (work->field_6DE < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_69C) >> 0xC;
}

/// Advances animation slots 1..0x12 by one frame, or, when `field_694` names a
/// new animation, restarts the frame count and cross-fades every slot to it
/// over the animation's `Actor02000_D03784` duration.
static inline void _actor02000TickAnim(Task* actor)
{
    Actor105600Work* work;
    s16              duration;
    s32              i;

    work = actor->work;
    if (work->field_694 != work->field_696) {
        work->field_696 = work->field_694;
        work->field_698 = 0;
        duration        = Actor02000_D03784[work->field_694];
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->field_694, 0, duration);
        }
    } else {
        work->field_698++;
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
}

/// Updates the enemy's colour from `coord`'s world position and draws the
/// ground quad under part 3.
static inline void _actor02000Draw(Task* actor, GpCoord* coord)
{
    VECTOR3  pos;
    GpCoord* root;
    GpCoord* part;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(actor->spawnArg2.pointer, (VECTOR*)&pos, 0, 0);
    root   = actor->extra.tmd->coords;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}

static void Actor02000_Fn02A34(GpEnemy* ctx, Task* actor)
{
    TmdObject*       model;
    Actor105600Work* work;
    GpCoord*         coord;

    work  = actor->work;
    model = actor->extra.tmd;
    coord = model->coords;
    switch (Gp_StateF0.field_4) {
        case 0:
            model->flags            = 0;
            ctx->node.state.b.flags = 0;
            break;
        case 1:
            _actor02000Draw(actor, coord);
            return;
        case 2:
            model->flags            = 0x80;
            ctx->node.state.b.flags = 1;
            return;
    }

    if (ctx->reactionFlags != 0) {
        _actor02000ApplyReaction(actor);
    }
    Actor02000_Fn00078(actor);
    Actor02000_D16064[work->field_6A6](actor);
    if (work->field_69E != 0) {
        Actor02000_Fn0150C(actor);
    }
    _actor02000StepRoot(actor);
    _actor02000TickAnim(actor);
    if (work->field_6B4 != 0) {
        Actor02000_Fn01698(actor);
    }
    Actor02000_Fn018A4(actor);
    coord->flg                      = 0;
    actor->extra.tmd->coords[3].flg = 0;
    Gp_UpdateCoord(coord);
    _actor02000Draw(actor, coord);
}

void Actor02000_Fn02D5C(Task* arg0)
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
    GpCoord*         coord;

    head             = SCRATCH_HEAD(u8);
    delta            = (VECTOR*)(head - 0x10);
    SCRATCH_HEAD(u8) = (u8*)delta;
    work             = arg0->work;
    state            = work->field_6A8;
    coord            = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= Actor02000_D03784[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
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
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                work->field_6A6 = 3;
                work->field_6A8 = 0;
                work->field_694 = 5;
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
    SCRATCH_POP_BYTES(0x10);
}

static s32 Actor02000_Fn0315C(SVECTOR* arg0, SVECTOR* arg1)
{
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret     = 0;
    node    = D_80115550;
    vec     = SCRATCH_PUSH(VECTOR);
    vec->vx = arg1->vx - arg0->vx;
    vec->vy = arg1->vy - arg0->vy;
    vec->vz = arg1->vz - arg0->vz;
    VectorNormal(vec, vec);
    for (; node != NULL; node = node->next) {
        if (node->field_3A & 0x40) {
            ret = func_800DFCCC(node, arg0, arg1, vec);
            if (ret == 1) {
                break;
            }
        }
    }
    SCRATCH_POP(VECTOR);
    return ret;
}

/// Per-frame tick, entry 0 of `Actor02000_D16064`. State 0 counts `field_6AE`
/// up to 0x5B frames and then hands over to state 1 with animation 4, running
/// `Actor02000_Fn00CD0` every frame meanwhile; state 1 waits for `field_698`
/// to reach 0x5E and drops back to state 0 with animation 1. Either way, once
/// `field_6B2` or the global `Gp_StateF0.field_29` is set the actor switches to
/// animation 2 and arms the shared state-F0 slot.
void Actor02000_Fn03268(Task* arg0)
{
    Actor105600Work* work;
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
            Actor02000_Fn00CD0(arg0);
            break;
        case 1:
            if (work->field_698 >= 0x5E) {
                work->field_694 = 1;
                work->field_6A8 = 0;
            }
            break;
    }

    if ((work->field_6B2 != 0) || (Gp_StateF0.field_29 != 0)) {
        work->field_6A6 = 2;
        work->field_6A8 = 0;
        work->field_694 = 2;
        work->field_6AE = 0;
        Gp_ArmStateF0(1);
    }
}

/// Per-frame tick. State 0 selects animation 0x11, hands over to state 1 and
/// clears the pair of counters at `field_69C`. State 1 waits for `field_698`
/// to reach 0x37, then parks at animation 2 / `field_6A6` 2 when `field_6E0`
/// is clear, or animation 0x14 / `field_6A6` 0xA otherwise, and drops back
/// to state 0.
void Actor02000_Fn03348(Task* arg0)
{
    Actor105600Work* work;
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

/// Per-frame tick. State 0 picks the animation from `field_6AA`: 1 selects
/// animation 0x12 and hands over to state 1, anything else selects 0x13 and
/// hands over to state 2; either way the pair of counters at `field_69C` is
/// cleared. State 1 waits for `field_698` to reach 0x50 and state 2 waits for
/// it to reach 0x3B, both dropping back to state 0 with animation 2.
void Actor02000_Fn033D4(Task* arg0)
{
    Actor105600Work* work;
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

/// Per-frame tick. State 0 waits for `Gp_TickObjFlag2` on the spawn block to
/// fire, then selects animation 0x13, clears `field_6E0` and advances to state
/// 1. State 1 waits for `field_698` to reach 0x3B and drops back to state 0
/// with animation 2.
void Actor02000_Fn0349C(Task* arg0)
{
    Actor105600Work* work;
    s16              state;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2(arg0->spawnArg2.pointer) != 0) {
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

/// Per-frame tick. State 0 picks the animation from `field_6B8`: 1 selects
/// animation 0x17 and hands over to state 1, anything else selects 0x1B and
/// hands over to state 2. State 1 waits for `field_698` to reach 0x10 and
/// state 2 waits for it to reach 0x16; both park the actor by writing its
/// dwell code to `field_30` and drop back to state 0.
void Actor02000_Fn03528(Task* arg0)
{
    Actor105600Work* work;
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
                arg0->state     = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                arg0->state     = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

void Actor02000_Fn035E0(Task* task)
{
}

void Actor02000_Fn035E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02000_D00060;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}

/// Parents this actor's model to part 7 of its spawner's model, points the
/// model at the spawner's light and colour matrices and seeds the spawner's
/// dwell counter, then advances the task to state 1. `arg0` is the enemy
/// context every state handler takes and is unused here.
static void Actor02000_Fn03644(GpEnemy* arg0, Task* task)
{
    Task*            parent;
    TmdObject*       obj;
    Actor105600Work* work;
    GpCoord*         coord;
    GpCoord*         parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = (Actor105600Work*)parent->work;

    coord->flg      = 0;
    coord->sub      = &parentCoords[7];
    obj->lightMtx   = &work->field_45C;
    obj->colorMtx   = &work->field_43C;
    obj->flags      = 0;
    task->state     = 1;
    work->field_6D8 = 0xA;
}

static void Actor02000_Fn03690(GpEnemy* arg0, Task* task)
{
    GpEffWork*       effect;
    Task*            parent;
    Actor105600Work* work;
    s16              count;

    parent                 = task->parent;
    work                   = (Actor105600Work*)parent->work;
    task->extra.tmd->flags = (u16)parent->extra.tmd->flags;
    if (work->field_6D8 > 0) {
        count           = (u16)work->field_6D8 - 1;
        work->field_6D8 = count;
        if (count == 0) {
            effect = Gp_SpawnEff(D_8011572C | 0x80000000,
                                 &task->parent->extra.tmd->coords[7], 0, NULL);
            if (effect != NULL) {
                Task_Reparent(task, effect->task);
            }
        }
    }
}

void Actor02000_Fn03728(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02000_D0006C;
    sp.funcs[arg0->state](((GpEnemy*)arg0->spawnArg2.pointer), arg0);
}

static const GpEnemyTaskFuncTable3 Actor02000_D00060 = { {
    Actor02000_Fn03644,
    Actor02000_Fn03690,
    Gp_DestroyEnemy,
} };

static const GpEnemyTaskFuncTable3 Actor02000_D0006C = { {
    Actor02000_Fn0251C,
    Actor02000_Fn02A34,
    Actor02000_Fn01A20,
} };
