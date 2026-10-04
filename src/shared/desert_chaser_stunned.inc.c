/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserStunned(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    s32               value;
    u32               magnitude;

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->animId                                         = DESERT_CHASER_CLIP_STUNNED;
        work->animRequest                                    = DESERT_CHASER_ANIM_REQUEST_RESET;
        work->animRate                                       = 0x10;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        do {
            desertChaserAnimTick(arg0);
        } while ((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) != 0xC);
        work->animRate = 0x20;
        return;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    value                                 = (s16)work->animRate / 2;
    work->animRate                        = (u16)value;
    magnitude                             = 0x10U;
    if (value == 1) {
        work->animRate = -magnitude;
    }
    if ((s16)work->animRate == -1) {
        work->animRate = 0x10;
    }
    desertChaserAnimTick(arg0);
    if (Gp_TickObjFlag2(ctx) == 1) {
        ctx->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
        work->state         = 0x24;
    }
}
