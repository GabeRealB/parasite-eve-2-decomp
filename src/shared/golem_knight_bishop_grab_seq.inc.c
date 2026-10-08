#include "gameplay/room_effects.h"

/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block of the grab sequence.
///
/// It holds the payloads of the messages the grab sends the player, each
/// borrowed for one synchronous dispatch, and the vectors the hold's placement
/// is worked out in. Reserved for the length of the call.
typedef struct {
    GameActorButtonPressHold buttonPressHold; // payload of the request that takes the player into the hold; only `pressCount` is set
    AnimationPlayRequest     playerAnim;      // animation the player is told to play at each stage of the hold
    ActorTransform           playerPlacement; // where the player is put as the hold is taken: 0x5AA in front of the golem, turned the way it faces
    VECTOR                   offset;          // that distance along the world axes: `operand` after the GTE rotation through the golem's root
    SVECTOR                  operand;         // short vector being worked on: the Euler angles the root's rotation is built from, the local offset to rotate, or the kill effect's offset from the player's part 4
} _GolemKnightBishopGrabScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopGrabScratch, 0x5C);

/// Replaces the player's animation bank and resets playback to one grab clip.
///
/// `clipId` is 1 start, 2 hold, 3 kill or 4 release in the carrier's five-entry
/// player bank. `request` is writable storage borrowed only through synchronous
/// dispatch; the bank and clip data must remain loaded through playback.
/// Requests grid collision enablement and resets without interpolation.
///
/// `player` is a live `Task*`, `request` an `AnimationPlayRequest*` and `clipId`
/// an integer. Use as a standalone statement inside a braced block. The request
/// expression is evaluated six times and must have no side effects; player and
/// clip are evaluated once. Borrows `gGolemKnightBishopPlayerAnims` from the
/// carrier and captures no caller locals.
#define GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, request, clipId)                    \
    {                                                                                              \
        (request)->source.sets          = gGolemKnightBishopPlayerAnims;                           \
        (request)->animationId          = (clipId);                                                \
        (request)->blend                = ANIMATION_BLEND_RESET;                                   \
        (request)->blendFrames          = 0;                                                       \
        (request)->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                        \
        TASK_MESSAGE_DISPATCH_POINTER((player), ANIMATION_MESSAGE_REPLACE_AND_PLAY, (request), 0); \
    }

