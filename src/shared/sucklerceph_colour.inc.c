/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Updates the Sucklerceph's lighting and hit colour at model part 1.
///
/// `enemy` must belong to `task`, whose model has a freshly composed part 1
/// and live work-owned light and colour matrices. The sampled translation is
/// in composition space. Reserves one `VECTOR` across the lighting call and
/// releases it before return; its fourth word is unused.
static void _sucklercephColour(Enemy* enemy, Task* task)
{
    GfxCoord* sampleCoord;
    VECTOR*   position;

    sampleCoord  = &task->extra.tmd->coords[1];
    position     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    position->vx = sampleCoord->workm.t[0];
    position->vy = sampleCoord->workm.t[1];
    position->vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, position, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}
