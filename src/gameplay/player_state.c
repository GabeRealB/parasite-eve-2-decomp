#include "gameplay/player_state.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "damage.h"
#include "gameplay/effects.h"
#include "items.h"
#include "loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/damage.h"
#include "gameplay/player_actor.h"
#include "player_actor.h"
#include "player_state.h"
#include "gameplay/room_effects.h"
#include "room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "actors/companion.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// Duration of each timed player ailment, in active status updates.
enum { PLAYER_STATE_STATUS_DURATION_TICKS = 600 };

/// Scratch-stack block for turning a player actor's body yaw toward a point.
///
/// Holds the target's displacement from the model's root coordinate, whose X
/// and Z give the heading to turn toward, and the yaw worked out from it. The
/// block lives only for the one call.
typedef struct {
    VECTOR3 targetDelta; // Target point minus the root coordinate's translation
    byte    field_C[4];  // Never accessed; role unproven
    s32     yaw;         // Heading of `targetDelta`, then the signed turn toward it, in 1/4096 turns
} _PlayerActorTurnScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorTurnScratch, 0x14);

/// Scratch-stack block for pushing a player actor out of the bodies it overlaps.
///
/// Each body contact is measured in view space, the space of the bodies'
/// cached transforms and of the contact points: the receiving body's centre,
/// then its separation from the contact point, whose length is compared with
/// the contact's distance to give the overlap. The deepest overlap's direction
/// is kept and turned back into room space, where it moves the actor's root.
/// The block is reserved uninitialized and lives only for the one call.
typedef struct {
    VECTOR position;      // Receiving body's centre in view space; afterwards the root's translation before the push (X and Z only)
    VECTOR separation;    // Body's rotated local offset while its centre is composed, then centre minus contact point, in view space
    VECTOR roomDirection; // `viewDirection` in room space (4096 per unit); scaled by the overlap to move the root
    VECTOR viewDirection; // Unit `separation` of the deepest contact so far (4096 per unit)
} _PlayerActorPushbackScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorPushbackScratch, 0x40);

/// Scratch-stack block for turning a player actor's aim yaw toward a point.
///
/// Holds a temporary coordinate node standing at model part 4, the part the
/// aim yaw rotates, and the target's displacement from it, whose X and Z give
/// the heading to turn toward. The block lives only for the one call.
typedef struct {
    VECTOR3  targetDelta;  // Target point minus the origin node's translation, in the view node's space
    byte     field_C[4];   // Never accessed; role unproven
    SVECTOR  originOffset; // Displacement of the origin node in model part 4's frame; always zero
    GfxCoord originCoord;  // Aim origin: model part 4's world transform re-expressed beneath the view node
} _PlayerActorAimScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorAimScratch, 0x68);

extern u16 D_80113F9C[70];

extern const TaskFuncTable4 D_80097AB0;

static inline void _gpResumeBaseState(Task* arg0);

static void func_8010AAB4(Task* arg0);

static void func_8010AB70(Task* arg0);

static void func_8010AE98(Task* arg0);

static void func_8010AF04(Task* arg0);

static void func_8010AF6C(Task* arg0);

static void func_8010AFC0(Task* arg0);

static void func_8010B010(Task* arg0);

static void func_8010B060(Task* arg0);

static void func_8010B0C8(Task* arg0);

static s32 Gp_TestHpDamage(s32 arg0);

static void func_8010B2D4(Task* arg0, WorldCollisionContact* arg1, s32 arg2);

static void func_8010B348(Task* arg0, WorldCollisionContact* arg1, s32 arg2);

static void _modelObjectInitChildTask(Task* task);

static void _modelObjectUpdateChildTask(Task* task);

static void _modelObjectDeferChildTaskRemoval(Task* task);

static void _modelObjectKillChildTask(Task* task);

s32 func_8010C30C(Task* arg0, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void func_8010C46C(Task* arg0);

s32 func_8010C4F0(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg);

s32 func_8010C648(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg);

s32 func_8010C688(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3);

s32 func_8010C6C8(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim);

s32 func_8010C708(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim);

s32 func_8010C75C(Task* arg0, s32 arg1, GameActorButtonPressHold* arg2, s32 unusedSecondArg);

s32 Gp_MoveActorByKeep(Task* arg0, s32 arg1, GameActorMoveBy* move, s32 unusedSecondArg);

s32 Gp_CopyAllyAnim(Task* arg0, s32 arg1, const AnimationBankCopyRequest* request, s32 unusedSecondArg);

static inline void _gpResumeBaseState(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    func_8010B210(arg0);
    inner->recoveryTicks = 0x12;
    if (inner->state != 0) {
        playerActorEnterAim(arg0, 0xC);
    } else {
        playerActorEnterLocomotion(arg0, 0);
    }
}

u16 D_80113F9C[70] = {
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
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
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
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    1,
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
};

void func_80109BB4(Task* arg0, WorldCollisionContact* arg1)
{
    _PlayerActorPushbackScratch* block;
    GameActor*                   actor;
    GfxCoord*                    coord;
    WorldCollisionContact*       rec;
    WorldCollisionBody*          obj;
    VECTOR*                      separation;
    s32                          i;
    s32                          best;
    s32                          push;
    s32                          id;
    s32                          val;

    rec   = arg1;
    best  = 0;
    i     = 0;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorPushbackScratch);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;

    for (i = 0; i < 0x12; rec++, i++) {
        separation = &block->separation;
        if (rec->flags & 1) {
            switch (rec->key.parts.kind) {
                case 0:
                case 1:
                case 2:
                    break;
                case 3:
                    if ((s8)actor->gridResponse != 0) {
                        break;
                    }
                    id = rec->key.parts.id;
                    if (id < 0x46 && D_80113F9C[id] == 1) {
                        obj = &actor->collisionBodies[(u8)rec->flags >> 4];
                        gte_SetRotMatrix(&obj->coord->workm);
                        gte_ldv0(&obj->pos);
                        gte_rtv0();
                        gte_stlvnl(&block->separation);
                        block->position.vx   = (obj->coord)->workm.t[0] + block->separation.vx;
                        block->position.vy   = (obj->coord)->workm.t[1] + block->separation.vy;
                        block->position.vz   = (obj->coord)->workm.t[2] + block->separation.vz;
                        block->separation.vx = block->position.vx - rec->point.vx;
                        block->separation.vy = block->position.vy - rec->point.vy;
                        block->separation.vz = block->position.vz - rec->point.vz;
                        push                 = rec->distance - SquareRoot0(block->separation.vx * block->separation.vx +
                                                                           block->separation.vy * block->separation.vy +
                                                                           block->separation.vz * block->separation.vz);
                        val                  = push;
                        if (push < 0) {
                            val = 0;
                        }
                        push = val;
                        if (best < push) {
                            best = push;
                            VectorNormal(separation, &block->viewDirection);
                            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm,
                                                   &block->viewDirection, &block->roomDirection);
                        }
                    }
                    break;
                case 4:
                    func_8010B2D4(arg0, rec, (u8)rec->flags >> 4);
                    break;
                case 5:
                    func_8010B348(arg0, rec, (u8)rec->flags >> 4);
                    break;
            }
        }
    }

    if (best > 0) {
        actor->usesPushbackDirection = 1;
        actor->pushbackDirection.vx  = coord->workm.t[0];
        actor->pushbackDirection.vy  = coord->workm.t[1];
        actor->pushbackDirection.vz  = coord->workm.t[2];
        block->position.vx           = coord->coord.t[0];
        block->position.vz           = coord->coord.t[2];
        coord->coord.t[0]           += (best * block->roomDirection.vx) >> 12;
        coord->coord.t[2]           += (best * block->roomDirection.vz) >> 12;
        coord->composeStamp          = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        actor->pushbackDirection.vx = coord->workm.t[0] - actor->pushbackDirection.vx;
        actor->pushbackDirection.vy = coord->workm.t[1] - actor->pushbackDirection.vy;
        actor->pushbackDirection.vz = coord->workm.t[2] - actor->pushbackDirection.vz;
        VectorNormal(&actor->pushbackDirection, &actor->pushbackDirection);
        coord->coord.t[0]   = block->position.vx + ((best * block->roomDirection.vx) >> 14);
        coord->coord.t[2]   = block->position.vz + ((best * block->roomDirection.vz) >> 14);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorPushbackScratch);
}

