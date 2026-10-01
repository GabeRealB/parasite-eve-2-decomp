/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Behaviour state 0: waits until the player is within
/// `MAGGOT_CATERPILLAR_WAKE_RANGE`, the contact pass has flagged a hit
/// (`field_3D0`) or the room is on alert, then wakes; sub-state 1 crawls
/// forward while its clip plays and hands over to the roam state (3).
void maggotCaterpillarWaitState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    dx;
    s32                    dz;
    u32                    random;
    s32                    index;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                                                                = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                     = scratchEnd - 1;
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                      = arg0->work;
    state                                                                     = work->field_39C;
    coord                                                                     = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            delta->vy         = 0;
            dz                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz         = dz;
            dx                = scratchEnd[-1].vx;
            if ((SquareRoot0((dx * dx) + (dz * dz)) < MAGGOT_CATERPILLAR_WAKE_RANGE) || (work->field_3D0 != 0) || (gSceneCombatState.signals.bytes.enemyAlert == 2)) {
                work->field_39C = 1;
                work->field_392 = 0xD;
                Gp_ArmStateF0(1);
            }
            break;
        case 1:
            if ((u32)(work->field_396 - 0xB) < 0x32U) {
                coord->coord.t[2] += 4;
            }
            if ((s16)work->field_396 >= 0x4B) {
                work->field_39A = 3;
                work->field_39C = 0;
                work->field_392 = state;
                index           = ((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                work->field_39E = gMaggotCaterpillarIdleDelay[index] + ((random >> 0x10) & 0xF);
            }
            break;
    }
    *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = *(VECTOR**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) + 1;
}
