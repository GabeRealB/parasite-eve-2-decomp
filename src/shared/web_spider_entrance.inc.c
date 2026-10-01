#include "main/random.h"

/* Part of the web spider library; see web_spider.h. */

/// Behaviour state 8, a scripted entrance: hidden and undrawn until the scene
/// flag fires, then waits a per-slot delay. Variant 0 leaps out along the
/// stride table and kicks debris behind it; variant 1 drops down at a per-slot
/// fall speed amid rising dust. Either way it then joins the idle state.
void spiderEntranceState(Task* arg0)
{
    TmdObject*       obj;
    Actor105500Work* work;
    GfxCoord*        coord;
    s16(*motion)[2];
    SVECTOR* scratchEnd;
    SVECTOR* velocity;
    s32      state;
    s32      one;
    s32      pan1;
    s32      pan2;
    s32      pan3;
    s16      timer;
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
    state      = work->field_39C;
    coord      = obj->coords;
    one        = 1;
    switch (state) {
        case 0:
            work->field_294.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_214.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            obj->flags                  = (u16)obj->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            ctx->node.state.parts.flags = one;
            if (Gp_StateF0.field_1E == one) {
                if (work->field_3C2 == 0) {
                    work->field_39E = gSpiderLeapInDelay[work->field_3C4];
                } else {
                    work->field_39E = gSpiderDropInDelay[work->field_3C4];
                }
                work->field_39C = 1;
            }
            break;
        case 1:
            timer           = (u16)work->field_39E - 1;
            work->field_39E = timer;
            if (timer <= 0) {
                work->field_39E = 0;
                work->field_39C = 2;
            }
            break;
        case 2:
            Tmd_AllocBuffers(obj);
            obj->flags   = (u16)obj->flags & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            indexOrSound = 0;
            if (work->field_3C2 == 0) {
                work->field_39C = 3;
                work->field_392 = 4;
                work->field_398 = 0;
                work->field_3A6 = 0;
                work->field_3A2 = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & 0xFFF;
            } else {
                work->field_392        = 7;
                work->field_39A        = state;
                work->field_39C        = 1;
                work->field_3A8        = gSpiderDropInSpeed[work->field_3C4];
                work->field_294.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_214.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                do {
                    velocity->vx    = 0;
                    velocity->vz    = 0;
                    randomRise      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = randomRise;
                    velocity->vy    = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    Gp_SpawnEff(0x6017C, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 5);
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x510D0012;
                pan1         = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan1, (s8)gpGetObjDepth(coord));
            }
            break;
        case 3:
            if ((s16)work->field_396 == 0x1E) {
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x51090007;
                pan2         = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan2, (s8)gpGetObjDepth(coord));
            }
            indexOrSound = 0;
            if ((s16)work->field_396 == 0x27) {
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
                    Gp_SpawnEff(0x60051, coord, 0, velocity);
                    indexOrSound++;
                } while (indexOrSound < 3);
                indexOrSound = 0;
            }
            motion = gSpiderPounceStride;
            do {
                indexOrSound++;
                if ((s16)work->field_396 <= ((*motion)[0] + gSpiderPounceLead)) {
                    coord->coord.t[0] += ((*motion)[1] * rsin(work->field_3A2)) >> 12;
                    coord->coord.t[2] += ((*motion)[1] * rcos(work->field_3A2)) >> 12;
                    break;
                }
                motion++;
            } while (indexOrSound < 9);
            if ((s16)work->field_396 == 0x28) {
                indexOrSound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0002;
                pan3         = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(indexOrSound, pan3, (s8)gpGetObjDepth(coord));
                work->field_3A8        = 0x80;
                work->field_294.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_214.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            if ((s16)work->field_396 >= (gSpiderPounceLead + 0x46)) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = 1;
                randomDelay     = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->field_39E = gSpiderIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                gRandomLcgState = randomDelay;
                Gp_ArmStateF0(1);
            }
            break;
    }
    *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(SVECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}