void func_80109FC4(Task* arg0)
{
    s32        flags;
    GameActor* actor;
    s32        temp;
    s32        mode;

    flags = gPlayerStatus.statusFlags;
    actor = arg0->work;
    if (flags != 0) {
        if (flags & PLAYER_STATUS_DARKNESS) {
            temp                             = (u16)actor->effectTimer.darknessTicks - 1;
            actor->effectTimer.darknessTicks = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_DARKNESS;
            }
        }
        if (flags & PLAYER_STATUS_PARALYSIS) {
            temp                  = (u16)actor->paralysisTicks - 1;
            actor->paralysisTicks = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_PARALYSIS;
            }
        }
        if (flags & PLAYER_STATUS_POISON) {
            temp                     = actor->poisonDamageTicks - 1;
            actor->poisonDamageTicks = temp;
            if ((s8)temp <= 0) {
                Gp_ApplyHpDamage(1);
                mode = (u16)actor->movementMode;
                if (mode == 0) {
                    actor->poisonDamageTicks = 0x78;
                } else if (mode == 3) {
                    actor->poisonDamageTicks = 0x14;
                } else {
                    actor->poisonDamageTicks = 0x3C;
                }
            }
            temp               = (u16)actor->poisonTicks - 1;
            actor->poisonTicks = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_POISON;
            }
        }
        if (flags & PLAYER_STATUS_SILENCE) {
            temp                = (u16)actor->silenceTicks - 1;
            actor->silenceTicks = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_SILENCE;
            }
        }
        if (flags & 0x20) {
            temp                 = (u16)actor->status20Ticks - 1;
            actor->status20Ticks = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x20;
            }
        }
        if (flags & PLAYER_STATUS_CONFUSION) {
            temp                  = (u16)actor->confusionTicks - 1;
            actor->confusionTicks = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_CONFUSION;
            }
        }
        if (flags & PLAYER_STATUS_BERSERKER) {
            if ((u32)((u8)Gp_StateC08.mode - ATTACHMENT_MODE_ARMED) >= 2U) {
                temp                  = (u16)actor->berserkerTicks - 1;
                actor->berserkerTicks = temp;
                if ((s16)temp <= 0) {
                    flags &= ~PLAYER_STATUS_BERSERKER;
                }
            }
        }
        gPlayerStatus.statusFlags = flags;
    }
}

void Gp_TriggerPeState(s32 arg0, s32 arg1)
{
    Task*      work;
    GameActor* inner;
    s32        mask;

    mask = arg1;
    if (arg0 == 0) {
        work = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (arg1 & PLAYER_STATUS_DARKNESS) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_DARKNESS) == 0) {
                gPlayerStatus.statusFlags       |= PLAYER_STATUS_DARKNESS;
                inner->effectTimer.darknessTicks = PLAYER_STATE_STATUS_DURATION_TICKS;
                func_800EC9C8();
                Gp_DetachLinkNode(work);
                Gp_SetState1CPe(1);
            }
        }
        if (mask & PLAYER_STATUS_PARALYSIS) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_PARALYSIS) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_PARALYSIS;
                inner->paralysisTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
                inner->paralysisProgress   = 0;
                func_8010B210(work);
                Gp_SetState1CPe(2);
            }
        }
        if (mask & PLAYER_STATUS_POISON) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_POISON) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_POISON;
                inner->poisonTicks         = PLAYER_STATE_STATUS_DURATION_TICKS;
                inner->poisonDamageTicks   = 0;
                Gp_SetState1CPe(4);
            }
        }
        if (mask & PLAYER_STATUS_SILENCE) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_SILENCE) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_SILENCE;
                inner->silenceTicks        = PLAYER_STATE_STATUS_DURATION_TICKS;
                Gp_SetState1CPe(0x10);
            }
        }
        if (mask & 0x20) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_TIMED_STATUS_20) == 0) {
                gPlayerStatus.statusFlags |= 0x20;
                inner->status20Ticks       = PLAYER_STATE_STATUS_DURATION_TICKS;
                Gp_SetState1CPe(0x20);
            }
        }
        if (mask & PLAYER_STATUS_CONFUSION) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_CONFUSION) == 0) {
                gPlayerStatus.statusFlags     |= PLAYER_STATUS_CONFUSION;
                inner->confusionTicks          = PLAYER_STATE_STATUS_DURATION_TICKS;
                inner->confusionDirectionTicks = (rand() & 0x1F) + 0xA;
                inner->confusionDirections     = 0;
                Gp_SetState1CPe(0x40);
            }
        }
        if (mask & PLAYER_STATUS_BERSERKER) {
            inner = work->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_BERSERKER) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_BERSERKER;
                inner->berserkerTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
                Gp_SetState1CPe(0x80);
                func_800ECA54();
            }
        }
    } else {
        gPlayerStatus.statusFlags &= ~arg1;
    }
}

