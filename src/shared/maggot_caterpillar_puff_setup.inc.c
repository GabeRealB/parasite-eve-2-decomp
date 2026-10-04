/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Setup state of the projectile task: allocates its 0x40-byte work, places
/// its model's coordinate at the spawning model's fifth node (expressed
/// relative to the view coordinate), and links the work's collision object
/// with its single record, carrying the parent work's `puffCount`. The enemy
/// is destroyed when the allocation fails.
void maggotCaterpillarPuffSetup(Enemy* enemy, Task* task)
{
    Task*                        parent;
    TmdObject*                   parentObj;
    GfxCoord*                    coord;
    MaggotCaterpillarWork*       parentWork;
    GfxCoord*                    parentCoord;
    ActorsShared80135c4cObjWork* work;
    u16                          pair;

    parent      = task->parent;
    parentObj   = parent->extra.tmd;
    coord       = task->extra.tmd->coords;
    parentWork  = parent->work;
    parentCoord = &parentObj->coords[4];
    work        = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                 = work;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(parentCoord);
    coord->parent = &gGfxViewCoord;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->composeStamp        = GRAPHICS_COORD_DIRTY;
    work->field_3A             = 0xC0;
    pair                       = parentWork->puffCount;
    work->obj.coord            = coord;
    work->obj.pos.vx           = 0;
    work->obj.pos.vy           = 0;
    work->obj.pos.vz           = 0;
    work->obj.context.contacts = &work->rec;
    work->field_3C             = pair;
    work->obj.key              = Gp_PackPair(gMaggotCaterpillarAttacks, 2);
    work->obj.radius           = 0x100;
    work->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(&work->rec, 1, 0);
    work->obj.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state      = 1;
}
