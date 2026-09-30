/* Part of the burster library; see burster.h. */

/// Per-frame handler of the first enemy while it drops into place. Mode 1 of
/// `Gp_StateF0.field_4` only re-colours it and mode 2 hides the model. Otherwise, once
/// `field_2E2` has armed the drop, the root steps along its facing and by the
/// fall speed `field_2DE`, the collision response is applied, the animation
/// ticks and the root is recomputed, with the step length decaying by 2 a
/// frame. Reaching the floor (Y at or above 0) plays the landing sound, pins
/// the root at 0 and moves the enemy to the live stage with animation 2 and
/// task state 1; until then the fall speed grows by 10 a frame, or by 20 once
/// the drop has hit something.
void bursterDropState(Enemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    GfxCoord*        coord;
    s32              soundId;

    work = (Actor104600Work*)arg1->work;
    switch (Gp_StateF0.field_4) {
        case 1:
            actorUpdateColor(arg0, &arg1->extra.tmd->coords[1]);
            break;
        case 2:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case 0:
        default:
            if (work->field_2E2 == 0) {
                return;
            }
            bursterFallStep(arg1);
            bursterDropCollide(arg1);
            Actor04600_TickAnim(arg1);
            actorUpdateColor(arg0, &arg1->extra.tmd->coords[1]);
            arg1->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(arg1->extra.tmd->coords);
            work->field_2BE -= 2;
            if (work->field_2BE < 0) {
                work->field_2BE = 0;
            }
            coord = arg1->extra.tmd->coords;
            if (coord->coord.t[1] >= 0) {
                soundId = ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0008;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                work->field_2B2                     = 1;
                work->field_2C8                     = 1;
                work->field_2BE                     = 0;
                work->field_2DE                     = 0;
                work->field_2B8                     = 2;
                work->field_2BA                     = 0;
                arg1->extra.tmd->coords->coord.t[1] = 0;
                arg1->state                         = 1;
            } else if (work->field_2E0 == 0) {
                work->field_2DE += 10;
            } else {
                work->field_2DE += 20;
            }
            break;
    }
}
