/* halo shade storage. Include at the original data position.
 * The configuration contract is documented in room_visual_effects.h. */

/// Three halo tints expressed as `RoomFxShade` RGB brightness shifts, indexed 0..2.
///
/// The carrier supplies either a three-row array or `RoomFxHaloStorage`.
/// Only those rows are read; the latter also preserves an unread halfword
/// whose role is unproven. Storage remains live while the overlay is loaded.
static ROOM_FX_HALO_STORAGE_TYPE _gRoomVisualEffectsHaloShades ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND = ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER;
