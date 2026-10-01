/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Ramps the second enemy's light blend `field_2A4` up or down as `field_2A6`
/// says. Rising, the first frame switches the enemy to light mode 2 and the
/// blend saturates at 0x12, where the enemy's node flag and the model's 0x80
/// bit are set. Falling, leaving 0x12 clears the node flag and returns to light
/// mode 0, and the blend bottoms out at 0 with the model bits cleared.
void skullStalkerLightRamp(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;
    TmdObject*        obj;

    work  = (SkullStalkerWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    obj   = task->extra.tmd;

    if (work->field_2A6 != 0) {
        if (work->field_2A4 == 0) {
            work->field_2A4++;
            obj->flags = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        } else {
            work->field_2A4++;
            if (work->field_2A4 >= 0x12) {
                work->field_2A4               = 0x12;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                obj->flags                    = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        if (work->field_2A4 == 0x12) {
            work->field_2A4--;
            enemy->node.state.parts.flags = 0;
            obj->flags                    = TMD_OBJECT_SEMI_TRANS;
            Gp_SetLightMode(task->spawnArg2.pointer, 0);
        } else {
            work->field_2A4--;
            if (work->field_2A4 <= 0) {
                work->field_2A4 = 0;
                obj->flags      = 0;
            }
        }
    }
}
