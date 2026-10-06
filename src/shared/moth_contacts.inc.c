/* Part of the Moth library; see moth.h. */

/// Loads the frame's movement record off the scratchpad, folds it into the
/// render coordinate, then applies whatever the collision record still holds:
/// state 1 nudges the actor by the fractional delta, state 2 snaps it to the
/// recorded position. The second half turns the hit record's id halfword into
/// an arm/damage reaction - state 2 measures the distance to the recorded
/// opponent, rolls damage, and spawns the hit effect.
void mothContacts(Task* arg0)
{
    MothWork*            work;
    GfxCoord*            coord;
    s32                  movement;
    s32                  dx;
    s32                  dy;
    s32                  dz;
    s32                  amount;
    s32                  damage;
    s32                  z;
    u16                  state;
    GfxCoord*            target;
    WorldCollisionDelta* head;
    WorldCollisionDelta* delta;

    work     = arg0->work;
    head     = SCRATCH_STACK_CURSOR(void);
    delta    = (SCRATCH_STACK_CURSOR(void) = head - 1);
    coord    = arg0->extra.tmd->coords;
    movement = func_800E0C10(work->gridContacts, delta, ARRAY_SIZE(work->gridContacts), 0);
    switch (movement) {
        case 0:
            break;
        case 1:
            coord->coord.t[0] += head[-1].fixed.vx.halves.integer;
            coord->coord.t[1] += delta->fixed.vy.halves.integer;
            z                  = coord->coord.t[2] + delta->fixed.vz.halves.integer;
            coord->coord.t[2]  = z;
            break;
        case 2:
            coord->coord.t[0] = work->prevPos.vx;
            coord->coord.t[1] = work->prevPos.vy;
            coord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
    state = (u16)work->hitContacts[0].key.parts.kind;
    switch ((u32)state) {
        case 0:
            break;
        case 1:
            arg0->state                           = 2;
            ((Enemy*)arg0->spawnArg2.pointer)->hp = 0;
            Gp_ArmStateF0(1);
            break;
        case 2:
            arg0->state      = (s32)state;
            target           = gPlayerActorTasks[(u8)work->hitContacts[0].key.parts.id >> 7]->extra.tmd->coords;
            dx               = target->coord.t[0] - coord->coord.t[0];
            delta->vector.vx = dx;
            dy               = target->coord.t[1] - coord->coord.t[1];
            delta->vector.vy = dy;
            dz               = target->coord.t[2] - coord->coord.t[2];
            delta->vector.vz = dz;
            damage           = Gp_ComputeDamage((s32)work->hitContacts[0].key.value,
                                                SquareRoot0((dx * dx) + (dy * dy) + (dz * dz)), 0, 0);
            amount           = damage;
            if (damage == 0) {
                damage = 1;
                amount = 1;
            }
            func_800DA6E8(&((Enemy*)arg0->spawnArg2.pointer)->node, amount, 0);
            damageAccumulateLifeDrainHp(arg0->spawnArg2.pointer, work->hitContacts[0].key.value, damage, 0);
            ((Enemy*)arg0->spawnArg2.pointer)->hp = 0;
            func_800FDB18(Gp_GetIdParam1((s32)work->hitContacts[0].key.value) & 0xFFFF, arg0->extra.tmd->coords, 0,
                          &work->hitEffectArg);
            break;
    }
    worldCollisionClearContacts(work->hitContacts);
    SCRATCH_STACK_RELEASE_BLOCK(WorldCollisionDelta);
}
