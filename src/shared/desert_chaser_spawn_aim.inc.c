/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserSpawnAim(Task* arg0)
{
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    DesertChaserWork* work;
#endif
    Enemy* ctx;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    DesertChaserWork* work;
#else
    SVECTOR* head;
#endif
    SVECTOR* vec;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    SVECTOR* head;
#else
    SVECTOR* gteVec;
#endif
    TmdObject* obj;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    Task* player;
#endif
    s32 x;
    s32 z;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    SVECTOR* gteVec;
#else
#endif
    s32 sound;
    s32 pan;
    s32 eventPan;
    s32 state;
    u16 tick;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    Task* player;
#else
#endif

    work                          = arg0->work;
    player                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 2;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    ctx                           = arg0->spawnArg2.pointer;
    gteVec                        = vec;
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->hitFlag = 0;
#else
#endif
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_82E          = 5;
        work->field_828          = 1;
        work->field_82A          = 0;
        work->field_83E          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserArmedAnimTick(arg0);
        Gfx_MatrixCol2(&arg0->extra.tmd->coords->coord, vec);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_C28 = 0;
#else
#endif
        work->field_6 = 0;
        VectorNormalSS(vec, vec);
        if (work->distance >= 0xFA1) {
            work->distance = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        gte_gpf12();
        gte_stsv(gteVec);
        x                 = head[-2].vx;
        work->routePos.vy = 0;
        work->routePos.vx = x;
        z                 = vec->vz;
        work->poseId      = 7;
        work->poseBlend   = 1;
        work->routePos.vz = z;
        pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(7, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        Gp_SpawnPadLerp(8, 0xFFU, 8U);
#else
#endif
    }
    tick          = work->field_6 + 1;
    work->field_6 = tick;
    if (((s16)tick == 0xF) && (work->poseId == 7)) {
        sound    = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000A;
        eventPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, (s32)eventPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
            Gp_SpawnEff(0x60054, player->extra.tmd->coords + 1, 0x80003A00, NULL);
        }
    }
    desertChaserArmedAnimTick(arg0);
    if (work->slots[1].flags & 0x100) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_0 = 0x1F;
#else
        state = work->actorId.word & 0xFFFFFF;
        if (state == 0x11402) {
            state = 5;
        } else {
            state = 0x1F;
        }
        work->field_0 = state;
#endif
    }
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}
