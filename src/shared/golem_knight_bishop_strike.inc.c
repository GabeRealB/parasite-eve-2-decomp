#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the appearance by the player that is either a feint or a strike.
/// Step 0 plants the display object at `targetPos` facing back along
/// `targetYaw` and rolls `feinting`. A feint (unless `feintBroken` is set)
/// goes to step 1 with a 0xF..0x1E frame appearance; otherwise the real
/// appearance starts with its sound and `reactionLock`, going to step 3 with
/// a 0x1E..0x2D frame one, or to step 4 at once when `feintBroken` is set.
/// The length is split into `translucencyFadeFrames` (two thirds) and
/// `colorBlendFadeFrames` (the rest). Steps 1, 3 and 4 arm `hurtBody` on
/// their first frame, as `auxTimer` asks - with no key for a feint. Steps 1
/// and 2 hold the feint for `timer`, cut short by `feintBroken`, and step 2
/// then starts its shrink into step 6. Steps 3 and 4 count `timer` down to
/// the blow and break off into the recover sequence while `interruptDamage`
/// is positive. Step 5 is the blow: `strikeBody` goes live on part 8 at
/// frame 0x14 and moves to part 12 at 0x1C, each with the strike sound, and
/// frame 0x23 switches it off and starts the vanish. Step 6 counts `timer`
/// down to the idle sequence.
void golemKnightBishopStrikeSeq(Task* arg0)
{
    GolemKnightBishopOffsetScratch* sc;
    GolemKnightBishopWork*          work;
    GfxCoord*                       coord;
    s32                             cue;
    u32                             random;
    u16                             delay;
    s16                             part;
    s16                             timer;

    SCRATCH_STACK_RESERVE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
    sc    = SCRATCH_STACK_CURSOR(GolemKnightBishopOffsetScratch);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->step) {
        case 0:
            work->anim     = 4;
            work->feinting = gGolemKnightBishopApproachRoll[((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF];
            sc->operand.vx = 0;
            sc->operand.vy = (work->targetYaw + 0x800) & 0xFFF;
            sc->operand.vz = 0;
            RotMatrix(&sc->operand, &coord->coord);
            coord->coord.t[0] = work->targetPos.vx;
            coord->coord.t[1] = work->targetPos.vy;
            coord->coord.t[2] = work->targetPos.vz;
            if (work->feinting == 1 && work->feintBroken == 0) {
                random                       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                delay                        = ((random >> 16) & 0xF) + 0xF;
                work->step                   = 1;
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR;
                gRandomLcgState              = random;
                work->timer                  = delay;
                part                         = delay * 2 / 3;
                work->translucencyFadeFrames = part;
                work->colorBlendFadeFrames   = delay - part;
            } else {
                work->feinting = 0;
                if (work->feintBroken == 0) {
                    timer                        = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) + 0x1E;
                    work->step                   = 3;
                    work->timer                  = timer;
                    part                         = timer * 2 / 3;
                    work->translucencyFadeFrames = part;
                    work->colorBlendFadeFrames   = work->timer - part;
                } else {
                    work->step                   = 4;
                    work->translucencyFadeFrames = 0x14;
                    work->timer                  = 0;
                    work->colorBlendFadeFrames   = 0xA;
                }
                work->fadeState   = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
                work->appearSound = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                work->interruptDamage = 0;
                work->reactionLock    = 1;
            }
            work->auxTimer    = 1;
            work->feintBroken = 0;
            break;
        case 1:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.key    = 0;
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                work->auxTimer = 0;
            }
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0 || work->feintBroken != 0) {
                work->step      = 2;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = ((gRandomLcgState >> 16) & 0xF) + 0x3C;
            }
            break;
        case 2:
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0 || work->feintBroken != 0) {
                work->step      = 6;
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
                worldTargetDisableNodeLockOn(&((Enemy*)arg0->spawnArg2.pointer)->node);
            }
            break;
        case 3:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | 0x30000;
                }
                work->auxTimer = 0;
            }
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                work->step      = 4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & 0xF;
            } else if (work->interruptDamage > 0) {
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step      = 0;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                if (work->appearSound != 0) {
                    sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    work->appearSound = 0;
                }
            }
            break;
        case 4:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | 0x30000;
                }
                work->auxTimer = 0;
            }
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                work->anim  = 5;
                work->step  = 5;
                work->timer = 0;
            } else if (work->interruptDamage > 0) {
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step      = 0;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
            }
            break;
        case 5:
            if (work->animFrame == 0x14) {
                work->strikeBody.coord  = &arg0->extra.tmd->coords[8];
                work->strikeBody.pos.vx = 0;
                work->strikeBody.pos.vy = 0;
                work->strikeBody.pos.vz = 0;
                work->strikeBody.radius = 0x12C;
                work->strikeBody.key    = Gp_PackPair(gGolemKnightBishopAttacks, 1);
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                cue                     = gGolemKnightBishopStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(cue, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (work->animFrame == 0x1C) {
                work->strikeBody.coord = &arg0->extra.tmd->coords[12];
                cue                    = gGolemKnightBishopStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(cue, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame == 0x23) {
                work->step                   = 6;
                work->timer                  = 0x1E;
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x14;
                work->colorBlendFadeFrames   = 0xA;
                work->strikeBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->vanishSound            = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 6:
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step         = 0;
                work->reactionLock = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}
