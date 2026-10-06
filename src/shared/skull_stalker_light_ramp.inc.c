/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Steps the second enemy's fade `fadeFrames` toward hidden or in sight as
/// `hiding` says. Hiding, the first frame makes the model semi-transparent and
/// starts the blend to the black colour mode, and the count saturates at
/// `SKULL_STALKER_FADE_FRAMES`, where the enemy stops being lockable and the
/// model leaves the draw. Showing, leaving that end makes the enemy lockable
/// again and starts the blend back to the default colour mode, and the count
/// bottoms out at 0 with the model flags cleared.
void skullStalkerLightRamp(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;
    TmdObject*        obj;

    work  = task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    obj   = task->extra.tmd;

    if (work->hiding != 0) {
        if (work->fadeFrames == 0) {
            work->fadeFrames++;
            obj->flags = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        } else {
            work->fadeFrames++;
            if (work->fadeFrames >= SKULL_STALKER_FADE_FRAMES) {
                work->fadeFrames              = SKULL_STALKER_FADE_FRAMES;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        if (work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
            work->fadeFrames--;
            enemy->node.state.parts.flags = 0;
            obj->flags                    = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        } else {
            work->fadeFrames--;
            if (work->fadeFrames <= 0) {
                work->fadeFrames = 0;
                obj->flags       = 0;
            }
        }
    }
}
