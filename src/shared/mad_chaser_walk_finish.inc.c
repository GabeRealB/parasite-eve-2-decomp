/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Steps the root along local -Z at the walk animation's current rate.
///
/// Requires live model/work storage and a heading in 4096ths of a turn. Motion
/// uses parent-coordinate units, with the rate-scaled distance narrowed to s16;
/// signed Q12 sine/cosine products retain their shift order. Dirties composition.
static __inline__ void _madChaserWalkFinishAdvanceRoot(Task* task, MadChaserWork* work)
{
    enum {
        MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS = 16,
        MAD_CHASER_WALK_TRIG_FRACTION_BITS         = 12,
    };
    s16 heading;
    s16 stepDistance;

    stepDistance                          = _madChaserScaleByAnimRate(task, MAD_CHASER_WALK_DISTANCE_AT_NORMAL_RATE);
    heading                               = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(heading) << (MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS - MAD_CHASER_WALK_TRIG_FRACTION_BITS)) * stepDistance) >> MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(heading) << (MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS - MAD_CHASER_WALK_TRIG_FRACTION_BITS)) * stepDistance) >> MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Finishes the walk cycle before handing the combat behavior to the leap.
///
/// Requires live work/model storage and the last tracked player offset in the
/// root's parent frame. Turns by 16 of 4096 angle units and steps along local
/// -Z by animRate parent-coordinate units, narrowed to s16, dirtying composition.
/// Slot-1 boundary/jump/held status selects leap at sub-state zero; distance and
/// cooldown are not rechecked. The caller ticks animation and collision.
static void _madChaserWalkFinish(Task* task)
{
    MadChaserWork* work = task->work;

    _madChaserTurnToPlayer(task, MAD_CHASER_WALK_BASE_TURN_STEP);
    _madChaserWalkFinishAdvanceRoot(task, work);
    if (_madChaserAnimHasBoundaryStatus(task)) {
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_LEAP);
    }
}
