/* Part of the burster library; see burster.h. */

/// Spawn handler of the first enemy, entry 0 of `Actor04600_D00004`. A spawn
/// arg whose high halfword is 1 destroys the enemy instead. Otherwise it
/// allocates the 0x2E4-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, then links the four bodies with their contact tables
/// and installs `bursterExit` as the exit callback. The spawn arg's two
/// halves are kept in `field_2DC`/`field_2D6`; a low half of 1 matching the
/// task's `bodyKind` steps the model's texture page and CLUT row and
/// re-streams it twice.
void bursterSpawnState(Enemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        part;
    u16              v;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1.value >> 16) == 1) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    work = memCalloc(0x2E4U, false);
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
    arg0->param                  = &gBursterParams;
    arg0->recs                   = &work->rec154[0];
    arg0->hp                     = gBursterParams.hpMax;
    func_800B3F84(&work->context, gBursterAnimSets, obj, work->field_8C, work->slots);
    i = 1;
    do {
        Gp_AnimResetSlot(&work->context, i, 1);
        i += 1;
    } while (i < 3);
    (Gp_IncStateF0Ref)(0);
    work->field_2B8              = 1;
    work->field_2BA              = 1;
    work->field_2AC              = 0x1000;
    work->field_2DA              = 0;
    work->field_2CE              = 0;
    work->field_2D4              = 0;
    work->field_2D2              = 0;
    work->field_2CC              = 0;
    arg1->killCountdown          = 0;
    work->field_284.coord        = &arg1->extra.tmd->coords[1];
    work->field_284.spawnArgLo   = 0x100;
    work->field_284.spawnArgHi   = 1;
    work->objFC.coord            = coord;
    work->objFC.context.contacts = &work->rec11C;
    work->objFC.pos.vx           = 0;
    work->objFC.pos.vy           = 0;
    work->objFC.pos.vz           = 0;
    work->objFC.key              = 0;
    work->objFC.radius           = 0xBB8;
    work->objFC.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->objFC);
    Gp_InitRec18Table(&work->rec11C, 1, 0);
    work->obj134.coord            = coord;
    work->obj134.context.contacts = &work->rec154[0];
    work->obj134.pos.vx           = 0;
    work->obj134.pos.vy           = -0xC8;
    work->obj134.pos.vz           = 0;
    work->obj134.key              = 0x3002E;
    work->obj134.radius           = 0xC8;
    work->obj134.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->objFC.flags             = (u16)(work->objFC.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->rec154[0], 4, 0);
    work->obj1B4.coord            = coord;
    work->obj1B4.context.contacts = &work->rec1D4;
    work->obj1B4.pos.vx           = 0;
    work->obj1B4.pos.vy           = 0;
    work->obj1B4.pos.vz           = 0;
    work->obj134.flags            = (u16)(work->obj134.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->obj1B4.key              = Gp_PackPair(&gBursterAttack, 0);
    work->obj1B4.radius           = 0x3E8;
    work->obj1B4.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj1B4);
    Gp_InitRec18Table(&work->rec1D4, 1, 0);
    work->obj1EC.coord            = coord;
    work->obj1EC.context.contacts = &work->rec20C;
    work->obj1EC.pos.vx           = 0;
    work->obj1EC.pos.vy           = 0;
    work->obj1EC.pos.vz           = 0;
    work->obj1EC.key              = 0x22323;
    work->obj1EC.radius           = 0x3E8;
    work->obj1EC.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->obj1B4.flags            = (u16)(work->obj1B4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    Gp_LinkObj(8, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->field_2DC    = (s16)(arg1->spawnArg1.value >> 16);
    v                  = (u16)arg1->spawnArg1.value;
    work->field_2D6    = v;
    if ((s16)v == 1 && arg1->bodyKind == (s16)v) {
        obj->texturePageOffset = obj->texturePageOffset + 1;
        obj->clutRowOffset     = obj->clutRowOffset + 1;
        if (obj->buffer != 0) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
    work->field_2B4    = 0;
    arg1->exitCallback = bursterExit;
    arg1->state       += 1;
}
