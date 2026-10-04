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
    if (work->field_4 != 0) {
        obj = arg0->extra.tmd;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        obj->flags                  = 0;
        work->objs[0].body.radius   = 0x19C;
        work->objs[2].body.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags = 0;
        work->field_828             = 1;
        work->field_82E             = DESERT_CHASER_CLIP_STAGGER;
        work->field_832             = 0x10;
        work->field_840             = 0;
        work->field_83E             = 0;
        if (ctx->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    desertChaserAnimTick(arg0);
    if (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x24;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}
