/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Tests whether the walker has reached its current navigation node on the XZ plane.
///
/// Returns 1 strictly inside either a 300-unit radius or the signed-halfword
/// radius obtained from `speedTarget` times four; equality is outside.
/// Offsets wrap to signed halfwords, and negative radii are squared as supplied.
/// Thus the 0xFFFE speed target supplies radius -8 and the 300-unit test decides.
/// Requires a live coordinate, a valid `node` in `nav->nodes`, and initialized
/// scratch storage for one `SVECTOR` plus the nested range test. Borrows the
/// walker read-only and restores the scratch cursor before returning.
static s16 _bossStrangerArrived(const BossStrangerWalker* walker)
{
    enum {
        BOSS_STRANGER_ARRIVAL_MIN_RADIUS   = 300, // Game-coordinate units
        BOSS_STRANGER_ARRIVAL_SPEED_FRAMES = 4    // Scales the target step into a radius
    };
    SVECTOR* toNode;

    toNode = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);

    // Stage the node's position, then subtract the walker's translation in
    // place. Only the low halfword of each coordinate takes part, and the
    // height is dropped because the test measures on the XZ plane.
    toNode->vx = walker->nav->nodes[walker->node].x;
    toNode->vy = walker->nav->nodes[walker->node].y;
    toNode->vz = walker->nav->nodes[walker->node].z;
    toNode->vx = toNode->vx - walker->coord->coord.t[0];
    toNode->vy = 0;
    toNode->vz = toNode->vz - walker->coord->coord.t[2];

    if (!actorOutsideRadius(toNode, walker->speedTarget * BOSS_STRANGER_ARRIVAL_SPEED_FRAMES) ||
        !actorOutsideRadius(toNode, BOSS_STRANGER_ARRIVAL_MIN_RADIUS)) {
        SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
        return 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return 0;
}

/// Marks patrol arrival and selects the next node, wrapping at the route end marker.
///
/// Requires a live, nonempty end-marked route whose byte cursor names its current
/// node and whose following entry is readable. Advances the cursor with byte
/// wrap, resetting it to zero at `BOSS_STRANGER_ROUTE_END`. Clears both turn
/// counters and returns the new navigation-node index stored in `walker->node`.
static inline u8 _bossStrangerAdvancePatrolNode(BossStrangerWalker* walker)
{
    BossStrangerRoute* routeAdvance;
    BossStrangerRoute* routeWrap;
    BossStrangerRoute* routeNext;
    u8                 node;

    // Reload the route for the cursor advance, the end-marker wrap and the
    // index the cursor then names. Each of those phases keeps its own pointer.
    walker->route->arrived = 1;
    routeAdvance           = walker->route;
    walker->turnRun        = 0;
    walker->turnBonus      = 0;
    routeAdvance->cursor++;

    routeWrap = walker->route;
    if (routeWrap->nodeIndices[routeWrap->cursor] == BOSS_STRANGER_ROUTE_END) {
        routeWrap->cursor = 0;
    }

    routeNext    = walker->route;
    node         = routeNext->nodeIndices[routeNext->cursor];
    walker->node = node;
    return node;
}

/// Selects the patrol route's current node and writes the next steering goal.
///
/// On arrival, raises `route->arrived`, clears the turn counters and advances
/// the route cursor, wrapping at `BOSS_STRANGER_ROUTE_END`; the output then
/// names the new node. Otherwise clears `arrived` and writes the current node.
/// Requires a nonempty, end-marked route with a valid cursor and node indices
/// into `nav->nodes`, a live coordinate, and initialized scratch storage for
/// the arrival test. `goal` supplies writable packed halfword XYZ storage
/// disjoint from the walker and navigation records. No pointer is retained.
static void _bossStrangerFollowRoute(BossStrangerWalker* walker, SVECTOR3* goal)
{
    BossStrangerRoute* route;
    u8                 node;

    route        = walker->route;
    walker->node = route->nodeIndices[route->cursor];
    if (_bossStrangerArrived(walker) == 0) {
        goal->vx               = walker->nav->nodes[walker->node].x;
        goal->vy               = walker->nav->nodes[walker->node].y;
        goal->vz               = walker->nav->nodes[walker->node].z;
        walker->route->arrived = 0;
        return;
    }

    node     = _bossStrangerAdvancePatrolNode(walker);
    goal->vx = walker->nav->nodes[node].x;
    goal->vy = walker->nav->nodes[walker->node].y;
    goal->vz = walker->nav->nodes[walker->node].z;
}

