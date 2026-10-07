/* Part of the library; see boss_stranger.h. Inline helpers the fragments use. */

/// Applies the signed facing step and stores the resulting movement vector.
///
/// Reserves one uninitialized `SVECTOR` even at zero speed; a zero speed leaves
/// `moveStep` intact. Nonzero speed uses the normalized local Z axis and GTE Q12
/// scaling, adds XYZ to the parent-frame translation, and dirties the cache.
/// `speed` is a signed distance in whole game-coordinate units per frame;
/// negative steps run against facing. Borrows a live writable coordinate and
/// walker, and releases its word-aligned scratch reservation before returning.
/// GTE state changes. The enclosing tick supplies the freeze gate.
static inline void _bossStrangerApplyFacingStep(BossStrangerWalker* walker, GfxCoord* coord, s16 speed)
{
    SVECTOR* moveScratchEnd;
    SVECTOR* moveScratch;
    SVECTOR* gteMove;

    moveScratchEnd                = SCRATCH_STACK_CURSOR(SVECTOR);
    moveScratch                   = moveScratchEnd - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = moveScratch;
    // Both aliases address the live move block; GTE transfers use this copy.
    gteMove = moveScratch;
    if (speed != 0) {
        gfxReadMatrixZAxis(&coord->coord, moveScratch);
        VectorNormalSS(moveScratch, moveScratch);
        gte_lddp(speed);
        gte_ldsv(gteMove);
        gte_gpf12();
        gte_stsv(gteMove);
        coord->coord.t[0]  += moveScratchEnd[-1].vx;
        coord->coord.t[1]  += moveScratch->vy;
        coord->coord.t[2]  += moveScratch->vz;
        walker->moveStep    = moveScratchEnd[-1];
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Selects the steering goal, turns, ramps speed and applies the walker's frame movement.
///
/// `frame` is the live tick reservation and equals `scratchEnd - 1`.
/// Chase copies the selected player's translation; patrol writes the route
/// goal. Idle and close-in leave the goal unwritten but still turn toward it.
/// No known carrier selects close-in: if selected, it reserves four additional
/// bytes, plans along `nodeOrder`, and releases those bytes only on arrival.
/// A non-arriving close-in tick therefore leaves the cursor four bytes lower.
/// The purpose of this extra reservation is unproven.
///
/// Speed uses an unsigned stored target/current value but a signed-halfword
/// difference to choose the ramp branch, and a signed-halfword step for GTE
/// movement. A zero speed retains the previous `moveStep`; actorsFrozen == 1
/// clears XYZ instead. Optional ground and avoidance steps follow facing motion.
/// Requires live walker/navigation/coordinate storage and the callees' contracts.
/// Chase requires `playerId` to select live resident player storage; only id 1
/// has established storage. Borrows the frame and nested initialized scratch
/// storage for this call. The enclosing tick releases the frame and dirties the coordinate.
static __inline__ void _bossStrangerStep(BossStrangerWalker* walker, BossStrangerTickScratch* scratchEnd,
                                         BossStrangerTickScratch* frame)
{
    const PlayerStatus* player;
    SVECTOR3*           goal;
    SVECTOR*            moveStep;
    GfxCoord*           coord;
    s16                 signedSpeedGap;
    s32                 speedGap;
    s16                 speed;
    s32                 targetSpeed;
    s32                 currentSpeed;
    s32                 nextSpeed;

    enum { BOSS_STRANGER_CLOSE_SCRATCH_RESERVATION_BYTES = 4 };

    // Select a goal only in the chase and patrol states.
    switch (walker->state) {
        case BOSS_STRANGER_WALKER_IDLE:
            break;
        case BOSS_STRANGER_WALKER_CHASE:
            player   = &gPlayerStatus + (walker->playerId - 1);
            goal     = &scratchEnd[-1].goal;
            goal->vx = player->coordMtx->t[0];
            goal->vy = player->coordMtx->t[1];
            goal->vz = player->coordMtx->t[2];
            break;
        case BOSS_STRANGER_WALKER_CLOSE:
            SCRATCH_STACK_RESERVE_BYTES(BOSS_STRANGER_CLOSE_SCRATCH_RESERVATION_BYTES);
            walker->actorNode = _bossStrangerNodeNearestPlayer(walker, 1);
            walker->selfNode  = _bossStrangerNodeNearestSelf(walker);
            if (walker->prevState != walker->state || walker->selfNode != walker->prevSelfNode ||
                walker->actorNode != walker->prevActorNode) {
                _bossStrangerPlanToward(walker, 1);
                walker->node = walker->nav->nodeOrder[walker->cursor];
            }
            walker->prevState     = walker->state;
            walker->prevSelfNode  = walker->selfNode;
            walker->prevActorNode = walker->actorNode;
            if (_bossStrangerArrived(walker) != 0) {
                walker->cursor += (u8)walker->orderStep;
                walker->node    = walker->nav->nodeOrder[walker->cursor];
                SCRATCH_STACK_RELEASE_BYTES(BOSS_STRANGER_CLOSE_SCRATCH_RESERVATION_BYTES);
            }
            break;
        case BOSS_STRANGER_WALKER_PATROL:
            _bossStrangerFollowRoute(walker, &scratchEnd[-1].goal);
            break;
    }
    _bossStrangerTurnToward(walker, &frame->goal);

    // Preserve the signed-halfword comparison of the unsigned speed gap.
    targetSpeed  = walker->speedTarget;
    currentSpeed = walker->speed;
    if (targetSpeed != currentSpeed) {
        speedGap       = targetSpeed - currentSpeed;
        signedSpeedGap = speedGap;
        if (signedSpeedGap > walker->speedStep) {
            nextSpeed = currentSpeed + walker->speedStep;
        } else if (signedSpeedGap < -walker->speedStep) {
            nextSpeed = currentSpeed - walker->speedStep;
        } else {
            nextSpeed = currentSpeed + speedGap;
        }
        walker->speed = nextSpeed;
    }

    // Convert the normalized facing axis into this frame's signed step.
    coord    = walker->coord;
    speed    = walker->speed;
    moveStep = &walker->moveStep;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        moveStep->vz        = 0;
        moveStep->vy        = 0;
        walker->moveStep.vx = 0;
    } else {
        _bossStrangerApplyFacingStep(walker, coord, speed);
    }
    if (walker->skipGround == 0) {
        _bossStrangerApplyGroundStep(walker);
    }
    if (walker->skipAvoid == 0) {
        _bossStrangerAvoidContacts(walker);
    }
}
