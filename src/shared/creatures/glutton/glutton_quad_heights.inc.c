/* Part of the Glutton library; see glutton.h. */

/// Sets two consecutive live grid quads to the Glutton's fixed wall heights.
///
/// Requires writable vertices `4 * firstQuadIndex` through
/// `4 * firstQuadIndex + 7` in the active grid, with a nonnegative quad index.
/// The first two vertices of each quad take Y = 500 and the last two Y = 800,
/// in world units; X/Z, face indices and normals are untouched. The first
/// argument is unused. Neither carrier calls this retained helper.
static void _gluttonSetQuadPairHeights(s32 unused, s16 firstQuadIndex)
{
    enum { GLUTTON_QUAD_PAIR_TOP_Y    = 500,
           GLUTTON_QUAD_PAIR_BOTTOM_Y = 800 };
    SVECTOR* vertices;

    vertices                            = gWorldCollisionActiveGrid->vertices;
    vertices[firstQuadIndex * 4].vy     = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 1].vy = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 2].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
    vertices[firstQuadIndex * 4 + 3].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
    vertices[firstQuadIndex * 4 + 4].vy = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 5].vy = GLUTTON_QUAD_PAIR_TOP_Y;
    vertices[firstQuadIndex * 4 + 6].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
    vertices[firstQuadIndex * 4 + 7].vy = GLUTTON_QUAD_PAIR_BOTTOM_Y;
}

/// Releases the host enemy after sending its surviving escorts to teardown.
///
/// When host work exists, requests state 2 on every surviving escort, unlinks
/// hit bodies 0, 1 and 3..8, and detaches the enemy contact table. Group 2 is
/// not unlinked here; its teardown ownership is unproven.
/// Requires the task's live enemy in `spawnArg2.pointer`. `enemyDestroy` releases
/// that enemy and starts task teardown; neither may be accessed afterwards.
static void _gluttonExit(Task* task)
{
    enum { GLUTTON_ESCORT_TEARDOWN_STATE = 2 };
    GluttonWork* work;
    Enemy*       enemy;
    s16          escortIndex;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work != NULL) {
        for (escortIndex = 0; escortIndex < ARRAY_SIZE(work->escorts); escortIndex++) {
            if (work->escorts[escortIndex] != NULL) {
                work->escorts[escortIndex]->task->state = GLUTTON_ESCORT_TEARDOWN_STATE;
            }
        }
        worldCollisionUnlinkBody(&work->hits[0].body);
        worldCollisionUnlinkBody(&work->hits[1].body);
        worldCollisionUnlinkBody(&work->hits[3].body);
        worldCollisionUnlinkBody(&work->hits[4].body);
        worldCollisionUnlinkBody(&work->hits[5].body);
        worldCollisionUnlinkBody(&work->hits[6].body);
        worldCollisionUnlinkBody(&work->hits[7].body);
        worldCollisionUnlinkBody(&work->hits[8].body);
        enemy->recs = NULL;
    }
    enemyDestroy(enemy, task);
}

#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
/// Requests a vertical screen shake from the dumping-hole Glutton host.
///
/// The successfully spawned host task and its work must still be alive. The
/// next shake update arms 5, 10 or 22 frames for `GLUTTON_SHAKE_SHORT`,
/// `GLUTTON_SHAKE_MEDIUM` or `GLUTTON_SHAKE_LONG` when the request differs from
/// the armed level; repeating it does not restart the shake. `level` is stored
/// as an unsigned byte without validation. Other values, including
/// `GLUTTON_SHAKE_NONE`, neither arm a changed request nor cancel the shake.
#endif
void GLUTTON_SET_SHAKE_LEVEL(s8 level)
{
    GluttonWork* work = GLUTTON_HOST_TASK->work;

    work->shakeLevel = level;
}

/// Stores the encounter's spinner-release word without normalization.
///
/// Waiting spinners advance for value 1; chasing spinners abort for value 0.
/// This accessor has no callers in either carrier.
static void _gluttonSetSpinnersReleased(s16 releaseState)
{
    _gGluttonSpinnersReleased = releaseState;
}

/// Returns the encounter's spinner-release word unchanged.
///
/// Waiting spinners advance for value 1; chasing spinners abort for value 0.
/// This accessor has no callers in either carrier.
static s16 _gluttonGetSpinnersReleased(void)
{
    return _gGluttonSpinnersReleased;
}
