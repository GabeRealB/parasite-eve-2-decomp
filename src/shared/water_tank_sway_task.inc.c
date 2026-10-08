/* Part of the water tank library; see water_tank.h. */

/// Advances the water tank's randomly driven, damped yaw motion.
///
/// Requires the room's persistent yaw words and SDK random state. One draw
/// selects a target change on the zero bucket of rand()*100/32768; a second
/// chooses a random target below 80 or zero, and only the former takes a third
/// draw. The step approaches the target by 256 even if it overshoots. Velocity
/// uses signed 19/20 damping with truncation toward zero; yaw accumulates in
/// 1/256 of a 4096th-turn angle. No angle wrapping or saturation is applied.
static inline void _waterTankAdvanceSway(void)
{
    enum { WATER_TANK_YAW_STEP               = 256,
           WATER_TANK_NONZERO_TARGET_PERCENT = 80 };

    if (((s32)(rand() * 100) >> 15) <= 0) {
        if (((s32)(rand() * 100) >> 15) < WATER_TANK_NONZERO_TARGET_PERCENT) {
            gWaterTankYawTarget = (s32)(rand() * 20) >> 7;
        } else {
            gWaterTankYawTarget = 0;
        }
    }
    if (gWaterTankYawStep < gWaterTankYawTarget) {
        gWaterTankYawStep += WATER_TANK_YAW_STEP;
    } else if (gWaterTankYawTarget < gWaterTankYawStep) {
        gWaterTankYawStep -= WATER_TANK_YAW_STEP;
    }
    gWaterTankYawSpeed =
        (gWaterTankYawSpeed + gWaterTankYawStep) * 19 / 20;
    gWaterTankYaw += gWaterTankYawSpeed;
}

void waterTankSwayTask(Task* task)
{
    enum {
        WATER_TANK_SWAY_INIT         = 0,
        WATER_TANK_SWAY_RUNNING      = 1,
        WATER_TANK_HIDDEN_VIEW       = 7,
        WATER_TANK_YAW_FRACTION_BITS = 8
    };
    TmdObject* model;
    GfxCoord*  rootCoord;
    VECTOR     worldPosition;

    rootCoord = task->extra.tmd->coords;
    model     = task->extra.tmd;
    switch (task->state) {
        case WATER_TANK_SWAY_INIT:
            model->flags          = 0;
            rootCoord->parent     = &gGfxViewCoord;
            rootCoord->coord.t[0] = 3000;
            rootCoord->coord.t[1] = -13480;
            rootCoord->coord.t[2] = -1240;
            task->state++;
            break;
        case WATER_TANK_SWAY_RUNNING:
            _waterTankAdvanceSway();
            break;
    }
    if (gGameSession->location.loc.view == WATER_TANK_HIDDEN_VIEW) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    // Sample lighting before installing this frame's new yaw basis.
    actorRenderComposeCoord(rootCoord);
    worldPosition.vx = rootCoord->workm.t[0];
    worldPosition.vy = rootCoord->workm.t[1];
    worldPosition.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &worldPosition, 0, 3);
    gfxRotMatrixY(&rootCoord->coord, gWaterTankYaw >> WATER_TANK_YAW_FRACTION_BITS, GRAPHICS_ROTATION_REPLACE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
