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

/// Recovery delay shared by the player and companion hit-completion paths.
enum { PLAYER_ACTOR_HIT_RECOVERY_TICKS = 18 };

/// Damage-dispatch selectors shared by contact hits and the stopped pose.
enum {
    PLAYER_ACTOR_HIT_REGION_NONE       = 0,
    PLAYER_ACTOR_HIT_REGION_PART4      = 1,
    PLAYER_ACTOR_HIT_REGION_OTHER_BODY = 2,
    PLAYER_ACTOR_HIT_REGION_STOPPED    = 3
};

/// Packed contact categories accepted by the player/companion response scan.
enum {
    PLAYER_ACTOR_CONTACT_NO_KIND            = 0,
    PLAYER_ACTOR_CONTACT_PLAYER_BODY_KIND   = WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16,
    PLAYER_ACTOR_CONTACT_PLAYER_ATTACK_KIND = WORLD_COLLISION_CONTACT_ATTACK >> 16,
    PLAYER_ACTOR_CONTACT_ENEMY_BODY_KIND    = WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16,
    PLAYER_ACTOR_CONTACT_ENEMY_ATTACK_KIND  = DAMAGE_ATTACK_CATEGORY >> 16,
    PLAYER_ACTOR_CONTACT_HAZARD_KIND        = 5
};

extern u16 D_80113F9C[70];

extern const TaskFuncTable4 D_80097AB0;

static inline void _playerActorResumeAfterHit(Task* task);

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

static void _playerActorRecordAttackContact(Task* task, const WorldCollisionContact* contact, s32 bodyIndex);

static void _playerActorRecordHazardContact(Task* task, const WorldCollisionContact* contact, s32 bodyIndex);

static void _modelObjectInitChildTask(Task* task);

static void _modelObjectUpdateChildTask(Task* task);

static void _modelObjectDeferChildTaskRemoval(Task* task);

static void _modelObjectKillChildTask(Task* task);

static void func_8010C46C(Task* arg0);