/// Returns the navigation node nearest the player, measured on the XZ plane.
///
/// `playerId` is a one-based resident player-status selector; every known
/// caller passes 1, the only record whose storage is established. The selected
/// player's root matrix must be live in the navigation nodes' coordinate frame.
/// Requires `nav->nodeCount` in 1..255 and that many readable node entries.
/// Axis differences wrap to signed halfwords; squared distances are compared
/// as unsigned words. Ties retain the first node in table order. An empty table
/// returns an unwritten scratch byte and is outside the caller contract.
/// Borrows the walker read-only and one initialized scratch block for the call,
/// restoring the cursor before reading the result without another reservation.
static u8 _bossStrangerNodeNearestPlayer(const BossStrangerWalker* walker, s16 playerId)
{
    BossStrangerNodeNearestPlayerScratch* scan;

    scan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerNodeNearestPlayerScratch);

    scan->player     = &gPlayerStatus + (playerId - 1);
    scan->bestDistSq = BOSS_STRANGER_NODE_DISTANCE_NONE;
    // Bound the translation to its low halfword before subtracting a node.
    for (scan->node = 0; scan->node < walker->nav->nodeCount; scan->node++) {
        scan->dx     = (u16)scan->player->coordMtx->t[0] - walker->nav->nodes[scan->node].x;
        scan->dy     = (u16)scan->player->coordMtx->t[1] - walker->nav->nodes[scan->node].y;
        scan->dz     = (u16)scan->player->coordMtx->t[2] - walker->nav->nodes[scan->node].z;
        scan->distSq = scan->dx * scan->dx + scan->dz * scan->dz;
        if (scan->distSq < scan->bestDistSq || scan->bestDistSq == BOSS_STRANGER_NODE_DISTANCE_NONE) {
            scan->bestDistSq = scan->distSq;
            scan->nearest    = scan->node;
        }
    }
    // Releasing only moves the cursor; the block is still intact for the read.
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerNodeNearestPlayerScratch);
    return scan->nearest;
}

/// Returns the navigation node nearest the walker, measured on the XZ plane.
///
/// The walker's local translation and navigation nodes must use the same
/// coordinate frame; no matrix composition or parent transform is performed.
/// Requires `nav->nodeCount` in 1..255 and that many readable node entries.
/// Axis differences wrap to signed halfwords; squared distances are compared
/// as unsigned words. Ties retain the first node in table order. An empty table
/// returns an unwritten scratch byte and is outside the caller contract.
/// Borrows the walker read-only and one initialized scratch block for the call,
/// restoring the cursor before reading the result without another reservation.
static u8 _bossStrangerNodeNearestSelf(const BossStrangerWalker* walker)
{
    BossStrangerNodeNearestSelfScratch* scan;

    scan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerNodeNearestSelfScratch);

    scan->bestDistSq = BOSS_STRANGER_NODE_DISTANCE_NONE;
    // Bound the translation to its low halfword before subtracting a node.
    for (scan->node = 0; scan->node < walker->nav->nodeCount; scan->node++) {
        scan->dx     = (u16)walker->coord->coord.t[0] - walker->nav->nodes[scan->node].x;
        scan->dz     = (u16)walker->coord->coord.t[2] - walker->nav->nodes[scan->node].z;
        scan->distSq = scan->dx * scan->dx + scan->dz * scan->dz;
        if (scan->distSq < scan->bestDistSq || scan->bestDistSq == BOSS_STRANGER_NODE_DISTANCE_NONE) {
            scan->bestDistSq = scan->distSq;
            scan->nearest    = scan->node;
        }
    }
    // Releasing only moves the cursor; the block is still intact for the read.
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerNodeNearestSelfScratch);
    return scan->nearest;
}

