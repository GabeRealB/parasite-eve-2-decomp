#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Four-step burst sequence driven by `step`: 0 spawns the effect and cue
/// on entry, rumbles every tenth frame and either ends after `interruptDamage` passes
/// 0x28 or times out at 0x96 frames; 1 counts `timer` down into 2; 2
/// triggers the PE state and plays the second cue; 3 spawns random-offset
/// sparks every fourth frame until `timer` runs out.
void golemPawnRookSilenceScreamState(Task* arg0)
{
    GolemPawnRookWork* work;
    GfxCoord*          self;
    SVECTOR*           scratch;
    s32                sound;
    u32                random;

    scratch = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    work    = arg0->work;
    self    = arg0->extra.tmd->coords;
    switch (work->step) {
        case 0:
            if (work->timer == 0) {
                scratch->vx        = 0;
                scratch->vy        = 0;
                scratch->vz        = 0;
                work->screamEffect = effectSpawn(gRoomEffectFlashId, &arg0->extra.tmd->coords[4], 0x96, scratch);
                sound              = gGolemPawnRookScreamCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
            }
            work->shieldRaised = work->shieldHp > 0;
            if ((s16)(work->timer % 10) == 0) {
                padScriptSpawnVariableMotorRamp(5, 0x80, 8);
            }
            if (work->interruptDamage >= 0x29) {
                work->screamActive = 0;
                if (--work->screamCharges <= 0) {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL;
                    work->step     = 0;
                } else {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SCREAM;
                    work->step     = 3;
                    work->anim     = 0x15;
                    work->timer    = 0x4F;
                }
                work->shieldRaised = 0;
                if (work->screamEffect != NULL) {
                    work->screamEffect->task->state = 3;
                }
                work->screamEffect = NULL;
            } else if (++work->timer >= 0x96) {
                work->timer        = 8;
                work->step         = 1;
                work->anim         = 0xB;
                work->screamActive = 0;
                work->shieldRaised = 0;
                work->screamEffect = NULL;
                padScriptSpawnVariableMotorRamp(0xF, 0xFF, 8);
            }
            break;
        case 1:
            if (--work->timer <= 0) {
                work->step  = 2;
                work->timer = 0;
            }
            break;
        case 2:
            Gp_TriggerPeState(0, PLAYER_STATUS_SILENCE);
            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
            work->step     = 0;
            work->anim     = 2;
            sound          = gGolemPawnRookSilenceCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(self), (s8)worldCoordGetOriginAudioDepth(self));
            break;
        case 3:
            if (!(work->animFrame & 3)) {
                scratch->vx     = 0;
                scratch->vz     = 0;
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                scratch->vy     = -((random >> 16) & 0x1FF);
                gRandomLcgState = random;
                effectSpawn(EFFECT_FLASH_BURST, &arg0->extra.tmd->coords[3], 0x100, scratch);
            }
            if (--work->timer <= 0) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = 0;
                work->anim     = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
