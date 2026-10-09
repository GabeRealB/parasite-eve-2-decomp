/* Include at the original data position. */

/// Glow-disc RGB brightness shifts, indexed by the spawn tint (0 or 1).
///
/// Row 0 halves red; row 1 halves green. Other channels keep full brightness.
static RoomFxShade _gRoomVisualEffectsGlowDiscShades[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};
