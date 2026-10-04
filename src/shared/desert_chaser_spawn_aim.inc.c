/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserSpawnAim(Task* arg0)
{
    Enemy*            ctx;
    DesertChaserWork* work;
    SVECTOR*          vec;
    SVECTOR*          head;
    TmdObject*        obj;
    s32               x;
    s32               z;
    SVECTOR*          gteVec;
    s32               sound;
    s32               pan;
    s32               eventPan;
    s32               state;
    u16               tick;
    Task*             player;

    work                          = arg0->work;
    player                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 2;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    ctx                           = arg0->spawnArg2.pointer;
    gteVec                        = vec;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animId                                          = 5;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        desertChaserAnimTick(arg0);
        gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, vec);
#if !DESERT_CHASER_RUN_SEQUENCE
        work->playerAnimFrames = 0;
#endif
        work->stateTimer = 0;
        VectorNormalSS(vec, vec);
        if (work->lungeDistance >= 0xFA1) {
            work->lungeDistance = 0xFA0;
        }
        gte_lddp(0x85);
        gte_ldsv(gteVec);
        gte_gpf12();
        gte_stsv(gteVec);
        x                                  = head[-2].vx;
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx   = x;
        z                                  = vec->vz;
        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        work->playerMove.keepControl       = 1;
        work->playerMove.displacement.vz   = z;
        pan                                = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(SOUND_COMMON(7), (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#if !DESERT_CHASER_RUN_SEQUENCE
        Gp_SpawnPadLerp(8, 0xFF, 8);
#endif
    }
    tick             = work->stateTimer + 1;
    work->stateTimer = tick;
    if (((s16)tick == 0xF) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        sound    = ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4001000A;
        eventPan = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(sound, (s32)eventPan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
            Gp_SpawnEff(EFFECT_DUST_PUFF, player->extra.tmd->coords + 1, 0x80003A00, NULL);
        }
    }
    desertChaserAnimTick(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
#if !DESERT_CHASER_RUN_SEQUENCE
        work->state = 0x1F;
#else
        state = work->lastCommand.word & DESERT_CHASER_COMMAND_MASK;
        if (state == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            state = 5;
        } else {
            state = 0x1F;
        }
        work->state = state;
#endif
    }
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}
