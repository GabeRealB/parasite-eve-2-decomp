/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Attaches the grenade launcher model to body part 7 and starts its tick.
///
/// `task` is the launcher child of a live GOLEM body task. It borrows the body's
/// lighting matrices and remains in its parent's teardown tree. The common
/// enemy-state callback argument `unusedEnemy` is ignored.
static void _golemPawnRookLauncherSpawn(Enemy* unusedEnemy, Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_WEAPON_PART = 7,
    };

    _golemPawnRookAttachChildModel(task, GOLEM_PAWN_ROOK_WEAPON_PART);
}
