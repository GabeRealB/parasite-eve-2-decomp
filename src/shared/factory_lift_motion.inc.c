/* Private motion support for the factory lift fragments.
 * Include in each carrier's prologue after its factory declarations.
 * Requires the lift work, coordinate, session and sound interfaces, plus
 * the carrier's _factoryLiftNotifyPanel forward declaration.
 */

/// Phases shared by the lift's independent vertical and yaw movements.
/// The stored selectors remain signed bytes; -1 is the setup-only rest value.
enum {
    FACTORY_LIFT_STEP_RESET = 0,
    FACTORY_LIFT_STEP_START_SOUND,
    FACTORY_LIFT_STEP_OUTBOUND,
    FACTORY_LIFT_STEP_SETTLE,
    FACTORY_LIFT_STEP_REST
};

/// Jam thresholds as 16.16 angles, with 4096 integer units per turn.
/// A lowered lift turns one thirty-second of a revolution before rebounding.
enum {
    FACTORY_LIFT_JAM_YAW_OUT  = 0x80 * 0x10000,
    FACTORY_LIFT_JAM_YAW_BACK = FACTORY_LIFT_YAW_TURNED - FACTORY_LIFT_JAM_YAW_OUT
};

/// Reports turn completion to the panel and replaces the turn loop with its stop cue.
///
/// The slot must be live; NULL contents mean no panel is open. Audio pan and
/// depth are narrowed to signed bytes at the lift origin, as for the start cue.
static __inline__ void _factoryLiftFinishTurn(Task* task, GfxCoord* coord)
{
    Task** panelSlot = task->spawnArg2.pointer;

    _factoryLiftNotifyPanel(*panelSlot);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStop(SOUND_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    } else {
        sndEvtRequestStageScriptStop(SOUND_NIGHT_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Reports vertical completion to the panel and replaces the motion loop with its stop cue.
///
/// The slot must be live; NULL contents mean no panel is open. Audio pan and
/// depth are narrowed to signed bytes at the lift origin, as for the start cue.
static __inline__ void _factoryLiftFinishVertical(Task* task, GfxCoord* coord)
{
    Task** panelSlot = task->spawnArg2.pointer;

    _factoryLiftNotifyPanel(*panelSlot);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStop(SOUND_FACTORY_LIFT_MOVE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    } else {
        sndEvtRequestStageScriptStop(SOUND_NIGHT_FACTORY_LIFT_MOVE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Replaces the lift root's rotation with its current integer yaw and marks it dirty.
///
/// Preserves translation, discards pitch/roll/scale, and reads yaw after the
/// identity stores. Work holds a 16.16 angle; the SDK receives its signed high half.
static __inline__ void _factoryLiftRebuildYaw(FactoryLiftWork* work, GfxCoord* coord)
{
    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(work->yaw.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
