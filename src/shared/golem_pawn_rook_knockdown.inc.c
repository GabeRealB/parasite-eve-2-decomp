#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xB of `Actor05600_D16540`: state 0 picks the pose from `field_6AA`
/// and arms the body objects; states 1 and 2 play cues at fixed frames and,
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
                snd = gGolemPawnRookVoiceCues[work->field_6D6 + 0xC] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, (s32)pan, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 == 0x2C) {
                snd  = gGolemPawnRookVoiceCues[work->field_6D6 + 8] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
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
                snd  = gGolemPawnRookVoiceCues[work->field_6D6 + 8] | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
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
