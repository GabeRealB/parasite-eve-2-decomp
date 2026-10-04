/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Teardown state of the effect child set up by `golemPawnRookBulletSpawn`: step 0
/// unlinks its three collision bodies and restarts the frame counter, step 1
/// destroys the child once 0x3D frames have passed.
void golemPawnRookBulletDestroy(Enemy* arg0, Task* arg1)
{
    GolemPawnRookGrenadeWork* work;
    u16                       temp_v0;

    work = arg1->work;
    switch (work->teardownStep) {
        case 0:
            Gp_UnlinkObj(&work->playerStrikeBody);
            Gp_UnlinkObj(&work->enemyStrikeBody);
            Gp_UnlinkObj(&work->wallBody);
            work->timer        = 0;
            work->teardownStep = 1;
            return;
        case 1:
            temp_v0     = work->timer + 1;
            work->timer = temp_v0;
            if ((s16)temp_v0 >= 0x3D) {
                enemyDestroy(arg0, arg1);
            }
            return;
    }
}
