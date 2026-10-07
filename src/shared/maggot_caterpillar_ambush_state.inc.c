/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Runs the hanging ambush, drop, landing and recovery sequence.
///
/// A leader triggers on player range; followers wait for the room trigger or a
/// reward, and a hit can start the landing clip directly. The placement row must
/// fit the eight-entry drop-speed and idle tables. During the drop the root
/// world matrix supplies the thread frame. Committed deaths wait for recovery.
static void _maggotCaterpillarAmbushState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_AMBUSH_WAIT               = 0,
        MAGGOT_CATERPILLAR_AMBUSH_DROP               = 1,
        MAGGOT_CATERPILLAR_AMBUSH_LAND               = 2,
        MAGGOT_CATERPILLAR_AMBUSH_GET_UP             = 3,
        MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS      = 300,
        MAGGOT_CATERPILLAR_AMBUSH_LANDING_FALL_FRAME = 12,
        MAGGOT_CATERPILLAR_AMBUSH_RECOVERY_FRAMES    = 30,
        MAGGOT_CATERPILLAR_AMBUSH_GROUND_STEP        = 128,
        MAGGOT_CATERPILLAR_AMBUSH_LAND_HEIGHT        = 1000,
        MAGGOT_CATERPILLAR_AMBUSH_DROP_SOUND         = 0x55200006,
        MAGGOT_CATERPILLAR_AMBUSH_LAND_SOUND         = 0x401A0002,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    step;
    s32                    dropPan;
    s32                    hitDropPan;
    s32                    landingPan;
    s32                    locationWord;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    s32                    playerInRange;
    s32                    soundKey;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    coord                        = actor->extra.tmd->coords;
    work                         = actor->work;
    step                         = work->step;
    locationWord                 = GAME_LOCATION_WORD(gGameSession->location.loc);
    playerInRange                = 0;
    switch (step) {
        case MAGGOT_CATERPILLAR_AMBUSH_WAIT:
            if (work->ambushFollower == 0) {
                scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                playerOffsetZ     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = playerOffsetZ;
                playerOffsetX     = scratchEnd[-1].vx;
                if (SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < MAGGOT_CATERPILLAR_AMBUSH_RANGE) {
                    playerInRange = 1;
                }
            }
            if ((playerInRange != 0) || (gSceneCombatState.maggotCaterpillarAmbushReady != 0) || (gSceneCombatState.expReward != 0)) {
                if (work->ambushFollower == 0) {
                    gSceneCombatState.maggotCaterpillarAmbushReady = 1;
                }
                sceneEngageBattle(1);
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_DROP;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_DROP;
                work->fallSpeed         = gMaggotCaterpillarDropSpeed[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->attackBody.coord  = coord;
                work->attackBody.radius = MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.pos.vy = -MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.key    = damagePackAttackKey(gMaggotCaterpillarAttacks, MAGGOT_CATERPILLAR_ATTACK_DROP);
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_AMBUSH_DROP_SOUND;
                    dropPan  = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundKey, (s32)dropPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                work->baseMatrix = coord->workm;
                break;
            }
            if (work->struck != 0) {
                if (work->ambushFollower == 0) {
                    gSceneCombatState.maggotCaterpillarAmbushReady = 1;
                }
                sceneEngageBattle(1);
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_LAND;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_LAND;
                work->fallSpeed         = gMaggotCaterpillarDropSpeed[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->threadFade        = MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
                work->attackBody.radius = MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.coord  = coord;
                work->attackBody.pos.vy = -MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.key    = damagePackAttackKey(gMaggotCaterpillarAttacks, MAGGOT_CATERPILLAR_ATTACK_DROP);
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    soundKey   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_AMBUSH_DROP_SOUND;
                    hitDropPan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundKey, (s32)hitDropPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                work->baseMatrix = coord->workm;
                break;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->baseMatrix = coord->workm;
            break;
        // Keep the upper thread endpoint fixed while the root descends.
        case MAGGOT_CATERPILLAR_AMBUSH_DROP:
            work->vertical.threadRise += work->prevPos.vy - coord->coord.t[1];
            if (((gPlayerStatus.coordMtx->t[1] - MAGGOT_CATERPILLAR_AMBUSH_LAND_HEIGHT) < coord->coord.t[1]) || (work->struck != 0) || (work->blocked != 0)) {
                work->step       = MAGGOT_CATERPILLAR_AMBUSH_LAND;
                work->animId     = MAGGOT_CATERPILLAR_ANIM_LAND;
                work->threadFade = MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->baseMatrix = coord->workm;
            break;
        case MAGGOT_CATERPILLAR_AMBUSH_LAND:
            work->fallSpeed = (work->animFrame >= MAGGOT_CATERPILLAR_AMBUSH_LANDING_FALL_FRAME) << 7;
            if (work->landed != 0) {
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_GET_UP;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_GET_UP;
                work->fallSpeed         = MAGGOT_CATERPILLAR_AMBUSH_GROUND_STEP;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                soundKey                = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_AMBUSH_LAND_SOUND;
                landingPan              = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)landingPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case MAGGOT_CATERPILLAR_AMBUSH_GET_UP:
            if (work->animFrame >= MAGGOT_CATERPILLAR_AMBUSH_RECOVERY_FRAMES) {
                sceneEngageBattle(1);
                work->behaviour         = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_WAIT;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->stateCounter      = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF);
                work->attackBody.coord  = actor->extra.tmd->coords + 4;
                work->attackBody.radius = 0xC8;
                work->attackBody.pos.vy = 0;
                work->reactionMode      = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                }
            }
            break;
    }
    _maggotCaterpillarDrawThread(actor);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
