/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Variant of `hopperSpawn`'s init: also destroys the enemy
/// when bit 16 of `spawnArg1` is set,
/// sets bit 0x80 of the model's `field_C` for spawn kind 2, and enters state 6
/// with `field_451` set and the collision flags 0x8000 / 0x4000 cleared on
/// `obj_2AC` / `obj_2CC`.
///
/// `two` is a variable for the same reason as `one` in the sibling: the ROM
/// keeps the constant in `$s5` across the calls. `kind` has to be its own
/// variable too - masking `flags` in place reuses `$v1` for the result.
void hopperSpawnHidden(Task* task)
{
    TmdObject*       model;
    Enemy*           enemy;
    GfxCoord*        root;
    Actor341700Work* work;
    TmdObject*       obj;
    Actor341700Work* w;
    Enemy*           e;
    GfxCoord*        coord;
    Actor341700Work* w2;
    Actor341700Work* w3;
    Enemy*           e2;
    s32              flags;
    s32              kind;
    s32              two;

    model      = task->extra.tmd;
    enemy      = task->spawnArg2.pointer;
    root       = model->coords;
    task->work = memCalloc(0x454, 0);
    work       = (Actor341700Work*)task->work;
    if (work == NULL) {
        goto destroy;
    }
    hopperLoadSoundBank();
    flags = task->spawnArg1.value;
    if ((flags >> 16) & 1) {
    destroy:
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    kind = flags & 0xF;
    two  = 2;
    if (kind == two) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    obj                   = task->extra.tmd;
    w                     = (Actor341700Work*)task->work;
    e                     = task->spawnArg2.pointer;
    coord                 = obj->coords;
    task->msgTable        = gHopperMsgTable;
    obj->lightMtx         = &w->lightMtx;
    obj->colorMtx         = &w->colorMtx;
    e->param              = &gHopperEnemyParams;
    e->recs               = w->rec_2EC;
    w->eff_3FC.coord      = &task->extra.tmd->coords[1];
    w->eff_3FC.spawnArgLo = 0x140;
    w->eff_3FC.spawnArgHi = two;
    e->hp = e->hpMax = gHopperEnemyParams.hpMax;
    func_800B3F84(&w->anim, gHopperAnimBank, obj, w->field_21C, &w->slot_B4);
    w2            = (Actor341700Work*)task->work;
    w2->field_41C = 0x10;
    w2->field_418 = 7;
    w2->field_414 = two;
    hopperTickAnim(task);
    coord->parent = &gGfxViewCoord;
    hopperLinkBodies(task);
    w->field_7A = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    (Gp_IncStateF0Ref)(0);
    e2 = task->spawnArg2.pointer;
    Gp_LinkNode(&e2->node);
    e2->field_4                = &task->extra.tmd->coords->coord;
    e2->field_48               = 0;
    e2->bodyPos.vx             = 0;
    e2->bodyPos.vy             = 0;
    e2->bodyPos.vz             = 0;
    e2->coord                  = &task->extra.tmd->coords[1];
    e2->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    work->field_80             = root->coord.t[0];
    root->coord.t[1]          -= 0x3C;
    work->field_82             = root->coord.t[1];
    work->field_84             = root->coord.t[2];
    work->field_451            = 1;
    work->obj_2AC.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj_2CC.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    w3                         = (Actor341700Work*)task->work;
    task->state                = 6;
    w3->field_420              = 0;
    w3->field_422              = 0;
}
