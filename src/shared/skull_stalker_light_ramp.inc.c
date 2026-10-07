/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Steps the 18-tick visibility fade and its draw and lock-on gates.
///
/// Fade progress is clamped to 0..SKULL_STALKER_FADE_FRAMES. Hiding starts a
/// black color blend with semitransparent drawing, then disables drawing and
/// lock-on at the hidden endpoint. Revealing restores lock-on and the default
/// color blend before clearing model flags at the visible endpoint.
static void _skullStalkerUpdateVisibility(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;

    if (work->hiding != 0) {
        if (work->fadeFrames == 0) {
            work->fadeFrames++;
            model->flags = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        } else {
            work->fadeFrames++;
            if (work->fadeFrames >= SKULL_STALKER_FADE_FRAMES) {
                work->fadeFrames              = SKULL_STALKER_FADE_FRAMES;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        if (work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
            work->fadeFrames--;
            enemy->node.state.parts.flags = 0;
            model->flags                  = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        } else {
            work->fadeFrames--;
            if (work->fadeFrames <= 0) {
                work->fadeFrames = 0;
                model->flags     = 0;
            }
        }
    }
}
