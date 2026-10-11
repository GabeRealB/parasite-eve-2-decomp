/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Requests a positioned character script with its placement instance tag.
///
/// Compound statement for a braced branch. Supply a live Enemy pointer or
/// generic task slot in enemyArg, a composed root and a modifiable s32 idLocal.
/// enemyArg/baseSoundId evaluate once; rootCoord twice and idLocal twice, so
/// use stable expressions without side effects. The read-only Enemy view
/// extracts the placement nibble into sound ID bits 8..15. Pan/depth narrow
/// to s8 and request failure is ignored. Include this before the death-state
/// fragment, which shares the helper and undefines it after its definition.
#define SUCKLERCEPH_REQUEST_POSITIONED_SOUND(enemyArg, rootCoord, baseSoundId, idLocal)                                                                 \
    {                                                                                                                                                   \
        enum { SUCKLERCEPH_POSITIONED_SOUND_INSTANCE_SHIFT = 8 };                                                                                       \
        (idLocal) = ((((const Enemy*)(enemyArg))->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_POSITIONED_SOUND_INSTANCE_SHIFT) | (baseSoundId); \
        sndEvtRequestScriptStart((idLocal), (s8)worldCoordGetOriginAudioPan((rootCoord)), (s8)worldCoordGetOriginAudioDepth((rootCoord)));              \
    }

/// Subtracts HP and adds its readout, starting death only when HP becomes negative.
///
/// Requires a live task, Enemy, model root and work; damage is an HP amount.
/// Zero remaining HP still plays the hurt cue and can request waking from idle.
/// A lethal call chooses burst/slump and arms the five-tick death countdown.
/// Surviving calls also store 15 in an unread work halfword whose role is unproven.
static void _sucklercephTakeDamage(Task* task, s32 damage)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              animId;
    s32              soundId;
    enum { SUCKLERCEPH_SURVIVED_HIT_VALUE = 15 };

    enemy      = task->spawnArg2.pointer;
    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = task->work;
    enemy->hp -= damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    if (enemy->hp < 0) {
        _sucklercephKill(task, 0);
        task->state         = SUCKLERCEPH_TASK_DEATH;
        task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
        return;
    }
    if (work->variant != 0) {
        SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_VARIANT, 10), soundId);
    } else {
        SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_DEFAULT, 2), soundId);
    }
    animId          = work->animId;
    work->field_2CC = SUCKLERCEPH_SURVIVED_HIT_VALUE;
    if (animId == SUCKLERCEPH_ANIM_IDLE) {
        work->wakeRequested = animId;
    }
}
