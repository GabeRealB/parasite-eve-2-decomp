/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserCollapse(Task* arg0)
{
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    Enemy* ctx;
#else
#endif
    TmdObject*        obj;
    DesertChaserWork* work;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    Enemy* ctx;
#endif

    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj = arg0->extra.tmd;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->hitFlag = 0;
#else
#endif
        obj->flags                  = 0;
        work->objs[0].obj.radius    = 0x19C;
        work->objs[2].obj.flags    |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_828             = 1;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_82E = 0x13;
#else
        work->field_82E = 0x16;
#endif
        work->field_832 = 0x10;
        work->field_840 = 0;
        work->field_83E = 0;
        if (ctx->hp <= 0) {
            Gp_SetStateF0Byte3(1);
        }
    }
    desertChaserAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
        work->field_0 = 0x15;
    }
}
