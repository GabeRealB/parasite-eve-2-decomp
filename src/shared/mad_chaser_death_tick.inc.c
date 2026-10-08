/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs one ordinary-death frame and refreshes colour and visible limb shadows.
///
/// Requires live enemy/model/work storage with at least nine model coordinates
/// and work->state in 0..8. Hidden actors skip drawing and all frame work.
/// Running actors increment the wrapping s16 frame counter, dispatch one copied
/// death handler and dirty the root. Running and paused actors both sample
/// colour at part 1 and draw three shadows unless shadowHidden is set. Retains
/// the model's draw-skip flag when actors resume.
static void _madChaserDeathTick(Task* task)
{
    TmdObject*     model  = task->extra.tmd;
    MadChaserWork* work   = task->work;
    GfxCoord*      root   = model->coords;
    TaskFuncTable9 states = gMadChaserDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states.funcs[(s16)work->state](task);
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            // Paused frames retain presentation updates without advancing death.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(task, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}
