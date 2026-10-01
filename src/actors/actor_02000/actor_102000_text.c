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
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/random.h"
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
#include "../../shared/golem_pawn_rook.h"

static const GpEnemyTaskFuncTable3 Actor02000_D00060;
static const GpEnemyTaskFuncTable3 Actor02000_D0006C;

static s32 Actor02000_Fn0315C(SVECTOR* start, SVECTOR* end);

extern DamageAttack gGolemPawnRookAttacks[];
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s32 value;
    u8  retained[16];
} Actor102000TextStorage7C50;
STATIC_ASSERT_SIZEOF(Actor102000TextStorage7C50, 20);

extern Actor102000TextStorage7C50 gGolemPawnRookSwingCue;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern s16 gGolemPawnRookAnimBlendFrames[];
extern s16 Actor02000_D15D20[];
extern s16 Actor02000_D15D7C[];
extern s32 gGolemPawnRookVoiceCues[];

extern AnimationSet Actor02000_D09C20;

extern AnimationSet Actor02000_D0A588;

extern AnimationSet Actor02000_D0ABB0;

extern AnimationSet Actor02000_D0B3B0;

extern AnimationSet Actor02000_D0CAA4;

extern AnimationSet Actor02000_D0D1E8;

extern AnimationSet Actor02000_D0E7FC;

extern AnimationSet Actor02000_D0F534;

extern AnimationSet Actor02000_D0FC6C;

extern AnimationSet Actor02000_D10190;

extern AnimationSet Actor02000_D10BA0;

extern AnimationSet Actor02000_D11648;

extern AnimationSet Actor02000_D11C28;

extern AnimationSet Actor02000_D1291C;

extern AnimationSet Actor02000_D13994;

extern AnimationSet Actor02000_D13E68;

extern AnimationSet Actor02000_D14178;

extern AnimationSet Actor02000_D14354;

extern AnimationSet Actor02000_D1529C;

extern AnimationSet Actor02000_D15830;

extern AnimationSet Actor02000_D15AF8;

extern AnimationSet Actor02000_D15CD4;

extern TmdSource Actor02000_D08AA8;

extern TmdSource Actor02000_D08FEC;

void Actor02000_Fn00E0C(Task*);

void Actor02000_Fn01DF0(Task*);

void Actor02000_Fn02D5C(Task*);

void Actor02000_Fn035E8(Task*);

void Actor02000_Fn03728(Task*);

extern AnimationSet* Actor02000_D15FE8[31];

extern TaskDesc Actor02000_D15FD0[];

extern u16* Actor02000_D15FB8[];

extern EnemyParams Actor02000_D15D10;

extern TaskFunc gGolemPawnRookStates[];

static void Actor02000_Fn0251C(Enemy* ctx, Task* actor);

