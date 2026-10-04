/* Part of the Moth library; see moth.h. */

/// Task state 2. Phase 0 raises the combat-state death alert that the other
/// moths react to, turns the model semi-transparent, saves the root transform,
/// picks a random spin rate, disables the hit and terrain spheres, enables the
/// attack sphere, plays sound 6 and unlinks the enemy node. Phase 1 squashes
/// the model, spins it, lowers the saved transform 0x18 a frame and draws the
/// burst sprite for 24 frames before hiding the model; at frame 30 it unlinks
/// the spheres, and phase 2 counts back down and destroys the enemy.
void mothDeath(Enemy* arg0, Task* arg1)
{
    MothWork* work;
    GfxCoord* coord;
    SVECTOR*  head;
    SVECTOR*  rot;
    s32       angle;
    u32       rnd;
    u32       seed;
    s32       id;
    s32       pan;

    coord = arg1->extra.tmd->coords;
    work  = arg1->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            head                          = SCRATCH_STACK_CURSOR(SVECTOR);
            rot                           = head - 1;
            SCRATCH_STACK_CURSOR(SVECTOR) = rot;
            switch (work->deathStep) {
                case 0:
                    gSceneCombatState.actor00700DeathAlert = 1;
                    seed                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rnd                                    = seed >> 16;
                    angle                                  = rnd & 0xFF;
                    arg1->extra.tmd->flags                 = TMD_OBJECT_SEMI_TRANS;
                    gRandomLcgState                        = seed;
                    work->squashScale                      = 0x1000;
                    work->savedRootMtx                     = coord->coord;
                    if (!(rnd & 0x100)) {
                        angle = -angle;
                    }
                    work->deathSpinRate    = angle;
                    arg0->recs             = 0;
                    work->hitBody.flags    = work->hitBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->gridBody.flags   = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    work->attackBody.flags = work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
                    id                     = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070006;
                    pan                    = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                    worldTargetUnlinkNode(&arg0->node);
                    Gp_ReleaseStateF0Add(arg1, 8);
                    work->timer     = 1;
                    work->deathStep = 1;
                    break;
                case 1:
                    mothSquash(arg1);
                    work->pitch = (work->pitch + work->deathSpinRate) & 0xFFF;
                    work->yaw   = (work->yaw + work->deathSpinRate) & 0xFFF;
                    rot->vx     = work->pitch;
                    rot->vy     = work->yaw;
                    rot->vz     = 0;
                    RotMatrix(rot, &coord->coord);
                    work->savedRootMtx.t[1] += 0x18;
                    if ((s16)(work->timer / 3) < 8) {
                        mothDrawBurst(arg1);
                    } else {
                        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->timer++;
                    if (work->timer >= 0x1E) {
                        Gp_UnlinkObj(&work->hitBody);
                        Gp_UnlinkObj(&work->gridBody);
                        Gp_UnlinkObj(&work->attackBody);
                        work->deathStep = 2;
                    }
                    break;
                case 2:
                    work->timer--;
                    if (work->timer <= 0) {
                        enemyDestroy(arg0, arg1);
                    }
                    break;
            }
            SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
            break;
    }
}
