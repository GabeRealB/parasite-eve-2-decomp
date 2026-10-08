/* Part of the Moth library; see moth.h. */

/// Advances the living moth's contacts, wing motion, flight and lighting.
///
/// Requires work initialized by spawn. Running actors restore drawing/targeting;
/// paused actors update colour alone, and hidden actors disable both and return.
/// A flock death alert starts pursuit and engages battle. A contact can select
/// the death state, but the current living tick still finishes before the next
/// dispatch. Composes the moved root, samples colour and rolls a 1/128 ambient
/// sound with placement-index identity and signed-byte spatial pan/depth.
static void _mothUpdate(Enemy* enemy, Task* task)
{
    enum {
        MOTH_AMBIENT_SOUND           = 0x40070008,
        MOTH_AMBIENT_SOUND_ROLL_MASK = 127
    };

    TmdObject* model;
    MothWork*  work;
    GfxCoord*  rootCoord;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _mothUpdateColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // A killing contact changes the next task state; this living tick still finishes.
    _mothContacts(task);
    _mothOscillateParts(task);
    if (work->alerted == false && gSceneCombatState.actor00700DeathAlert != 0) {
        work->alerted = true;
        sceneEngageBattle(1);
    }
    _mothSteer(task);
    _mothDrift(task);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _mothUpdateColor(task);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16 & MOTH_AMBIENT_SOUND_ROLL_MASK) == 0) {
        s32 soundPan;
        s32 soundId;

        soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MOTH_SOUND_PLACE_INDEX_SHIFT) | MOTH_AMBIENT_SOUND;
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}
