/* Part of the burster library; see burster.h. */

/// Death-state handler of the first enemy, under the `Gp_StateF0.field_4` mode byte:
/// mode 2 hides the model and mode 1 does nothing. Otherwise `field_2B4` steps
/// the death through three phases. Phase 0 shrinks the model and counts the
/// kill countdown down; when it runs out the death sound plays, state 0xF0 is
/// released, an optional final effect is spawned, the root transform is saved
/// and the enemy's node and four bodies are unlinked. Phase 1 folds the saved
/// transform back with a decaying Y scale for up to 0x3D frames, and phase 2
/// destroys the enemy once that count is spent. Outside reaction states 5 and 6
/// the first two phases also tick the animation, scale and recompute the
/// second part and re-colour the enemy.
void bursterDeathState(Enemy* enemy, Task* task)
{
    TmdObject*       model;
    Actor104600Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              soundId;

    obj   = task->extra.tmd;
    work  = (Actor104600Work*)task->work;
    coord = obj->coords;
    model = obj;
    switch (Gp_StateF0.field_4) {
        case 1:
            break;
        case 2:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case 0:
        default:
            switch (work->field_2B4) {
                case 0:
                    work->obj1EC.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->field_2AC    -= 0x12C;
                    task->killCountdown--;
                    if ((u32)((u16)work->field_2B2 - 5) >= 2 && task->killCountdown == 3) {
                        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->field_2B2 == 6) {
                        work->field_2B8 = 1;
                        Actor04600_TickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->field_2D6 != 0) {
                            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000D;
                            SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
                        } else {
                            soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0005;
                            SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)gpGetObjDepth(coord));
                        }
                        task->killCountdown = 0;
                        Gp_ReleaseStateF0Add(task, 0x2E);
                        if (work->field_2DA != 0) {
                            Gp_SpawnEff(0x6009E, task->extra.tmd->coords, 0, NULL);
                        }
                        work->field_2B4 = 1;
                        work->field_2B6 = 0;
                        work->field_2CA = 0x1000;
                        work->field_28C = coord->coord;
                        enemy->recs     = NULL;
                        Gp_UnlinkNode(&enemy->node);
                        Gp_UnlinkObj(&work->objFC);
                        Gp_UnlinkObj(&work->obj134);
                        Gp_UnlinkObj(&work->obj1B4);
                        Gp_UnlinkObj(&work->obj1EC);
                    }
                    break;
                case 1:
                    if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                        work->field_2B4 = 2;
                    }
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        work->field_2B4 = 2;
                    }
                    bursterFlatten(task);
                    if (work->field_2B6 == 0xA) {
                        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    break;
                case 2:
                    work->field_2B6++;
                    if (work->field_2B6 >= 0x3D) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    return;
            }
            if ((u32)((u16)work->field_2B2 - 5) >= 2) {
                Actor04600_TickAnim(task);
                bursterScalePart(task, &task->extra.tmd->coords[1]);
                task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
                task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&task->extra.tmd->coords[1]);
                actorUpdateColor(enemy, &task->extra.tmd->coords[1]);
            }
            break;
    }
}
