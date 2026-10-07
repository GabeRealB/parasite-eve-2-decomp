/* Part of the Odd Stranger library; see odd_stranger.h.
 * ODD_STRANGER_VARIANT selects the horizontal-offset calculation, cap and
 * fraction. Each carrier includes this at its original function position.
 */

static s32 _oddStrangerApplyBodyPushback(Task* task, const WorldCollisionContact* contacts, s16 contactCount)
{
#if ODD_STRANGER_VARIANT == 1
    enum { ODD_STRANGER_BODY_PUSH_MAX_OFFSET = 107 };
#else
    enum { ODD_STRANGER_BODY_PUSH_MAX_OFFSET = 150 };
#endif
    ActorBodyPushScratch* savedCursor;
    ActorBodyPushScratch* push;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    savedCursor                             = SCRATCH_STACK_CURSOR(ActorBodyPushScratch);
    push                                    = (SCRATCH_STACK_CURSOR(ActorBodyPushScratch) = savedCursor - 1);
    // Measure every overlap from part 1's current composed position.
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    push->position.vx = task->extra.tmd->coords[1].workm.t[0];
    push->position.vy = task->extra.tmd->coords[1].workm.t[1];
    push->position.vz = task->extra.tmd->coords[1].workm.t[2];
    push->hit         = 0;
    for (push->recordIndex = 0; push->recordIndex < contactCount; push->recordIndex++) {
        if (contacts[push->recordIndex].key.value == 0) {
            push->marks[push->recordIndex] = ACTOR_BODY_PUSH_MARK_END;
            break;
        }
        push->kind = contacts[push->recordIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if (push->kind == WORLD_COLLISION_CONTACT_PLAYER_BODY || push->kind == WORLD_COLLISION_CONTACT_ENEMY_BODY) {
            push->hit = 1;
#if ODD_STRANGER_VARIANT == 1
            _actorContactCalcHorizontalPushback(&push->position, &contacts[push->recordIndex], &push->offset);
#else
            worldCollisionCalcContactWorldOffset(&push->position, &contacts[push->recordIndex], &push->offset);
#endif
            push->offsetLength = push->offset.vx * push->offset.vx + push->offset.vz * push->offset.vz;
            push->offsetLength = SquareRoot0(push->offsetLength);
            // Keep both update tails: their shape preserves the signed-loop scheduling.
            if (push->offsetLength >= ODD_STRANGER_BODY_PUSH_MAX_OFFSET) {
                push->offset.vy = 0;
                VectorNormalSS(&push->offset, &push->offset);
                gte_lddp(ODD_STRANGER_BODY_PUSH_MAX_OFFSET);
                gte_ldsv(&push->offset);
                gte_gpf12();
                gte_stsv(&push->offset);
#if ODD_STRANGER_VARIANT == 1
                task->extra.tmd->coords->coord.t[0] += push->offset.vx >> 2;
#else
                task->extra.tmd->coords->coord.t[0] += push->offset.vx / 2;
#endif
#if ODD_STRANGER_VARIANT == 1
                task->extra.tmd->coords->coord.t[2] += push->offset.vz >> 2;
#else
                task->extra.tmd->coords->coord.t[2] += push->offset.vz / 2;
#endif
            } else {
#if ODD_STRANGER_VARIANT == 1
                task->extra.tmd->coords->coord.t[0] += push->offset.vx >> 2;
#else
                task->extra.tmd->coords->coord.t[0] += push->offset.vx / 2;
#endif
#if ODD_STRANGER_VARIANT == 1
                task->extra.tmd->coords->coord.t[2] += push->offset.vz >> 2;
#else
                task->extra.tmd->coords->coord.t[2] += push->offset.vz / 2;
#endif
            }
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorBodyPushScratch);
    return push->hit;
}
