/* Part of the Desert Chaser library; see desert_chaser.h. */

/// The heavy hit reaction: plays the stagger clip once, then returns to the
/// chase (0x24), to the buildup reaction (4) if a buildup is pending, or dies
/// (0x15).
void desertChaserStagger(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
    TmdObject*        obj;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj = arg0->extra.tmd;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        obj->flags                                            = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags                           = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = DESERT_CHASER_CLIP_STAGGER;
        work->animRate                                        = 0x10;
        work->lookYawTarget                                   = 0;
        work->waistYawTarget                                  = 0;
        if (ctx->hp <= 0) {
            sceneSetEnemyAlert(1);
        }
    }
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->state = 4;
            } else {
                work->state = 0x24;
            }
        } else {
            work->state = 0x15;
        }
    }
}
