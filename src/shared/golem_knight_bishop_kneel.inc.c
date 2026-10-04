#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the fall at low hit points and the writhing after it. Step 0 takes
/// the fall animation 0xD or 0x11 by `hitFromFront`, files the matching
/// `downedPose`, starts the flicker, sets `reactionLock` for the fall and
/// moves the grid test from `groundBody` to the shifted `hurtBody`. Steps 1
/// and 2 queue the fall sound at frame 0x2C / 0x19 and, once `animFrame`
/// reaches 0x42 / 0x31, release `reactionLock` and go to step 3 with an
/// LCG-rolled `timer`; steps 3 and 4 then alternate between the lying and the
/// writhing animation on that countdown.
void golemKnightBishopKneelSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    snd;
    s32                    anim;
    u32                    random;
    s16                    timer;

    work  = arg0->work;
    state = work->step;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->hitFromFront == 0) {
                work->anim            = 0xD;
                work->step            = 1;
                work->downedPose      = 1;
                work->hurtBody.pos.vz = -0xA7;
            } else {
                work->anim            = 0x11;
                work->step            = 2;
                work->downedPose      = 2;
                work->hurtBody.pos.vz = 0x109;
            }
            work->hurtBody.radius   = 0x15E;
            work->knockdownStage    = 1;
            work->fadeState         = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
            work->reactionLock      = 2;
            work->forwardSpeed      = 0;
            work->hurtBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case 1:
            if (work->animFrame == 0x2C) {
                snd = gGolemKnightBishopAnimCues[work->soundSet + 8] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x42) {
                work->anim         = 0x10;
                work->step         = 3;
                work->reactionLock = 0;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = random;
                work->timer        = (random >> 16) & 0x3F;
            }
            if (work->knockdownStage == 1) {
                work->knockdownStage = 2;
            }
            break;
        case 2:
            if (work->animFrame == 0x19) {
                snd = gGolemKnightBishopAnimCues[work->soundSet + 8] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x31) {
                work->anim         = 0x14;
                work->step         = 3;
                work->reactionLock = 0;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState    = random;
                work->timer        = (random >> 16) & 0x3F;
            }
            if (work->knockdownStage == 1) {
                work->knockdownStage = 2;
            }
            break;
        case 3:
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                anim = 0x13;
                if (work->downedPose == 1) {
                    anim = 0xF;
                }
                work->timer = 0xA;
                work->anim  = anim;
                work->step  = 4;
            }
            break;
        case 4:
            timer       = work->timer - 1;
            work->timer = timer;
            if (timer <= 0) {
                anim = 0x14;
                if (work->downedPose == 1) {
                    anim = 0x10;
                }
                work->anim      = anim;
                work->step      = 3;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                work->timer     = (random >> 16) & 0x3F;
            }
            break;
    }
}
