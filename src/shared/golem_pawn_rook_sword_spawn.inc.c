/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Attaches the Beam Sword model and arms its delayed trail effect.
///
/// `task` is the sword child of a live GOLEM body task. It attaches to body part
/// 7, borrows the body's lighting matrices, and starts a ten-tick trail delay in
/// the parent's work. It remains in the parent's teardown tree. The common
/// enemy-state callback argument `unusedEnemy` is ignored.
static void _golemPawnRookSwordSpawn(Enemy* unusedEnemy, Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_SWORD_TRAIL_DELAY_FRAMES = 10,
        GOLEM_PAWN_ROOK_WEAPON_PART              = 7,
    };

    GolemPawnRookWork* work;

    work                  = _golemPawnRookAttachChildModel(task, GOLEM_PAWN_ROOK_WEAPON_PART);
    work->swordTrailDelay = GOLEM_PAWN_ROOK_SWORD_TRAIL_DELAY_FRAMES;
}
