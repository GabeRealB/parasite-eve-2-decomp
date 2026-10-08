#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Appears beside the player as a feint or a two-part strike.
///
/// Requires a target previously placed and collision-tested by the idle sequence.
/// A feint takes no weapon damage and shrinks away, or triggers an immediate
/// counterattack when broken. A real appearance can be interrupted before the
/// blow. The strike sphere is live on part 8 at frame 20, part 12 at frame 28,
/// and disabled at frame 35 as vanishing begins. Fade durations are positive
/// frame divisors, split from the sampled appearance length.
static void _golemKnightBishopStrikeSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_STRIKE_FIRST_FRAME  = 20,
        GOLEM_KNIGHT_BISHOP_STRIKE_SECOND_FRAME = 28,
        GOLEM_KNIGHT_BISHOP_STRIKE_END_FRAME    = 35,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_STRIKE_START        = 0,
        GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_APPEAR = 1,
        GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_WAIT   = 2,
        GOLEM_KNIGHT_BISHOP_STRIKE_APPEAR       = 3,
        GOLEM_KNIGHT_BISHOP_STRIKE_WAIT         = 4,
        GOLEM_KNIGHT_BISHOP_STRIKE_BLOW         = 5,
        GOLEM_KNIGHT_BISHOP_STRIKE_VANISH       = 6,
    };
    GolemKnightBishopOffsetScratch* scratch;
    GolemKnightBishopWork*          work;
    GfxCoord*                       root;
    s32                             strikeSound;
    u32                             randomDraw;
    u16                             appearanceFrames;
    s16                             translucencyFrames;
    s16                             framesLeft;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopOffsetScratch);
    work    = task->work;
    root    = task->extra.tmd->coords;
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_STRIKE_START:
            work->anim          = GOLEM_KNIGHT_BISHOP_ANIM_STAND;
            work->feinting      = gGolemKnightBishopApproachRoll[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            scratch->operand.vx = 0;
            scratch->operand.vy = (work->targetYaw + GOLEM_KNIGHT_BISHOP_YAW_HALF_TURN) & GOLEM_KNIGHT_BISHOP_YAW_MASK;
            scratch->operand.vz = 0;
            RotMatrix(&scratch->operand, &root->coord);
            root->coord.t[0] = work->targetPos.vx;
            root->coord.t[1] = work->targetPos.vy;
            root->coord.t[2] = work->targetPos.vz;
            if (work->feinting == 1 && work->feintBroken == 0) {
                randomDraw                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                appearanceFrames             = ((randomDraw >> 16) & 0xF) + 0xF;
                work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_APPEAR;
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR;
                gRandomLcgState              = randomDraw;
                work->timer                  = appearanceFrames;
                translucencyFrames           = appearanceFrames * 2 / 3;
                work->translucencyFadeFrames = translucencyFrames;
                work->colorBlendFadeFrames   = appearanceFrames - translucencyFrames;
            } else {
                work->feinting = 0;
                if (work->feintBroken == 0) {
                    framesLeft                   = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) + 0x1E;
                    work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_APPEAR;
                    work->timer                  = framesLeft;
                    translucencyFrames           = framesLeft * 2 / 3;
                    work->translucencyFadeFrames = translucencyFrames;
                    work->colorBlendFadeFrames   = work->timer - translucencyFrames;
                } else {
                    work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_WAIT;
                    work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                    work->timer                  = 0;
                    work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                }
                work->fadeState   = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
                work->appearSound = gGolemKnightBishopApproachCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                work->interruptDamage = 0;
                work->reactionLock    = GOLEM_KNIGHT_BISHOP_REACTION_ATTACKING;
            }
            work->auxTimer    = 1;
            work->feintBroken = 0;
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_APPEAR:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.key    = 0;
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                work->auxTimer = 0;
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0 || work->feintBroken != 0) {
                work->step      = GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_WAIT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = ((gRandomLcgState >> 16) & 0xF) + 0x3C;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_WAIT:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0 || work->feintBroken != 0) {
                work->step      = GOLEM_KNIGHT_BISHOP_STRIKE_VANISH;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHRINK;
                if (work->feintBroken != 0) {
                    work->translucencyFadeFrames = 5;
                    work->colorBlendFadeFrames   = 3;
                    work->timer                  = work->translucencyFadeFrames + work->colorBlendFadeFrames;
                } else {
                    work->translucencyFadeFrames = 8;
                    work->colorBlendFadeFrames   = 8;
                    work->timer                  = work->translucencyFadeFrames + work->colorBlendFadeFrames;
                }
                worldTargetDisableNodeLockOn(&((Enemy*)task->spawnArg2.pointer)->node);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_APPEAR:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
                }
                work->auxTimer = 0;
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->step      = GOLEM_KNIGHT_BISHOP_STRIKE_WAIT;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & 0xF;
            } else if (work->interruptDamage > 0) {
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step      = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                if (work->appearSound != 0) {
                    sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    work->appearSound = 0;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_WAIT:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
                }
                work->auxTimer = 0;
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->anim  = GOLEM_KNIGHT_BISHOP_ANIM_STRIKE;
                work->step  = GOLEM_KNIGHT_BISHOP_STRIKE_BLOW;
                work->timer = 0;
            } else if (work->interruptDamage > 0) {
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step      = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_BLOW:
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_STRIKE_FIRST_FRAME) {
                work->strikeBody.coord  = &task->extra.tmd->coords[8];
                work->strikeBody.pos.vx = 0;
                work->strikeBody.pos.vy = 0;
                work->strikeBody.pos.vz = 0;
                work->strikeBody.radius = 0x12C;
                work->strikeBody.key    = damagePackAttackKey(gGolemKnightBishopAttacks, 1);
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                strikeSound             = gGolemKnightBishopStrikeCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(strikeSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            } else if (work->animFrame == GOLEM_KNIGHT_BISHOP_STRIKE_SECOND_FRAME) {
                work->strikeBody.coord = &task->extra.tmd->coords[12];
                strikeSound            = gGolemKnightBishopStrikeCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(strikeSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_STRIKE_END_FRAME) {
                work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_VANISH;
                work->timer                  = 0x1E;
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                work->strikeBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->vanishSound            = gGolemKnightBishopPainCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_VANISH:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step         = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->reactionLock = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopOffsetScratch);
}