/// Collects ordered occurrences of the two nearest nodes into the plan's slot lists.
///
/// `actorNode` and `selfNode` must be initialized in the borrowed plan. Reads
/// `orderCount` (0..255) navigation-order entries and stores their byte indices
/// in ascending order. Each list takes up to eight entries, dropping later
/// matches. The count-indexed end marker needs a spare byte: valid caller orders
/// have fewer than eight occurrences of either node. A full list retains the
/// marker overrun described by `BossStrangerPlanTowardScratch`.
static inline void _bossStrangerCollectPlanSlots(const BossStrangerWalker* walker, BossStrangerPlanTowardScratch* plan)
{
    plan->actorSlotCount = 0;
    plan->selfSlotCount  = 0;
    for (plan->outerIndex = 0; plan->outerIndex < walker->nav->orderCount; plan->outerIndex++) {
        if (walker->nav->nodeOrder[plan->outerIndex] == plan->actorNode && plan->actorSlotCount < BOSS_STRANGER_PLAN_SLOT_CAPACITY) {
            plan->actorSlots[plan->actorSlotCount] = plan->outerIndex;
            plan->actorSlotCount++;
        }
        if (walker->nav->nodeOrder[plan->outerIndex] == plan->selfNode && plan->selfSlotCount < BOSS_STRANGER_PLAN_SLOT_CAPACITY) {
            plan->selfSlots[plan->selfSlotCount] = plan->outerIndex;
            plan->selfSlotCount++;
        }
    }

    // A full list has no room for its end marker; see the block's notes.
    plan->actorSlots[plan->actorSlotCount] = BOSS_STRANGER_PLAN_SLOT_END;
    plan->selfSlots[plan->selfSlotCount]   = BOSS_STRANGER_PLAN_SLOT_END;
}

/// Selects the closest slot pair and records the walker's starting slot and direction.
///
/// Ties keep the first pair. An empty list leaves the walker's cursor, goal slot
/// and direction untouched, with `bestGap` at its no-pair sentinel. The main
/// plan operation subsequently advances the cursor even in that case.
/// Requires the two initialized, byte-indexed slot lists from collection.
/// Differences promote to signed 32 bits, fit the stored signed halfword gap,
/// and choose direction -1 for a negative gap or +1 otherwise. Borrows the
/// writable walker and plan; their storage and the lists remain caller-owned.
static inline void _bossStrangerSelectClosestPlanSlots(BossStrangerWalker* walker, BossStrangerPlanTowardScratch* plan)
{
    s32 slotGap;
    s32 previousBestGap;

    // Take the pair of slots, one from each list, that are closest together.
    plan->bestGap = BOSS_STRANGER_PLAN_GAP_NONE;
    for (plan->outerIndex = 0; plan->outerIndex < BOSS_STRANGER_PLAN_SLOT_CAPACITY; plan->outerIndex++) {
        if (plan->actorSlots[plan->outerIndex] == BOSS_STRANGER_PLAN_SLOT_END) {
            break;
        }
        for (plan->innerIndex = 0; plan->innerIndex < BOSS_STRANGER_PLAN_SLOT_CAPACITY; plan->innerIndex++) {
            if (plan->selfSlots[plan->innerIndex] == BOSS_STRANGER_PLAN_SLOT_END) {
                break;
            }
            slotGap         = plan->actorSlots[plan->outerIndex] - plan->selfSlots[plan->innerIndex];
            previousBestGap = plan->bestGap;
            plan->gap       = slotGap;
            slotGap         = ABS(slotGap);
            if (slotGap < previousBestGap) {
                plan->bestGap    = slotGap;
                walker->cursor   = plan->selfSlots[plan->innerIndex];
                walker->goalSlot = plan->actorSlots[plan->outerIndex];
                if (plan->gap < 0) {
                    walker->orderStep = -1;
                } else {
                    walker->orderStep = 1;
                }
            }
        }
    }
}

/// Plans one step along the navigation node order toward the selected player.
///
/// Finds the nodes nearest the player and walker, collects their order slots,
/// and selects the pair with the smallest absolute slot gap (first pair wins
/// ties). Sets `goalSlot`, sets `orderStep` to -1 or +1, and advances `cursor`
/// one slot from the selected walker-side slot, with byte wrap and no bound check.
/// If no pair exists, prints a diagnostic and still advances the old cursor
/// using the old direction; it does not leave the cursor unchanged.
///
/// Requires the nearest-node scan contracts, a readable `orderCount`-entry
/// `nodeOrder`, and a resulting cursor that names a valid order slot.
/// `playerId` is one-based; every known caller supplies the resident player 1.
/// Each scratch list accepts eight slots, then drops later matches, but its
/// marker is written at the count: eight player slots write one byte beyond
/// the block, and eight walker slots overwrite the first player slot with the
/// end marker, preventing any pair. Valid orders keep each count below eight;
/// both carriers name each node once. Borrows initialized scratch storage for
/// the plan and nested node scans; all walker/navigation pointers remain borrowed.
static void _bossStrangerPlanToward(BossStrangerWalker* walker, s16 playerId)
{
    BossStrangerPlanTowardScratch* plan;

    plan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerPlanTowardScratch);

    // Collect the order slots that name each of the two nodes.
    plan->actorNode = _bossStrangerNodeNearestPlayer(walker, playerId);
    plan->selfNode  = _bossStrangerNodeNearestSelf(walker);
    _bossStrangerCollectPlanSlots(walker, plan);
    _bossStrangerSelectClosestPlanSlots(walker, plan);

    if (plan->bestGap == BOSS_STRANGER_PLAN_GAP_NONE) {
        printf(_gBossStrangerNoPlanPairMessage);
    }
    walker->cursor += (u8)walker->orderStep;
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerPlanTowardScratch);
}

