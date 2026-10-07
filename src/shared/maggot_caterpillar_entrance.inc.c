#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_ENTRANCE`, a scripted entrance: hidden and
/// undrawn until the scene flag fires, then waits a per-slot delay. Variant 0
/// leaps out along the stride table and kicks debris behind it; variant 1 drops
/// down at a per-slot fall speed amid rising dust. The leap ends in
/// `MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM`; the drop carries on as the drop of
/// `MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH`.
void maggotCaterpillarEntranceState(Task* arg0)
{
    TmdObject*             obj;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s16(*motion)[2];
    SVECTOR* scratchEnd;
    SVECTOR* velocity;
    s32      state;
    s32      one;
    s32      pan1;
    s32      pan2;
    s32      pan3;
    Enemy*   ctx;
    s32      indexOrSound;
    u32      randomY;
    u32      randomZ;
    u32      randomX;
    u32      randomDelay;
    u32      randomRise;

    obj        = arg0->extra.tmd;
    work       = arg0->work;
    ctx        = arg0->spawnArg2.pointer;
    scratchEnd = *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    velocity   = (*(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = scratchEnd - 1);
    state      = work->step;
    coord      = obj->coords;
    one        = 1;
    switch (state) {
        case 0:
            work->body.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->gridBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            obj->flags                  = (u16)obj->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = one;
            if (gSceneCombatState.maggotCaterpillarEntranceReady == one) {
                if (work->entranceKind == 0) {
                    work->stateCounter = gMaggotCaterpillarLeapInDelay[work->entranceSlot];
                } else {
                    work->stateCounter = gMaggotCaterpillarDropInDelay[work->entranceSlot];
                }
                work->step = 1;
            }
            break;
        case 1:
            if (--work->stateCounter <= 0) {
                work->stateCounter = 0;
                work->step         = 2;
            }
            break;
        case 2:
            tmdAllocPrimitiveBuffer(obj);
            obj->flags   = (u16)obj->flags & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            indexOrSound = 0;
            if (work->entranceKind == 0) {
                work->step         = 3;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_POUNCE;
                work->forwardSpeed = 0;
                work->turnRate     = 0;
                work->yaw          = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
            } else {
                work->animId          = MAGGOT_CATERPILLAR_ANIM_DROP;
                work->behaviour       = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->step            = 1;
                work->fallSpeed       = gMaggotCaterpillarDropInSpeed[work->entranceSlot];
                work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                do {
                    velocity->vx    = 0;
                    velocity->vz    = 0;
                    randomRise      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomRise;
                    velocity->vy    = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    effectSpawn(EFFECT_ACROPOLIS_ROOF_GARDEN_LEAF, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 5);
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x510D0012;
                pan1         = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(indexOrSound, pan1, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 3:
            if (work->animFrame == 0x1E) {
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51090007;
                pan2         = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(indexOrSound, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            indexOrSound = 0;
            if (work->animFrame == 0x27) {
                do {
                    randomX         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomX;
                    velocity->vx    = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomY;
                    velocity->vy    = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomZ;
                    velocity->vz    = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    effectSpawn(EFFECT_ACROPOLIS_FORKED_ROAD_FALLING_LEAF, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 3);
                indexOrSound = 0;
            }
            motion = gMaggotCaterpillarPounceStride;
            do {
                indexOrSound++;
                if (work->animFrame <= ((*motion)[0] + gMaggotCaterpillarPounceLead)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->yaw)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->yaw)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if (work->animFrame == 0x28) {
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan3         = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(indexOrSound, pan3, (s8)worldCoordGetOriginAudioDepth(coord));
                work->fallSpeed       = 0x80;
                work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            if (work->animFrame >= (gMaggotCaterpillarPounceLead + 0x46)) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                randomDelay        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                gRandomLcgState    = randomDelay;
                sceneEngageBattle(1);
            }
            break;
    }
    *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}
