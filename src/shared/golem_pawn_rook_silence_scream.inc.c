#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Charges the Rook's Silence scream, applies the status, or recovers from a break.
///
/// actor must have a live GOLEM body and Enemy record. Step 0 charges for 150
/// calls with shield cover, a room flash and periodic rumble. Taking at least
/// 41 HP of interrupt damage consumes one scream charge, ending the flash and
/// entering recoil or 79 calls of recovery sparks. A completed charge waits
/// eight calls, applies Silence to the player, and resumes engagement. Its
/// eight-byte local effect offset is released before every return.
static void _golemPawnRookSilenceScreamState(Task* actor)
{

    enum {
        GOLEM_PAWN_ROOK_SCREAM_CHARGE                = 0,
        GOLEM_PAWN_ROOK_SCREAM_RELEASE_DELAY         = 1,
        GOLEM_PAWN_ROOK_SCREAM_APPLY_SILENCE         = 2,
        GOLEM_PAWN_ROOK_SCREAM_RECOVER               = 3,
        GOLEM_PAWN_ROOK_SCREAM_CHARGE_FRAMES         = 150,
        GOLEM_PAWN_ROOK_SCREAM_RELEASE_FRAMES        = 8,
        GOLEM_PAWN_ROOK_SCREAM_RECOVERY_FRAMES       = 79,
        GOLEM_PAWN_ROOK_SCREAM_INTERRUPT_HP          = 41,
        GOLEM_PAWN_ROOK_SCREAM_RELEASE_ANIM          = 11,
        GOLEM_PAWN_ROOK_SCREAM_RECOVERY_ANIM         = 21,
        GOLEM_PAWN_ROOK_SCREAM_RUMBLE_INTERVAL       = 10,
        GOLEM_PAWN_ROOK_SCREAM_CHARGE_RUMBLE_FRAMES  = 5,
        GOLEM_PAWN_ROOK_SCREAM_RELEASE_RUMBLE_FRAMES = 15,
        GOLEM_PAWN_ROOK_SCREAM_CHARGE_RUMBLE_POWER   = 128,
        GOLEM_PAWN_ROOK_SCREAM_RELEASE_RUMBLE_POWER  = 255,
        GOLEM_PAWN_ROOK_SCREAM_RUMBLE_END_INTENSITY  = 8,
        GOLEM_PAWN_ROOK_SCREAM_EFFECT_END_STATE      = 3,
        GOLEM_PAWN_ROOK_SCREAM_SPARK_FRAME_MASK      = 3,
        GOLEM_PAWN_ROOK_SCREAM_SPARK_Y_MASK          = 511,
        GOLEM_PAWN_ROOK_SCREAM_SPARK_SIZE            = 256,
    };

    GolemPawnRookWork* work;
    GfxCoord*          root;
    SVECTOR*           effectOffset;
    s32                soundId;
    u32                randomDraw;

/// Plays one scream-phase cue with the placement tag and sampled pan/depth.
///
/// actor and root are side-effect-free live pointers, cue is a bank script ID,
/// and soundId is a caller-provided s32 local lvalue. root is composed. Repeats
/// root while preserving the call argument evaluation and signed-byte narrowing.
/// Expands to two statements; use only at a standalone braced call site.
#define GOLEM_PAWN_ROOK_PLAY_SCREAM_CUE(actor, root, cue, soundId)                                                                             \
    (soundId) = (cue) | ((((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT); \
    sndEvtRequestScriptStart((soundId), (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));

    effectOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work         = actor->work;
    root         = actor->extra.tmd->coords;
    switch (work->step) {
        case GOLEM_PAWN_ROOK_SCREAM_CHARGE:
            if (work->timer == 0) {
                effectOffset->vx   = 0;
                effectOffset->vy   = 0;
                effectOffset->vz   = 0;
                work->screamEffect = effectSpawn(gRoomEffectFlashId, &actor->extra.tmd->coords[4], GOLEM_PAWN_ROOK_SCREAM_CHARGE_FRAMES, effectOffset);
                GOLEM_PAWN_ROOK_PLAY_SCREAM_CUE(actor, root, gGolemPawnRookScreamCue, soundId);
            }
            work->shieldRaised = work->shieldHp > 0;
            if ((s16)(work->timer % GOLEM_PAWN_ROOK_SCREAM_RUMBLE_INTERVAL) == 0) {
                padScriptSpawnVariableMotorRamp(GOLEM_PAWN_ROOK_SCREAM_CHARGE_RUMBLE_FRAMES, GOLEM_PAWN_ROOK_SCREAM_CHARGE_RUMBLE_POWER, GOLEM_PAWN_ROOK_SCREAM_RUMBLE_END_INTENSITY);
            }
            if (work->interruptDamage >= GOLEM_PAWN_ROOK_SCREAM_INTERRUPT_HP) {
                work->screamActive = 0;
                if (--work->screamCharges <= 0) {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_RECOIL;
                    work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                } else {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SCREAM;
                    work->step     = GOLEM_PAWN_ROOK_SCREAM_RECOVER;
                    work->anim     = GOLEM_PAWN_ROOK_SCREAM_RECOVERY_ANIM;
                    work->timer    = GOLEM_PAWN_ROOK_SCREAM_RECOVERY_FRAMES;
                }
                work->shieldRaised = 0;
                if (work->screamEffect != NULL) {
                    work->screamEffect->task->state = GOLEM_PAWN_ROOK_SCREAM_EFFECT_END_STATE;
                }
                work->screamEffect = NULL;
            } else if (++work->timer >= GOLEM_PAWN_ROOK_SCREAM_CHARGE_FRAMES) {
                work->timer        = GOLEM_PAWN_ROOK_SCREAM_RELEASE_FRAMES;
                work->step         = GOLEM_PAWN_ROOK_SCREAM_RELEASE_DELAY;
                work->anim         = GOLEM_PAWN_ROOK_SCREAM_RELEASE_ANIM;
                work->screamActive = 0;
                work->shieldRaised = 0;
                work->screamEffect = NULL;
                padScriptSpawnVariableMotorRamp(GOLEM_PAWN_ROOK_SCREAM_RELEASE_RUMBLE_FRAMES, GOLEM_PAWN_ROOK_SCREAM_RELEASE_RUMBLE_POWER, GOLEM_PAWN_ROOK_SCREAM_RUMBLE_END_INTENSITY);
            }
            break;
        case GOLEM_PAWN_ROOK_SCREAM_RELEASE_DELAY:
            if (--work->timer <= 0) {
                work->step  = GOLEM_PAWN_ROOK_SCREAM_APPLY_SILENCE;
                work->timer = 0;
            }
            break;
        case GOLEM_PAWN_ROOK_SCREAM_APPLY_SILENCE:
            playerStateSetStatusEffects(0, PLAYER_STATUS_SILENCE);
            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
            work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
            work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
            GOLEM_PAWN_ROOK_PLAY_SCREAM_CUE(actor, root, gGolemPawnRookSilenceCue, soundId);
            break;
        case GOLEM_PAWN_ROOK_SCREAM_RECOVER:
            if (!(work->animFrame & GOLEM_PAWN_ROOK_SCREAM_SPARK_FRAME_MASK)) {
                effectOffset->vx = 0;
                effectOffset->vz = 0;
                randomDraw       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectOffset->vy = -((randomDraw >> 16) & GOLEM_PAWN_ROOK_SCREAM_SPARK_Y_MASK);
                gRandomLcgState  = randomDraw;
                effectSpawn(EFFECT_FLASH_BURST, &actor->extra.tmd->coords[3], GOLEM_PAWN_ROOK_SCREAM_SPARK_SIZE, effectOffset);
            }
            if (--work->timer <= 0) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_ANIM_WALK;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#undef GOLEM_PAWN_ROOK_PLAY_SCREAM_CUE
