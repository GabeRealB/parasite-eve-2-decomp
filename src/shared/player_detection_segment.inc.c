/* Part of the player detection library; see player_detection.h. */

/// Writes end-minus-start normalized to 4096 per unit; inputs stay unchanged.
static inline void _playerDetectionNormalizeSegment(const SVECTOR* segmentStart, const SVECTOR* segmentEnd, VECTOR* direction)
{
    direction->vx = segmentEnd->vx - segmentStart->vx;
    direction->vy = segmentEnd->vy - segmentStart->vy;
    direction->vz = segmentEnd->vz - segmentStart->vz;
    VectorNormal(direction, direction);
}

/// Returns 1 when an enabled room sight occluder crosses the segment, else 0.
///
/// Endpoints use view-space game coordinates in the current composed view
/// frame. Both crossing directions and quad edges count; endpoint and parallel
/// intersections do not. The scan stops at the first hit. This tests sight
/// occluders, independently of the room's movement collision grid.
///
/// The delta must fit signed halfwords and have squared length in 1..0x7FFFFFFF
/// for SDK normalization to a direction with 4096 per unit. Inputs are neither
/// changed nor retained. Keep them clear of the initialized scratch stack's
/// 144-byte peak reservation. Scratch is released before return; GTE state
/// is clobbered.
static s32 _playerDetectionSegmentOccluded(const SVECTOR* segmentStart, const SVECTOR* segmentEnd)
{
    VECTOR*                 direction;
    WorldCollisionOccluder* occluder;
    s32                     occluded;

    occluded  = 0;
    occluder  = D_80115550;
    direction = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    // Normalize once; every enabled quad uses the same segment direction.
    _playerDetectionNormalizeSegment(segmentStart, segmentEnd, direction);
    for (; occluder != NULL; occluder = occluder->next) {
        if (occluder->flags & WORLD_COLLISION_OCCLUDER_ENABLED) {
            occluded = worldCollisionTestOccluderSegment(occluder, segmentStart, segmentEnd, direction);
            if (occluded == 1) {
                break;
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
    return occluded;
}