/// Hit and push tick. Applies the `field_584` / `field_4EC` collision deltas
/// to the root coordinate, then walks the five `field_4EC` records: kind 2 is a
/// weapon hit (damage, crit roll, the `field_6D0` weak-point budget, and the
/// reaction animation picked into `field_6A6`), kind 3 a push-out whose
/// deepest overlap is applied to the root after the loop. Finally raises
/// `field_6B2` when the player's segment test against `field_4B4` fails.
void golemPawnRookTakeHits(Task* arg0)
{
    s32                      result;
    s32                      maxPush;
    s32                      hit;
    u32                      lastId;
    GolemPawnRookWork*       work;
    GpDeltaScratch*          head;
    GolemPawnRookHitScratch* scratch;
    Enemy*                   enemy;
    GfxCoord*                self;
    GfxCoord*                other;
    GfxCoord*                part;
    s32                      i;
    s32                      x, y, z;
    s32                      damage;
    s32                      kind;
    s32                      dz;
    s32                      clamped;
    s32                      val;
    s32                      push;
    s16                      cooldown;
    u32                      rng;
    s32                      tilt;
    s32                      byte1;
    s32                      max;

    result  = 0;
    maxPush = 0;
    hit     = 0;
    lastId  = 0;
    work    = arg0->work;
    head    = SCRATCH_STACK_CURSOR(GpDeltaScratch);
    self    = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BLOCK(GolemPawnRookHitScratch);
    scratch = SCRATCH_STACK_CURSOR(GolemPawnRookHitScratch);
    enemy   = arg0->spawnArg2.pointer;

    switch (func_800E0C10(work->field_584, head - 4, 4, NULL)) {
        case 0:
            break;
        case 1:
            self->coord.t[0] += head[-4].vx.halves.integer;
            self->coord.t[1] += scratch->delta.vy.halves.integer;
            self->coord.t[2] += scratch->delta.vz.halves.integer;
            break;
        case 2:
            self->coord.t[0] = work->field_678;
            self->coord.t[1] = work->field_67C;
            self->coord.t[2] = work->field_680;
            break;
    }
    Gp_ClearRec18Occupied(work->field_584);

    if (work->field_4CC.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (func_800E0C10(work->field_4EC, &scratch->delta, 5, NULL)) {
            case 0:
                break;
            case 1:
                self->coord.t[0] += scratch->delta.vx.halves.integer;
                self->coord.t[2] += scratch->delta.vz.halves.integer;
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
        switch ((u32)work->field_4EC[i].key.value >> 16) {
            case 0:
            case 1:
                break;
            case 2:
                if (work->field_69A != 0) {
                    break;
                }
                other                  = gPlayerActorTasks[((u32)work->field_4EC[i].key.value >> 7) & 1]->extra.tmd->coords;
                scratch->delta.vx.word = other->coord.t[0] - self->coord.t[0];
                scratch->delta.vy.word = other->coord.t[1] - self->coord.t[1];
                dz                     = other->coord.t[2] - self->coord.t[2];
                scratch->delta.vz.word = dz;
                val                    = (scratch->delta.vx.word * self->coord.m[0][2]) + (scratch->delta.vy.word * self->coord.m[1][2]) + (dz * self->coord.m[2][2]);
                work->field_6AA        = val >= 0;
                damage                 = Gp_ComputeDamage(work->field_4EC[i].key.value,
                                                          SquareRoot0((scratch->delta.vx.word * scratch->delta.vx.word) + (scratch->delta.vy.word * scratch->delta.vy.word) + (scratch->delta.vz.word * scratch->delta.vz.word)),
                                                          0, 0);
                kind                   = Gp_GetIdParam0(work->field_4EC[i].key.value);
                if (work->field_6CE != 0 && work->field_6AA == 1 && work->field_6B8 == 0) {
                    if (work->field_4EC[i].key.value & 0x8000) {
                        if (Actor02000_D15D7C[work->field_4EC[i].key.value & 0x7F] != 0) {
                            hit              = 1;
                            work->field_6D0 -= damage;
                        }
                    } else if (Actor02000_D15D20[work->field_4EC[i].key.value & 0x7F] != 0) {
                        hit              = 1;
                        work->field_6D0 -= damage;
                    }
                    if (hit == 1) {
                        if (work->field_6D0 <= 0) {
                            work->field_6A6        = 9;
                            work->field_6CE        = 0;
                            work->field_6D2        = 1;
                            work->field_6A8        = 0;
                            work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                            if (work->field_690 != NULL) {
                                work->field_690->task->state = 3;
                                work->field_690              = NULL;
                            }
                        }
                        func_800DA6E8(&enemy->node, 0, 0);
                        cooldown = Gp_GetIdParam2(work->field_4EC[i].key.value);
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
                if (Gp_RollEnemyChance(enemy, work->field_4EC[i].key.value, 0) != 0) {
                    damage *= 4;
                    if ((kind & 0xFFFF) != 5) {
                        Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                    }
                    if (work->field_6E0 == 0) {
                        result = 1;
                    }
                }
                if (work->field_6C4 != 0 && (work->field_4EC[i].key.value & 0x8000)) {
                    damage >>= 2;
                }
                func_800DA6E8(&enemy->node, damage, 0);
                func_800E2C78(enemy, work->field_4EC[i].key.value, damage, 0);
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
                            Gp_SetObjFlag2(enemy, work->field_4EC[i].key.value, 0);
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
                if (lastId != work->field_4EC[i].key.value) {
                    lastId             = work->field_4EC[i].key.value;
                    scratch->effOfs.vx = 0;
                    scratch->effOfs.vy = 0;
                    scratch->effOfs.vz = (work->field_6AA == 1) ? 0x12C : -0x96;
                    func_800FDB18(Gp_GetIdParam1(work->field_4EC[i].key.value) & 0xFFFF, &arg0->extra.tmd->coords[3],
                                  &scratch->effOfs, &work->field_670);
                }
                cooldown = Gp_GetIdParam2(work->field_4EC[i].key.value);
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
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        rng             = gRandomLcgState >> 16;
                        tilt            = (rng & 0x7F) + 0x40;
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
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 2:
                        work->field_6A6        = 9;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                        break;
                    case 3:
                        work->field_6A6        = 0xB;
                        work->field_6A8        = 0;
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
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
                        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
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
                part                   = &arg0->extra.tmd->coords[3];
                x                      = part->workm.t[0] - work->field_4EC[i].point.vx;
                scratch->delta.vx.word = x;
                y                      = part->workm.t[1] - work->field_4EC[i].point.vy;
                scratch->delta.vy.word = y;
                z                      = part->workm.t[2] - work->field_4EC[i].point.vz;
                scratch->delta.vz.word = z;
                push                   = work->field_4EC[i].distance - SquareRoot0((x * x) + (y * y) + (z * z));
                clamped                = push;
                if (push <= 0) {
                    clamped = 0;
                }
                push = clamped;
                if (maxPush < push) {
                    maxPush = push;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, &scratch->normal, &scratch->push);
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
        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        Gp_ClearRec18Occupied(work->field_604);
    }
    work->field_6B2 = 0;
    if (Gp_CountRec18Hi(work->field_4B4, 0x10000) != 0) {
        part               = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[4];
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
    SCRATCH_STACK_RELEASE_BYTES(0x40);
}

s16 gGolemPawnRookAnimBlendFrames[32] = {
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

AnimationRecord Actor02000_D09600[382] = {
#include "assets/actor_102000_animation_09C20_records.inc"
};

u16 Actor02000_D09BF8[20] = {
#include "assets/actor_102000_animation_09C20_indices.inc"
};

AnimationSet Actor02000_D09C20 = {
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

AnimationRecord Actor02000_D0A0BC[297] = {
#include "assets/actor_102000_animation_0A588_records.inc"
};

u16 Actor02000_D0A560[20] = {
#include "assets/actor_102000_animation_0A588_indices.inc"
};

AnimationSet Actor02000_D0A588 = {
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

AnimationRecord Actor02000_D0A878[196] = {
#include "assets/actor_102000_animation_0ABB0_records.inc"
};

u16 Actor02000_D0AB88[20] = {
#include "assets/actor_102000_animation_0ABB0_indices.inc"
};

AnimationSet Actor02000_D0ABB0 = {
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

AnimationRecord Actor02000_D0AF68[264] = {
#include "assets/actor_102000_animation_0B3B0_records.inc"
};

u16 Actor02000_D0B388[20] = {
#include "assets/actor_102000_animation_0B3B0_indices.inc"
};

AnimationSet Actor02000_D0B3B0 = {
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

AnimationRecord Actor02000_D0BFA0[695] = {
#include "assets/actor_102000_animation_0CAA4_records.inc"
};

u16 Actor02000_D0CA7C[20] = {
#include "assets/actor_102000_animation_0CAA4_indices.inc"
};

AnimationSet Actor02000_D0CAA4 = {
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

AnimationRecord Actor02000_D0CE30[228] = {
#include "assets/actor_102000_animation_0D1E8_records.inc"
};

u16 Actor02000_D0D1C0[20] = {
#include "assets/actor_102000_animation_0D1E8_indices.inc"
};

AnimationSet Actor02000_D0D1E8 = {
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

AnimationRecord Actor02000_D0DDBC[646] = {
#include "assets/actor_102000_animation_0E7FC_records.inc"
};

u16 Actor02000_D0E7D4[20] = {
#include "assets/actor_102000_animation_0E7FC_indices.inc"
};

AnimationSet Actor02000_D0E7FC = {
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

AnimationRecord Actor02000_D0EEA8[409] = {
#include "assets/actor_102000_animation_0F534_records.inc"
};

u16 Actor02000_D0F50C[20] = {
#include "assets/actor_102000_animation_0F534_indices.inc"
};

AnimationSet Actor02000_D0F534 = {
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

AnimationRecord Actor02000_D0F8E0[217] = {
#include "assets/actor_102000_animation_0FC6C_records.inc"
};

u16 Actor02000_D0FC44[20] = {
#include "assets/actor_102000_animation_0FC6C_indices.inc"
};

AnimationSet Actor02000_D0FC6C = {
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

AnimationRecord Actor02000_D0FEC4[169] = {
#include "assets/actor_102000_animation_10190_records.inc"
};

u16 Actor02000_D10168[20] = {
#include "assets/actor_102000_animation_10190_indices.inc"
};

AnimationSet Actor02000_D10190 = {
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

AnimationRecord Actor02000_D10698[312] = {
#include "assets/actor_102000_animation_10BA0_records.inc"
};

u16 Actor02000_D10B78[20] = {
#include "assets/actor_102000_animation_10BA0_indices.inc"
};

AnimationSet Actor02000_D10BA0 = {
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

AnimationRecord Actor02000_D11104[327] = {
#include "assets/actor_102000_animation_11648_records.inc"
};

u16 Actor02000_D11620[20] = {
#include "assets/actor_102000_animation_11648_indices.inc"
};

AnimationSet Actor02000_D11648 = {
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

AnimationRecord Actor02000_D118D8[202] = {
#include "assets/actor_102000_animation_11C28_records.inc"
};

u16 Actor02000_D11C00[20] = {
#include "assets/actor_102000_animation_11C28_indices.inc"
};

AnimationSet Actor02000_D11C28 = {
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

AnimationRecord Actor02000_D1227C[414] = {
#include "assets/actor_102000_animation_1291C_records.inc"
};

u16 Actor02000_D128F4[20] = {
#include "assets/actor_102000_animation_1291C_indices.inc"
};

AnimationSet Actor02000_D1291C = {
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

AnimationRecord Actor02000_D1316C[512] = {
#include "assets/actor_102000_animation_13994_records.inc"
};

u16 Actor02000_D1396C[20] = {
#include "assets/actor_102000_animation_13994_indices.inc"
};

AnimationSet Actor02000_D13994 = {
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

AnimationRecord Actor02000_D13BEC[149] = {
#include "assets/actor_102000_animation_13E68_records.inc"
};

u16 Actor02000_D13E40[20] = {
#include "assets/actor_102000_animation_13E68_indices.inc"
};

AnimationSet Actor02000_D13E68 = {
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

AnimationRecord Actor02000_D13FD0[96] = {
#include "assets/actor_102000_animation_14178_records.inc"
};

u16 Actor02000_D14150[20] = {
#include "assets/actor_102000_animation_14178_indices.inc"
};

AnimationSet Actor02000_D14178 = {
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

AnimationRecord Actor02000_D141FC[76] = {
#include "assets/actor_102000_animation_14354_records.inc"
};

u16 Actor02000_D1432C[20] = {
#include "assets/actor_102000_animation_14354_indices.inc"
};

AnimationSet Actor02000_D14354 = {
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

AnimationRecord Actor02000_D14AF4[480] = {
#include "assets/actor_102000_animation_1529C_records.inc"
};

u16 Actor02000_D15274[20] = {
#include "assets/actor_102000_animation_1529C_indices.inc"
};

AnimationSet Actor02000_D1529C = {
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

AnimationRecord Actor02000_D1552C[183] = {
#include "assets/actor_102000_animation_15830_records.inc"
};

u16 Actor02000_D15808[20] = {
#include "assets/actor_102000_animation_15830_indices.inc"
};

AnimationSet Actor02000_D15830 = {
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

AnimationRecord Actor02000_D15974[87] = {
#include "assets/actor_102000_animation_15AF8_records.inc"
};

u16 Actor02000_D15AD0[20] = {
#include "assets/actor_102000_animation_15AF8_indices.inc"
};

AnimationSet Actor02000_D15AF8 = {
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

AnimationRecord Actor02000_D15B7C[76] = {
#include "assets/actor_102000_animation_15CD4_records.inc"
};

u16 Actor02000_D15CAC[20] = {
#include "assets/actor_102000_animation_15CD4_indices.inc"
};

AnimationSet Actor02000_D15CD4 = {
    Actor02000_D15B7C,
    Actor02000_D15CAC,
    { NULL, Actor02000_D15B20, NULL, NULL, Actor02000_D15B38, NULL, NULL, NULL },
};

DamageAttack gGolemPawnRookAttacks[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, 8 },
    { 15, 2 },
    { 5, 0 },
};

EnemyParams Actor02000_D15D10 = { gGolemPawnRookAttacks, 425, 125, 100, 5, 50, 6, 0, 0 };

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

s32 gGolemPawnRookVoiceCues[17] = {
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

Actor102000TextStorage7C50 gGolemPawnRookSwingCue = { 0x40140007, { 0 } };

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
    { { { TASK_BODY_TMD, 96 } }, Actor02000_Fn03728, { .model = &Actor02000_D08AA8 } },
    { { { TASK_BODY_TMD, 96 } }, Actor02000_Fn035E8, { .model = &Actor02000_D08FEC } },
};

AnimationSet* Actor02000_D15FE8[31] = {
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

TaskFunc gGolemPawnRookStates[15] = {
    golemPawnRookIdleState,
    golemPawnRookApproachState,
    Actor02000_Fn02D5C,
    Actor02000_Fn01DF0,
    golemPawnRookBeamSwingState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookNopState,
    golemPawnRookHitReactionState,
    golemPawnRookRecoilState,
    golemPawnRookFlagWaitState,
    Actor02000_Fn00E0C,
    golemPawnRookDownedShiftState,
    golemPawnRookCollapseState,
    golemPawnRookDownedFinishState,
};

#include "../../shared/golem_pawn_rook_approach.inc.c"

#include "../../shared/golem_pawn_rook_proximity.inc.c"

void Actor02000_Fn00E0C(Task* arg0)
{
    s16                state;
    s16                nextAnim;
    s16                nextAnim2;
    s32                snd;
    s32                random3;
    s32                pan;
    s32                pan2;
    s32                pan3;
    u16                timer;
    u16                timer2;
    u32                random;
    u32                random2;
    GolemPawnRookWork* work;
    GfxCoord*          self;

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
            work->field_4CC.radius                           = 0x15E;
            work->field_69C                                  = 0;
            work->field_69E                                  = 0;
            work->field_6DE                                  = 1;
            work->field_4CC.flags                            = (u16)(work->field_4CC.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_564.flags                            = (u16)(work->field_564.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
            ((Enemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->field_6D4                                  = 1;
            break;
        case 1:
            if (work->field_698 == 0x14) {
                snd = gGolemPawnRookVoiceCues[work->field_6D6 + 0xC] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = gGolemPawnRookVoiceCues[work->field_6D6 + 8] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan2 = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 >= 0x42) {
                work->field_694 = 0x19;
                work->field_6D4 = 0;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_6AE = (u16)((random >> 0x10) & 0x3F);
                gRandomLcgState = random;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
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
                snd  = gGolemPawnRookVoiceCues[work->field_6D6 + 8] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan3 = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 >= 0x31) {
                work->field_694 = 0x1D;
                work->field_6D4 = 0;
                random2         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_6AE = (u16)((random2 >> 0x10) & 0x3F);
                gRandomLcgState = random2;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
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
                random3         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random3;
                work->field_6AE = (u16)(((u32)random3 >> 0x10) & 0x3F);
            }
            break;
    }
}

#include "../../shared/golem_pawn_rook_downed_shift.inc.c"

#include "../../shared/golem_pawn_rook_collapse.inc.c"

#include "../../shared/golem_pawn_rook_turn.inc.c"

#include "../../shared/golem_pawn_rook_hit_tilt.inc.c"

#include "../../shared/golem_pawn_rook_anim_cues.inc.c"

#include "../../shared/golem_pawn_rook_dead.inc.c"

void Actor02000_Fn01DF0(Task* arg0)
{
    s16                state;
    s16                yawDiff;
    s32                magnitude;
    s16                angle;
    s16                wrapped;
    void**             scratch;
    s32                sound;
    s32                dx0;
    s32                dx2;
    s32                dz2;
    s32                dx1;
    s32                dz1;
    s32                dist;
    s32                dz0;
    s32                pan;
    u8*                head;
    GolemPawnRookWork* work;
    GfxCoord*          self;
    VECTOR*            delta;

    head                       = SCRATCH_STACK_CURSOR(void);
    SCRATCH_STACK_CURSOR(void) = head - 0x10;
    delta                      = (VECTOR*)(head - 0x10);
    work                       = arg0->work;
    state                      = work->field_6A8;
    self                       = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if ((work->field_698 >= 0x47) && (work->field_6CE = (s16)(work->field_6D0 > 0), ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]), dz0 = gPlayerStatus.coordMtx->t[2] - self->coord.t[2], delta->vz = dz0, dx0 = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx0 * dx0) + (dz0 * dz0)) < 0x3E8) == 0))) {
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
                work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            }
            if (work->field_698 >= (gGolemPawnRookAnimBlendFrames[work->field_694] + 0x52)) {
                work->field_6A8 = 1;
                work->field_694 = 6;
            }
            break;
        case 1:
            work->field_69C              = 0x84;
            work->field_69E              = 0xF;
            ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(gPlayerStatus.coordMtx->t[2] - self->coord.t[2]);
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
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
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
                work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                break;
            }
            if (work->field_698 == 0xD) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_5E4.key   = Gp_PackPair(gGolemPawnRookAttacks, 1);
                sound                 = gGolemPawnRookSwingCue.value | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan                   = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 == 0x1E) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            if (work->field_698 >= 0x3B) {
                work->field_6CE = 0;
                dx2             = gPlayerStatus.coordMtx->t[0] - self->coord.t[0];
                delta->vx       = dx2;
                dz2             = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
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

#include "../../shared/golem_pawn_rook_beam_swing.inc.c"

/// Enemy init. Allocates the 0x6E4-byte work block, points the model object at
/// the light / color matrices inside it, runs the animation context over its
/// nineteen slots, and spawns the companion enemy from `Actor02000_D15FD0`,
/// copying that model's texture page and CLUT row out of the current area
/// record. `Enemy.spawnState` then selects the variant: 0 builds the
/// full object set (list node, the four `Gp_LinkObj` nodes and their
/// `WorldCollisionContact` tables, and the optional CD prefetch of `field_6D6`),
/// while 1 and 2 only prime the animation state and hand the task to state 2.
static void Actor02000_Fn0251C(Enemy* ctx, Task* actor)
{
    GolemPawnRookWork* work;
    TmdObject*         obj;
    GfxCoord*          coord;
    GfxCoord*          parts;
    GfxCoord*          partsA;
    GfxCoord*          partsB;
    GfxCoord*          partsC;
    GfxCoord*          effParts;
    Enemy*             eff;
    u16*               tbl;
    u8                 param1[8];
    u8                 param2[8];
    s32                i;
    s32                one;
    s32                kind;
    s32                param;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x6E4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work                = work;
    obj->flags                 = 0;
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
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

    tbl = Actor02000_D15FB8[gGameSession->location.loc.stage];
    if (tbl != NULL) {
        work->field_6D6 = tbl[gGameSession->location.loc.area];
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

    work->field_49C.ends[0].vz      = 0x1F40;
    work->field_49C.end0Radius      = 0x3E8;
    work->field_49C.ends[0].vx      = 0;
    work->field_49C.ends[0].vy      = 0;
    work->field_49C.ends[1].vx      = 0;
    work->field_49C.ends[1].vy      = 0;
    work->field_49C.ends[1].vz      = 0;
    work->field_49C.end1Radius      = 0x5DC;
    work->field_49C.contacts        = work->field_4B4;
    partsA                          = actor->extra.tmd->coords;
    work->field_47C.context.capsule = &work->field_49C;
    work->field_47C.pos.vx          = 0;
    work->field_47C.pos.vy          = 0;
    work->field_47C.pos.vz          = 0;
    work->field_47C.key             = 0;
    work->field_47C.radius          = 0;
    work->field_47C.flags           = WORLD_COLLISION_BODY_CAPSULE;
    work->field_47C.coord           = &partsA[4];
    Gp_LinkObj(3, &work->field_47C);
    Gp_InitRec18Table(work->field_4B4, 1, 0);
    work->field_47C.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

    partsB                           = actor->extra.tmd->coords;
    work->field_4CC.context.contacts = work->field_4EC;
    work->field_4CC.pos.vx           = 0;
    work->field_4CC.pos.vy           = 0;
    work->field_4CC.pos.vz           = 0;
    work->field_4CC.key              = 0x30014;
    work->field_4CC.radius           = 0x190;
    work->field_4CC.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_4CC.coord            = &partsB[3];
    Gp_LinkObj(2, &work->field_4CC);
    Gp_InitRec18Table(work->field_4EC, 5, 0);
    work->field_4CC.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    partsC                           = actor->extra.tmd->coords;
    work->field_564.pos.vy           = -0x226;
    work->field_564.context.contacts = work->field_584;
    work->field_564.pos.vx           = 0;
    work->field_564.pos.vz           = 0;
    work->field_564.key              = 0;
    work->field_564.radius           = 0x226;
    work->field_564.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_564.coord            = partsC;
    Gp_LinkObj(2, &work->field_564);
    Gp_InitRec18Table(work->field_584, 4, 0);
    work->field_564.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

    effParts                         = eff->task->extra.tmd->coords;
    work->field_5E4.context.contacts = work->field_604;
    work->field_5E4.pos.vx           = 0;
    work->field_5E4.pos.vy           = 0x1F4;
    work->field_5E4.pos.vz           = 0;
    work->field_5E4.key              = 0;
    work->field_5E4.radius           = 0x1F4;
    work->field_5E4.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_5E4.coord            = effParts;
    Gp_LinkObj(3, &work->field_5E4);
    Gp_InitRec18Table(work->field_604, 1, 0);
    work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
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

#include "../../shared/golem_pawn_rook_inlines.inc.c"

/// Saves the root coordinate's translation in `field_678`..`field_680`, then
/// Updates the enemy's colour from `coord`'s world position and draws the
#include "../../shared/golem_pawn_rook_frame_no_dust.inc.c"

void Actor02000_Fn02D5C(Task* arg0)
{
    s16                yaw;
    s16                yaw2;
    s16                state;
    s16                deltaYaw;
    s16                deltaYaw2;
    s16                speed;
    s32                magnitude;
    s32                magnitude2;
    s16                wrapped;
    s16                wrapped2;
    s16                angle;
    s32                dx;
    s32                dz;
    u16                flags;
    u16                flags2;
    u8*                head;
    VECTOR*            delta;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    head                     = SCRATCH_STACK_CURSOR(u8);
    delta                    = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)delta;
    work                     = arg0->work;
    state                    = work->field_6A8;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= gGolemPawnRookAnimBlendFrames[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
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
                flags                 = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            dz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
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
            flags2                = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
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
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static s32 Actor02000_Fn0315C(SVECTOR* arg0, SVECTOR* arg1)
{
    VECTOR*  vec;
    GpObj3A* node;
    s32      ret;

    ret     = 0;
    node    = D_80115550;
    vec     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
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
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
    return ret;
}

#include "../../shared/golem_pawn_rook_idle.inc.c"

#include "../../shared/golem_pawn_rook_hit_reaction.inc.c"

#include "../../shared/golem_pawn_rook_recoil.inc.c"

#include "../../shared/golem_pawn_rook_flag_wait.inc.c"

#include "../../shared/golem_pawn_rook_downed_finish.inc.c"

#include "../../shared/golem_pawn_rook_nop.inc.c"

void Actor02000_Fn035E8(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02000_D00060;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

#include "../../shared/golem_pawn_rook_delayed_effect_spawn.inc.c"

#include "../../shared/golem_pawn_rook_delayed_effect_tick.inc.c"

void Actor02000_Fn03728(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02000_D0006C;
    sp.funcs[arg0->state](((Enemy*)arg0->spawnArg2.pointer), arg0);
}

static const GpEnemyTaskFuncTable3 Actor02000_D00060 = { {
    golemPawnRookDelayedEffectSpawn,
    golemPawnRookDelayedEffectTick,
    Gp_DestroyEnemy,
} };

static const GpEnemyTaskFuncTable3 Actor02000_D0006C = { {
    Actor02000_Fn0251C,
    golemPawnRookFrameStateNoDust,
    golemPawnRookDeadState,
} };
