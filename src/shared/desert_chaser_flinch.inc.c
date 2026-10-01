/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserFlinch(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    TmdObject* obj;
#else
    Enemy* enemy;
#endif

    work = arg0->work;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    ctx = arg0->spawnArg2.pointer;
#else
    enemy = arg0->spawnArg2.pointer;
#endif
    if (work->field_4 != 0) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        obj           = arg0->extra.tmd;
        work->hitFlag = 0;
        obj->flags    = 0;
#else
        arg0->extra.tmd->flags = 0;
#endif
        work->objs[0].obj.radius = 0x19C;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        ctx->node.state.parts.flags = 0;
#else
        enemy->node.state.parts.flags = 0;
#endif
        work->field_828 = 1;
        work->field_82E = 0xA;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
        work->field_832 = 0x10;
#endif
        work->field_844 = 0;
        work->field_840 = 0;
        work->field_83E = 0;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_832 = work->field_834;
        if (ctx->hp <= 0) {
#else
        if (enemy->hp <= 0) {
#endif
            Gp_SetStateF0Byte3(1);
        }
    }
    desertChaserAnimTick(arg0);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    if ((work->slots[1].flags & 0x100) && (work->field_82E == 0xA)) {
        if (ctx->hp > 0) {
            if (ctx->reactionFlags & ENEMY_REACTION_BUILDUP) {
#else
    if (work->slots[1].flags & 0x100) {
        if (work->field_82E == 0xA) {
            if (enemy->hp > 0) {
                if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
#endif
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        } else {
            work->field_0 = 0x15;
        }
    }
}
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
}
#endif
