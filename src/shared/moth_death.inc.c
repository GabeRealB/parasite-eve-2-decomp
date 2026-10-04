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
            switch (work->field_2DE) {
                case 0:
                    gSceneCombatState.actor00700DeathAlert = 1;
                    seed                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rnd                                    = seed >> 16;
                    angle                                  = rnd & 0xFF;
                    arg1->extra.tmd->flags                 = TMD_OBJECT_SEMI_TRANS;
                    gRandomLcgState                        = seed;
                    work->field_2E2                        = 0x1000;
                    work->savedRootMtx                     = coord->coord;
                    if (!(rnd & 0x100)) {
                        angle = -angle;
                    }
                    work->field_2E4                 = angle;
                    arg0->recs                      = 0;
                    ((MothWork*)work)->obj134.flags = ((MothWork*)work)->obj134.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    ((MothWork*)work)->obj16C.flags = ((MothWork*)work)->obj16C.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    ((MothWork*)work)->obj1EC.flags = ((MothWork*)work)->obj1EC.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
                    id                              = ((arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40070006;
                    pan                             = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(id, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                    worldTargetUnlinkNode(&arg0->node);
                    Gp_ReleaseStateF0Add(arg1, 8);
                    work->field_2E0 = 1;
                    work->field_2DE = 1;
                    break;
                case 1:
                    mothSquash(arg1);
                    work->field_2DA = (work->field_2DA + work->field_2E4) & 0xFFF;
                    work->field_2DC = (work->field_2DC + work->field_2E4) & 0xFFF;
                    rot->vx         = work->field_2DA;
                    rot->vy         = work->field_2DC;
                    rot->vz         = 0;
                    RotMatrix(rot, &coord->coord);
                    work->savedRootMtx.t[1] += 0x18;
                    if ((s16)(work->field_2E0 / 3) < 8) {
                        mothDrawBurst(arg1);
                    } else {
                        arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->field_2E0++;
                    if (work->field_2E0 >= 0x1E) {
                        Gp_UnlinkObj(&((MothWork*)work)->obj134);
                        Gp_UnlinkObj(&((MothWork*)work)->obj16C);
                        Gp_UnlinkObj(&((MothWork*)work)->obj1EC);
                        work->field_2DE = 2;
                    }
                    break;
                case 2:
                    work->field_2E0--;
                    if (work->field_2E0 <= 0) {
                        enemyDestroy(arg0, arg1);
                    }
                    break;
            }
            SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
            break;
    }
}
