/* Part of the Glutton library; see glutton.h. */

/// Rise state of the enemy dispatched through `D_actor_444000_80131EA8`: for
/// the first nine steps the model is stretched taller and thinner each step --
/// horizontally `step * 400 + 0x800` and vertically `0x800 / step` -- around
/// the yaw it already faces. On step 7 it is squashed to 0x17A0 wide at normal
/// height, and if the player is within 1000 units horizontally, is not in mode
/// 2, still has HP and answers the 0x3F8 query, the overlay's own animation-set
/// table is sent as message 0x3FF and the take-over is latched in `field_1B2`.
/// The task steps on once the count passes ten with no animation installed,
/// once the latched animation has been released, or after 200 steps. Every
/// step refreshes the model's colour from its world position and damps the two
/// shake terms. Bails to `Gp_DestroyEnemy` when the overlay is shutting down,
/// cancelling a still-installed animation on the way out.
void gluttonGlobEngulf(Enemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Task*                player;
    GameActor*           actor;
    PlayerStatus*        cfg;
    SVECTOR              gap;
    VECTOR               pos;
    s16                  step;
    s16                  scale;
    s32                  shrink;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor  = (GameActor*)player->work;
    cfg    = &gPlayerStatus;

    if (gGluttonEnded == 1) {
        if (work->field_1B2 == 1) {
            Gp_DispatchMsg(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 2, 0);
            work->field_1B2 = 0;
        }
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    step = ++work->field_1AC;
    if (step < 10) {
        scale  = step * 0x190 + 0x800;
        shrink = 0x800 / step;
        gluttonScaleRotation(task->extra.tmd->coords, scale, shrink);
    }

    if (work->field_1AC == 7) {
        gluttonScaleRotation(task->extra.tmd->coords, 0x17A0, 0x800);

        gap.vx = task->extra.tmd->coords->coord.t[0] -
                 player->extra.tmd->coords->coord.t[0];
        gap.vy = 0;
        gap.vz = task->extra.tmd->coords->coord.t[2] -
                 player->extra.tmd->coords->coord.t[2];

        if (actorOutOfReach(&gap) == 0 && actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
            cfg->hp > 0) {
            gGluttonGrabQuery.value.field_14 = 0x28;
            if (Gp_DispatchMsgPtr(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, &gGluttonGrabQuery.value, 0) == 0) {
                gGluttonGrabActive     = 1;
                work->anim.source.sets = gGluttonCaughtAnimSets;
                work->anim.animationId = 1;
                work->anim.blend       = ANIMATION_BLEND_RESET;
                work->anim.blendFrames = 3;
                Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->anim, 0);
                work->field_1B2 = 1;
            }
        }
    }

    if (work->field_1AC >= 11 && work->field_1B2 == 0) {
        task->state++;
    } else if (work->field_1B2 == 1 && gGluttonGrabActive == 0) {
        task->state++;
    } else if (work->field_1AC >= 0xC9) {
        gGluttonGrabActive = 0;
        task->state++;
    }

    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}