void func_8010A42C(Task* arg0, s32 arg1)
{
    u8 kind;

    kind = arg1;
    if (kind != 0) {
        switch (kind) {
            case 0:
                break;
            case 1: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_DARKNESS) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags       |= PLAYER_STATUS_DARKNESS;
                inner->effectTimer.darknessTicks = PLAYER_STATE_STATUS_DURATION_TICKS;
                func_800EC9C8();
                Gp_DetachLinkNode(arg0);
                Gp_SetState1CPe(1);
                break;
            }
            case 2: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_PARALYSIS) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags |= PLAYER_STATUS_PARALYSIS;
                inner->paralysisTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
                inner->paralysisProgress   = 0;
                func_8010B210(arg0);
                Gp_SetState1CPe(2);
                break;
            }
            case 3: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_POISON) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags |= PLAYER_STATUS_POISON;
                inner->poisonTicks         = PLAYER_STATE_STATUS_DURATION_TICKS;
                inner->poisonDamageTicks   = 0;
                Gp_SetState1CPe(4);
                break;
            }
            case 4:
                Gp_SetState1CPe(8);
                break;
            case 8: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_SILENCE) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags |= PLAYER_STATUS_SILENCE;
                inner->silenceTicks        = PLAYER_STATE_STATUS_DURATION_TICKS;
                Gp_SetState1CPe(0x10);
                break;
            }
            case 9: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_TIMED_STATUS_20) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags |= 0x20;
                inner->status20Ticks       = PLAYER_STATE_STATUS_DURATION_TICKS;
                Gp_SetState1CPe(0x20);
                break;
            }
            case 10: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_CONFUSION) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags     |= PLAYER_STATUS_CONFUSION;
                inner->confusionTicks          = PLAYER_STATE_STATUS_DURATION_TICKS;
                inner->confusionDirectionTicks = (rand() & 0x1F) + 0xA;
                inner->confusionDirections     = 0;
                Gp_SetState1CPe(0x40);
                break;
            }
            case 11: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_BERSERKER) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags |= PLAYER_STATUS_BERSERKER;
                inner->berserkerTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
                Gp_SetState1CPe(0x80);
                func_800ECA54();
                break;
            }
        }
    }
}

void func_8010A670(Task* arg0)
{
    GameActor*       inner;
    WorldTargetNode* node;
    s32              left;
    s32              right;
    s32              pad;
    s32              bits;
    s32              timer;
    s32              next;
    s32              mode;
    s32              dir;

    inner                          = arg0->work;
    timer                          = inner->confusionDirectionTicks - 1;
    inner->confusionDirectionTicks = timer;
    if ((s8)timer == 0) {
        left                           = 0x8000;
        next                           = (rand() & 0x1F) + 0xA;
        pad                            = inner->padHeld;
        inner->confusionDirectionTicks = next;
        bits                           = pad & 0xF000;
        if (bits == left || bits == (right = 0x2000)) {
            if (!(inner->confusionDirections & 0x5000)) {
                if (rand() & 1) {
                    dir = 0x4000;
                } else {
                    dir = 0x1000;
                }
                inner->confusionDirections = dir;
            }
        } else {
            bits = pad & 0x5000;
            if (bits) {
                if (rand() & 4) {
                    inner->confusionDirections &= 0xAFFF;
                } else if (rand() & 1) {
                    inner->confusionDirections = left;
                } else {
                    inner->confusionDirections = right;
                }
            }
        }
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED) {
            if (inner->targetNode != NULL) {
                if (rand() & 3) {
                    Gp_DetachLinkNode(arg0);
                }
            } else {
                mode = inner->state;
                if (mode == 2 && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) && (rand() & 3)) {
                    node = Gp_FindLockNode(arg0);
                    if (node != NULL) {
                        inner->aimTrackingState = mode;
                        playerActorSetLockTarget(arg0, node);
                    }
                }
            }
        }
    }
    if (gGameSession->padHeld & 0xF000) {
        inner->padHeld |= inner->confusionDirections;
    }
}

s32 Gp_ApplyHpDamage(s16 arg0)
{
    s16           amount;
    s32           ret;
    PlayerStatus* p;
    Task*         slot;
    GfxCoord*     coords;

    amount = arg0;
    ret    = 0;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_HOLY_WATER) != 0) {
        amount = arg0 - (arg0 >> 2);
    }
    if (equipmentHasEffect(EQUIPMENT_EFFECT_MP_GENERATION) != 0) {
        gPlayerStatus.mp += amount / 5;
        if (gPlayerStatus.mpMax < gPlayerStatus.mp) {
            gPlayerStatus.mp = gPlayerStatus.mpMax;
        }
    }
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_IMPACT) != 0) {
        p = &gPlayerStatus;
        if (p->hp >= 5 && amount >= p->hp) {
            slot   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            coords = slot->extra.tmd->coords;
            p->hp  = 1;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, coords + 1, 5, 0);
            return 0;
        }
    }
    gPlayerStatus.hp -= amount;
    if (gPlayerStatus.hp > 0) {
        return ret;
    }
    if (gGameSession->eventState != 0) {
        gPlayerStatus.hp = 1;
    } else {
        ret = 1;
        Display_AcquireRef();
    }
    return ret;
}

void func_8010A9D0(Task* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->work;
    func_8010AAB4(arg0);
    if ((u16)inner->hitRegion == 1) {
        mode = 0x10;
    } else {
        mode = 0x11;
    }
    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 3);
}

void Gp_StopPlayerAnim(Task* arg0, s32 arg1)
{
    GameActor* inner;

    inner                 = arg0->work;
    inner->mode           = GAME_ACTOR_MODE_DAMAGE;
    inner->movementMode   = 0;
    inner->turnRateIndex  = 0;
    inner->animationState = 0;
    inner->statePhase     = 0;
    inner->hitRegion      = 3;
    if (arg1 == 0) {
        playerActorResetChildSlots(arg0, 0x12);
    } else {
        playerActorPlayChildSlotsWithBlend(arg0, 0x12, 0, arg1);
    }
    Gp_DetachLinkNode(arg0);
    inner->pendingCollisionUpdates |= (GAME_ACTOR_COLLISION_FIRST_TWO_REQUESTS << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT);
}

static void func_8010AAB4(Task* arg0)
{
    GameActor*    inner;
    PlayerStatus* p;

    p                  = &gPlayerStatus;
    inner              = arg0->work;
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    func_80106350(arg0, p->weapon, 0);
    if (p->hp > 0) {
        inner->mode           = GAME_ACTOR_MODE_DAMAGE;
        inner->movementMode   = 0;
        inner->turnRateIndex  = 0;
        inner->animationState = 7;
        inner->statePhase     = 0;
        inner->movementSign   = 0;
        Gp_ApplyHpDamage(inner->pendingDamage);
        inner->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        if ((s8)inner->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
            inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        }
        func_8010A42C(arg0, inner->damageReaction);
    }
}

static void func_8010AB70(Task* arg0)
{
    _gpResumeBaseState(arg0);
}

void func_8010ABD4(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (inner->statePhase != 0) {
        if (inner->statePhase == 1) {
            func_8010B210(arg0);
            inner->recoveryTicks = 0x12;
            if (inner->state != 0) {
                playerActorEnterAim(arg0, 0xC);
            } else {
                playerActorEnterLocomotion(arg0, 0);
            }
        }
    }
}

