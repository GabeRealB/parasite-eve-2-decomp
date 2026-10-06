/* Part of the Generator library; see generator.h. */

/// Tick handler of the main task (its state 1), switched on the gameplay mode
/// `gSceneCombatState.actorControl`. Mode 1 only updates the colour; mode 2 sets the model's
/// `field_C` to 0x80 and the lock-on node not lockable, and stops there. Any
/// other mode runs the frame - mode 0 first clearing the model's `field_C` and
/// hiding the node's HP: the hit handler, the idle schedule, the pose
/// tick, the model's coordinate refresh, the colour update and the
/// regeneration step.
void generatorTickState(Enemy* arg0, Task* arg1)
{
    GfxCoord*  temp_s1;
    TmdObject* temp_a1;

    temp_a1 = arg1->extra.tmd;
    temp_s1 = temp_a1->coords;
    switch (gSceneCombatState.actorControl) {
        case 0:
            temp_a1->flags               = 0;
            arg0->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case 1:
            generatorUpdateColor(arg1);
            return;
        case 2:
            temp_a1->flags               = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        default:
            break;
    }
    generatorBodyHit(arg1);
    generatorPulse(arg1);
    generatorTickPose(arg1);
    temp_s1->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(temp_s1);
    generatorUpdateColor(arg1);
    generatorRegenerate(arg1);
}
