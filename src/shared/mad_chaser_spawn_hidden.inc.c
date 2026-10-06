/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Variant of `madChaserSpawn`'s init: also destroys the enemy
/// when bit 16 of `spawnArg1` is set,
/// sets bit 0x80 of the model's `field_C` for spawn kind 2, and enters state 6
/// with `shadowHidden` set and the collision flags 0x8000 / 0x4000 cleared on
/// `pairBody` / `gridBody`.
///
/// `kind` has to be its own variable - masking `flags` in place reuses `$v1`
/// for the result.
void madChaserSpawnHidden(Task* task)
{
    TmdObject*     model;
    Enemy*         enemy;
    GfxCoord*      root;
    MadChaserWork* work;
    TmdObject*     obj;
    MadChaserWork* w;
    Enemy*         e;
    GfxCoord*      coord;
    MadChaserWork* w2;
    MadChaserWork* w3;
    Enemy*         e2;
    s32            flags;
    s32            kind;

    model      = task->extra.tmd;
    enemy      = task->spawnArg2.pointer;
    root       = model->coords;
    task->work = memCalloc(sizeof(MadChaserWork), 0);
    work       = (MadChaserWork*)task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    madChaserLoadSoundBank();
    flags = task->spawnArg1.value;
    if ((flags >> 16) & 1) {
        enemyDestroy(enemy, task);
        return;
    }
    kind = flags & 0xF;
    if (kind == 2) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    obj                     = task->extra.tmd;
    w                       = (MadChaserWork*)task->work;
    e                       = task->spawnArg2.pointer;
    coord                   = obj->coords;
    task->msgTable          = gMadChaserMsgTable;
    obj->lightMtx           = &w->lightMtx;
    obj->colorMtx           = &w->colorMtx;
    e->param                = &gMadChaserEnemyParams;
    e->recs                 = w->contacts;
    w->effectArg.coord      = &task->extra.tmd->coords[1];
    w->effectArg.spawnArgLo = 0x140;
    w->effectArg.spawnArgHi = 2;
    e->hp = e->hpMax = gMadChaserEnemyParams.hpMax;
    animationInitContext(&w->anim, (AnimationSet**)gMadChaserAnimBank, obj, w->poses, w->slots);
    w2              = (MadChaserWork*)task->work;
    w2->animRate    = ANIMATION_RATE_ONE;
    w2->animId      = 7;
    w2->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    madChaserTickAnim(task);
    coord->parent = &gGfxViewCoord;
    madChaserLinkBodies(task);
    w->rotation.vy = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]) + 0x800;
    (sceneAcquireBattleRef)(0);
    e2 = task->spawnArg2.pointer;
    worldTargetLinkNode(&e2->node);
    e2->field_4                = &task->extra.tmd->coords->coord;
    e2->field_48               = 0;
    e2->bodyPos.vx             = 0;
    e2->bodyPos.vy             = 0;
    e2->bodyPos.vz             = 0;
    e2->coord                  = &task->extra.tmd->coords[1];
    e2->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    work->anchorPos.vx         = root->coord.t[0];
    root->coord.t[1]          -= 0x3C;
    work->anchorPos.vy         = root->coord.t[1];
    work->anchorPos.vz         = root->coord.t[2];
    work->shadowHidden         = 1;
    work->pairBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    w3                         = (MadChaserWork*)task->work;
    task->state                = 6;
    w3->state                  = 0;
    w3->subState               = 0;
}