void func_8010AC54(Task* arg0)
{
    GameActor* inner;
    GameActor* inner2;

    inner = arg0->work;
    if (inner->statePhase == 0) {
        inner->statePhase  = 1;
        inner->stateTimer  = 0;
        inner->actionValue = 0;
    }
    if (inner->stateTimer == 0) {
        inner->actionValue++;
        if (inner->actionValue == 3) {
            inner2 = arg0->work;
            func_8010B210(arg0);
            inner2->recoveryTicks = 0x12;
            if (inner2->state != 0) {
                playerActorEnterAim(arg0, 0xC);
            } else {
                playerActorEnterLocomotion(arg0, 0);
            }
        } else {
            inner->stateTimer = 5;
        }
        Gp_SpawnEff(
            EFFECT_FLASH_BURST, &arg0->extra.tmd->coords[4 - inner->actionValue], 0x320, 0);
    } else {
        inner->stateTimer--;
    }
}

void func_8010AD64(Task* arg0)
{
    void**          scratch;
    u8*             head;
    SVECTOR*        vec;
    GameActor*      inner;
    EffectSpawnArg* params;
    GfxCoord*       coord;
    s32             val;
    s32             idx;

    inner                             = arg0->work;
    scratch                           = SCRATCH_HEAD_ADDR;
    head                              = SCRATCH_HEAD_AT(scratch, u8);
    params                            = &D_80113358;
    vec                               = (SVECTOR*)(head - 8);
    SCRATCH_HEAD_AT(scratch, SVECTOR) = vec;
    switch (inner->statePhase) {
        case 0:
            idx                     = (s8)inner->hitBodyIndex;
            inner->statePhase       = 1;
            coord                   = (idx + inner->collisionBodies)->coord;
            params->spawnArgLo      = 0xC0;
            params->spawnArgHi      = 2;
            D_80113358.coord        = coord;
            ((SVECTOR*)head)[-1].vx = 0;
            val                     = 0;
            if ((s8)inner->hitBodyIndex == 0) {
                val = -0x190;
            }
            vec->vy = val;
            vec->vz = 0;
            func_800FDB18(2, D_80113358.coord, vec, params);
            break;
        case 1:
            break;
        case 2:
            _gpResumeBaseState(arg0);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_8010AE98(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_DARKNESS) != 0) {
        return;
    }
    gPlayerStatus.statusFlags       |= PLAYER_STATUS_DARKNESS;
    inner->effectTimer.darknessTicks = PLAYER_STATE_STATUS_DURATION_TICKS;
    func_800EC9C8();
    Gp_DetachLinkNode(arg0);
    Gp_SetState1CPe(1);
}

static void func_8010AF04(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_PARALYSIS) != 0) {
        return;
    }
    gPlayerStatus.statusFlags |= PLAYER_STATUS_PARALYSIS;
    inner->paralysisTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
    inner->paralysisProgress   = 0;
    func_8010B210(arg0);
    Gp_SetState1CPe(2);
}

static void func_8010AF6C(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_POISON) != 0) {
        return;
    }
    gPlayerStatus.statusFlags |= PLAYER_STATUS_POISON;
    inner->poisonTicks         = PLAYER_STATE_STATUS_DURATION_TICKS;
    inner->poisonDamageTicks   = 0;
    Gp_SetState1CPe(4);
}

static void func_8010AFC0(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_SILENCE) != 0) {
        return;
    }
    gPlayerStatus.statusFlags |= PLAYER_STATUS_SILENCE;
    inner->silenceTicks        = PLAYER_STATE_STATUS_DURATION_TICKS;
    Gp_SetState1CPe(0x10);
}

static void func_8010B010(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_TIMED_STATUS_20) != 0) {
        return;
    }
    gPlayerStatus.statusFlags |= 0x20;
    inner->status20Ticks       = PLAYER_STATE_STATUS_DURATION_TICKS;
    Gp_SetState1CPe(0x20);
}

static void func_8010B060(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_CONFUSION) != 0) {
        return;
    }
    gPlayerStatus.statusFlags     |= PLAYER_STATUS_CONFUSION;
    inner->confusionTicks          = PLAYER_STATE_STATUS_DURATION_TICKS;
    inner->confusionDirectionTicks = (rand() & 0x1F) + 0xA;
    inner->confusionDirections     = 0;
    Gp_SetState1CPe(0x40);
}

static void func_8010B0C8(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_BERSERKER) != 0) {
        return;
    }
    gPlayerStatus.statusFlags |= PLAYER_STATUS_BERSERKER;
    inner->berserkerTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
    Gp_SetState1CPe(0x80);
    func_800ECA54();
}

void Gp_PlayerStepSfx(Task* arg0)
{
    GameActor* inner;
    GameActor* inner2;
    GfxCoord*  obj;
    s32        mode;
    s32        snd;
    s32        temp;
    s32        temp2;

    inner = arg0->work;
    obj   = arg0->extra.tmd->coords;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode != 0) {
        return;
    }
    if ((s8)inner->recoveryTicks != 0) {
        return;
    }
    func_80109BB4(arg0, inner->collisionContacts);
    if ((u16)inner->hitRegion == 0) {
        return;
    }
    inner2 = arg0->work;
    func_8010AAB4(arg0);
    mode = 0x11;
    if ((u16)inner2->hitRegion == 1) {
        mode = 0x10;
    }
    playerActorPlayChildSlotsWithBlend(arg0, mode, 0, 3);
    temp  = (s8)worldCoordGetOriginAudioPan(obj);
    temp2 = (s8)worldCoordGetOriginAudioDepth(obj);
    snd   = 7;
    if ((u16)inner->hitRegion == 1) {
        snd = 6;
    }
    sndEvtRequestScriptStart(snd, temp, temp2);
}

void func_8010B210(Task* arg0)
{
    GameActor* inner;

    inner                 = arg0->work;
    inner->hitRegion      = 0;
    inner->damageReaction = GAME_ACTOR_REACTION_ORDINARY;
    inner->pendingDamage  = 0;
}

static s32 Gp_TestHpDamage(s32 arg0)
{
    PlayerStatus* p;
    u16           saved18;
    u16           saved1c;
    s32           out;
    s32           ret;

    p       = &gPlayerStatus;
    saved18 = p->hp;
    saved1c = p->mp;
    ret     = Gp_ApplyHpDamage(damageComputeReceived(arg0, 0, &out, 0));
    p->hp   = saved18;
    p->mp   = saved1c;
    if (ret != 0) {
        displayReleaseMenuHold();
    }
    return ret;
}

void func_8010B2A0(s32 arg0, s32 arg1)
{
    taskSpawnFromTable(D_80113340, arg0, arg1, 0);
}

