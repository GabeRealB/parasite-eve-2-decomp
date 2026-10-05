/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Setup state of the projectile task: allocates its `MaggotCaterpillarPuffWork`,
/// places its model's coordinate at the spawning model's fifth node (expressed
/// relative to the view coordinate), and links the work's collision sphere
/// with its one-entry contact table, noting the parent work's `puffCount`.
/// The enemy is destroyed when the allocation fails.
void maggotCaterpillarPuffSetup(Enemy* enemy, Task* task)
{
    Task*                      parent;
    TmdObject*                 parentObj;
    GfxCoord*                  coord;
    MaggotCaterpillarWork*     parentWork;
    GfxCoord*                  parentCoord;
    MaggotCaterpillarPuffWork* work;
    u16                        sprayOrdinal;

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
    actorRenderComposeCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(parentCoord);
    coord->parent = &gGfxViewCoord;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    work->forwardSpeed          = 0xC0;
    sprayOrdinal                = parentWork->puffCount;
    work->body.coord            = coord;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.context.contacts = work->contacts;
    work->sprayOrdinal          = sprayOrdinal;
    work->body.key              = Gp_PackPair(gMaggotCaterpillarAttacks, 2);
    work->body.radius           = 0x100;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state       = 1;
}
