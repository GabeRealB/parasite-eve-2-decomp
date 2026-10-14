/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Unlinks a burst grenade's collision bodies and destroys it after 61 frames.
///
/// `grenade` owns a live `GolemPawnRookGrenadeWork`; `enemy` is its enemy record.
/// The first call ends collision participation, then the hidden model and work
/// remain alive during the burst effect's delay. The timer retains signed
/// halfword wrapping.
static void _golemPawnRookGrenadeDestroy(Enemy* enemy, Task* grenade)
{
    enum {
        GOLEM_PAWN_ROOK_GRENADE_UNLINK               = 0,
        GOLEM_PAWN_ROOK_GRENADE_WAIT_DESTROY         = 1,
        GOLEM_PAWN_ROOK_GRENADE_DESTROY_DELAY_FRAMES = 61,
    };

    GolemPawnRookGrenadeWork* work;

    work = grenade->work;
    switch (work->teardownStep) {
        case GOLEM_PAWN_ROOK_GRENADE_UNLINK:
            worldCollisionUnlinkBody(&work->playerStrikeBody);
            worldCollisionUnlinkBody(&work->enemyStrikeBody);
            worldCollisionUnlinkBody(&work->wallBody);
            work->timer        = 0;
            work->teardownStep = GOLEM_PAWN_ROOK_GRENADE_WAIT_DESTROY;
            return;
        case GOLEM_PAWN_ROOK_GRENADE_WAIT_DESTROY:
            if (++work->timer >= GOLEM_PAWN_ROOK_GRENADE_DESTROY_DELAY_FRAMES) {
                enemyDestroy(enemy, grenade);
            }
            return;
    }
}