static void func_8010B2D4(Task* arg0, WorldCollisionContact* arg1, s32 arg2)
{
    GameActor* inner;
    s32        out;
    s32        flag;

    inner = arg0->work;
    flag  = inner->companionWork != 0;
    if ((u16)inner->hitRegion == 0) {
        inner->hitBodyIndex = arg2;
        arg2                = (u16)arg2;
        if (arg2 == 1) {
            inner->hitRegion = arg2;
        } else {
            inner->hitRegion = 2;
        }
        inner->pendingDamage  = damageComputeReceived(arg1->key.value, 0, &out, flag);
        inner->damageReaction = out;
    }
}

static void func_8010B348(Task* arg0, WorldCollisionContact* arg1, s32 arg2)
{
    GameActor* inner;
    u32        kind;

    inner = arg0->work;
    kind  = (u16)arg1->key.value;
    if ((u16)inner->hitRegion == 0) {
        inner->hitBodyIndex = arg2;
        switch (kind) {
            case 1: // placeholder: the tree pivots on 2, so one value below it was listed
                break;
            case 2:
            case 4:
                arg2 = (u16)arg2;
                if (arg2 == 1) {
                    inner->hitRegion = arg2;
                } else {
                    inner->hitRegion = 2;
                }
                inner->damageReaction = 5;
                break;
            case 3:
                arg2 = (u16)arg2;
                if (arg2 == 1) {
                    inner->hitRegion = arg2;
                } else {
                    inner->hitRegion = 2;
                }
                inner->damageReaction = GAME_ACTOR_REACTION_ORDINARY;
                break;
        }
        inner->pendingDamage = damageGetHazardDamage(arg1->key.value, DAMAGE_HAZARD_VICTIM_PLAYER);
    }
}

void func_8010B3F8(Task* arg0)
{
    Task*           slot;
    EffectSpawnArg* params;
    GfxCoord*       coords;
    s32             argLo;
    s32             idx;
    u16             count;
    s16             next;

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0;
            /* fallthrough */
        case 1:
            count = arg0->killCountdown;
            if ((count & 0xF) == 0) {
                next                = count + 0x100;
                params              = &D_80113358;
                arg0->killCountdown = next;
                idx                 = arg0->spawnArg1.value & 3;
                if (next >= 0x300) {
                    taskKill(arg0);
                } else {
                    arg0->killCountdown = next | 6;
                }
                argLo              = (idx * 0x60) + 0xC0;
                coords             = &slot->extra.tmd->coords[((arg0->killCountdown & 0xF00) >> 8) + 1];
                params->spawnArgLo = argLo;
                params->spawnArgHi = idx + 1;
                params->coord      = coords;
                func_800FDB18(3, coords, 0, params);
            } else {
                arg0->killCountdown = count - 1;
            }
            break;
    }
}

void func_8010B520(Task* arg0)
{
    GfxCoord*       raw;
    Task*           slot;
    TmdObject*      extra;
    EffectSpawnArg* params;
    GfxCoord*       coords;

    params             = &D_80113358;
    slot               = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    extra              = slot->extra.tmd;
    raw                = extra->coords;
    params->spawnArgLo = 0xC0;
    coords             = &raw[3];
    params->coord      = coords;
    params->spawnArgHi = (u16)arg0->spawnArg1.value + 1;
    func_800FDB18(2, coords, 0, params);
    taskKill(arg0);
}

/// Initializes an attached child model and advances its task from state 0 to state 1.
///
/// The model's root coordinate is already parented by the spawner. A nonzero
/// `param.clearFlags` clears all model flags on this first update; zero keeps
/// the spawn-time flags until the state-1 update copies the parent's flags.
static void _modelObjectInitChildTask(Task* task)
{
    TmdObject* model;
    GfxCoord*  rootCoord;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    task->state++;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (rootCoord->param.clearFlags != 0) {
        model->flags = 0;
    }
}

/// Copies the live parent's model flags and invalidates the child's root transform.
///
/// State 1 requires a non-NULL parent whose TMD body and coordinate ancestry
/// remain live. Both task bodies must be TMD models.
static void _modelObjectUpdateChildTask(Task* task)
{
    Task*      parent;
    TmdObject* model;

    parent                      = task->parent;
    model                       = task->extra.tmd;
    model->flags                = parent->extra.tmd->flags;
    model->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Advances state 2 to teardown on the child-model task's next dispatch.
static void _modelObjectDeferChildTaskRemoval(Task* task)
{
    enum { MODEL_OBJECT_CHILD_TASK_STATE_KILL = 3 };

    task->state = MODEL_OBJECT_CHILD_TASK_STATE_KILL;
}

/// Hands the state-3 child model and its task to `taskKill` for teardown.
static void _modelObjectKillChildTask(Task* task)
{
    taskKill(task);
}

void modelObjectChildTask(Task* task)
{
    TaskFuncTable4 handlers;

    handlers = D_80097AB0;
    handlers.funcs[task->state](task);
}

void Gp_EndPlayerActorTask(Task* arg0)
{
    GameActor* actor;
    GameActor* inner;
    GameActor* next;
    TmdObject* extra;
    Task*      task;

    actor = arg0->work;
    task  = actor->equipmentTasks[1];
    if (task != NULL) {
        taskKill(task);
        actor->equipmentTasks[1]  = NULL;
        extra                     = arg0->extra.tmd;
        inner                     = arg0->work;
        inner->animationBankIndex = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
        inner->animationSets      = Gp_AnimBlkTbl[inner->animationBankIndex]->table.sets;
        animationInitContext(&inner->animationContext, inner->animationSets, extra, inner->poseBuffer,
                             inner->animationSlots);
        playerActorResetChildSlots(arg0, 1);
        next                 = arg0->work;
        next->mode           = GAME_ACTOR_MODE_NORMAL;
        next->state          = 0;
        next->movementMode   = 0;
        next->turnRateIndex  = 0;
        next->animationState = 0;
        next->statePhase     = 0;
        next->idleTicks      = 0;
        next->actionValue    = 0;
        next->movementSign   = 0;
        next->turnSign       = 0;
        playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 4);
    }
    task = actor->weaponEffectTask;
    if (task != NULL) {
        taskKill(task);
        actor->weaponEffectTask = NULL;
    }
}

