#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Spawn handler of the second enemy, entry 0 of `Actor04600_D0003C`. It
/// allocates the 0x2B0-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, starts animation 1 with the light blend fully up and
/// rolls the first 0x64..0xA3 frame wait, then links the three bodies with
/// their contact tables. The placement's mode is kept in `field_2AC`; mode 1
/// matching the task's `bodyKind` steps the model's texture page and CLUT
/// row and re-streams it twice. `skullStalkerExit` becomes the exit
/// callback.
void skullStalkerSpawnState(Enemy* arg0, Task* arg1)
{
    SkullStalkerWork*      work;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              part;
    u32                    seed;
    WorldCollisionContact* records1;
    WorldCollisionContact* records2;
    WorldCollisionContact* records3;
    s32                    i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    work  = memCalloc(0x2B0U, false);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_DC;
    obj->colorMtx       = &work->field_BC;
    arg0->field_4       = &coord[1].coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = part;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &gSkullStalkerParams;
    arg0->recs                   = work->field_1A4;
    arg0->hp                     = gSkullStalkerParams.hpMax;
    func_800B3F84(&work->context, gSkullStalkerAnimSets, obj, work->field_8C, work->slots);
    i = 1;
    do {
        animationResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    (Gp_IncStateF0Ref)(0);
    work->field_28C              = 1;
    work->field_28E              = 1;
    work->field_2A6              = 1;
    work->field_2A4              = 0x12;
    arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    obj->flags                   = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    seed                         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_2A8              = ((seed >> 16) & 0x3F) + 0x64;
    gRandomLcgState              = seed;
    Gp_SetLightMode(arg1->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    work->field_11C.ends[0].vz     = 0x1388;
    work->field_11C.end0Radius     = 0xFA0;
    work->field_11C.end1Radius     = 0x7D0;
    records1                       = work->field_134;
    work->field_11C.contacts       = records1;
    work->field_FC.context.capsule = &work->field_11C;
    work->field_FC.coord           = coord;
    work->field_FC.pos.vx          = 0;
    work->field_FC.pos.vy          = 0;
    work->field_FC.pos.vz          = 0;
    work->field_FC.key             = 0;
    work->field_FC.radius          = 0;
    work->field_FC.flags           = WORLD_COLLISION_BODY_CAPSULE;
    Gp_LinkObj(3, &work->field_FC);
    Gp_InitRec18Table(records1, 1, 0);
    work->field_14C.coord            = coord;
    records2                         = work->field_16C;
    work->field_14C.context.contacts = records2;
    work->field_14C.pos.vx           = 0;
    work->field_14C.pos.vy           = 0;
    work->field_14C.pos.vz           = 0;
    work->field_14C.key              = 0;
    work->field_14C.radius           = 0x7D0;
    work->field_14C.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_FC.flags             = work->field_FC.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(3, &work->field_14C);
    Gp_InitRec18Table(records2, 1, 0);
    records3                         = work->field_1A4;
    work->field_184.coord            = coord;
    work->field_184.context.contacts = records3;
    work->field_184.pos.vx           = 0;
    work->field_184.pos.vy           = -0xC8;
    work->field_184.pos.vz           = 0;
    work->field_184.key              = 0x3002F;
    work->field_184.radius           = 0xC8;
    work->field_184.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->field_14C.flags            = work->field_14C.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_LinkObj(2, &work->field_184);
    Gp_InitRec18Table(records3, 4, 0);
    work->field_184.flags = work->field_184.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->field_2AC       = arg0->place->mode;
    if (work->field_2AC == 1 && arg1->bodyKind == work->field_2AC) {
        obj->texturePageOffset++;
        obj->clutRowOffset++;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    arg1->exitCallback = skullStalkerExit;
    arg1->state++;
}
