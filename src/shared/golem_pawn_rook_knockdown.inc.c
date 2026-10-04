#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xB of `Actor05600_D16540`: state 0 picks the pose from `hitFromFront`
/// and moves the grid collision from `groundBody` to `hurtBody`; states 1 and 2 play cues at fixed frames and,
/// once their clip is done, either roll an idle length or, with the enemy's
/// hit points gone, advance the task to state 2; states 3 and 4 alternate
/// the two idle clips until each length runs out.
void golemPawnRookKnockdownState(Task* arg0)
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
    state = work->step;
    switch (state) {
        case 0:
            if (work->hitFromFront == 0) {
                work->anim            = 0x16;
                work->step            = 1;
                work->downedPose      = 1;
                work->hurtBody.pos.vz = -0xA7;
            } else {
                work->anim            = 0x1A;
                work->step            = 2;
                work->downedPose      = 2;
                work->hurtBody.pos.vz = 0x109;
            }
            work->hurtBody.radius                            = 0x15E;
            work->forwardSpeed                               = 0;
            work->turnRate                                   = 0;
            work->knockdownStage                             = 1;
            work->hurtBody.flags                             = (u16)(work->hurtBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
            work->groundBody.flags                           = (u16)(work->groundBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED));
            ((Enemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->fallingDown                                = 1;
            break;
        case 1:
            if (work->animFrame == 0x14) {
                snd = gGolemPawnRookVoiceCues[work->soundSet + 0xC] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(self);
                sndEvtRequestScriptStart(snd, (s32)pan, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->animFrame == 0x2C) {
                snd  = gGolemPawnRookVoiceCues[work->soundSet + 8] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan2 = (s8)worldCoordGetOriginAudioPan(self);
                sndEvtRequestScriptStart(snd, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->animFrame >= 0x42) {
                work->anim        = 0x19;
                work->fallingDown = 0;
                random            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->timer       = (u16)((random >> 0x10) & 0x3F);
                gRandomLcgState   = random;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
                    work->step = 3;
                } else {
                    arg0->state = 2;
                    work->step  = 0;
                }
            }
            if (work->knockdownStage == 1) {
                work->knockdownStage = 2;
                break;
            }
            break;
        case 2:
            if (work->animFrame == 0x19) {
                snd  = gGolemPawnRookVoiceCues[work->soundSet + 8] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan3 = (s8)worldCoordGetOriginAudioPan(self);
                sndEvtRequestScriptStart(snd, (s32)pan3, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->animFrame >= 0x31) {
                work->anim        = 0x1D;
                work->fallingDown = 0;
                random2           = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->timer       = (u16)((random2 >> 0x10) & 0x3F);
                gRandomLcgState   = random2;
                if (((Enemy*)arg0->spawnArg2.pointer)->hp > 0) {
                    work->step = 3;
                } else {
                    arg0->state = 2;
                    work->step  = 0;
                }
            }
            if (work->knockdownStage == 1) {
                work->knockdownStage = 2;
            }
            break;
        case 3:
            timer       = work->timer - 1;
            work->timer = timer;
            if ((s16)timer <= 0) {
                nextAnim = 0x1C;
                if (work->downedPose == 1) {
                    nextAnim = 0x18;
                }
                work->timer = 0xAU;
                work->anim  = nextAnim;
                work->step  = 4;
                break;
            }
            break;
        case 4:
            timer2      = work->timer - 1;
            work->timer = timer2;
            if ((s16)timer2 <= 0) {
                nextAnim2 = 0x1D;
                if (work->downedPose == 1) {
                    nextAnim2 = 0x19;
                }
                work->anim      = nextAnim2;
                work->step      = 3;
                random3         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random3;
                work->timer     = (u16)(((u32)random3 >> 0x10) & 0x3F);
            }
            break;
    }
}