Task* Gp_SetupAllyWeapon(void)
{
    Task*          work;
    GameActor*     actor;
    GameActor*     inner;
    GameActor*     next;
    Task*          task;
    McSaveData*    save;
    CompanionWork* companion;
    s16            val1;
    s16            val2;
    EffectWork*    eff;
    TmdObject*     extra;
    Task*          ret;

    work  = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    actor = work->work;
    if (!work | !actor) {
        return 0;
    }

    if (actor->attachmentTasks[1] != NULL) {
        save                     = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        task                     = func_80104364(actor->attachmentTasks[1], save->state.companionType + 1, save->state.companionVariant, 0);
        actor->equipmentTasks[1] = task;
        if (task != NULL) {
            companion = actor->companionWork;
            val1      = D_actor_800100_80167218[save->state.companionVariant];
            val2      = D_actor_800100_80167224[save->state.companionVariant];
            Gp_AttachActorObj(work, val1, val2);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key |= 0x80;
            companion->activity.combat.attacksRemaining         = D_actor_800100_80167230[save->state.companionVariant];
            if ((u8)save->state.companionVariant == 4 && actor->weaponEffectTask == NULL) {
                eff = Gp_SpawnEff(
                    (EFFECT_COMPANION_WEAPON_FLARE | EFFECT_SPAWN_UNLIMITED), actor->equipmentTasks[1]->extra.tmd->coords, (s32)(val1), 0);
                if (eff != NULL) {
                    actor->weaponEffectTask = eff->task;
                    func_80106350(work, val1, 0);
                }
            }
        }
    }

    inner                     = work->work;
    extra                     = work->extra.tmd;
    inner->animationBankIndex = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
    inner->animationSets      = Gp_AnimBlkTbl[inner->animationBankIndex]->table.sets;
    animationInitContext(&inner->animationContext, inner->animationSets, extra, inner->poseBuffer,
                         inner->animationSlots);
    next                 = work->work;
    next->mode           = GAME_ACTOR_MODE_NORMAL;
    next->state          = 0;
    next->movementMode   = 0;
    next->turnRateIndex  = 0;
    next->animationState = 0;
    next->statePhase     = 0;
    next->idleTicks      = 0;
    next->actionValue    = 0;
    next->movementSign   = 0;
    next->turnSign       = 0;
    playerActorResetChildSlots(work, 1);
    ret                            = actor->equipmentTasks[1];
    actor->pendingCollisionUpdates = GAME_ACTOR_COLLISION_REQUEST_MASK;
    return ret;
}

void func_8010B9A4(Task* arg0)
{
    GameActor*  actor;
    McSaveData* save;
    s32         field13;
    u16         temp;
    u16         anim;

    actor                 = arg0->work;
    actor->mode           = GAME_ACTOR_MODE_DAMAGE;
    actor->animationState = 7;
    save                  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->statePhase     = 0;
    actor->stateAux       = 0;
    actor->movementSign   = 0;
    actor->turnSign       = 0;
    if (save->state.cheatMode == 0 && (field13 = save->state.companionType) == 1) {
        temp                    = save->state.companionHp - actor->pendingDamage;
        save->state.companionHp = temp;
        if ((s16)temp <= 0 && gGameSession->eventState != 0) {
            save->state.companionHp = field13;
        }
    }
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    }
    func_80106350(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
    anim = 0x11;
    if ((u16)actor->hitRegion == 1) {
        anim = 0x10;
    }
    playerActorPlayChildSlotsWithBlend(arg0, anim, 0, 3);
}

Task* Gp_SpawnAlly(const ActorSpawnTransform* spawnTransform, u16 arg1, s32 arg2, ActorSpawnOptions* options)
{
    Task*          task;
    GameActor*     actor;
    CompanionWork* companion;
    GfxCoord*      coord;
    s32            type;

    if (arg1 == 1) {
        type = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant + 0x7F;
    } else {
        type = arg1 + 0x82;
    }
    task = taskSpawn(7, type, arg2, options);
    if (task == NULL) {
        return NULL;
    }
    actor = memCalloc(sizeof(*actor), 0);
    if (actor == NULL) {
    fail:
        taskKill(task);
        return NULL;
    }
    companion = memCalloc(sizeof(*companion), 0);
    if (companion == NULL) {
        goto fail;
    }
    gameSetTaskSlot(task, GAME_TASK_SLOT_COMPANION);
    memFillBytes(actor, 0, sizeof(*actor));
    memFillBytes(companion, 0, sizeof(*companion));
    task->work           = actor;
    actor->companionWork = companion;
    companionRelocateModelTextures(task);
    actor->actionArgument = options->initialAnimationId;
    actor->rotation.vy    = spawnTransform->yaw.angle;
    coord                 = task->extra.tmd->coords;
    coord->coord.t[0]     = spawnTransform->x;
    coord->coord.t[1]     = spawnTransform->y;
    coord->coord.t[2]     = spawnTransform->z;
    return task;
}

/// Resets the companion's normal-mode idle state, motion selectors and counters.
static inline void _companionResetIdleState(GameActor* actor)
{
    enum {
        COMPANION_IDLE_STATE                     = 0,
        COMPANION_IDLE_MOVEMENT_STOPPED          = 0,
        COMPANION_IDLE_TURN_DISABLED             = 0,
        COMPANION_IDLE_ANIMATION_CONTROLLER_NONE = 0
    };

    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = COMPANION_IDLE_STATE;
    actor->movementMode   = COMPANION_IDLE_MOVEMENT_STOPPED;
    actor->turnRateIndex  = COMPANION_IDLE_TURN_DISABLED;
    actor->animationState = COMPANION_IDLE_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
    actor->actionValue    = 0;
    actor->movementSign   = 0;
    actor->turnSign       = 0;
}

void companionEnterIdle(Task* task, s16 resetAnimation)
{
    enum {
        COMPANION_IDLE_ANIMATION_SET = 1,
        COMPANION_IDLE_BLEND_FRAMES  = 4
    };
    GameActor* actor;

    actor = task->work;
    _companionResetIdleState(actor);
    if (resetAnimation != 0) {
        playerActorResetChildSlots(task, COMPANION_IDLE_ANIMATION_SET);
    } else {
        playerActorPlayChildSlotsWithBlend(task, COMPANION_IDLE_ANIMATION_SET, 0, COMPANION_IDLE_BLEND_FRAMES);
    }
}

s32 companionGetPlayerPlanarDistance(const GfxCoord* coord)
{
    PlayerActorPlanarDistanceScratch* block;
    TmdObject*                        playerModel;
    s32                               distance;

    playerModel = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd;
    block       = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorPlanarDistanceScratch);
    playerActorGetPointDelta(coord, MATRIX_TRANS(&playerModel->coords->coord), &block->delta);
    distance = playerActorPlanarLength(block->delta.vx, block->delta.vz);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorPlanarDistanceScratch);
    return distance;
}

s32 func_8010BCF4(Task* arg0, VECTOR3* arg1)
{
    GfxCoord*  coords;
    VECTOR3*   vec;
    GameActor* actor;
    s16        ret;

    coords = arg0->extra.tmd->coords;
    vec    = SCRATCH_STACK_RESERVE_BYTES(0x10);
    actor  = arg0->work;
    playerActorGetPointDelta(coords, arg1, vec);
    ret = func_80103E7C(actor->rotation.vy, ratan2(vec->vx, vec->vz));
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    return ret;
}