/// Clears a pending hit and blends back to aim or locomotion with 18 recovery ticks.
///
/// Requires live actor/native animation storage. The retained normal-state
/// selector chooses aim for any nonzero value, locomotion for zero.
static inline void _playerActorResumeAfterHit(Task* task)
{
    enum { PLAYER_ACTOR_RECOVERY_AIM_BLEND_FRAMES = 12 };
    GameActor* actor;

    actor = task->work;
    playerActorClearPendingHit(task);
    actor->recoveryTicks = PLAYER_ACTOR_HIT_RECOVERY_TICKS;
    if (actor->state != 0) {
        playerActorEnterAim(task, PLAYER_ACTOR_RECOVERY_AIM_BLEND_FRAMES);
    } else {
        playerActorEnterLocomotion(task, 0);
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

/// Measures a receiving body's centre minus a contact point in cached view space.
///
/// Rotates the local centre using the body's composed basis, adds its cached
/// translation, then subtracts the signed-halfword contact point. Requires live
/// composed transforms and initialized GTE state; writes only the supplied
/// scratch position/separation XYZ and retains no pointers.
static inline void _playerActorMeasureContactSeparation(const WorldCollisionBody* body, const WorldCollisionContact* contact, _PlayerActorPushbackScratch* block)
{
    gte_SetRotMatrix(&body->coord->workm);
    gte_ldv0(&body->pos);
    gte_rtv0();
    gte_stlvnl(&block->separation);
    block->position.vx   = body->coord->workm.t[0] + block->separation.vx;
    block->position.vy   = body->coord->workm.t[1] + block->separation.vy;
    block->position.vz   = body->coord->workm.t[2] + block->separation.vz;
    block->separation.vx = block->position.vx - contact->point.vx;
    block->separation.vy = block->position.vy - contact->point.vy;
    block->separation.vz = block->position.vz - contact->point.vz;
}

void playerActorResolveBodyContacts(Task* task, const WorldCollisionContact* contacts)
{
    enum {
        PLAYER_ACTOR_CONTACT_DIRECTION_FRACTION_BITS = 12,
        PLAYER_ACTOR_CONTACT_PUSH_DIVISOR_BITS       = 2
    };
    _PlayerActorPushbackScratch* block;
    GameActor*                   actor;
    GfxCoord*                    rootCoord;
    const WorldCollisionContact* contact;
    WorldCollisionBody*          body;
    VECTOR*                      separation;
    s32                          contactIndex;
    s32                          deepestOverlap;
    s32                          overlap;
    s32                          bodyId;
    s32                          nonnegativeOverlap;

    contact        = contacts;
    deepestOverlap = 0;
    contactIndex   = 0;
    block          = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorPushbackScratch);
    actor          = task->work;
    rootCoord      = task->extra.tmd->coords;

    // Keep the deepest blocking body overlap and latch the first pending hit.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(actor->collisionContacts); contact++, contactIndex++) {
        separation = &block->separation;
        if (contact->flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
            switch (contact->key.parts.kind) {
                case PLAYER_ACTOR_CONTACT_NO_KIND:
                case PLAYER_ACTOR_CONTACT_PLAYER_BODY_KIND:
                case PLAYER_ACTOR_CONTACT_PLAYER_ATTACK_KIND:
                    break;
                case PLAYER_ACTOR_CONTACT_ENEMY_BODY_KIND:
                    if ((s8)actor->gridResponse != 0) {
                        break;
                    }
                    bodyId = contact->key.parts.id;
                    if (bodyId < ARRAY_SIZE(D_80113F9C) && D_80113F9C[bodyId] == 1) {
                        body = &actor->collisionBodies[(u8)contact->flags >> WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT];
                        _playerActorMeasureContactSeparation(body, contact, block);
                        overlap            = contact->distance - SquareRoot0(block->separation.vx * block->separation.vx +
                                                                             block->separation.vy * block->separation.vy +
                                                                             block->separation.vz * block->separation.vz);
                        nonnegativeOverlap = overlap;
                        if (overlap < 0) {
                            nonnegativeOverlap = 0;
                        }
                        overlap = nonnegativeOverlap;
                        if (deepestOverlap < overlap) {
                            deepestOverlap = overlap;
                            VectorNormal(separation, &block->viewDirection);
                            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm,
                                                   &block->viewDirection, &block->roomDirection);
                        }
                    }
                    break;
                case PLAYER_ACTOR_CONTACT_ENEMY_ATTACK_KIND:
                    _playerActorRecordAttackContact(task, contact, (u8)contact->flags >> WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT);
                    break;
                case PLAYER_ACTOR_CONTACT_HAZARD_KIND:
                    _playerActorRecordHazardContact(task, contact, (u8)contact->flags >> WORLD_COLLISION_CONTACT_BODY_INDEX_SHIFT);
                    break;
            }
        }
    }

    // Measure view-space heading at the full push, then retain a quarter push.
    if (deepestOverlap > 0) {
        actor->usesPushbackDirection = 1;
        actor->pushbackDirection.vx  = rootCoord->workm.t[0];
        actor->pushbackDirection.vy  = rootCoord->workm.t[1];
        actor->pushbackDirection.vz  = rootCoord->workm.t[2];
        block->position.vx           = rootCoord->coord.t[0];
        block->position.vz           = rootCoord->coord.t[2];
        rootCoord->coord.t[0]       += (deepestOverlap * block->roomDirection.vx) >> PLAYER_ACTOR_CONTACT_DIRECTION_FRACTION_BITS;
        rootCoord->coord.t[2]       += (deepestOverlap * block->roomDirection.vz) >> PLAYER_ACTOR_CONTACT_DIRECTION_FRACTION_BITS;
        rootCoord->composeStamp      = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        actor->pushbackDirection.vx = rootCoord->workm.t[0] - actor->pushbackDirection.vx;
        actor->pushbackDirection.vy = rootCoord->workm.t[1] - actor->pushbackDirection.vy;
        actor->pushbackDirection.vz = rootCoord->workm.t[2] - actor->pushbackDirection.vz;
        VectorNormal(&actor->pushbackDirection, &actor->pushbackDirection);
        rootCoord->coord.t[0]   = block->position.vx + ((deepestOverlap * block->roomDirection.vx) >> (PLAYER_ACTOR_CONTACT_DIRECTION_FRACTION_BITS + PLAYER_ACTOR_CONTACT_PUSH_DIVISOR_BITS));
        rootCoord->coord.t[2]   = block->position.vz + ((deepestOverlap * block->roomDirection.vz) >> (PLAYER_ACTOR_CONTACT_DIRECTION_FRACTION_BITS + PLAYER_ACTOR_CONTACT_PUSH_DIVISOR_BITS));
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
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
                playerStateApplyHpDamage(1);
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

void playerStateSetStatusEffects(s32 clearEffects, s32 statusMask)
{
    enum {
        PLAYER_STATE_TIMED_STATUS_20                 = 0x20, // Timed effect identity remains unproven
        PLAYER_STATE_CONFUSION_DIRECTION_RANDOM_MASK = 0x1F,
        PLAYER_STATE_CONFUSION_DIRECTION_BASE_TICKS  = 10
    };
    Task*      playerTask;
    GameActor* actor;
    s32        effects;

    effects = statusMask;
    // Applying a mask refreshes each accepted effect independently.
    if (clearEffects == 0) {
        playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        if (statusMask & PLAYER_STATUS_DARKNESS) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_DARKNESS) == 0) {
                gPlayerStatus.statusFlags       |= PLAYER_STATUS_DARKNESS;
                actor->effectTimer.darknessTicks = PLAYER_STATE_STATUS_DURATION_TICKS;
                roomEffectStartDarknessDim();
                playerActorClearLockTarget(playerTask);
                roomEffectStartStatusTint(PLAYER_STATUS_DARKNESS);
            }
        }
        if (effects & PLAYER_STATUS_PARALYSIS) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_PARALYSIS) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_PARALYSIS;
                actor->paralysisTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
                actor->paralysisProgress   = 0;
                playerActorClearPendingHit(playerTask);
                roomEffectStartStatusTint(PLAYER_STATUS_PARALYSIS);
            }
        }
        if (effects & PLAYER_STATUS_POISON) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_POISON) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_POISON;
                actor->poisonTicks         = PLAYER_STATE_STATUS_DURATION_TICKS;
                actor->poisonDamageTicks   = 0;
                roomEffectStartStatusTint(PLAYER_STATUS_POISON);
            }
        }
        if (effects & PLAYER_STATUS_SILENCE) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_SILENCE) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_SILENCE;
                actor->silenceTicks        = PLAYER_STATE_STATUS_DURATION_TICKS;
                roomEffectStartStatusTint(PLAYER_STATUS_SILENCE);
            }
        }
        if (effects & PLAYER_STATE_TIMED_STATUS_20) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_TIMED_STATUS_20) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATE_TIMED_STATUS_20;
                actor->status20Ticks       = PLAYER_STATE_STATUS_DURATION_TICKS;
                roomEffectStartStatusTint(ROOM_EFFECT_STATUS_TINT_STATUS_20);
            }
        }
        if (effects & PLAYER_STATUS_CONFUSION) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_CONFUSION) == 0) {
                gPlayerStatus.statusFlags     |= PLAYER_STATUS_CONFUSION;
                actor->confusionTicks          = PLAYER_STATE_STATUS_DURATION_TICKS;
                actor->confusionDirectionTicks = (rand() & PLAYER_STATE_CONFUSION_DIRECTION_RANDOM_MASK) + PLAYER_STATE_CONFUSION_DIRECTION_BASE_TICKS;
                actor->confusionDirections     = 0;
                roomEffectStartStatusTint(PLAYER_STATUS_CONFUSION);
            }
        }
        if (effects & PLAYER_STATUS_BERSERKER) {
            actor = playerTask->work;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_BERSERKER) == 0) {
                gPlayerStatus.statusFlags |= PLAYER_STATUS_BERSERKER;
                actor->berserkerTicks      = PLAYER_STATE_STATUS_DURATION_TICKS;
                roomEffectStartStatusTint(PLAYER_STATUS_BERSERKER);
                roomEffectStartBerserkerGlow();
            }
        }
    } else {
        gPlayerStatus.statusFlags &= ~statusMask;
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
                roomEffectStartDarknessDim();
                playerActorClearLockTarget(arg0);
                roomEffectStartStatusTint(PLAYER_STATUS_DARKNESS);
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
                playerActorClearPendingHit(arg0);
                roomEffectStartStatusTint(PLAYER_STATUS_PARALYSIS);
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
                roomEffectStartStatusTint(PLAYER_STATUS_POISON);
                break;
            }
            case 4:
                roomEffectStartStatusTint(ROOM_EFFECT_STATUS_TINT_REACTION_4);
                break;
            case 8: {
                GameActor* inner;

                inner = arg0->work;
                if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_SILENCE) != 0) {
                    return;
                }
                gPlayerStatus.statusFlags |= PLAYER_STATUS_SILENCE;
                inner->silenceTicks        = PLAYER_STATE_STATUS_DURATION_TICKS;
                roomEffectStartStatusTint(PLAYER_STATUS_SILENCE);
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
                roomEffectStartStatusTint(ROOM_EFFECT_STATUS_TINT_STATUS_20);
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
                roomEffectStartStatusTint(PLAYER_STATUS_CONFUSION);
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
                roomEffectStartStatusTint(PLAYER_STATUS_BERSERKER);
                roomEffectStartBerserkerGlow();
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
                    playerActorClearLockTarget(arg0);
                }
            } else {
                mode = inner->state;
                if (mode == 2 && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS) && (rand() & 3)) {
                    node = worldTargetFindLockNode(arg0);
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

s32 playerStateApplyHpDamage(s16 damagePoints)
{
    enum {
        PLAYER_STATE_SURVIVAL_MIN_HP      = 5,
        PLAYER_STATE_SURVIVAL_HP          = 1,
        PLAYER_STATE_MP_DAMAGE_DIVISOR    = 5,
        PLAYER_STATE_SURVIVAL_BURST_STYLE = 5
    };
    s16           adjustedDamage;
    s32           fatal;
    PlayerStatus* playerStatus;
    Task*         playerTask;
    GfxCoord*     rootCoord;

    adjustedDamage = damagePoints;
    fatal          = 0;
    if (equipmentHasEffect(EQUIPMENT_EFFECT_HOLY_WATER) != 0) {
        adjustedDamage = damagePoints - (damagePoints >> 2);
    }
    // Credit MP from the reduced damage before either survival path.
    if (equipmentHasEffect(EQUIPMENT_EFFECT_MP_GENERATION) != 0) {
        gPlayerStatus.mp += adjustedDamage / PLAYER_STATE_MP_DAMAGE_DIVISOR;
        if (gPlayerStatus.mpMax < gPlayerStatus.mp) {
            gPlayerStatus.mp = gPlayerStatus.mpMax;
        }
    }
    if (equipmentHasEffect(EQUIPMENT_EFFECT_RESIST_IMPACT) != 0) {
        playerStatus = &gPlayerStatus;
        if (playerStatus->hp >= PLAYER_STATE_SURVIVAL_MIN_HP && adjustedDamage >= playerStatus->hp) {
            playerTask       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
            rootCoord        = playerTask->extra.tmd->coords;
            playerStatus->hp = PLAYER_STATE_SURVIVAL_HP;
            effectSpawn(EFFECT_CRITICAL_HIT, rootCoord + 1, PLAYER_STATE_SURVIVAL_BURST_STYLE, 0);
            return 0;
        }
    }
    gPlayerStatus.hp -= adjustedDamage;
    if (gPlayerStatus.hp > 0) {
        return fatal;
    }
    if (gGameSession->eventState != 0) {
        gPlayerStatus.hp = PLAYER_STATE_SURVIVAL_HP;
    } else {
        fatal = 1;
        displayAcquireMenuHold();
    }
    return fatal;
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

void playerActorEnterStoppedPose(Task* task, s32 blendFrames)
{
    enum {
        PLAYER_ACTOR_STOPPED_ANIMATION_SET             = 18,
        PLAYER_ACTOR_STOPPED_MOVEMENT                  = 0,
        PLAYER_ACTOR_STOPPED_TURN_DISABLED             = 0,
        PLAYER_ACTOR_STOPPED_ANIMATION_CONTROLLER_NONE = 0
    };
    GameActor* actor;

    actor                 = task->work;
    actor->mode           = GAME_ACTOR_MODE_DAMAGE;
    actor->movementMode   = PLAYER_ACTOR_STOPPED_MOVEMENT;
    actor->turnRateIndex  = PLAYER_ACTOR_STOPPED_TURN_DISABLED;
    actor->animationState = PLAYER_ACTOR_STOPPED_ANIMATION_CONTROLLER_NONE;
    actor->statePhase     = 0;
    actor->hitRegion      = PLAYER_ACTOR_HIT_REGION_STOPPED;
    if (blendFrames == 0) {
        playerActorResetChildSlots(task, PLAYER_ACTOR_STOPPED_ANIMATION_SET);
    } else {
        playerActorPlayChildSlotsWithBlend(task, PLAYER_ACTOR_STOPPED_ANIMATION_SET, 0, blendFrames);
    }
    playerActorClearLockTarget(task);
    actor->pendingCollisionUpdates |= (GAME_ACTOR_COLLISION_FIRST_TWO_REQUESTS << GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT);
}

static void func_8010AAB4(Task* arg0)
{
    GameActor*    inner;
    PlayerStatus* p;

    p                  = &gPlayerStatus;
    inner              = arg0->work;
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
    playerActorResetWeaponAttack(arg0, p->weapon, 0);
    if (p->hp > 0) {
        inner->mode           = GAME_ACTOR_MODE_DAMAGE;
        inner->movementMode   = 0;
        inner->turnRateIndex  = 0;
        inner->animationState = 7;
        inner->statePhase     = 0;
        inner->movementSign   = 0;
        playerStateApplyHpDamage(inner->pendingDamage);
        inner->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        if ((s8)inner->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
            inner->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
        }
        func_8010A42C(arg0, inner->damageReaction);
    }
}

static void func_8010AB70(Task* arg0)
{
    _playerActorResumeAfterHit(arg0);
}

void playerActorFinishDamageReaction(Task* task)
{
    enum { PLAYER_ACTOR_DAMAGE_CLIP_FINISHED_PHASE = 1 };
    GameActor* actor;

    actor = task->work;
    // Animation controller 7 advances phase 0 to 1 when its child clip finishes.
    if (actor->statePhase != 0) {
        if (actor->statePhase == PLAYER_ACTOR_DAMAGE_CLIP_FINISHED_PHASE) {
            _playerActorResumeAfterHit(task);
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
            playerActorClearPendingHit(arg0);
            inner2->recoveryTicks = 0x12;
            if (inner2->state != 0) {
                playerActorEnterAim(arg0, 0xC);
            } else {
                playerActorEnterLocomotion(arg0, 0);
            }
        } else {
            inner->stateTimer = 5;
        }
        effectSpawn(
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
            effectSpawnHit(EFFECT_HIT_KIND_TINTED_PUFF, D_80113358.coord, vec, params);
            break;
        case 1:
            break;
        case 2:
            _playerActorResumeAfterHit(arg0);
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
    roomEffectStartDarknessDim();
    playerActorClearLockTarget(arg0);
    roomEffectStartStatusTint(PLAYER_STATUS_DARKNESS);
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
    playerActorClearPendingHit(arg0);
    roomEffectStartStatusTint(PLAYER_STATUS_PARALYSIS);
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
    roomEffectStartStatusTint(PLAYER_STATUS_POISON);
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
    roomEffectStartStatusTint(PLAYER_STATUS_SILENCE);
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
    roomEffectStartStatusTint(ROOM_EFFECT_STATUS_TINT_STATUS_20);
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
    roomEffectStartStatusTint(PLAYER_STATUS_CONFUSION);
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
    roomEffectStartStatusTint(PLAYER_STATUS_BERSERKER);
    roomEffectStartBerserkerGlow();
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
    playerActorResolveBodyContacts(arg0, inner->collisionContacts);
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

void playerActorClearPendingHit(Task* task)
{
    GameActor* actor;

    actor                 = task->work;
    actor->hitRegion      = PLAYER_ACTOR_HIT_REGION_NONE;
    actor->damageReaction = GAME_ACTOR_REACTION_ORDINARY;
    actor->pendingDamage  = 0;
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
    ret     = playerStateApplyHpDamage(damageComputeReceived(arg0, 0, &out, 0));
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

/// Latches the first category-4 attack against a player or companion motion body.
///
/// `bodyIndex` identifies the receiving motion sphere (0..2). The contact is
/// borrowed for this call; calculated HP loss and reaction are stored for the
/// later damage state. Companion work selects companion damage modifiers.
static void _playerActorRecordAttackContact(Task* task, const WorldCollisionContact* contact, s32 bodyIndex)
{
    GameActor* actor;
    s32        reaction;
    s32        isCompanion;

    actor       = task->work;
    isCompanion = actor->companionWork != 0;
    if ((u16)actor->hitRegion == PLAYER_ACTOR_HIT_REGION_NONE) {
        actor->hitBodyIndex = bodyIndex;
        bodyIndex           = (u16)bodyIndex;
        if (bodyIndex == GAME_ACTOR_BODY_PART4) {
            actor->hitRegion = bodyIndex;
        } else {
            actor->hitRegion = PLAYER_ACTOR_HIT_REGION_OTHER_BODY;
        }
        actor->pendingDamage  = damageComputeReceived(contact->key.value, 0, &reaction, isCompanion);
        actor->damageReaction = reaction;
    }
}

/// Records category-5 hazard damage while no collision hit is pending.
///
/// The borrowed key must select a hazard row 0..10. IDs 2..4 latch the receiving
/// motion body's hit region (body index 1 or another body); other IDs update
/// damage without establishing a hit region. IDs 2/4 select the player's
/// three-part flash sequence; ID 3 selects the ordinary reaction.
static void _playerActorRecordHazardContact(Task* task, const WorldCollisionContact* contact, s32 bodyIndex)
{
    enum {
        PLAYER_ACTOR_HAZARD_HELIPAD_LIGHT_BLAST    = 2,
        PLAYER_ACTOR_HAZARD_HELIPAD_FINISHED_FLARE = 3,
        PLAYER_ACTOR_HAZARD_HELIPAD_ACTIVE_FLARE   = 4,
        PLAYER_ACTOR_HAZARD_REACTION_FLASH_BURSTS  = 5 // Player flashes model parts 3, 2, then 1
    };
    GameActor* actor;
    u32        hazardId;

    actor    = task->work;
    hazardId = (u16)contact->key.value;
    // Unhandled hazard IDs still supply damage without latching a hit region.
    if ((u16)actor->hitRegion == PLAYER_ACTOR_HIT_REGION_NONE) {
        actor->hitBodyIndex = bodyIndex;
        switch (hazardId) {
            case 1: // Placeholder no-op arm; the original selector is unproven
                break;
            case PLAYER_ACTOR_HAZARD_HELIPAD_LIGHT_BLAST:
            case PLAYER_ACTOR_HAZARD_HELIPAD_ACTIVE_FLARE:
                bodyIndex = (u16)bodyIndex;
                if (bodyIndex == GAME_ACTOR_BODY_PART4) {
                    actor->hitRegion = bodyIndex;
                } else {
                    actor->hitRegion = PLAYER_ACTOR_HIT_REGION_OTHER_BODY;
                }
                actor->damageReaction = PLAYER_ACTOR_HAZARD_REACTION_FLASH_BURSTS;
                break;
            case PLAYER_ACTOR_HAZARD_HELIPAD_FINISHED_FLARE:
                bodyIndex = (u16)bodyIndex;
                if (bodyIndex == GAME_ACTOR_BODY_PART4) {
                    actor->hitRegion = bodyIndex;
                } else {
                    actor->hitRegion = PLAYER_ACTOR_HIT_REGION_OTHER_BODY;
                }
                actor->damageReaction = GAME_ACTOR_REACTION_ORDINARY;
                break;
        }
        actor->pendingDamage = damageGetHazardDamage(contact->key.value, DAMAGE_HAZARD_VICTIM_PLAYER);
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
                effectSpawnHit(EFFECT_HIT_KIND_BLAST, coords, 0, params);
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
    effectSpawnHit(EFFECT_HIT_KIND_TINTED_PUFF, coords, 0, params);
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

void companionRemoveEquipment(Task* task)
{
    enum {
        COMPANION_REMOVE_EQUIPMENT_IDLE_SET          = 1,
        COMPANION_REMOVE_EQUIPMENT_IDLE_BLEND_FRAMES = 4
    };
    GameActor* actor;
    GameActor* animationActor;
    GameActor* idleActor;
    TmdObject* model;
    Task*      equipmentTask;

    actor         = task->work;
    equipmentTask = actor->equipmentTasks[1];
    // Restore native idle playback only when a weapon model was attached.
    if (equipmentTask != NULL) {
        taskKill(equipmentTask);
        actor->equipmentTasks[1]           = NULL;
        model                              = task->extra.tmd;
        animationActor                     = task->work;
        animationActor->animationBankIndex = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
        animationActor->animationSets      = Gp_AnimBlkTbl[animationActor->animationBankIndex]->table.sets;
        animationInitContext(&animationActor->animationContext, animationActor->animationSets, model, animationActor->poseBuffer,
                             animationActor->animationSlots);
        playerActorResetChildSlots(task, COMPANION_REMOVE_EQUIPMENT_IDLE_SET);
        idleActor = task->work;
        /// Returns the surviving companion to normal idle after equipment removal.
        ///
        /// Requires a live GameActor pointer expression without side effects;
        /// each store evaluates it. Playback is selected after this reset.
#define COMPANION_REMOVE_EQUIPMENT_RESET_IDLE(actor)      \
    {                                                     \
        (actor)->mode           = GAME_ACTOR_MODE_NORMAL; \
        (actor)->state          = 0;                      \
        (actor)->movementMode   = 0;                      \
        (actor)->turnRateIndex  = 0;                      \
        (actor)->animationState = 0;                      \
        (actor)->statePhase     = 0;                      \
        (actor)->idleTicks      = 0;                      \
        (actor)->actionValue    = 0;                      \
        (actor)->movementSign   = 0;                      \
        (actor)->turnSign       = 0;                      \
    }
        COMPANION_REMOVE_EQUIPMENT_RESET_IDLE(idleActor);
#undef COMPANION_REMOVE_EQUIPMENT_RESET_IDLE
        playerActorPlayChildSlotsWithBlend(task, COMPANION_REMOVE_EQUIPMENT_IDLE_SET, 0, COMPANION_REMOVE_EQUIPMENT_IDLE_BLEND_FRAMES);
    }
    equipmentTask = actor->weaponEffectTask;
    if (equipmentTask != NULL) {
        taskKill(equipmentTask);
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
            playerActorInitWeaponCollision(work, val1, val2);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key |= 0x80;
            companion->activity.combat.attacksRemaining         = D_actor_800100_80167230[save->state.companionVariant];
            if ((u8)save->state.companionVariant == 4 && actor->weaponEffectTask == NULL) {
                eff = effectSpawn(
                    (EFFECT_COMPANION_WEAPON_FLARE | EFFECT_SPAWN_UNLIMITED), actor->equipmentTasks[1]->extra.tmd->coords, (s32)(val1), 0);
                if (eff != NULL) {
                    actor->weaponEffectTask = eff->task;
                    playerActorResetWeaponAttack(work, val1, 0);
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

/// Applies pending companion damage to saved family 1, retaining its event survival floor.
///
/// Cheat mode and other families leave HP intact. Borrows the actor and live
/// save for this call; subtraction keeps its low halfword and tests it as s16.
static inline void _companionApplyPendingHitDamage(McSaveData* save, const GameActor* actor)
{
    enum { COMPANION_DAMAGE_HP_FAMILY = 1 };
    s32 companionType;
    u16 remainingHpBits;

    // Keep halfword HP arithmetic and the event survival floor for family 1.
    if (save->state.cheatMode == 0 && (companionType = save->state.companionType) == COMPANION_DAMAGE_HP_FAMILY) {
        remainingHpBits         = save->state.companionHp - actor->pendingDamage;
        save->state.companionHp = remainingHpBits;
        if ((s16)remainingHpBits <= 0 && gGameSession->eventState != 0) {
            save->state.companionHp = companionType;
        }
    }
}

void companionEnterDamageReaction(Task* task)
{
    enum {
        COMPANION_DAMAGE_ANIMATION_CONTROLLER = 7,
        COMPANION_DAMAGE_PART4_SET            = 16,
        COMPANION_DAMAGE_OTHER_BODY_SET       = 17,
        COMPANION_DAMAGE_BLEND_FRAMES         = 3
    };
    GameActor*  actor;
    McSaveData* save;
    u16         animationSet;

    actor                 = task->work;
    actor->mode           = GAME_ACTOR_MODE_DAMAGE;
    actor->animationState = COMPANION_DAMAGE_ANIMATION_CONTROLLER;
    save                  = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    actor->movementMode   = 0;
    actor->turnRateIndex  = 0;
    actor->statePhase     = 0;
    actor->stateAux       = 0;
    actor->movementSign   = 0;
    actor->turnSign       = 0;
    _companionApplyPendingHitDamage(save, actor);
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    }
    playerActorResetWeaponAttack(task, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
    animationSet = COMPANION_DAMAGE_OTHER_BODY_SET;
    if ((u16)actor->hitRegion == PLAYER_ACTOR_HIT_REGION_PART4) {
        animationSet = COMPANION_DAMAGE_PART4_SET;
    }
    playerActorPlayChildSlotsWithBlend(task, animationSet, 0, COMPANION_DAMAGE_BLEND_FRAMES);
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

/// Returns a live companion work block to its normal idle dispatch state.
///
/// Stops movement and turning and resets the idle/action counters. The caller
/// selects the idle animation after this reset; the block remains task-owned.
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

s32 playerActorGetTurnToPoint(Task* task, const VECTOR3* targetPoint)
{
    GfxCoord*                         coords;
    PlayerActorPlanarDistanceScratch* block;
    GameActor*                        actor;
    s16                               turn;

    coords = task->extra.tmd->coords;
    block  = SCRATCH_STACK_RESERVE_BLOCK(PlayerActorPlanarDistanceScratch);
    actor  = task->work;
    playerActorGetPointDelta(coords, targetPoint, &block->delta);
    turn = playerActorShortestTurn(actor->rotation.vy, ratan2(block->delta.vx, block->delta.vz));
    SCRATCH_STACK_RELEASE_BLOCK(PlayerActorPlanarDistanceScratch);
    return turn;
}

void playerActorTurnBodyTowardPoint(Task* task, const VECTOR3* targetPoint)
{
    enum { PLAYER_ACTOR_BODY_YAW_STEP = 0x40 };
    _PlayerActorTurnScratch* block;
    TmdObject*               model;
    GameActor*               actor;
    s32                      yawStep;

    model = task->extra.tmd;
    block = SCRATCH_STACK_RESERVE_BLOCK(_PlayerActorTurnScratch);
    actor = task->work;
    playerActorGetPointDelta(model->coords, targetPoint, &block->targetDelta);
    block->yaw = ratan2(block->targetDelta.vx, block->targetDelta.vz);
    yawStep    = playerActorShortestTurn(actor->rotation.vy, block->yaw);
    block->yaw = yawStep;
    if (yawStep > PLAYER_ACTOR_BODY_YAW_STEP) {
        block->yaw = PLAYER_ACTOR_BODY_YAW_STEP;
    } else if (yawStep < -PLAYER_ACTOR_BODY_YAW_STEP) {
        block->yaw = -PLAYER_ACTOR_BODY_YAW_STEP;
    }
    actor->rotation.vy = (actor->rotation.vy + block->yaw) & ACTOR_TRANSFORM_ANGLE_MASK;
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorTurnScratch);
}

void playerActorTurnAimTowardPoint(Task* task, const VECTOR3* targetPoint)
{
    enum {
        PLAYER_ACTOR_AIM_YAW_STEP  = 0x20,
        PLAYER_ACTOR_AIM_YAW_LIMIT = 0x1A0
    };
    _PlayerActorAimScratch* head;
    _PlayerActorAimScratch* block;
    GfxCoord*               originCoord;
    SVECTOR*                originOffset;
    TmdObject*              model;
    GfxCoord*               modelCoords;
    GameActor*              actor;
    s32                     yawStep;

    head         = SCRATCH_STACK_CURSOR(_PlayerActorAimScratch);
    model        = task->extra.tmd;
    originCoord  = &head[-1].originCoord;
    originOffset = &head[-1].originOffset;
    modelCoords  = model->coords;
    actor        = task->work;
    block = SCRATCH_STACK_CURSOR(_PlayerActorAimScratch) = head - 1;
    block->originOffset.vx                               = 0;
    block->originOffset.vy                               = 0;
    block->originOffset.vz                               = 0;
    actorRenderPlaceCoordOffset(modelCoords + 4, originCoord, originOffset);
    playerActorGetPointDelta(originCoord, targetPoint, &block->targetDelta);
    // Turn toward the target relative to body facing, preserving the strict aim limit.
    yawStep = ratan2(head[-1].targetDelta.vx, block->targetDelta.vz) - actor->rotation.vy;
    yawStep = playerActorShortestTurn(actor->aimYaw, yawStep);
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

void companionInitNativeAnimation(Task* task)
{
    GameActor* actor;
    TmdObject* model;

    actor                     = task->work;
    model                     = task->extra.tmd;
    actor->animationBankIndex = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
    actor->animationSets      = Gp_AnimBlkTbl[actor->animationBankIndex]->table.sets;
    animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
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

void companionTrackLockTarget(Task* task, s32 trackingAxes)
{
    enum { COMPANION_LOCK_MIN_GROUND_DISTANCE = 896 };
    GameActor*       actor;
    WorldTargetNode* target;
    s32              minGroundDistance;

    actor  = task->work;
    target = actor->targetNode;
    if (target == NULL || (target->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
        actor->targetNode       = NULL;
        actor->aimTrackingState = GAME_ACTOR_AIM_TRACKING_DECAY;
    } else if ((s8)actor->aimTrackingState == GAME_ACTOR_AIM_TRACKING_TARGET) {
        if (trackingAxes & COMPANION_LOCK_TRACK_YAW) {
            minGroundDistance = 0;
            if (trackingAxes != COMPANION_LOCK_TRACK_YAW) {
                minGroundDistance = COMPANION_LOCK_MIN_GROUND_DISTANCE;
            }
            playerActorAimYawToLock(task, minGroundDistance);
        }
        if (trackingAxes & COMPANION_LOCK_TRACK_PITCH) {
            if (D_80113388[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant] != 0) {
                playerActorAimPitchToLock(task);
            } else {
                playerActorAimPart6PitchToLock(task, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], COMPANION_LOCK_MIN_GROUND_DISTANCE);
            }
        }
    }
}

void companionRecoverToIdle(Task* task)
{
    enum {
        COMPANION_RECOVERY_IDLE_SET          = 1,
        COMPANION_RECOVERY_IDLE_BLEND_FRAMES = 4
    };
    GameActor* hitActor;
    GameActor* actor;

    hitActor = task->work;
    playerActorClearPendingHit(task);
    hitActor->recoveryTicks = PLAYER_ACTOR_HIT_RECOVERY_TICKS;
    actor                   = task->work;
    _companionResetIdleState(actor);
    playerActorPlayChildSlotsWithBlend(task, COMPANION_RECOVERY_IDLE_SET, 0, COMPANION_RECOVERY_IDLE_BLEND_FRAMES);
}

void companionBindCollisionProbe(Task* task, const SVECTOR3* nearEndpoint, s32 farEndpointZ)
{
    enum {
        COMPANION_PROBE_ORIGIN_Z    = -160,
        COMPANION_PROBE_CONTACT_KEY = 0x60000,
        COMPANION_PROBE_RADIUS      = 128
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    CompanionWork*         companion;
    WorldCollisionBody*    body;
    WorldCollisionCapsule* capsule;
    s16                    nearEndpointZ;

    actor                  = task->work;
    companion              = actor->companionWork;
    rootCoord              = task->extra.tmd->coords;
    body                   = &companion->probe.body;
    capsule                = &companion->probe.shape;
    companion->probe.coord = *rootCoord;
    body->coord            = &companion->probe.coord;
    body->pos.vz           = COMPANION_PROBE_ORIGIN_Z;
    body->key              = COMPANION_PROBE_CONTACT_KEY;
    body->context.capsule  = capsule;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->flags            = WORLD_COLLISION_BODY_CAPSULE;
    capsule->ends[1].vx    = nearEndpoint->vx;
    capsule->ends[1].vy    = nearEndpoint->vy;
    nearEndpointZ          = nearEndpoint->vz;
    capsule->ends[0].vz    = farEndpointZ;
    capsule->ends[0].vx    = capsule->ends[1].vx;
    capsule->end1Radius    = COMPANION_PROBE_RADIUS;
    capsule->end0Radius    = COMPANION_PROBE_RADIUS;
    capsule->contacts      = companion->probe.contacts;
    capsule->ends[1].vz    = nearEndpointZ;
    capsule->ends[0].vy    = capsule->ends[1].vy;
    // Link the shape before enabling its one-contact grid and pair scans.
    worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, body);
    worldCollisionInitContacts(capsule->contacts, ARRAY_SIZE(companion->probe.contacts), 0);
    body->flags |= (WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
}

s32 companionEndScriptedMotion(Task* task, s32 unusedMessageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    TmdObject*     model;
    GfxCoord*      rootCoord;
    GfxCoord*      animatedRoot;
    GameActor*     actor;
    VECTOR         rootMotion;
    AnimationSet** previousSets;
    AnimationSet** nativeSets;
    s32            bankChanged;

    model        = task->extra.tmd;
    rootCoord    = model->coords;
    actor        = task->work;
    animatedRoot = rootCoord + 1;
    // Fold animation-root XZ motion into the model root before restoring native clips.
    rootMotion.vx = animatedRoot->coord.t[0];
    rootMotion.vy = animatedRoot->coord.t[1];
    rootMotion.vz = animatedRoot->coord.t[2];
    ApplyMatrixLV(&rootCoord->coord, &rootMotion, &rootMotion);
    rootCoord->coord.t[0]     += rootMotion.vx;
    rootCoord->coord.t[2]     += rootMotion.vz;
    animatedRoot->coord.t[0]   = 0;
    animatedRoot->coord.t[2]   = 0;
    actor->previousPosition.vx = rootCoord->coord.t[0];
    actor->previousPosition.vy = rootCoord->coord.t[1];
    actor->previousPosition.vz = rootCoord->coord.t[2];
    // Restart on a bank change; otherwise blend the existing pose back to idle.
    previousSets              = actor->animationSets;
    actor->animationBankIndex = Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant;
    nativeSets                = Gp_AnimBlkTbl[actor->animationBankIndex]->table.sets;
    bankChanged               = previousSets != nativeSets;
    actor->animationSets      = nativeSets;
    animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
                         actor->animationSlots);
    actor->animationRate           = ANIMATION_RATE_ONE;
    actor->pendingCollisionUpdates = GAME_ACTOR_COLLISION_REQUEST_MASK;
    actor->statePhase              = 0;
    actor->stateAux                = 0;
    companionEnterIdle(task, bankChanged);
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
    playerActorResetWeaponAttack(arg0, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
}

/// Stops companion motion and aim offsets and disables its weapon attack for scripted control.
///
/// Requires live GameActor work and the saved variant's native weapon resources.
/// Clears the pending hit selector and requests scripted dispatch; the caller
/// chooses the state and playback. Preserves the player's interaction latch and
/// the companion's selected lock target. The task and actor must describe the
/// same companion; no pointers are retained.
static inline void _companionEnterScriptedMode(Task* task, GameActor* actor)
{
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
    actor->hitRegion                                      = PLAYER_ACTOR_HIT_REGION_NONE;
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    playerActorResetWeaponAttack(task, D_actor_800100_80167218[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant], 0);
}

s32 companionPlayScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    enum { COMPANION_SCRIPTED_ANIMATION_STATE = 1 };
    GameActor* actor;
    TmdObject* model;

    actor = task->work;
    model = task->extra.tmd;
    _companionEnterScriptedMode(task, actor);
    // Select the animation table before resetting or blending its slots.
    actor->state = COMPANION_SCRIPTED_ANIMATION_STATE;
    if (actor->animationSets != Gp_AnimBlkTbl[request->source.index]->table.sets) {
        actor->animationSets = Gp_AnimBlkTbl[request->source.index]->table.sets;
        animationInitContext(&actor->animationContext, actor->animationSets, model, actor->poseBuffer,
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

s32 companionInstallScriptedAnimation(Task* task, s32 unusedMessageId, const AnimationPlayRequest* request, s32 unusedSecondArg)
{
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;

    playerStatus            = &gPlayerStatus;
    savedInteractionPressed = playerStatus->interactionPressed;
    playerActorInstallScriptedAnimation(task, unusedMessageId, request, unusedSecondArg);
    playerStatus->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 companionTurnToYaw(Task* task, s32 unusedMessageId, const ActorTransform* transform, s32 unusedSecondArg)
{
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;

    playerStatus            = &gPlayerStatus;
    savedInteractionPressed = playerStatus->interactionPressed;
    playerActorTurnToYaw(task, unusedMessageId, transform, unusedSecondArg);
    playerStatus->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 companionMoveTo(Task* task, s32 unusedMessageId, const ActorTransform* transform, const GameActorMoveAnim* moveAnim)
{
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;

    playerStatus            = &gPlayerStatus;
    savedInteractionPressed = playerStatus->interactionPressed;
    playerActorMoveTo(task, unusedMessageId, transform, moveAnim);
    playerStatus->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 companionRunTo(Task* task, s32 unusedMessageId, const ActorTransform* transform, const GameActorMoveAnim* moveAnim)
{
    enum { COMPANION_SCRIPTED_RUN_TO_STATE = 8 };
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;
    GameActor*    actor;

    playerStatus            = &gPlayerStatus;
    actor                   = task->work;
    savedInteractionPressed = playerStatus->interactionPressed;
    playerActorMoveTo(task, unusedMessageId, transform, moveAnim);
    playerStatus->interactionPressed = savedInteractionPressed;
    actor->state                     = COMPANION_SCRIPTED_RUN_TO_STATE;
    return 0;
}

s32 companionAwaitButtonPresses(Task* task, s32 unusedMessageId, const GameActorButtonPressHold* request, s32 unusedSecondArg)
{
    enum { COMPANION_SCRIPTED_PRESS_HOLD_STATE = 6 };
    GameActor* actor;

    actor = task->work;
    if ((s8)actor->recoveryTicks != 0) {
        return 1;
    }
    _companionEnterScriptedMode(task, actor);
    actor->state       = COMPANION_SCRIPTED_PRESS_HOLD_STATE;
    actor->stateTimer  = request->pressCount;
    actor->actionValue = 0;
    return 0;
}

s32 companionMoveBy(Task* task, s32 unusedMessageId, const GameActorMoveBy* move, s32 unusedSecondArg)
{
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;
    s32           result;

    playerStatus                     = &gPlayerStatus;
    savedInteractionPressed          = playerStatus->interactionPressed;
    result                           = playerActorMoveBy(task, unusedMessageId, move, unusedSecondArg);
    playerStatus->interactionPressed = savedInteractionPressed;
    return result;
}

s32 animationCopyCompanionBankExtension(Task* unusedTask, s32 unusedMessageId, const AnimationBankCopyRequest* request, s32 unusedSecondArg)
{
    s32*       destinationWords;
    const s32* sourceWords;
    s32        wordIndex;
    s32        wordCount;

    destinationWords = Gp_AnimBlkTbl[Gp_AllyIdBase[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType - 1] + gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionVariant]->table.words;
    sourceWords      = request->source.words;
    wordCount        = request->wordCount;
    if (wordCount >= ANIMATION_BANK_EXTENSION_CAPACITY + 1) {
        return 1;
    }
    // Transfer raw words: the span can include records after the clip pointers.
    destinationWords = &destinationWords[ANIMATION_BANK_BASE_SET_COUNT];
    for (wordIndex = 0; wordIndex < request->wordCount; wordIndex++) {
        destinationWords[wordIndex] = sourceWords[wordIndex];
    }
    return 0;
}

s32 companionApplyDamage(Task* unusedTask, s32 unusedMessageId, s32 attackKey, s32 unusedSecondArg)
{
    enum { COMPANION_DAMAGE_RECEIVER = 1 };
    s32 fatal;

    fatal = 0;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.cheatMode == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp -= damageComputeReceived(attackKey, 0, NULL, COMPANION_DAMAGE_RECEIVER);
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp <= 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, 0, ACTOR_MESSAGE_RELEASE_HOLD);
            fatal = 1;
        }
    }
    return fatal;
}

void worldCollisionBindEnemySphere(GfxCoord* coord, WorldCollisionBody* body, WorldCollisionContact* contacts, s32 contactCount, s32 bodyId, s32 radius)
{
    body->coord            = coord;
    body->context.contacts = contacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    body->key              = bodyId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
    body->radius           = radius;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    // The exported count is narrowed to a signed halfword before clearing.
    worldCollisionInitContacts(body->context.contacts, (s16)contactCount, 0);
}

const TaskFuncTable4 D_80097AB0 = { {
    _modelObjectInitChildTask,
    _modelObjectUpdateChildTask,
    _modelObjectDeferChildTaskRemoval,
    _modelObjectKillChildTask,
} };
