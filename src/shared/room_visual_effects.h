/* Room effect tasks, drawing helpers and their colour and trail tables.
 *
 * The library comes in three sections, and a room carries the ones it uses, in
 * this order and each complete:
 *
 *   halo   _halo, _glow_quad, _flash
 *   flash  _flash_task, _trails, _trail_task, _sparks, _glow
 *   flying _flying_tasks, _burst, _burst_draw
 *
 * (each room_visual_effects<name>.inc.c). A room includes this header in its
 * prologue, then room_visual_effects.inc.c before its first section, then every
 * fragment of its sections at the position of that code, with its task entry
 * points between them. The twin-trail entry includes _trail_task.inc.c as its
 * body, with parameter Task* task, since GCC 2.8.1 changes spills when that task
 * is inlined. Task implementations are static inline, so a room compiles only
 * the ones its entry points call; the drawing helpers are static functions,
 * which GCC emits whether used or not, which is why a section's fragments go in
 * whole. A room with no task of the library may still use a helper fragment on
 * its own, as one uses _glow_quad.
 *
 * Disc and halo shades are private to each carrier. Twin-trail offsets are
 * private except in Acropolis Cafeteria and Neo Ark Forest Zone, whose data
 * and task live in different translation units. Those carriers define
 * ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS before including this header.
 * Include _halo_data, _trail_data and _disc_data for the room's sections, at
 * the tables' position in the data. The halo shades have the same three readable
 * rows. Some carriers also preserve a following unread halfword whose role is
 * unproven, so the room sets
 * ROOM_FX_HALO_STORAGE_TYPE, ROOM_VISUAL_EFFECTS_HALO_STORAGE_BOUND and
 * ROOM_VISUAL_EFFECTS_HALO_STORAGE_INITIALIZER before including
 * _halo_data - RoomFxShade [3], or RoomFxHaloStorage to retain that halfword - and
 * defines _roomVisualEffectsGetHaloShades, the array view the halo code reads, where its
 * halo code is.
 */

#ifndef SRC_SHARED_ROOM_VISUAL_EFFECTS_H
#define SRC_SHARED_ROOM_VISUAL_EFFECTS_H

#include "gameplay/display.h"

#include "main/task_types.h"

#include "rooms/room.h"
#include "rooms/room_visual_effects.h"

/// Projection numerators: multiply a world-unit radius, then divide by SZ3 / 4.
///
/// Radial drawers add one to the depth before dividing. The star shoulders use
/// one eighth of its disc numerator; the layered glow follows its 55-texel cell.
enum {
    ROOM_VISUAL_EFFECTS_RADIAL_PROJECTION_SCALE        = 64,
    ROOM_VISUAL_EFFECTS_GLOW_PROJECTION_SCALE          = 55,
    ROOM_VISUAL_EFFECTS_STAR_SHOULDER_PROJECTION_SCALE = 8,
    ROOM_VISUAL_EFFECTS_TRIG_FRACTION_BITS             = 12,
    ROOM_VISUAL_EFFECTS_FULL_TURN                      = 0x1000 // Angle units per turn; rsin/rcos return Q12 values
};

/// Two eight-slot histories form seven quads, fading by nine levels per older edge.
enum {
    ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT    = 8,
    ROOM_VISUAL_EFFECTS_TRAIL_INITIAL_LEVEL = 64,
    ROOM_VISUAL_EFFECTS_TRAIL_LEVEL_STEP    = 9
};

/// GPU command bytes for shaded/raw semi-transparent textured quads and Gouraud quads.
enum {
    ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_BLEND     = 0x2E,
    ROOM_VISUAL_EFFECTS_TEXTURED_QUAD_RAW_BLEND = 0x2F,
    ROOM_VISUAL_EFFECTS_GOURAUD_QUAD            = 0x38,
    ROOM_VISUAL_EFFECTS_RAW_TEXTURE_FLAG        = 1
};