void func_8010BD88(Task* arg0, VECTOR3* arg1)
{
    _PlayerActorTurnScratch* block;
    TmdObject*               extra;
    GameActor*               actor;
    s32                      val;

    extra = arg0->extra.tmd;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorTurnScratch);
    actor = arg0->work;
    playerActorGetPointDelta(extra->coords, arg1, &block->targetDelta);
    block->yaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
    val        = func_80103E7C(actor->rotation.vy, block->yaw);
    block->yaw = val;
    if (val > 0x40) {
        block->yaw = 0x40;
    } else if (val < -0x40) {
        block->yaw = -0x40;
    }
    actor->rotation.vy = (actor->rotation.vy + block->yaw) & 0xFFF;
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorTurnScratch);
}

void func_8010BE5C(Task* task, VECTOR3* targetPoint)
{
    enum {
        PLAYER_ACTOR_AIM_YAW_STEP  = 0x20,
        PLAYER_ACTOR_AIM_YAW_LIMIT = 0x1A0
    };
    _PlayerActorAimScratch* head;
    _PlayerActorAimScratch* block;
    GfxCoord*               coord;
    SVECTOR*                offset;
    TmdObject*              extra;
    GfxCoord*               parts;
    GameActor*              actor;
    s32                     yawStep;

    head   = SCRATCH_STACK_CURSOR(_PlayerActorAimScratch);
    extra  = task->extra.tmd;
    coord  = &head[-1].originCoord;
    offset = &head[-1].originOffset;
    parts  = extra->coords;
    actor  = task->work;
    block = SCRATCH_STACK_CURSOR(_PlayerActorAimScratch) = head - 1;
    block->originOffset.vx                               = 0;
    block->originOffset.vy                               = 0;
    block->originOffset.vz                               = 0;
    actorRenderPlaceCoordOffset(parts + 4, coord, offset);
    playerActorGetPointDelta(coord, targetPoint, &block->targetDelta);
    // Turn toward the target relative to body facing, preserving the strict aim limit.
    yawStep = ratan2(head[-1].targetDelta.vx, block->targetDelta.vz) - actor->rotation.vy;
    yawStep = func_80103E7C(actor->aimYaw, yawStep);
    if (yawStep > PLAYER_ACTOR_AIM_YAW_STEP) {
        yawStep = PLAYER_ACTOR_AIM_YAW_STEP;
    } else if (yawStep < -PLAYER_ACTOR_AIM_YAW_STEP) {
        yawStep = -PLAYER_ACTOR_AIM_YAW_STEP;
    }
    if (ABS(actor->aimYaw + yawStep) < PLAYER_ACTOR_AIM_YAW_LIMIT) {
        actor->aimYaw += yawStep;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimScratch);
}

void companionSetDecisionDelay(Task* task, s32 baseTicks, s32 randomMask)
{
    s32        randomTicks;
    GameActor* actor;

    randomTicks                         = randomMask & rand();
    actor                               = task->work;
    actor->companionWork->decisionTimer = baseTicks + randomTicks;
}

void func_8010BFCC(Task* arg0)
{
    GameActor* actor;
    TmdObject* extra;

    actor                     = arg0->work;
    extra                     = arg0->extra.tmd;
    actor->animationBankIndex = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
    actor->animationSets      = Gp_AnimBlkTbl[actor->animationBankIndex]->table.sets;
    animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                         actor->animationSlots);
}

s32 companionGetHealthBand(void)
{
    s32 healthBand;

    if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax >> 1) < gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp) {
        healthBand = COMPANION_HEALTH_BAND_ABOVE_HALF;
    } else if ((gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax >> 2) >= gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp) {
        healthBand = COMPANION_HEALTH_BAND_AT_MOST_QUARTER;
    } else {
        healthBand = COMPANION_HEALTH_BAND_ABOVE_QUARTER;
    }
    return healthBand;
}

void Gp_TrackAllyLockTarget(Task* arg0, s32 arg1)
{
    GameActor*       actor;
    WorldTargetNode* node;
    s32              val;

    actor = arg0->work;
    node  = actor->targetNode;
    if (node == NULL || (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
        actor->targetNode       = NULL;
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    } else if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
        if (arg1 & 1) {
            val = 0;
            if (arg1 != 1) {
                val = 0x380;
            }
            Gp_AimYawToLock(arg0, val);
        }
        if (arg1 & 2) {
            if (D_80113388[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] != 0) {
                Gp_AimPitchToLock(arg0);
            } else {
                Gp_AimPitchRec(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0x380);
            }
        }
    }
}

void func_8010C180(Task* arg0)
{
    GameActor* inner;
    GameActor* actor;

    inner = arg0->work;
    func_8010B210(arg0);
    inner->recoveryTicks  = 0x12;
    actor                 = arg0->work;
    actor->mode           = GAME_ACTOR_MODE_NORMAL;
    actor->state          = 0;
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->animationState = 0;
    actor->statePhase     = 0;
    actor->idleTicks      = 0;
    actor->actionValue    = 0;
    actor->movementSign   = 0;
    actor->turnSign       = 0;
    playerActorPlayChildSlotsWithBlend(arg0, 1, 0, 4);
}

void Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2)
{
    GfxCoord*              src;
    CompanionWork*         companion;
    WorldCollisionBody*    obj;
    WorldCollisionCapsule* rec;
    s16                    vz;

    companion              = ((GameActor*)arg0->work)->companionWork;
    src                    = arg0->extra.tmd->coords;
    obj                    = &companion->probe.body;
    rec                    = &companion->probe.shape;
    companion->probe.coord = *src;
    obj->coord             = &companion->probe.coord;
    obj->pos.vz            = -0xA0;
    obj->key               = 0x60000;
    obj->context.capsule   = rec;
    obj->pos.vx            = 0;
    obj->pos.vy            = 0;
    obj->flags             = WORLD_COLLISION_BODY_CAPSULE;
    rec->ends[1].vx        = arg1->vx;
    rec->ends[1].vy        = arg1->vy;
    vz                     = arg1->vz;
    rec->ends[0].vz        = arg2;
    rec->ends[0].vx        = rec->ends[1].vx;
    rec->end1Radius        = 0x80;
    rec->end0Radius        = 0x80;
    rec->contacts          = companion->probe.contacts;
    rec->ends[1].vz        = vz;
    rec->ends[0].vy        = rec->ends[1].vy;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, obj);
    worldCollisionInitContacts(rec->contacts, ARRAY_SIZE(companion->probe.contacts), 0);
    obj->flags |= (WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
}

