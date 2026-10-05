/* Part of the water tank library; see water_tank.h. */

/// Per-frame model update for the tank: the callback word at 0x801868A8 in the
/// room's task table `D_dryfield_water_tank_801868A4`. State 0 parents the
/// model's coordinate to `gGfxViewCoord` and places it against the room's north
/// wall, then advances to state 1. State 1 drives the tank's slow wobble about
/// `y`:
/// an occasional roll re-picks the target yaw, the step moves toward it 0x100
/// at a time, and the velocity follows 19/20 of the way to that step. Either
/// way the frame ends by lighting the model at its `workm` translation through
/// `worldCoordSetModelLighting`, rebuilding the coordinate's yaw matrix from the
/// accumulated angle, and clearing `composeStamp` so the parent recomputes the world
/// matrix next frame.
///
/// The coordinate's load is written through the cast expression, *before* the
/// object pointer is assigned, because the pointer assignment has to stay a
/// separate register copy: assigned first, cse.c's `(set REG0 REG1)` swap folds
/// the load and the copy into one and the overlay comes up an `addu` short (see
/// DECOMPILATION_LEARNINGS.md, "A load the pointer variable must copy").
void waterTankSwayTask(Task* arg0)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    coord = arg0->extra.tmd->coords;
    obj   = arg0->extra.tmd;
    switch (arg0->state) {
        case 0:
            obj->flags        = 0;
            coord->parent     = &gGfxViewCoord;
            coord->coord.t[0] = 0xBB8;
            coord->coord.t[1] = -0x34A8;
            coord->coord.t[2] = -0x4D8;
            arg0->state++;
            break;
        case 1:
            if (((s32)(rand() * 100) >> 15) <= 0) {
                if (((s32)(rand() * 100) >> 15) < 0x50) {
                    gWaterTankYawTarget = (s32)(rand() * 20) >> 7;
                } else {
                    gWaterTankYawTarget = 0;
                }
            }
            if (gWaterTankYawStep < gWaterTankYawTarget) {
                gWaterTankYawStep += 0x100;
            } else if (gWaterTankYawTarget < gWaterTankYawStep) {
                gWaterTankYawStep -= 0x100;
            }
            gWaterTankYawSpeed =
                (gWaterTankYawSpeed + gWaterTankYawStep) * 19 / 20;
            gWaterTankYaw += gWaterTankYawSpeed;
            break;
    }
    if (gGameSession->location.loc.view == 7) {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags = 0;
    }
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    gfxRotMatrixY(&coord->coord, gWaterTankYaw >> 8, 1);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