/// Adjusts an initialized halfword move component by the sign of a 16.16 fraction.
///
/// `moveComponent` must be a modifiable halfword lvalue; it is read and written
/// only when the fraction is nonzero. `fixedDelta` is a signed 32-bit 16.16
/// value and is evaluated up to twice. Both arguments must have no side effects.
/// The block captures no identifiers and preserves the negative extra unit.
/// Expands to a block; place the statement within braces in control-flow bodies.
#define BOSS_STRANGER_GROUND_ADJUST_FRACTION(moveComponent, fixedDelta) \
    {                                                                   \
        enum { BOSS_STRANGER_GROUND_FRACTION_MASK = 0xFFFF };           \
        if (((fixedDelta) & BOSS_STRANGER_GROUND_FRACTION_MASK) != 0) { \
            if ((fixedDelta) > 0) {                                     \
                (moveComponent)++;                                      \
            } else {                                                    \
                (moveComponent)--;                                      \
            }                                                           \
        }                                                               \
    }

/// Applies the walker's collision correction and vertical fall in game-coordinate units.
///
/// Takes each signed 16.16 delta's integer half and, for a nonzero fractional
/// half, adds one in the delta's sign. Positive fractions round up; negative
/// fractions step one beyond floor (-0.5 becomes -2). XZ replace the scratch
/// move components, but a resolved, unlocked Y adds onto the uninitialized
/// scratch halfword. No-hit clears XYZ. `lockHeight` suppresses Y and the fall.
///
/// Adds 16 to unlocked Y, saves the whole `moveDelta` including untouched pad,
/// then applies XZ and banded Y: values above 32 apply +8, below -32 apply -32,
/// absolute values below 32 apply themselves, and exactly +/-32 apply nothing.
/// Sets `offOrigin` from the resulting local XZ translation. Does not dirty
/// the composition cache; the enclosing tick does so.
/// Requires a live coordinate, the resolver's contact contract, and initialized
/// scratch storage for the block and nested resolver. Retains no pointer.
static void _bossStrangerApplyGroundStep(BossStrangerWalker* walker)
{
    enum {
        BOSS_STRANGER_GROUND_FALL_STEP       = 16, // Positive Y points down
        BOSS_STRANGER_GROUND_VERTICAL_BAND   = 32,
        BOSS_STRANGER_GROUND_POSITIVE_Y_STEP = 8
    };
    BossStrangerGroundStepScratch* scratchEnd;
    BossStrangerGroundStepScratch* scratch;
    s32                            fixedX;
    s32                            fixedY;
    s32                            fixedZ;
    s32                            integerX;
    s32                            integerY;
    s32                            integerZ;
    s32                            verticalStep;

    scratchEnd = SCRATCH_STACK_CURSOR(BossStrangerGroundStepScratch);
    scratch    = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerGroundStepScratch);
    // Convert the resolver correction without clearing the reserved move block.
    if (worldCollisionResolvePushback(walker->recs, &scratch->delta, walker->recCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        integerX         = scratchEnd[-1].delta.fixed.vx.halves.integer;
        integerZ         = scratch->delta.fixed.vz.halves.integer;
        scratch->move.vx = integerX;
        scratch->move.vz = integerZ;
        fixedX           = scratchEnd[-1].delta.fixed.vx.word;
        BOSS_STRANGER_GROUND_ADJUST_FRACTION(scratch->move.vx, fixedX);
        fixedZ = scratch->delta.fixed.vz.word;
        BOSS_STRANGER_GROUND_ADJUST_FRACTION(scratch->move.vz, fixedZ);
        if (walker->lockHeight == 0) {
            integerY         = scratch->delta.fixed.vy.halves.integer;
            fixedY           = scratch->delta.fixed.vy.word;
            scratch->move.vy = scratch->move.vy + integerY;
            BOSS_STRANGER_GROUND_ADJUST_FRACTION(scratch->move.vy, fixedY);
        } else {
            scratch->move.vy = 0;
        }
    } else {
        scratch->move.vx = 0;
        scratch->move.vy = 0;
        scratch->move.vz = 0;
    }
    if (walker->lockHeight == 0) {
        scratch->move.vy += BOSS_STRANGER_GROUND_FALL_STEP;
    }
    // Preserve the full move record, then apply the retained vertical bands.
    walker->moveDelta          = scratch->move;
    walker->coord->coord.t[0] += scratch->move.vx;
    if (scratch->move.vy > BOSS_STRANGER_GROUND_VERTICAL_BAND) {
        walker->coord->coord.t[1] += BOSS_STRANGER_GROUND_POSITIVE_Y_STEP;
    }
    if (scratch->move.vy < -BOSS_STRANGER_GROUND_VERTICAL_BAND) {
        walker->coord->coord.t[1] -= BOSS_STRANGER_GROUND_VERTICAL_BAND;
    }
    verticalStep = scratch->move.vy;
    if (ABS(verticalStep) < BOSS_STRANGER_GROUND_VERTICAL_BAND) {
        walker->coord->coord.t[1] += verticalStep;
    }
    walker->coord->coord.t[2] += scratch->move.vz;
    if (walker->coord->coord.t[0] != 0 || walker->coord->coord.t[2] != 0) {
        walker->offOrigin = 1;
    } else {
        walker->offOrigin = 0;
    }
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerGroundStepScratch);
}

#undef BOSS_STRANGER_GROUND_ADJUST_FRACTION

/// Applies a normalized yaw-axis step to the walker and its accumulated XZ push.
///
/// `parentYaw` uses 4096 units per turn; `signedDistance` is in game-coordinate
/// units along that yaw's +Z axis; a negative distance reverses the step.
/// Borrows the caller's live scratch block, overwrites its rotation and direction,
/// and retains GTE Q12 scaling.
/// Adds XZ in full-width parent-coordinate translation, narrowing the summed
/// `push` to signed halfwords. Y stays unchanged. The caller initializes `push`
/// and dirties the coordinate after the step; no pointer is retained.
static inline void _bossStrangerAccumulateAvoidanceStep(BossStrangerWalker* walker, ActorContactSteerScratch* scratch, s16 parentYaw, s16 signedDistance)
{
    gfxRotMatrixY(&scratch->rot, parentYaw, GRAPHICS_ROTATION_REPLACE);
    gfxReadMatrixZAxis(&scratch->rot, &scratch->dir);
    VectorNormalSS(&scratch->dir, &scratch->dir);
    gte_lddp(signedDistance);
    gte_ldsv(&scratch->dir);
    gte_gpf12();
    gte_stsv(&scratch->dir);
    walker->push.vx           += scratch->dir.vx;
    walker->push.vz           += scratch->dir.vz;
    walker->coord->coord.t[0] += scratch->dir.vx;
    walker->coord->coord.t[2] += scratch->dir.vz;
}

