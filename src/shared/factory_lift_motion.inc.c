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

/// Notifies the operator panel that a turn ended and queues the turn stop cue.
///
/// Used when a normal turn or jam rebound settles or is skipped. The live
/// `liftTask` borrows a room-owned `Task*` slot in `spawnArg2.pointer`; the slot
/// must remain live, and its contents must be NULL or a live panel task.
/// Notification is synchronous and precedes the sound requests.
///
/// Runtime stage selects daytime Dryfield sounds, or nighttime sounds otherwise.
/// Stopping retains the loop's existing release settings. `liftRoot` must have
/// its local-to-view matrix already composed: the stop cue samples that cached
/// origin before the caller rebuilds yaw. Pan [-16, 15] and depth [-128, 127]
/// are narrowed to signed bytes; scratch and GTE requirements follow
/// `worldCoordGetOriginAudioPan`.
static __inline__ void _factoryLiftFinishTurn(const Task* liftTask, const GfxCoord* liftRoot)
{
    Task* const* panelSlot = liftTask->spawnArg2.pointer;

    _factoryLiftNotifyPanel(*panelSlot);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStop(SOUND_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(liftRoot), (s8)worldCoordGetOriginAudioDepth(liftRoot));
    } else {
        sndEvtRequestStageScriptStop(SOUND_NIGHT_FACTORY_LIFT_TURN, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_TURN_STOP, (s8)worldCoordGetOriginAudioPan(liftRoot), (s8)worldCoordGetOriginAudioDepth(liftRoot));
    }
}

/// Notifies the operator panel of a finished raise or lower and queues the stop cue.
///
/// Used for both natural completion and an accepted skip. `liftTask` borrows
/// a room-owned `Task*` slot in `spawnArg2.pointer`; the slot must be live and
/// contain NULL or a live panel task. Notification is synchronous and precedes
/// the sound requests; NULL suppresses only the notification.
///
/// Requires a live session and loaded factory sound bank. Stops the vertical
/// motion script without a fade, retaining its release settings, then starts
/// the stop cue: daytime IDs in `GAME_STAGE_DRYFIELD`, nighttime IDs otherwise.
/// `liftCoord` is the model root with its local-to-view matrix already composed;
/// projection and scratch requirements follow `worldCoordGetOriginAudioPan`.
/// Pan (-16..15) and depth (-128..127, 256 world units per step) are passed as
/// signed bytes. The caller commits the final position and movement phase.
static __inline__ void _factoryLiftFinishVertical(const Task* liftTask, const GfxCoord* liftCoord)
{
    Task* const* panelTaskSlot = liftTask->spawnArg2.pointer;

    _factoryLiftNotifyPanel(*panelTaskSlot);
    // Sample the cached origin before the caller writes this frame's new local Y.
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStop(SOUND_FACTORY_LIFT_MOVE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(liftCoord), (s8)worldCoordGetOriginAudioDepth(liftCoord));
    } else {
        sndEvtRequestStageScriptStop(SOUND_NIGHT_FACTORY_LIFT_MOVE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(liftCoord), (s8)worldCoordGetOriginAudioDepth(liftCoord));
    }
}

/// Replaces the lift root's rotation with its current integer yaw and marks it dirty.
///
/// Preserves translation, discards pitch/roll/scale, and reads yaw after the
/// identity stores. Work holds a 16.16 angle; the SDK receives its signed high half.
/// Both objects are borrowed; the caller's motion state and fractional yaw remain intact.
static __inline__ void _factoryLiftRebuildYaw(const FactoryLiftWork* work, GfxCoord* coord)
{
    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(work->yaw.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