/// The layered burst refreshes transient light slot 2 for two frames on every draw.
///
/// Inner and outer falloff radii are world-unit distances; channel intensity is
/// Q12. Random bits choose an intensity from 0x800 to 0xF00 in 0x100 steps.
enum {
    ROOM_VISUAL_EFFECTS_BURST_LIGHT_SLOT                  = 2,
    ROOM_VISUAL_EFFECTS_BURST_LIGHT_LIFETIME_FRAMES       = 2,
    ROOM_VISUAL_EFFECTS_BURST_LIGHT_INNER_RADIUS          = 0x300,
    ROOM_VISUAL_EFFECTS_BURST_LIGHT_OUTER_RADIUS          = 0x3000,
    ROOM_VISUAL_EFFECTS_BURST_LIGHT_BASE_INTENSITY        = 0x800,
    ROOM_VISUAL_EFFECTS_BURST_LIGHT_RANDOM_INTENSITY_MASK = 0x700
};

/// Writes an orange tint at full, half and quarter byte brightness.
///
/// `rgb` addresses three writable bytes. Both arguments are evaluated repeatedly
/// and must be side-effect-free; `level` must not alias the destination. The
/// level is shifted with its integer signedness, then narrowed to colour bytes.
/// Expands to a standalone statement list; use inside a braced block.
#define ROOM_VISUAL_EFFECTS_SET_ORANGE_TINT(rgb, level) \
    (rgb)[0] = (level);                                 \
    (rgb)[1] = (level) >> 1;                            \
    (rgb)[2] = (level) >> 2

/* Interface for the including source. */

/// Selects the mote's second four-frame strip in packed half-extent/texture-row arguments.
enum { ROOM_VISUAL_EFFECTS_MOTE_TEXTURE_ROW_1 = 0x1000 };

static void _roomVisualEffectsDrawMote(const GfxCoord* coord, u16 animationFrame, u16 packedHalfExtentRow, u16 packedBrightnessPalette);
static void _roomVisualEffectsDrawHaloRing(const GfxCoord* coord, s32 blackRadius, s32 tintRadiusDelta, const u8 rgb[3]);
static void _roomVisualEffectsDrawHaloDisc(const GfxCoord* coord, s16 radius, const u8 rgb[3]);
static void _roomVisualEffectsDrawHaloBurstGlow(const GfxCoord* coord, s16 halfExtent);
static void _roomVisualEffectsDrawHaloBurstGroundQuad(const GfxCoord* coord, s32 halfExtent);
static void _roomVisualEffectsDrawHaloStar(const GfxCoord* coord, s16 radius, const u8 rgb[3]);
static void _roomVisualEffectsDrawFlashRing(const GfxCoord* coord, s32 blackRadius, s32 tintRadiusDelta, const u8 rgb[3]);
static void _roomVisualEffectsDrawFlashDisc(const GfxCoord* coord, s16 radius, const u8 rgb[3]);
static void _roomVisualEffectsDrawTwinTrail(const GfxCoord firstTrail[ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT], const GfxCoord secondTrail[ROOM_VISUAL_EFFECTS_TRAIL_SLOT_COUNT], s16 newestSlot, s16 packedColorMultipliers);
static void _roomVisualEffectsDrawBurstStar(const GfxCoord* coord, s16 radius, const u8 rgb[3]);
static void _roomVisualEffectsDrawFlyingSpark(const GfxCoord* coord, s32 animationFrame, s32 halfExtent, s32 brightness);
static void _roomVisualEffectsDrawFlyingRing(const GfxCoord* coord, s32 blackRadius, s32 tintRadiusDelta, const u8 rgb[3]);
static void _roomVisualEffectsDrawFlyingDisc(const GfxCoord* coord, s32 radius, const u8 rgb[3]);
static void _roomVisualEffectsDrawFlyingBurstGlow(const GfxCoord* coord, s16 halfExtent);
static void _roomVisualEffectsDrawFlyingBurstGroundQuad(const GfxCoord* coord, s32 halfExtent);

static inline void _roomVisualEffectsMoteTask(Task* task);

