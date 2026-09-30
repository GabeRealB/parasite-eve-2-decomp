#include "main/random.h"

/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Four-step burst sequence driven by `field_6A8`: 0 spawns the effect and cue
/// on entry, rumbles every tenth frame and either ends after `field_6B6` passes
/// 0x28 or times out at 0x96 frames; 1 counts `field_6AE` down into 2; 2
/// triggers the PE state and plays the second cue; 3 spawns random-offset
/// sparks every fourth frame until `field_6AE` runs out.
void lungerSilenceScreamState(Task* arg0)
{
    Actor105600Work* work;
    GfxCoord*        self;
    SVECTOR*         scratch;
    s32              sound;
    u32              random;

    scratch = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    work    = arg0->work;
    self    = arg0->extra.tmd->coords;
    switch (work->field_6A8) {
        case 0:
            if (work->field_6AE == 0) {
                scratch->vx     = 0;
                scratch->vy     = 0;
                scratch->vz     = 0;
                work->field_690 = Gp_SpawnEff(D_80115758, &arg0->extra.tmd->coords[4], 0x96, scratch);
                sound           = gLungerScreamCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            }
            work->field_6CE = work->field_6D0 > 0;
            if ((s16)(work->field_6AE % 10) == 0) {
                Gp_SpawnPadLerp(5, 0x80, 8);
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6C2 = 0;
                if (--work->field_6C4 <= 0) {
                    work->field_6A6 = 9;
                    work->field_6A8 = 0;
                } else {
                    work->field_6A6 = 5;
                    work->field_6A8 = 3;
                    work->field_694 = 0x15;
                    work->field_6AE = 0x4F;
                }
                work->field_6CE = 0;
                if (work->field_690 != NULL) {
                    work->field_690->task->state = 3;
                }
                work->field_690 = NULL;
            } else if (++work->field_6AE >= 0x96) {
                work->field_6AE = 8;
                work->field_6A8 = 1;
                work->field_694 = 0xB;
                work->field_6C2 = 0;
                work->field_6CE = 0;
                work->field_690 = NULL;
                Gp_SpawnPadLerp(0xF, 0xFF, 8);
            }
            break;
        case 1:
            if (--work->field_6AE <= 0) {
                work->field_6A8 = 2;
                work->field_6AE = 0;
            }
            break;
        case 2:
            Gp_TriggerPeState(0, PLAYER_STATUS_SILENCE);
            work->field_6A6 = 2;
            work->field_6A8 = 0;
            work->field_694 = 2;
            sound           = gLungerSilenceCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(self), (s8)gpGetObjDepth(self));
            break;
        case 3:
            if (!(work->field_698 & 3)) {
                scratch->vx     = 0;
                scratch->vz     = 0;
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                scratch->vy     = -((random >> 16) & 0x1FF);
                gRandomLcgState = random;
                Gp_SpawnEff(0x600E0, &arg0->extra.tmd->coords[3], 0x100, scratch);
            }
            if (--work->field_6AE <= 0) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
