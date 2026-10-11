#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

#ifndef SUCKLERCEPH_BURST_PARTICLE_OFFSET
/// Binds the burst particle's model-root-local displacement in game units.
///
/// Supply a live word-aligned SVECTOR lvalue before including this fragment;
/// its address is passed once to effectSpawn, which copies it during placement.
/// Both carriers default to their private (0,-10,0) vector. It need only survive
/// the spawn call. This binding is undefined after the fragment.
#define SUCKLERCEPH_BURST_PARTICLE_OFFSET _gSucklercephBurstParticleOffset
#endif
#ifndef SUCKLERCEPH_BURST_PAD_SCRIPT
/// Binds the burst's two-lane PadScriptCmd command array.
///
/// Supply the array or a pointer to its first command before this inclusion.
/// Both lanes must eventually STOP and every reachable PLAY operand must index
/// SUCKLERCEPH_BURST_VIBRATION_SEGMENTS. Both carriers default to a private
/// three-command array. The task borrows it, unmodified, until teardown.
/// Evaluated once at spawn and undefined after the fragment.
#define SUCKLERCEPH_BURST_PAD_SCRIPT _gSucklercephBurstPadScript
#endif
#ifndef SUCKLERCEPH_BURST_VIBRATION_SEGMENTS
/// Binds the PadScriptVibrationSegment table used by the burst commands.
///
/// Supply a readable array or first-entry pointer covering every PLAY index in
/// SUCKLERCEPH_BURST_PAD_SCRIPT. Each record supplies byte intensities and a
/// duration in script ticks; both carriers default to three records for hold,
/// full intensity and fade. The task borrows this table unmodified until
/// teardown. Evaluated once at spawn and undefined after the fragment.
#define SUCKLERCEPH_BURST_VIBRATION_SEGMENTS _gSucklercephBurstVibrationSegments
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
    /// Queues a placement-scoped death cue from a live enemy argument and root.
    ///
    /// enemyArg and baseId are evaluated once, rootCoord twice and idLocal
    /// twice. Expands a compound statement; use within a braced branch.
    /// Supply stable pointers and a modifiable s32 local without side
    /// effects. Pan/depth retain their signed-byte conversion. The read-only
    /// Enemy view reloads the generic spawn slot at the sound site.
    /// The placement nibble occupies the sound ID's instance byte. Uses the
    /// containing function's SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT constant.
#define SUCKLERCEPH_REQUEST_DEATH_SOUND(enemyArg, rootCoord, baseId, idLocal)                                                                 \
    {                                                                                                                                         \
        (idLocal) = ((((const Enemy*)(enemyArg))->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT) | (baseId); \
        sndEvtRequestScriptStart((idLocal), (s8)worldCoordGetOriginAudioPan((rootCoord)), (s8)worldCoordGetOriginAudioDepth((rootCoord)));    \
    }
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
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 0x0B), soundId);
        } else {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_DEFAULT_SOUND_BANK, 3), soundId);
        }
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->blastBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords, SUCKLERCEPH_BURST_FLASH_STYLE, NULL);
        effectSpawn(EFFECT_030, task->extra.tmd->coords, SUCKLERCEPH_BURST_PARTICLE_SIZE, &SUCKLERCEPH_BURST_PARTICLE_OFFSET);
        padScriptSpawn(SUCKLERCEPH_BURST_PAD_SCRIPT, SUCKLERCEPH_BURST_VIBRATION_SEGMENTS);
        work->hasBurst = 1;
    } else {
        if (work->variant != 0) {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 0x0C), soundId);
        } else {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_DEFAULT_SOUND_BANK, 4), soundId);
        }
        work->state = SUCKLERCEPH_STATE_SLUMP_DEATH;
    }
#undef SUCKLERCEPH_REQUEST_DEATH_SOUND
}

#undef SUCKLERCEPH_BURST_PARTICLE_OFFSET
#undef SUCKLERCEPH_BURST_PAD_SCRIPT
#undef SUCKLERCEPH_BURST_VIBRATION_SEGMENTS