static inline void _roomVisualEffectsHaloTask(Task* task);

static inline void _roomVisualEffectsHaloOrangeBurstTask(Task* task);

static inline void _roomVisualEffectsSparkEmitterTask(Task* task);

static inline void _roomVisualEffectsFlashTask(Task* task);

static inline void _roomVisualEffectsSparkBurstTask(Task* task);

static inline void _roomVisualEffectsGlowDiscTask(Task* task);

static inline void _roomVisualEffectsFlyingSparkTask(Task* task);

static inline void _roomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Number of halo tint rows available to spawn indices 0..2.
enum { ROOM_VISUAL_EFFECTS_HALO_SHADE_COUNT = 3 };

/// A tint given as a right shift per colour channel of an effect's brightness
/// level: channel = level >> shift, so 0 keeps the channel at full level and
/// each step halves it. Halo and glow-disc effects keep a small table of these
/// and pick a row from their spawn argument.
typedef struct {
    s16 rShift;
    s16 gShift;
    s16 bShift;
} RoomFxShade;
STATIC_ASSERT_SIZEOF(RoomFxShade, 0x6);

/// The halo shade table together with the alignment gap that follows it, for
/// rooms whose image holds non-zero bytes in that gap.
///
/// The three shades end two bytes short of the next word boundary, where the
/// following table starts. Rooms whose gap is zero declare the plain
/// `RoomFxShade [3]` and let the compiler pad it. The rest carry the same shades
/// followed by a value that differs from room to room, so it is not part of the
/// shared table; C can reproduce it only as a member. Code reads the shades
/// alone, through `_roomVisualEffectsGetHaloShades`.
typedef struct {
    RoomFxShade entries[3];    // the halo's colour variants, indexed by spawn argument
    u16         alignmentFill; // build fill before the next word-aligned table; never read
} RoomFxHaloStorage;
STATIC_ASSERT_SIZEOF(RoomFxHaloStorage, 0x14);

/// Binds the twin-trail task to its carrier's two parent-local endpoint offsets.
///
/// Resolves to SVECTOR[2], with signed whole-coordinate XYZ and an unread pad.
/// Index 0 is copied to the task coordinate on initialization; index 1 is read
/// on initialization and every recording tick to reconstruct the second endpoint.
/// Both offsets use the borrowed effect parent's local axes before composition.
/// The expression remains an array lvalue, preserving indexing, address-taking
/// and sizeof; it neither evaluates arguments nor allocates or owns storage.
///
/// Define `ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS` before this guarded
/// header in provider and reader TUs of Acropolis Cafeteria or Neo Ark Forest
/// Zone to select the externally linked table. Other carriers provide the static
/// table from the data fragment before including the task body. Keep the table
/// loaded for the trail task's lifetime; changing the selector after including
/// this header does not change this binding.
#ifdef ROOM_VISUAL_EFFECTS_SHARED_TRAIL_ENDPOINT_OFFSETS
/// Twin-trail endpoint positions in whole units along the borrowed parent's axes.
///
/// Entry 0 places the task's coordinate; entry 1 places the second endpoint.
/// Both are composed into world space before recording the trail histories.
extern SVECTOR gRoomVisualEffectsTwinTrailEndpointOffsets[2];
/// Array-lvalue binding to the overlay-shared endpoint table; contract above.
#define ROOM_VISUAL_EFFECTS_TWIN_TRAIL_ENDPOINT_OFFSETS gRoomVisualEffectsTwinTrailEndpointOffsets
#else
/// Array-lvalue binding to this TU's private endpoint table; contract above.
#define ROOM_VISUAL_EFFECTS_TWIN_TRAIL_ENDPOINT_OFFSETS _gRoomVisualEffectsTwinTrailEndpointOffsets
#endif

/* Each carrier returns its three readable tint rows, live for that overlay's
 * lifetime. Spawn tint indices must be 0..2; alignment fill is not a row. */
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void);

#endif /* SRC_SHARED_ROOM_VISUAL_EFFECTS_H */
