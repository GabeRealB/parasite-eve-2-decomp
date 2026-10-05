/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Main enemy init. Allocates the `MadChaserWork`, points the
/// model at the light / color matrices inside it, runs the animation context,
/// links the collision objects and the enemy's list node, and enters state 2
/// for spawn kind 1 (low nibble of `spawnArg1`), state 1 otherwise. The root
/// coord is lifted by 0x3C and its translation kept as the spawn position.
///
/// `one` is a separate variable set before `sceneAcquireBattleRef`: the ROM holds
/// the constant in `$s0`, which GCC only picks for a pseudo that crosses a
/// call (sched2 then sinks the `li` below the `jal`).
void madChaserSpawn(Task* task)
{
    Enemy*         enemy;
    GfxCoord*      root;
    MadChaserWork* work;
    TmdObject*     obj;
    MadChaserWork* w;
    Enemy*         e;
    GfxCoord*      coord;
    MadChaserWork* w2;
    MadChaserWork* w3;
    MadChaserWork* w4;
    s32            one;

    enemy      = task->spawnArg2.pointer;
    root       = task->extra.tmd->coords;
    task->work = memCalloc(sizeof(MadChaserWork), 0);
    work       = (MadChaserWork*)task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    madChaserLoadSoundBank();
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
    enemy          = task->spawnArg2.pointer;
    worldTargetLinkNode(&enemy->node);
    enemy->field_4                = &task->extra.tmd->coords->coord;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = &task->extra.tmd->coords[1];
    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
    one                           = 1;
    (sceneAcquireBattleRef)(0);
    if ((task->spawnArg1.value & 0xF) == one) {
        w3           = (MadChaserWork*)task->work;
        task->state  = 2;
        w3->state    = 0;
        w3->subState = 0;
    } else {
        w4           = (MadChaserWork*)task->work;
        task->state  = one;
        w4->state    = 0;
        w4->subState = 0;
    }
    work->anchorPos.vx = root->coord.t[0];
    root->coord.t[1]  -= 0x3C;
    work->anchorPos.vy = root->coord.t[1];
    work->anchorPos.vz = root->coord.t[2];
}