/// Takes scripted control of the player for a damaging hold or fatal grab.
///
/// The live player must accept the button-press hold before either actor is placed.
/// Message payloads borrow this call's scratch storage only for synchronous
/// dispatch. After the initial grace period, per-kind checks apply damage or
/// select a kill from the difficulty row (0..4), HP and accumulated damage ticks. The first
/// above-limit check arms the kill roll without rolling. Struggle or weapon
/// damage can break the hold; release playback finishes through
/// `_golemKnightBishopTickGrabRelease`. A fatal grab loads and queues the player's
/// death sound after the killing animation frame.
static void _golemKnightBishopGrabSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_START   = 1,
        GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_HOLD    = 2,
        GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_KILL    = 3,
        GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_RELEASE = 4,
        GOLEM_KNIGHT_BISHOP_DEATH_SOUND_LOAD    = 0,
        GOLEM_KNIGHT_BISHOP_DEATH_SOUND_WAIT    = 1,
        GOLEM_KNIGHT_BISHOP_DEATH_SOUND_DONE    = 2,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_GRAB_KILL_FRAME              = 26,
        GOLEM_KNIGHT_BISHOP_GRAB_ESCAPE_PRESSES          = 25,
        GOLEM_KNIGHT_BISHOP_PLAYER_SCRIPTED_ATTACK_STATE = 10,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_PLAYER_ENTER_SCRIPTED_ATTACK = 1024,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_GRAB_TAKE        = 0,
        GOLEM_KNIGHT_BISHOP_GRAB_PLAY_START  = 1,
        GOLEM_KNIGHT_BISHOP_GRAB_WAIT_START  = 2,
        GOLEM_KNIGHT_BISHOP_GRAB_HOLD        = 3,
        GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY   = 4,
        GOLEM_KNIGHT_BISHOP_GRAB_KILL        = 5,
        GOLEM_KNIGHT_BISHOP_GRAB_EARLY_BREAK = 6,
        GOLEM_KNIGHT_BISHOP_GRAB_DEATH_SOUND = 7,
    };
    GolemKnightBishopWork*         work;
    GfxCoord*                      root;
    Task*                          player;
    _GolemKnightBishopGrabScratch* scratch;
    GfxCoord*                      playerRoot;
    s32                            killSelected;
    s32                            sound;
    s32                            killChance;
    s16                            framesLeft;
    s16                            retreatSpeed;
    s16                            deathSoundStep;

    work         = task->work;
    root         = task->extra.tmd->coords;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopGrabScratch);
    playerRoot   = player->extra.tmd->coords;
    killSelected = 0;
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_GRAB_TAKE:
            // Take control before moving either actor; refusal leaves placement intact.
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED && gPlayerStatus.hp > 0) {
                scratch->buttonPressHold.pressCount = GOLEM_KNIGHT_BISHOP_GRAB_ESCAPE_PRESSES;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) == 0) {
                    work->anim          = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_START;
                    work->step          = GOLEM_KNIGHT_BISHOP_GRAB_PLAY_START;
                    work->grabBreak     = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE;
                    work->feintBroken   = 0;
                    work->grabStage     = GOLEM_KNIGHT_BISHOP_GRAB_HOLDING;
                    scratch->operand.vx = 0;
                    scratch->operand.vy = work->targetYaw;
                    scratch->operand.vz = 0;
                    RotMatrix(&scratch->operand, &root->coord);
                    root->coord.t[0]    = work->targetPos.vx;
                    root->coord.t[1]    = work->targetPos.vy;
                    root->coord.t[2]    = work->targetPos.vz;
                    scratch->operand.vx = 0;
                    scratch->operand.vy = 0;
                    scratch->operand.vz = GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
                    gte_SetRotMatrix(&root->coord);
                    gte_ldv0(&scratch->operand);
                    gte_rtv0();
                    gte_stlvnl(&scratch->offset);
                    scratch->playerPlacement.pos.vx = root->coord.t[0] + scratch->offset.vx;
                    scratch->playerPlacement.pos.vy = root->coord.t[1] + scratch->offset.vy;
                    scratch->playerPlacement.pos.vz = root->coord.t[2] + scratch->offset.vz;
                    scratch->playerPlacement.rot.vx = 0;
                    scratch->playerPlacement.rot.vy = work->targetYaw;
                    scratch->playerPlacement.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
                    padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x80);
                    sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                    sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(playerRoot), (s8)worldCoordGetOriginAudioDepth(playerRoot));
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                    work->step     = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_PLAY_START:
            GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_START);
            work->step                   = GOLEM_KNIGHT_BISHOP_GRAB_WAIT_START;
            work->translucencyFadeFrames = 0x3C;
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->colorBlendFadeFrames   = 0x1E;
            work->appearSound            = gGolemKnightBishopApproachCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_WAIT_START:
            if (work->animFrame >= 0x29) {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_HOLD;
                work->step            = GOLEM_KNIGHT_BISHOP_GRAB_HOLD;
                work->auxTimer        = 0x1E;
                work->timer           = 0;
                work->grabDamageTicks = 0;
                GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_HOLD);
                sceneEngageBattle(1);
                work->interruptDamage = 0;
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_HOLD:
            // Delay the first damage check, then roll a kill only after arming it.
            if (work->auxTimer > 0) {
                work->auxTimer--;
            } else {
                if (work->timer == 2) {
                    padScriptSpawnVariableMotorRamp(5, 0xC0, 0x80);
                }
                framesLeft  = work->timer - 1;
                work->timer = framesLeft;
                if (framesLeft <= 0) {
                    if (gPlayerStatus.hp > gGolemKnightBishopGrabHpLimits[gSceneCombatState.difficulty]) {
                        if (work->grabKillRollArmed == 0) {
                            work->grabKillRollArmed = 1;
                        } else {
                            killChance = work->grabDamageTicks * (0x32 - (gPlayerStatus.hp * 100) / gPlayerStatus.hpMax) / 2;
                            if (killChance > 0) {
                                killChance      = (killChance * 0xFFF) / 100;
                                gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                                if ((s32)((gRandomLcgState >> 16) & 0xFFF) < killChance) {
                                    killSelected = 1;
                                }
                            }
                        }
                    } else {
                        killSelected = 1;
                    }
                    if (killSelected != 0) {
                        work->anim = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_KILL;
                        work->step = GOLEM_KNIGHT_BISHOP_GRAB_KILL;
                        GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_KILL);
                    } else {
                        work->timer = GOLEM_KNIGHT_BISHOP_GRAB_RECHECK;
                        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(gGolemKnightBishopAttacks, 0), 0);
                        work->grabDamageTicks++;
                    }
                }
            }
            if (work->grabBreak != GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE) {
                if (work->grabBreak == GOLEM_KNIGHT_BISHOP_GRAB_BREAK_STRUGGLE) {
                    if (work->auxTimer > 0 && work->counterattacking == 0) {
                        work->anim             = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_EARLY_RELEASE;
                        work->step             = GOLEM_KNIGHT_BISHOP_GRAB_EARLY_BREAK;
                        work->grabReleaseTimer = 0;
                        if (work->appearSound != 0) {
                            sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                            work->appearSound = 0;
                        }
                        work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                    } else {
                        work->anim                   = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE;
                        work->step                   = GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY;
                        work->timer                  = 0x69;
                        work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                        work->translucencyFadeFrames = 0x4B;
                        work->grabReleaseTimer       = 0;
                        work->colorBlendFadeFrames   = 0x1E;
                        work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                        sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                    }
                } else {
                    _golemKnightBishopPickHitReaction(task, work->interruptDamage);
                    work->grabReleaseTimer = 0;
                }
                work->grabStage = GOLEM_KNIGHT_BISHOP_GRAB_RELEASED;
                work->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE;
                GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_RELEASE);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY:
            retreatSpeed = 0;
            if (work->animFrame < 0x5F) {
                retreatSpeed = -0xA;
            }
            work->forwardSpeed = retreatSpeed;
            framesLeft         = work->timer - 1;
            work->timer        = framesLeft;
            if (framesLeft <= 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step     = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            _golemKnightBishopTickGrabRelease(task);
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_KILL:
            // The killing frame commits death; earlier interruptions still release.
            if (work->animFrame < GOLEM_KNIGHT_BISHOP_GRAB_KILL_FRAME) {
                if (work->grabBreak != GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE) {
                    work->anim      = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE;
                    work->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE;
                    GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_RELEASE);
                    work->timer                  = 0x69;
                    work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                    work->translucencyFadeFrames = 0x4B;
                    work->step                   = GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY;
                    work->colorBlendFadeFrames   = 0x1E;
                    work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                }
            } else if (work->animFrame == GOLEM_KNIGHT_BISHOP_GRAB_KILL_FRAME) {
                ((GameActor*)player->work)->state = GOLEM_KNIGHT_BISHOP_PLAYER_SCRIPTED_ATTACK_STATE;
                work->step                        = GOLEM_KNIGHT_BISHOP_GRAB_DEATH_SOUND;
                work->timer                       = 0;
                gGameSession->deathRestartDelay   = 0x5A;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                scratch->operand.vy               = -0x96;
                scratch->operand.vx               = 0;
                scratch->operand.vz               = 0xC8;
                effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[4], &scratch->operand, &gGolemKnightBishopGrabEffect);
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 8);
                taskMessageDispatch(player, GOLEM_KNIGHT_BISHOP_PLAYER_ENTER_SCRIPTED_ATTACK, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_EARLY_BREAK:
            work->forwardSpeed = -0xA;
            _golemKnightBishopTickGrabRelease(task);
            if (work->grabStage == GOLEM_KNIGHT_BISHOP_GRAB_NONE) {
                work->forwardSpeed = 0;
                work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step         = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_DEATH_SOUND:
            deathSoundStep = work->timer;
            switch (deathSoundStep) {
                case GOLEM_KNIGHT_BISHOP_DEATH_SOUND_LOAD:
                    cdCmdEnqueueDisplayResource(9, 0x1E, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                    work->timer = GOLEM_KNIGHT_BISHOP_DEATH_SOUND_WAIT;
                    break;
                case GOLEM_KNIGHT_BISHOP_DEATH_SOUND_WAIT:
                    if (cdCmdIsIdle() == 1) {
                        // Use the player's root for the death cue.
                        root = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                        sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                        work->timer = GOLEM_KNIGHT_BISHOP_DEATH_SOUND_DONE;
                    }
                    break;
                case GOLEM_KNIGHT_BISHOP_DEATH_SOUND_DONE:
                    break;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopGrabScratch);
}

#undef GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION
