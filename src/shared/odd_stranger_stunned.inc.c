/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Enter the live-actor state: reinstate the model buffers, seed the
/// `field_898` / `field_8A2` animation pair, fold the current `field_89E`
/// state onto the 0x17/0x18 pair, then hold in `oddStrangerDrive`
/// until the clip's record index (`rig.slots[1].currentPose.indices.recordIndex`) passes 6 (state 0x17) or 9 (state
/// 0x18), or the `rig.slots[1].status` word reports the actor gone. The un-flagged path
/// halves `field_8A2` down to the +-0x10 turntable step and retires the actor
/// once the enemy is spent. Same body as `func_actor_401300_80135DDC`, which
/// drops the frame-count loop's `rig.slots[1].status` guard and its own 0x36 test.
void oddStrangerStunned(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       tmd;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        tmd                           = arg0->extra.tmd;
        enemy->node.state.parts.flags = 0;
        tmd->flags                    = 0;
        Tmd_AllocBuffers(tmd);
        work->field_898        = 2;
        work->field_8A2        = 0x10;
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if ODD_STRANGER_VARIANT == 1
        if (work->field_89E == 11 || work->field_89E == 23) {
#else
        if (work->field_89E == 11) {
#endif
            work->field_89E = 0x17;
#if ODD_STRANGER_VARIANT == 1
        } else if (work->field_89E == 12 || work->field_89E == 25 || work->field_89E == 24) {
#else
        } else if (work->field_89E == 12 || work->field_89E == 25) {
#endif
            work->field_89E = 0x18;
        }
        if ((u16)(work->field_89E - 0x17) >= 2) {
            work->field_89E = 0x17;
        }
        do {
            oddStrangerDrive(arg0);
            if (work->field_89E == 0x17 && (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) >= 6) {
                break;
            }
            if (work->field_89E == 0x18 && (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) >= 9) {
                break;
            }
#if ODD_STRANGER_VARIANT == 1
        } while (!(work->rig.slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED)));
#else
        } while (1);
#endif
        work->field_8A2 = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->field_8A2                       = work->field_8A2 / 2;
    if (work->field_8A2 == 1) {
        work->field_8A2 = -0x10;
    }
    if (work->field_8A2 == -1) {
        work->field_8A2 = 0x10;
    }
    oddStrangerDrive(arg0);
    if (Gp_TickObjFlag2(enemy) == 1) {
        enemy->reactionFlags &= ~ENEMY_REACTION_BUILDUP;
        work->field_0         = 0x11;
    }
#if ODD_STRANGER_VARIANT == 1
    if (enemy->hp <= 0) {
        work->field_0 = 0x11;
    }
#endif
}
