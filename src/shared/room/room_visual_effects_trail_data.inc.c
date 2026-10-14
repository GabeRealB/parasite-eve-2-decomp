/* Include at the original data position. */

#ifdef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
SVECTOR gRoomVisualEffectsTwinTrailEndpointOffsets[2] = {
#else
/// Twin-trail endpoint positions in whole units along the borrowed parent's axes.
///
/// Entry 0 places the task's coordinate; entry 1 places the second endpoint.
/// Both are composed into world space before recording the trail histories.
static SVECTOR _gRoomVisualEffectsTwinTrailEndpointOffsets[2] = {
#endif
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};
