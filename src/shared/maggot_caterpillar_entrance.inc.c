/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Releases a hidden enemy into its scripted leap or drop entrance.
///
/// Requires placement variant 0..3, initialized work and an enemy/model pair.
/// The room release starts a per-variant delay before allocating draw buffers.
/// A drop emits five leaf effects and hands over to the ambush drop; a leap
/// emits three leaves, follows the nine-row stride table, then starts roam.
/// Leaf vectors are coordinate-local spawn offsets, copied during each spawn;
/// their scratch storage is released before return. Idle placement rows are
/// 0..7. Animation counters include the clip's initial blend duration.
static void _maggotCaterpillarEntranceState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_ENTRANCE_HIDDEN           = 0,
        MAGGOT_CATERPILLAR_ENTRANCE_DELAY            = 1,
        MAGGOT_CATERPILLAR_ENTRANCE_START            = 2,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAPING          = 3,
        MAGGOT_CATERPILLAR_ENTRANCE_DROP_LEAVES      = 5,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_LEAVES      = 3,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND_FRAME = 30,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAF_FRAME       = 39,
        MAGGOT_CATERPILLAR_ENTRANCE_LAND_FRAME       = 40,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_FRAMES      = 70,
        MAGGOT_CATERPILLAR_ENTRANCE_DROP_SOUND       = 0x510D0012,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND       = 0x51090007,
        MAGGOT_CATERPILLAR_ENTRANCE_LAND_SOUND       = 0x401A0002
    };
    TmdObject*             model;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s16(*stride)[2];
    SVECTOR* scratchEnd;
    SVECTOR* spawnOffset;
    s32      entranceStep;
    s32      one;
    s32      dropAudioPan;
    s32      leapAudioPan;
    s32      landingAudioPan;
    Enemy*   enemy;
    s32      loopIndex;
    s32      soundKey;
    u32      randomY;
    u32      randomZ;
    u32      randomX;
    u32      randomDelay;
    u32      randomRise;

    model        = actor->extra.tmd;
    work         = actor->work;
    enemy        = actor->spawnArg2.pointer;
    scratchEnd   = SCRATCH_STACK_CURSOR(SVECTOR);
    spawnOffset  = (SCRATCH_STACK_CURSOR(SVECTOR) = scratchEnd - 1);
    entranceStep = work->step;
    coord        = model->coords;
    one          = WORLD_TARGET_NOT_LOCKABLE; // Retained shared value for visibility and the release test.
    switch (entranceStep) {
        case MAGGOT_CATERPILLAR_ENTRANCE_HIDDEN:
            work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            model->flags                  = (u16)model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            enemy->node.state.parts.flags = one;
            if (gSceneCombatState.maggotCaterpillarEntranceReady == one) {
                if (work->entranceKind == MAGGOT_CATERPILLAR_ENTRANCE_LEAP) {
                    work->stateCounter = gMaggotCaterpillarLeapInDelay[work->entranceSlot];
                } else {
                    work->stateCounter = gMaggotCaterpillarDropInDelay[work->entranceSlot];
                }
                work->step = MAGGOT_CATERPILLAR_ENTRANCE_DELAY;
            }
            break;
        case MAGGOT_CATERPILLAR_ENTRANCE_DELAY:
            if (--work->stateCounter <= 0) {
                work->stateCounter = 0;
                work->step         = MAGGOT_CATERPILLAR_ENTRANCE_START;
            }
            break;
        case MAGGOT_CATERPILLAR_ENTRANCE_START:
            // Restore draw storage before handing a drop to the ambush behavior.
            tmdAllocPrimitiveBuffer(model);
            model->flags = (u16)model->flags & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            loopIndex    = 0;
            if (work->entranceKind == MAGGOT_CATERPILLAR_ENTRANCE_LEAP) {
                work->step         = MAGGOT_CATERPILLAR_ENTRANCE_LEAPING;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_POUNCE;
                work->forwardSpeed = 0;
                work->turnRate     = 0;
                work->yaw          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
            } else {
                work->animId          = MAGGOT_CATERPILLAR_ANIM_DROP;
                work->behaviour       = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->step            = 1;
                work->fallSpeed       = gMaggotCaterpillarDropInSpeed[work->entranceSlot];
                work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                do {
                    spawnOffset->vx = 0;
                    spawnOffset->vz = 0;
                    randomRise      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomRise;
                    spawnOffset->vy = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LEAF, coord, 0, spawnOffset);
                    loopIndex++;
                } while (loopIndex < MAGGOT_CATERPILLAR_ENTRANCE_DROP_LEAVES);
                soundKey     = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ENTRANCE_DROP_SOUND;
                dropAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, dropAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case MAGGOT_CATERPILLAR_ENTRANCE_LEAPING:
            if (work->animFrame == MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND_FRAME) {
                soundKey     = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND;
                leapAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, leapAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            loopIndex = 0;
            if (work->animFrame == MAGGOT_CATERPILLAR_ENTRANCE_LEAF_FRAME) {
                do {
                    randomX         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomX;
                    // These randomized offsets place leaves behind the facing axis.
                    spawnOffset->vx = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomY;
                    spawnOffset->vy = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomZ;
                    spawnOffset->vz = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_FALLING_LEAF, coord, 0, spawnOffset);
                    loopIndex++;
                } while (loopIndex < MAGGOT_CATERPILLAR_ENTRANCE_LEAP_LEAVES);
                loopIndex = 0;
            }
            stride = gMaggotCaterpillarPounceStride;
            do {
                loopIndex++;
                if (work->animFrame <= ((*stride)[0] + gMaggotCaterpillarPounceLead)) {
                    coord->coord.t[0] += ((*stride)[1] * rsin(work->yaw)) >> 12;
                    coord->coord.t[2] += ((*stride)[1] * rcos(work->yaw)) >> 12;
                    break;
                }
                stride++;
            } while (loopIndex < (s32)ARRAY_SIZE(gMaggotCaterpillarPounceStride));
            if (work->animFrame == MAGGOT_CATERPILLAR_ENTRANCE_LAND_FRAME) {
                soundKey        = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ENTRANCE_LAND_SOUND;
                landingAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, landingAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->fallSpeed       = MAGGOT_CATERPILLAR_GROUND_FALL_STEP;
                work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            if (work->animFrame >= (gMaggotCaterpillarPounceLead + MAGGOT_CATERPILLAR_ENTRANCE_LEAP_FRAMES)) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                randomDelay        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                gRandomLcgState    = randomDelay;
                sceneEngageBattle(1);
            }
            break;
    }
    SCRATCH_STACK_CURSOR(SVECTOR) = SCRATCH_STACK_CURSOR(SVECTOR) + 1;
}
