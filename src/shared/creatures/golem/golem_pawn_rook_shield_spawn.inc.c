/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Attaches the Rook's shield model to body part 11 and starts its tick.
///
/// `task` is the shield child of a live Rook body task. It borrows the body's
/// lighting matrices and remains in its parent's teardown tree. The common
/// enemy-state callback argument `unusedEnemy` is ignored.
static void _golemPawnRookShieldSpawn(Enemy* unusedEnemy, Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_SHIELD_PART = 11,
    };

    _golemPawnRookAttachChildModel(task, GOLEM_PAWN_ROOK_SHIELD_PART);
}