s32 func_8010C30C(Task* arg0, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    TmdObject*     extra;
    GfxCoord*      coord;
    GfxCoord*      next;
    GameActor*     actor;
    VECTOR         vec;
    AnimationSet** prev;
    AnimationSet** anim;
    s32            changed;

    extra  = arg0->extra.tmd;
    coord  = extra->coords;
    actor  = arg0->work;
    next   = coord + 1;
    vec.vx = next->coord.t[0];
    vec.vy = next->coord.t[1];
    vec.vz = next->coord.t[2];
    ApplyMatrixLV(&coord->coord, &vec, &vec);
    coord->coord.t[0]         += vec.vx;
    coord->coord.t[2]         += vec.vz;
    next->coord.t[0]           = 0;
    next->coord.t[2]           = 0;
    actor->previousPosition.vx = coord->coord.t[0];
    actor->previousPosition.vy = coord->coord.t[1];
    actor->previousPosition.vz = coord->coord.t[2];
    prev                       = actor->animationSets;
    actor->animationBankIndex  = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
    anim                       = Gp_AnimBlkTbl[actor->animationBankIndex]->table.sets;
    changed                    = prev != anim;
    actor->animationSets       = anim;
    animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                         actor->animationSlots);
    actor->animationRate           = ANIMATION_RATE_ONE;
    actor->pendingCollisionUpdates = GAME_ACTOR_COLLISION_REQUEST_MASK;
    actor->statePhase              = 0;
    actor->stateAux                = 0;
    companionEnterIdle(arg0, changed);
    return 0;
}

static void func_8010C46C(Task* arg0)
{
    GameActor* actor;

    actor                                                 = arg0->work;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
}

s32 func_8010C4F0(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg)
{
    GameActor* actor;
    TmdObject* extra;

    actor                                                 = task->work;
    extra                                                 = task->extra.tmd;
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(task, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
    // Select the animation table before resetting or blending its slots.
    actor->state = 1;
    if (actor->animationSets != Gp_AnimBlkTbl[request->source.index]->table.sets) {
        actor->animationSets = Gp_AnimBlkTbl[request->source.index]->table.sets;
        animationInitContext(&actor->animationContext, actor->animationSets, extra, actor->poseBuffer,
                             actor->animationSlots);
        actor->animationBankIndex = (u16)request->source.index;
    }
    actor->animationRate = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        playerActorResetChildSlots(task, request->animationId);
    } else {
        playerActorPlayChildSlotsWithBlend(task, request->animationId, 1, request->blendFrames);
    }
    if (request->enableWorldCollision == ANIMATION_WORLD_COLLISION_DISABLE) {
        actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    } else {
        actor->pendingCollisionUpdates = PLAYER_ACTOR_WORLD_COLLISION_ENABLE;
    }
    return 0;
}

s32 func_8010C648(Task* task, s32 msgId, AnimationPlayRequest* request, s32 unusedSecondArg)
{
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;

    playerStatus            = &gPlayerStatus;
    savedInteractionPressed = playerStatus->interactionPressed;
    func_80104B54(task, msgId, request, unusedSecondArg);
    playerStatus->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 func_8010C688(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;

    p                       = &gPlayerStatus;
    savedInteractionPressed = p->interactionPressed;
    func_80104E00(arg0, arg1, transform, arg3);
    p->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 func_8010C6C8(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;

    p                       = &gPlayerStatus;
    savedInteractionPressed = p->interactionPressed;
    Gp_SetActorDest(arg0, arg1, transform, moveAnim);
    p->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 func_8010C708(Task* arg0, s32 arg1, ActorTransform* transform, GameActorMoveAnim* moveAnim)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;
    GameActor*    actor;

    p                       = &gPlayerStatus;
    actor                   = arg0->work;
    savedInteractionPressed = p->interactionPressed;
    Gp_SetActorDest(arg0, arg1, transform, moveAnim);
    p->interactionPressed = savedInteractionPressed;
    actor->state          = 8;
    return 0;
}

s32 func_8010C75C(Task* arg0, s32 arg1, GameActorButtonPressHold* arg2, s32 unusedSecondArg)
{
    GameActor* actor;

    actor = arg0->work;
    if ((s8)actor->recoveryTicks != 0) {
        return 1;
    }
    actor->mode                                           = GAME_ACTOR_MODE_SCRIPTED;
    actor->statePhase                                     = 0;
    actor->movementSign                                   = 0;
    actor->turnSign                                       = 0;
    actor->part3Pitch                                     = 0;
    actor->part2Pitch                                     = 0;
    actor->part3Roll                                      = 0;
    actor->part2Roll                                      = 0;
    actor->aimYaw                                         = 0;
    actor->field_68                                       = 0;
    actor->part6Pitch                                     = 0;
    actor->hitRegion                                      = 0;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    func_80106350(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
    actor->state       = 6;
    actor->stateTimer  = arg2->pressCount;
    actor->actionValue = 0;
    return 0;
}

s32 Gp_MoveActorByKeep(Task* arg0, s32 arg1, GameActorMoveBy* move, s32 unusedSecondArg)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;
    s32           result;

    p                       = &gPlayerStatus;
    savedInteractionPressed = p->interactionPressed;
    result                  = Gp_MoveActorBy(arg0, arg1, move, unusedSecondArg);
    p->interactionPressed   = savedInteractionPressed;
    return result;
}

s32 Gp_CopyAllyAnim(Task* arg0, s32 arg1, const AnimationBankCopyRequest* request, s32 unusedSecondArg)
{
    union {
        AnimationBank* block;
        s32*           words;
    } dest;
    const s32* src;
    s32        i;
    s32        count;

    dest.block = Gp_AnimBlkTbl[Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant];
    src        = request->source.words;
    count      = request->wordCount;
    if (count >= ANIMATION_BANK_EXTENSION_CAPACITY + 1) {
        return 1;
    }
    // Transfer raw words: the span can include records after the clip pointers.
    dest.words = &dest.block->table.words[ANIMATION_BANK_BASE_SET_COUNT];
    for (i = 0; i < request->wordCount; i++) {
        dest.words[i] = src[i];
    }
    return 0;
}

s32 Gp_HurtAlly(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 ret;

    ret = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp -= damageComputeReceived(arg2, 0, 0, 1);
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7DA, 0, 0x7DE);
            ret = 1;
        }
    }
    return ret;
}

void func_8010C980(void* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3, s32 arg4, s32 arg5)
{
    arg1->coord            = arg0;
    arg1->context.contacts = arg2;
    arg1->pos.vx           = 0;
    arg1->pos.vy           = 0;
    arg1->pos.vz           = 0;
    arg1->flags            = WORLD_COLLISION_BODY_SPHERE;
    arg1->key              = arg4 | 0x30000;
    arg1->radius           = arg5;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, arg1);
    arg1->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(arg1->context.contacts, (s16)arg3, 0);
}

const TaskFuncTable4 D_80097AB0 = { {
    _modelObjectInitChildTask,
    _modelObjectUpdateChildTask,
    _modelObjectDeferChildTaskRemoval,
    _modelObjectKillChildTask,
} };