/// Nudges the walker away from compatible contact bearings and records the correction.
///
/// Unless `actorsFrozen` equals 1, clears `push` and `blocked`, scans at most
/// `avoidCount` records (0..255), and stops at a zero key or eight accepted
/// contacts. Accepts player/companion and enemy body kinds, plus any nonzero key
/// with a zero low halfword. Seeing a player body sets `blocked` even if its
/// bearing is later rejected. Contact flags and distances are not tested.
///
/// Each surviving bearing adds a nominal ten-unit step away in parent-frame
/// XZ and accumulates it in the signed-halfword `push`; Y stays zero. The
/// pairwise pass rejects every bearing more than a quarter turn from another
/// collected bearing before that outer bearing's push is considered.
/// It chooses XY rather than XZ when the normalized cached Y axis has Z
/// magnitude at least 2072 (4096 is unit length).
///
/// Requires a live coordinate with `workm` composed in the contact points'
/// frame, that many readable contact entries, and initialized scratch storage.
/// No pointer is retained; the enclosing tick dirties the coordinate cache.
static void _bossStrangerAvoidContacts(BossStrangerWalker* walker)
{
    enum {
        BOSS_STRANGER_AVOID_XY_AXIS_Z_THRESHOLD    = 0x818, // Normalized Q12 Y-axis Z magnitude
        BOSS_STRANGER_AVOID_MAX_BEARING_SEPARATION = 0x400, // Quarter turn
        BOSS_STRANGER_AVOID_PUSH_DISTANCE          = 10     // Game-coordinate units
    };
    ActorContactSteerScratch* scratch;
    s16                       pushYaw;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return;
    }

    walker->blocked = 0;
    walker->push.vz = 0;
    walker->push.vy = 0;
    walker->push.vx = 0;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorContactSteerScratch);

    // Choose the bearing plane and cache its heading before collecting contacts.
    gfxReadMatrixYAxis(&walker->coord->workm, &scratch->dir);
    VectorNormalSS(&scratch->dir, &scratch->dir);

    if (ABS(scratch->dir.vz) < BOSS_STRANGER_AVOID_XY_AXIS_Z_THRESHOLD) {
        scratch->heading = ratan2(-walker->coord->workm.m[2][0], walker->coord->workm.m[2][2]);
    } else {
        scratch->heading = -ratan2(-walker->coord->workm.m[0][2], walker->coord->workm.m[1][2]);
    }

    scratch->origin.vx = walker->coord->workm.t[0];
    scratch->origin.vy = walker->coord->workm.t[1];
    scratch->origin.vz = walker->coord->workm.t[2];
    scratch->count     = 0;

    for (scratch->i = 0; scratch->i < walker->avoidCount; scratch->i++) {
        if (walker->avoidRecs[scratch->i].key.value == 0) {
            break;
        }
        scratch->kind = walker->avoidRecs[scratch->i].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if (scratch->kind != WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            if (scratch->kind != WORLD_COLLISION_CONTACT_ENEMY_BODY && (u16)walker->avoidRecs[scratch->i].key.value != 0) {
                continue;
            }
        } else {
            walker->blocked = 1;
        }

        if (ABS(scratch->dir.vz) < BOSS_STRANGER_AVOID_XY_AXIS_Z_THRESHOLD) {
            scratch->bearing[scratch->count] =
                _actorAngleBearingXZ(&walker->avoidRecs[scratch->i].point, &scratch->origin);
        } else {
            scratch->bearing[scratch->count] =
                _actorAngleBearingXY(&walker->avoidRecs[scratch->i].point, &scratch->origin);
        }
        scratch->kept[scratch->count] = 1;
        scratch->count++;
        if (scratch->count >= ARRAY_SIZE(scratch->bearing)) {
            break;
        }
    }

    // Reject conflicting pairs while applying the surviving outer bearings.
    for (scratch->i = 0; scratch->i < scratch->count; scratch->i++) {
        for (scratch->j = scratch->i + 1; scratch->j < scratch->count; scratch->j++) {
            scratch->diff = _actorAngleNormalizeYaw(scratch->bearing[scratch->i] - scratch->bearing[scratch->j]);
            if (abs(scratch->diff) > BOSS_STRANGER_AVOID_MAX_BEARING_SEPARATION) {
                scratch->kept[scratch->i] = 0;
                scratch->kept[scratch->j] = 0;
            }
        }
        if (scratch->kept[scratch->i] != 0) {
            pushYaw = (scratch->bearing[scratch->i] - scratch->heading) +
                      ratan2(-walker->coord->coord.m[2][0], walker->coord->coord.m[2][2]);
            scratch->diff = pushYaw;
            _bossStrangerAccumulateAvoidanceStep(walker, scratch, pushYaw, -BOSS_STRANGER_AVOID_PUSH_DISTANCE);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(ActorContactSteerScratch);
}

/// Stages a point's XYZ offset from a coordinate's local translation for a bearing query.
///
/// `point` is packed signed-halfword XYZ in `originCoord`'s parent frame.
/// The translation stays signed 32-bit; differences must fit signed 32-bit
/// game-coordinate units. Rotation and the composition cache are not read.
/// Requires one free word-aligned scratch `VECTOR` below the initialized
/// cursor. Writes XYZ without touching `pad` and restores the cursor;
/// consume the returned block before anything reserves scratch again.
static __inline__ VECTOR* _actorAngleStageCoordPointOffset(const SVECTOR3* point, const GfxCoord* originCoord)
{
    VECTOR* scratchHead;
    VECTOR* delta;

    scratchHead                  = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = scratchHead - 1;
    delta->vx                    = point->vx - originCoord->coord.t[0];
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    delta->vy                    = point->vy - originCoord->coord.t[1];
    delta->vz                    = point->vz - originCoord->coord.t[2];
    // Consume the released block before another scratch reservation can reuse it.
    SCRATCH_STACK_CURSOR(VECTOR) = scratchHead;
    return delta;
}

/// Measures the XZ bearing from a coordinate's local translation to a target point.
///
/// `point` is a packed signed-halfword position in `originCoord`'s parent
/// frame and game-coordinate units. The local translation retains its full
/// signed 32-bit width; XYZ differences and their XZ magnitudes must fit
/// signed 32 bits. Rotation, scale, parent links and the composition cache
/// do not affect this query.
/// Inputs are only read and are not retained.
///
/// Returns the SDK's signed 32-bit angle in [-2048, 2048], with 4096 units
/// per turn: zero along +Z, positive toward +X, and zero when the XZ
/// positions coincide.
/// Requires an initialized, word-aligned scratch cursor with room for one
/// `VECTOR` (16 bytes). All three offsets are staged, although only X and Z
/// determine the angle; `pad` is untouched and the cursor is restored before
/// `ratan2` runs.
static __inline__ s32 _actorAngleBearingFromCoordXZ(const SVECTOR3* point, const GfxCoord* originCoord)
{
    VECTOR* delta;

    delta = _actorAngleStageCoordPointOffset(point, originCoord);
    return ratan2(delta->vx, delta->vz);
}

/// Turns the walker toward a packed target position within its per-frame yaw limit.
///
/// `goal` and the coordinate's local translation must share the parent frame;
/// angles use 4096 units per turn. Narrows the bearing-minus-heading to a signed
/// halfword and normalizes to [-2048, 2048], retaining both half-turn endpoints.
/// Updates `turnRun` modulo 65536; all three duration tiers set `turnBonus` to
/// zero. `turnLimit` zero suppresses turning. The saved scale basis is copied
/// back and yaw is multiplied onto it, retaining translation.
/// Returns unchanged when the live save's `unknown_5C0` equals 1.
/// Requires the bearing helper's coordinate/range contract and initialized
/// scratch storage for the turn block and nested bearing/rotation blocks.
/// Borrows the read-only goal for this call; the enclosing tick dirties the cache.
static void _bossStrangerTurnToward(BossStrangerWalker* walker, const SVECTOR3* goal)
{
    enum {
        BOSS_STRANGER_TURN_FIRST_DURATION_FRAMES  = 30,
        BOSS_STRANGER_TURN_SECOND_DURATION_FRAMES = 60
    };
    BossStrangerTurnTowardScratch* scratch;
    GfxCoord*                      coord;
    s32                            normalizedTurn;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.unknown_5C0 == 1)
        return;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerTurnTowardScratch);
    coord          = walker->coord;
    normalizedTurn = _actorAngleNormalizeYaw(_actorAngleBearingFromCoordXZ(goal, coord) -
                                             ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    scratch->angle = normalizedTurn;
    if (normalizedTurn != 0)
        walker->turnRun++;
    else
        walker->turnRun = 0;
    // Extra turn allowance by how long the walker has kept turning; every
    // tier grants nothing, so the limit is always `turnLimit` alone.
    if (walker->turnRun > BOSS_STRANGER_TURN_SECOND_DURATION_FRAMES)
        walker->turnBonus = 0;
    else if (walker->turnRun > BOSS_STRANGER_TURN_FIRST_DURATION_FRAMES)
        walker->turnBonus = 0;
    else
        walker->turnBonus = 0;
    if (walker->turnLimit + walker->turnBonus < scratch->angle)
        scratch->angle = walker->turnLimit + walker->turnBonus;
    if (scratch->angle < -(walker->turnLimit + walker->turnBonus))
        scratch->angle = -(walker->turnLimit + walker->turnBonus);
    if (walker->turnLimit == 0)
        scratch->angle = 0;
    // Rebuild yaw on the saved scale basis after clamping the relative turn.
    scratch->angle += ratan2(-walker->coord->coord.m[2][0], walker->coord->coord.m[2][2]);
    memcpy(walker->coord->coord.m, walker->scaleMtx.m, sizeof(walker->scaleMtx.m));
    gfxRotMatrixY(&walker->coord->coord, scratch->angle, GRAPHICS_ROTATION_COMPOSE);
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerTurnTowardScratch);
}
