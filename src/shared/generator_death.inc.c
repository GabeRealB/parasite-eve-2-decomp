#include "main/random.h"

/* Part of the Generator library; see generator.h. */

/// Death handler of the main task (its state 2). Death state `field_32E` 3 is
/// the wait the hit handler enters when the enemy dies: effects are spawned
/// every fourth frame until message bit 1 moves it to 0. State 0 drops the
/// enemy's lock-on node and both collision objects and starts the sequence;
/// state 1 runs it for 0x78 frames, shrinking the model towards an eighth of
/// its scale while flickering through `gGeneratorHitPulse`, spawning the
/// same two randomly offset effects every fourth frame, setting bit 1 of the
/// model's `field_C` at frame 0x14, spawning effect 0x600A5 at 0x1E and
/// switching the light mode at 0x6E, then ends in state 2. Independently,
/// `field_330` 0 calls `Gp_ReleaseStateF0Add` once with this sub-state's entry
/// of `gGeneratorReleaseIds` (message bit 2 clears the hold value 2). The
/// pose and colour are ticked every frame, and the enemy is destroyed once the
/// sequence has ended and the release has run.
void generatorDeathState(Enemy* arg0, Task* arg1)
{
    SVECTOR        ofs;
    VECTOR         pos;
    TmdObject*     obj;
    GeneratorWork* work;
    GfxCoord*      coord;
    GfxCoord*      tmp;
    GeneratorClip* clip;
    u16            scale;
    s32            r;
    s8             flag;
    s32            x;
    s32            z;
    s32            x2;
    s32            z2;

    obj   = arg1->extra.tmd;
    work  = arg1->work;
    coord = obj->coords;
    scale = 0x1000;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
    }
    if ((work->field_33A & 1) && (s16)work->field_32E == 3) {
        work->field_32E = 0;
    }
    if ((work->field_33A & 2) && (s16)work->field_330 == 2) {
        work->field_330 = 0;
    }
    switch ((s16)work->field_32E) {
        case 0:
            work->field_326 = 0x1000;
            work->field_2FC = coord->coord;
            arg0->recs      = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->node0);
            Gp_UnlinkObj(&work->node1);
            Gp_SetLightMode(arg0, 1);
            if (work->kind == 0) {
                work->field_328 = 0;
                work->field_32C = 0;
                work->field_32E = 1;
                r               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = r;
            } else {
                work->field_328 = 0;
                work->field_32C = 0;
                work->field_32E = 1;
                r               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = r;
            }
            flag                                    = 1;
            work->field_32A                         = (((u32)r >> 16) & 0xF) + 0xA;
            gSceneCombatState.generatorDeathStarted = flag;
            break;
        case 1:
            if ((s16)work->field_326 > 0x200) {
                work->field_326 -= 0x20;
            }
            switch ((s16)work->field_32C) {
                case 0:
                    work->field_32A--;
                    if ((s16)work->field_32A <= 0) {
                        work->field_32C = 1;
                    }
                    scale = work->field_326;
                    break;
                case 1:
                    clip  = &gGeneratorHitPulse[(s16)work->field_32A];
                    scale = ((s16)work->field_326 * (s16)clip->field_2) >> 12;
                    if (clip->field_0 != 0) {
                        work->field_32C = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->field_32A = ((gRandomLcgState >> 16) & 0xF) + 0xA;
                    } else {
                        work->field_32A++;
                    }
                    break;
            }
            modelPlacementSetScaled(arg1, &work->field_2FC, scale, 0);
            if (!(work->field_328 & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx          = x;
                ofs.vy          = -0x9C4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x400, &ofs);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx          = x2;
                ofs.vy          = -0x960;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x30011600, &ofs);
            }
            work->field_328++;
            if ((s16)work->field_328 == 0x14) {
                obj->flags |= TMD_OBJECT_SEMI_TRANS;
            }
            if ((s16)work->field_328 == 0x1E) {
                Gp_SpawnEff(EFFECT_CORPSE_BURN, coord, 5, NULL);
            }
            if ((s16)work->field_328 == 0x6E) {
                Gp_SetLightMode(arg0, 2);
            }
            if ((s16)work->field_328 >= 0x78) {
                work->field_32E = 2;
            }
            tmp    = arg1->extra.tmd->coords;
            pos.vx = tmp->workm.t[0];
            pos.vy = tmp->workm.t[1];
            pos.vz = tmp->workm.t[2];
            Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            break;
        case 2:
            break;
        case 3:
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->field_328++;
            if (!(work->field_328 & 3)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx          = x;
                ofs.vy          = -0x9C4;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z               = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x400, &ofs);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                x2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx          = x2;
                ofs.vy          = -0x960;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                z2              = (gRandomLcgState >> 16) & 0x3FF;
                if (!((gRandomLcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0x30011600, &ofs);
            }
            break;
    }
    if ((s16)work->field_330 == 0) {
        Gp_ReleaseStateF0Add(arg1, gGeneratorReleaseIds[work->kind]);
        work->field_330 = 1;
        Gp_ClearAreaFlag4(&gGameSession->location.loc);
    }
    generatorTickPoseInline(arg1);
    tmp    = arg1->extra.tmd->coords;
    pos.vx = tmp->workm.t[0];
    pos.vy = tmp->workm.t[1];
    pos.vz = tmp->workm.t[2];
    Gp_UpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
    if ((s16)work->field_32E == 2 && (s16)work->field_330 == 1) {
        Gp_DestroyEnemy(arg0, arg1);
    }
}
