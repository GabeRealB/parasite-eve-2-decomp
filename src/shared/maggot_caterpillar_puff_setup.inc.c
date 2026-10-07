/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Allocates a puff work block and launches it from its parent actor fifth node.
///
/// The task owns a coordinate body, has a live TMD parent with at least five
/// nodes and uses its own enemy context for teardown. The launch transform is
/// expressed beneath the view coordinate. Its owned collision sphere is linked
/// to the enemy-attack list and borrows the work contact array. Allocation
/// failure destroys the child; success advances to the flying task phase.
static void _maggotCaterpillarPuffSetup(Enemy* enemy, Task* task)
{
    Task*                      parent;
    TmdObject*                 parentModel;
    GfxCoord*                  coord;
    MaggotCaterpillarWork*     parentWork;
    GfxCoord*                  parentCoord;
    MaggotCaterpillarPuffWork* work;
    u16                        sprayOrdinal;

    parent      = task->parent;
    parentModel = parent->extra.tmd;
    coord       = task->extra.coordBody->coord;
    parentWork  = parent->work;
    parentCoord = &parentModel->coords[4];
    work        = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Detach the launch pose into the view frame before linking collision.
    task->work                 = work;
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    parentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(parentCoord);
    coord->parent = &gGfxViewCoord;
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &parentCoord->workm, &coord->coord);
    coord->composeStamp         = GRAPHICS_COORD_DIRTY;
    work->forwardSpeed          = MAGGOT_CATERPILLAR_PUFF_INITIAL_SPEED;
    sprayOrdinal                = parentWork->puffCount;
    work->body.coord            = coord;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = 0;
    work->body.pos.vz           = 0;
    work->body.context.contacts = work->contacts;
    work->sprayOrdinal          = sprayOrdinal;
    work->body.key              = damagePackAttackKey(gMaggotCaterpillarAttacks, MAGGOT_CATERPILLAR_ATTACK_PUFF);
    work->body.radius           = 0x100;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->body.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    task->state       = MAGGOT_CATERPILLAR_PUFF_TASK_FLY;
}
