#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Burst resources supplied by the carrier of this fragment.
///
/// The offset binding is a live SVECTOR object; the script and segment bindings
/// name PadScriptCmd and PadScriptVibrationSegment arrays that outlive the
/// spawned vibration task. actor_04600 uses the defaults; actor_07000 binds its
/// own objects before inclusion. All three bindings are undefined afterwards.
#ifndef SUCKLERCEPH_BURST_FX_OFFSET
#define SUCKLERCEPH_BURST_FX_OFFSET gSucklercephBurstFxOffset
#endif
#ifndef SUCKLERCEPH_BURST_PAD_SCRIPT
#define SUCKLERCEPH_BURST_PAD_SCRIPT gSucklercephBurstScriptA
#endif
#ifndef SUCKLERCEPH_BURST_VIBRATION_SEGMENTS
#define SUCKLERCEPH_BURST_VIBRATION_SEGMENTS gSucklercephBurstScriptB
#endif

/// Clears the Sucklerceph's HP and chooses its burst or slump death.
///
/// Requires live Sucklerceph work, enemy and model. A nonzero forceBurst selects
/// the burst; otherwise one random bit selects it. Both paths consume one draw.
/// The burst arms its two damage spheres, spawns its visual and vibration
/// effects and latches hasBurst; the slump changes the behavior state. The
/// caller starts the task's death state and countdown; storage stays live.
static void _sucklercephKill(Task* task, u8 forceBurst)
{
    enum { SUCKLERCEPH_DEFAULT_SOUND_BANK         = 0x2E,
           SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT = 8,
           SUCKLERCEPH_BURST_ROLL_BIT             = 2,
           SUCKLERCEPH_BURST_FLASH_STYLE          = 1,
           SUCKLERCEPH_BURST_PARTICLE_SIZE        = 768 };
    /// Plays a placement-scoped cue from a live enemy argument and root.
    ///
    /// enemyArg and baseId are evaluated once, rootCoord twice and idLocal
    /// twice. Expands two statements: use only in a braced branch.
    /// Supply stable pointers and a modifiable s32 local without side
    /// effects. Pan/depth retain their signed-byte conversion; the argument
    /// view reloads the task's generic spawn slot at the sound site. Uses the
    /// containing function's SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT constant.
#define SUCKLERCEPH_PLAY_DEATH_SOUND(enemyArg, rootCoord, baseId, idLocal)                                                          \
    (idLocal) = ((((Enemy*)(enemyArg))->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT) | (baseId); \
    sndEvtRequestScriptStart((idLocal), (s8)worldCoordGetOriginAudioPan((rootCoord)), (s8)worldCoordGetOriginAudioDepth((rootCoord)));
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              soundId;

    model     = task->extra.tmd;
    enemy     = task->spawnArg2.pointer;
    work      = task->work;
    rootCoord = model->coords;
    enemy->hp = 0;
    // Consume the death draw even when the caller forces a burst.
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if (((gRandomLcgState >> 16) & SUCKLERCEPH_BURST_ROLL_BIT) || forceBurst) {
        if (work->variant != 0) {
            SUCKLERCEPH_PLAY_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 0x0B), soundId);
        } else {
            SUCKLERCEPH_PLAY_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_DEFAULT_SOUND_BANK, 3), soundId);
        }
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->blastBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords, SUCKLERCEPH_BURST_FLASH_STYLE, NULL);
        effectSpawn(EFFECT_030, task->extra.tmd->coords, SUCKLERCEPH_BURST_PARTICLE_SIZE, &SUCKLERCEPH_BURST_FX_OFFSET);
        padScriptSpawn(SUCKLERCEPH_BURST_PAD_SCRIPT, SUCKLERCEPH_BURST_VIBRATION_SEGMENTS);
        work->hasBurst = 1;
    } else {
        if (work->variant != 0) {
            SUCKLERCEPH_PLAY_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 0x0C), soundId);
        } else {
            SUCKLERCEPH_PLAY_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_DEFAULT_SOUND_BANK, 4), soundId);
        }
        work->state = SUCKLERCEPH_STATE_SLUMP_DEATH;
    }
#undef SUCKLERCEPH_PLAY_DEATH_SOUND
}

#undef SUCKLERCEPH_BURST_FX_OFFSET
#undef SUCKLERCEPH_BURST_PAD_SCRIPT
#undef SUCKLERCEPH_BURST_VIBRATION_SEGMENTS
