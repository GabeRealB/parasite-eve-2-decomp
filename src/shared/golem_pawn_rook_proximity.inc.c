/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Proximity check of the approach states. Measures the player's horizontal
/// distance from the root coordinate through a 0x10-byte scratch stack
/// block: under 0x5DC one of `gSceneCombatState.signals.bytes.actionFlags`'s bit groups raises `playerSpotted`;
/// past it the other two (the second only within 0xBB8) put the enemy into
/// animation 4 and state 1.
void golemPawnRookCheckProximity(Task* arg0)
{
    GolemPawnRookWork* work;
    GfxCoord*          self;
    s32                dx;
    s32                distance;
    s32                dz;
    s32                trigger;
    VECTOR*            head;
    VECTOR*            delta;

    self                         = arg0->extra.tmd->coords;
    work                         = arg0->work;
    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = head - 1;
    head[-1].vx                  = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
    delta->vy                    = 0;
    dz                           = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
    delta->vz                    = dz;
    dx                           = head[-1].vx;
    trigger                      = 0;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    distance                     = SquareRoot0((dx * dx) + (dz * dz));
    if (distance < 0x5DC) {
        if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_PE_CAST_OTHER | SCENE_COMBAT_ACTION_FOOTSTEP)) {
            work->playerSpotted = 1;
        }
    } else {
        if (gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_NOISE | SCENE_COMBAT_ACTION_PE_CAST_OTHER)) {
            trigger = 1;
        }
        if ((gSceneCombatState.signals.bytes.actionFlags & (SCENE_COMBAT_ACTION_PE_ACTIVE | SCENE_COMBAT_ACTION_FOOTSTEP)) && (distance < 0xBB8)) {
            trigger = 1;
        }
        if (trigger != 0) {
            work->anim         = 4;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            work->timer        = 0;
            work->step         = 1;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
