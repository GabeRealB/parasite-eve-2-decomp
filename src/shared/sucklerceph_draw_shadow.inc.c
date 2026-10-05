/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Draws the first enemy's ground shadow under the model root, at the world
/// translation of the root part staged in a `VECTOR3` on the scratch stack.
void sucklercephDrawShadow(Task* task)
{
    GfxCoord* coord;
    VECTOR3*  vec;

    coord   = task->extra.tmd->coords;
    vec     = (VECTOR3*)SCRATCH_STACK_RESERVE_BYTES(0x18);
    vec->vx = coord->workm.t[0];
    vec->vy = coord->workm.t[1];
    vec->vz = coord->workm.t[2];
    effectDrawGroundShadow(vec, 0x1C0, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
