/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Detaches the command drop-death's pair, grid and attack collision spheres.
///
/// `bodyWork` must be live with valid list links on any linked body. Unlinks in
/// pair/grid/attack order, clearing linked bodies' pass enables and list links.
/// The work block, body shapes and contact storage remain task-owned and live.
static __inline__ void _madChaserDropBodiesUnlink(MadChaserWork* bodyWork)
{
    worldCollisionUnlinkBody(&bodyWork->pairBody);
    worldCollisionUnlinkBody(&bodyWork->gridBody);
    worldCollisionUnlinkBody(&bodyWork->attackBody);
}

/// Detaches command drop-death from collision and enters its final hold.
///
/// Requires live enemy and task-owned Mad Chaser work in drop-death behavior 3.
/// Clears the enemy's contact-record pointer and unlinks pair, grid and attack
/// bodies. Their storage and the model remain task-owned. Clears stateFrames
/// and advances to behavior 4 without changing animation or root transforms.
static void _madChaserDropBodies(Task* task)
{
    MadChaserWork* bodyWork;
    MadChaserWork* work;

    work                                    = task->work;
    ((Enemy*)task->spawnArg2.pointer)->recs = NULL;
    bodyWork                                = task->work;
    _madChaserDropBodiesUnlink(bodyWork);
    work->stateFrames = 0;
    work->state       = work->state + 1;
}
