/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH`: waits until the player is within
/// `MAGGOT_CATERPILLAR_AMBUSH_RANGE` (or another of the room's ambushers has
/// sprung), then drops on its line at its row's `gMaggotCaterpillarDropSpeed`,
/// lands, and joins the fight.
void maggotCaterpillarAmbushState(Task* actor)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    pan0;
    s32                    pan1;
    s32                    pan2;
    s32                    locationWord;
    s32                    dx;
    s32                    dz;
    s32                    value;
    s32                    sound;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    coord                                                                     = actor->extra.tmd->coords;
    work                                                                      = actor->work;
    state                                                                     = work->step;
    locationWord                                                              = GAME_LOCATION_WORD(gGameSession->location.loc);
    value                                                                     = 0;
    switch (state) {
        case 0:
            if (work->ambushFollower == 0) {
                scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                dz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = dz;
                dx                = scratchEnd[-1].vx;
                if (SquareRoot0((dx * dx) + (dz * dz)) < MAGGOT_CATERPILLAR_AMBUSH_RANGE) {
                    value = 1;
                }
            }
            if ((value != 0) || (gSceneCombatState.maggotCaterpillarAmbushReady != 0) || (gSceneCombatState.expReward != 0)) {
                if (work->ambushFollower == 0) {
                    gSceneCombatState.maggotCaterpillarAmbushReady = 1;
                }
                Gp_ArmStateF0(1);
                work->step              = 1;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_DROP;
                work->fallSpeed         = gMaggotCaterpillarDropSpeed[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->attackBody.coord  = coord;
                work->attackBody.radius = 0x12C;
                work->attackBody.pos.vy = -0x12C;
                work->attackBody.key    = damagePackAttackKey(gMaggotCaterpillarAttacks, 5);
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55200006;
                    pan0  = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(sound, (s32)pan0, (s8)worldCoordGetOriginAudioDepth(coord));
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
                Gp_ArmStateF0(1);
                work->step              = 2;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_LAND;
                work->fallSpeed         = gMaggotCaterpillarDropSpeed[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->threadFade        = 0x2D;
                work->attackBody.radius = 0x12C;
                work->attackBody.coord  = coord;
                work->attackBody.pos.vy = -0x12C;
                work->attackBody.key    = damagePackAttackKey(gMaggotCaterpillarAttacks, 5);
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    sound = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x55200006;
                    pan1  = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(sound, (s32)pan1, (s8)worldCoordGetOriginAudioDepth(coord));
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
        case 1:
            work->vertical.threadRise += work->prevPos.vy - coord->coord.t[1];
            if (((gPlayerStatus.coordMtx->t[1] - 0x3E8) < coord->coord.t[1]) || (work->struck != 0) || (work->blocked != 0)) {
                work->step       = 2;
                work->animId     = MAGGOT_CATERPILLAR_ANIM_LAND;
                work->threadFade = 0x2D;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->baseMatrix = coord->workm;
            break;
        case 2:
            work->fallSpeed = (work->animFrame >= 0xC) << 7;
            if (work->landed != 0) {
                work->step              = 3;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_GET_UP;
                work->fallSpeed         = 0x80;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                sound                   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan2                    = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(sound, (s32)pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 3:
            if (work->animFrame >= 0x1E) {
                Gp_ArmStateF0(1);
                work->behaviour         = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step              = 0;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->stateCounter      = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 0x10) & 0xF);
                work->attackBody.coord  = actor->extra.tmd->coords + 4;
                work->attackBody.radius = 0xC8;
                work->attackBody.pos.vy = 0;
                work->reactionMode      = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = 2;
                }
            }
            break;
    }
    maggotCaterpillarDrawThread(actor);
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}
