/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in room_visual_effects.h. */

static ROOM_FX_HALO_STORAGE_TYPE RoomFx_HaloShades ROOM_FX_HALO_STORAGE_BOUND = ROOM_FX_HALO_STORAGE_INITIALIZER;

static SVECTOR RoomFx_TrailOffsets[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

static RoomHaloShade RoomFx_DiscShades[2] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};
