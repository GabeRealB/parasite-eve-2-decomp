#include "rooms/dryfield_night_motel_balcony.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_motel_balcony_private.h"

#include "actors/actor_01500.h"
#include "actors/actor_01600.h"
#include "actors/actor_335800.h"
#include "actors/actor_400600.h"
#include "actors/actor_403100.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/effects/glow_draw.h"

static u32 _gDryfieldNightMotelBalconyGlowViewMasks[2][20];

/// A horizontal row of eight square texture frames for the room's tumbling debris.
///
/// Frames start at U=0 and advance by `frameSize` texels. Their inclusive UV
/// span, `frameSize - 1`, also scales the projected billboard. Textures are
/// 4-bit, on VRAM page Y=0, with additive semitransparency when tinted.
/// The room's three rows are selected by the initialized debris task's
/// `spawnArg1.value` (0..2), which also selects its 16-colour palette.
typedef struct {
    u16 vramX;     // Texture-page X in 16-bit VRAM words, aligned to 64 words
    s16 frameSize; // Square frame side and horizontal frame step in texels (16 or 32)
    u8  v;         // Top texture row within the page, in texels (0..255)
    u8  field_5;   // Unread byte, zero in all three rows; role unproven
} _DryfieldNightMotelBalconyDebrisTextureRow;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelBalconyDebrisTextureRow, 6);

/// VRAM position of the first 16-colour CLUT in an animation row.
///
/// `x` and `y` are unencoded coordinates in the units `getClut` takes.
/// Each later frame starts 16 pixels further along `x`, the width of one
/// 4-bit CLUT, and the drawer packs that position with `getClut`.
typedef struct {
    s16 x; // First frame's palette X in VRAM pixels
    u16 y; // Palette Y in VRAM scanlines
} _ClutOrigin;
STATIC_ASSERT_SIZEOF(_ClutOrigin, 4);

static void _dryfieldNightMotelBalconyDrawDebris(const Task* task, const u8* tintRgb, s32 unusedRowIndex);
static void _dryfieldNightMotelBalconyDrawFallingParticle(const Task* task, const u8* tintRgb, s32 unusedDrawArg);
static void _dryfieldNightMotelBalconyDrawDriftPuff(const Task* task, s32 clutOriginIndex);
static void _dryfieldNightMotelBalconyDrawFlame(const Task* task, const u8* tintRgb, s16 unusedAge);

/// Applies an atlas frame's CLUT and inclusive 40-texel UV rectangle.
///
/// Borrows the packet and frame for this call; command, page, geometry and
/// submission remain with the drawer. Palette coordinates are VRAM words and
/// scanlines; UV origins and spans are page-relative texels.
static inline void _dryfieldNightMotelBalconySetBreathFlameTexture(POLY_FT4* quad, const EffectSpriteTextureFrame* textureFrame)
{
    quad->clut = getClut(textureFrame->clutX, textureFrame->clutY);
    quad->u0   = textureFrame->u;
    quad->v0   = textureFrame->v;
    quad->u1   = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->v1   = textureFrame->v;
    quad->u2   = textureFrame->u;
    quad->v2   = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->u3   = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    quad->v3   = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
}

/// Clears the broken lamp shaft's view masks and the adjacent point's masks.
///
/// Clears both words covering views 0..63 in the room-owned table. The drawer
/// tests the first point's mask for the shaft; the adjacent point's mask has no
/// current reader. Already queued packets are unaffected. No masks are restored.
static inline void _dryfieldNightMotelBalconyDisableBrokenLamp(void)
{
    enum { LOW_VIEW_MASK_WORD  = 0,
           HIGH_VIEW_MASK_WORD = 1,
           LAMP_SHAFT_POINT    = 2,
           LAMP_ADJACENT_POINT = 3 };

    _gDryfieldNightMotelBalconyGlowViewMasks[LOW_VIEW_MASK_WORD][LAMP_ADJACENT_POINT]  = 0;
    _gDryfieldNightMotelBalconyGlowViewMasks[LOW_VIEW_MASK_WORD][LAMP_SHAFT_POINT]     = 0;
    _gDryfieldNightMotelBalconyGlowViewMasks[HIGH_VIEW_MASK_WORD][LAMP_ADJACENT_POINT] = 0;
    _gDryfieldNightMotelBalconyGlowViewMasks[HIGH_VIEW_MASK_WORD][LAMP_SHAFT_POINT]    = 0;
}

/// Chooses a burst offset in model-part coordinates with three shared LCG draws.
///
/// Writes XYZ only: X -127..128, Y -639..-384 and Z 1409..1664. The writable
/// vector pointer is evaluated three times and must have no side effects.
/// Advances `gRandomLcgState` once per axis in XYZ order; the flame loop takes
/// a fourth draw for size, while the smoke loop takes only these three.
/// Leaves the vector's pad halfword intact. Y uses unsigned arithmetic with
/// the encoded base 0xFE80, then narrows to signed 16 bits. Invoke inside braces.
#define DRYFIELD_NIGHT_MOTEL_BALCONY_CHOOSE_FLAME_BURST_OFFSET(offset) \
    {                                                                  \
        RANDOM_LCG_ADVANCE();                                          \
        (offset)->vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);              \
        RANDOM_LCG_ADVANCE();                                          \
        (offset)->vy = 0xFE80 - (RANDOM_LCG_DRAW() & 0xFF);            \
        RANDOM_LCG_ADVANCE();                                          \
        (offset)->vz = 0x680 - (RANDOM_LCG_DRAW() & 0xFF);             \
    }

/// Projects a debris billboard's cached centre through the current screen matrix.
///
/// `composedCoord->workm.t` must contain the centre to draw in `GsWSMATRIX`'s
/// input space; these effect coordinates compose through the view. Each signed
/// translation is narrowed to its low 16 bits in `scratch->worldPoint` before
/// projection, without clamping. The vector's ignored fourth halfword is unchanged.
///
/// Requires a live, writable, word-aligned `EffectShapeScratch`, disjoint from
/// the borrowed coordinate, and the current GTE projection settings. Loads
/// `GsWSMATRIX`'s rotation and translation, then stores raw pixel halfwords in
/// `screenX`/`screenY` as one word and the signed GTE `projectionFlags` word,
/// including rejected results (FLAG bit 31 set). Leaves `depth` and `extent`
/// unchanged and SZ3 ready for the caller's depth read before another GTE command.
/// The caller owns the scratch reservation. Reserves no storage, queues no GPU
/// packet and retains no pointer.
static inline void _dryfieldNightMotelBalconyProjectDebrisCentre(EffectShapeScratch* scratch, const GfxCoord* composedCoord)
{
    scratch->worldPoint.vx = composedCoord->workm.t[0];
    scratch->worldPoint.vy = composedCoord->workm.t[1];
    scratch->worldPoint.vz = composedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
}

/// Projects a falling particle, puff or flame anchor through the current screen matrix.
///
/// `composedCoord->workm.t` must contain the anchor to draw in `GsWSMATRIX`'s
/// input space; these effect coordinates compose through the view. Each signed
/// translation is narrowed to its low 16 bits in `scratch->worldPoint` before
/// projection, without clamping. The vector's ignored fourth halfword is unchanged.
///
/// Requires a live, writable, word-aligned `EffectCentreScratch`, disjoint from
/// the borrowed coordinate, and the current GTE projection settings. Loads
/// `GsWSMATRIX`'s rotation and translation, then stores raw pixel halfwords in
/// `screenX`/`screenY` as one word and the signed GTE `projectionFlags` word,
/// including rejected results (FLAG bit 31 set). Leaves `depth` and `screenExtent`
/// unchanged and SZ3 ready for the caller's depth read before another GTE command.
/// The caller owns the scratch reservation. Reserves no storage, queues no GPU
/// packet and retains no pointer.
static inline void _dryfieldNightMotelBalconyProjectParticleCentre(EffectCentreScratch* scratch, const GfxCoord* composedCoord)
{
    scratch->worldPoint.vx = composedCoord->workm.t[0];
    scratch->worldPoint.vy = composedCoord->workm.t[1];
    scratch->worldPoint.vz = composedCoord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
}

static WorldCollisionGrid               _gDryfieldNightMotelBalconyRooms1And2CollisionGrid;
static WorldCollisionGrid               _gDryfieldNightMotelBalconyRoom3CollisionGrid;
static WorldCollisionOccluder           _gDryfieldNightMotelBalconySightOccluders[2];
static WorldCollisionTrigger            _gDryfieldNightMotelBalconyRoom1ViewBoundaryTriggers[8];
static WorldCollisionTrigger            _gDryfieldNightMotelBalconyRoom2ViewBoundaryTriggers[10];
static WorldCollisionTrigger            _gDryfieldNightMotelBalconyRoom3ViewBoundaryTriggers[6];
static WorldCollisionTrigger            _gDryfieldNightMotelBalconyRoom1ActionTriggers[4];
static WorldCollisionTrigger            _gDryfieldNightMotelBalconyRoom2ActionTriggers[4];
static WorldCollisionTrigger            _gDryfieldNightMotelBalconyRoom3ActionTriggers[7];
static const WorldCoordRoomAmbientEntry _gDryfieldNightMotelBalconyRooms1And3AmbientByView[40];
static const WorldCoordRoomAmbientEntry _gDryfieldNightMotelBalconyRoom2AmbientByView[40];
static WorldCoordRoomLights             _gDryfieldNightMotelBalconyRooms1And3Lights;
static WorldCoordRoomLights             _gDryfieldNightMotelBalconyRoom2Lights;

/// World-space end of the fourth balcony light shaft, in signed game units.
///
/// Drawing reaches this point through `gDryfieldNightMotelBalconyShaft4Start` in
/// the preceding translation unit. The containing two-point declaration
/// remains unresolved; only this translation unit draws the endpoint.
static const SVECTOR _gDryfieldNightMotelBalconyShaft4End SECTION(".data") = { -160, -2800, 8790, 0 };

/// World-space first point of the balcony's fifth light shaft.
///
/// XYZ uses signed game-coordinate units. The two-point drawer also reads the
/// following storage; the containing glow-table declaration remains unresolved.
static SVECTOR _gDryfieldNightMotelBalconyShaft5Start = { -300, -3190, -4610, 0 };

/// Fifth-shaft endpoint followed by the balcony's eight world-space flare centres.
///
/// Signed game-coordinate XYZ. Element 0 is the preceding shaft's end;
/// elements 1..8 correspond to glow-mask slots 10..17. The shaft pair still
/// crosses separate C objects; flare reads stay within this array.
static const SVECTOR _gDryfieldNightMotelBalconyShaft5EndAndFlarePoints[9] SECTION(".data") = {
    { -300, -3250, -4100, 0 },
    { -4140, -1700, 0x2AB2, 0 },
    { -6910, -1700, 0x299A, 0 },
    { -6910, -1700, 6660, 0 },
    { -6910, -1700, -3150, 0 },
    { -3150, -4850, 0x2AA8, 0 },
    { -6930, -4850, 5150, 0 },
    { -6930, -4850, 3870, 0 },
    { -6930, -4850, -3150, 0 },
};

/// World-space first point of the balcony's sixth light shaft.
///
/// XYZ uses signed game-coordinate units. The two-point drawer also reads the
/// following storage; the containing glow-table declaration remains unresolved.
static SVECTOR _gDryfieldNightMotelBalconyShaft6Start = { -0x312E, -5670, 490, 0 };

/// World-space end of the sixth balcony light shaft, in signed game units.
///
/// The drawer reaches this point through the preceding scalar; their
/// containing pair is not yet represented by an in-bounds C declaration.
static const SVECTOR _gDryfieldNightMotelBalconyShaft6End SECTION(".data") = { -0x2F3A, -5320, 490, 0 };

/// Scattered-puff and debris origin for the balcony break in logical view 20.
///
/// Signed game-coordinate XYZ relative to the ambient task's coordinate.
/// Spawning copies XYZ and retains a borrowed vector pointer; keep the overlay
/// loaded for its effects. Storage follows the spawner's mutable-vector interface.
static SVECTOR _gDryfieldNightMotelBalconyView20BreakOffset = { -6400, -4608, 0x2AA8, 0 };

/// Puff and large-debris origin for the balcony break in logical view 18.
///
/// Signed game-coordinate XYZ relative to the ambient task's coordinate.
/// Spawning copies XYZ and retains a borrowed vector pointer; keep the overlay
/// loaded for its effects. Storage follows the spawner's mutable-vector interface.
static SVECTOR _gDryfieldNightMotelBalconyView18BreakOffset = { -2560, 0, -6144, 0 };

/// Puff-only origin for the balcony break in logical view 19.
///
/// Signed game-coordinate XYZ relative to the ambient task's coordinate.
/// Spawning copies XYZ and retains a borrowed vector pointer; keep the overlay
/// loaded for its effects. Storage follows the spawner's mutable-vector interface.
static SVECTOR _gDryfieldNightMotelBalconyView19BreakOffset = { -4608, 0, -2940, 0 };

/// Small-debris burst origin for the balcony scene in logical view 29.
///
/// Signed game-coordinate XYZ relative to the ambient task's coordinate.
/// Spawning copies XYZ and retains a borrowed vector pointer; keep the overlay
/// loaded for its effects. Storage follows the spawner's mutable-vector interface.
static SVECTOR _gDryfieldNightMotelBalconyView29DebrisOffset = { -5110, -3250, -4100, 0 };

/// Model-part-local offset for the balcony's intermittent flame emissions.
///
/// XYZ is (0, -576, 1600) game units relative to model part 3. Spawning copies
/// XYZ and retains a borrowed pointer; this room-owned vector stays live while
/// the overlay is loaded.
static SVECTOR _gDryfieldNightMotelBalconyPeriodicFlameOffset = { 0, -576, 1600, 0 };

/// Drifting-puff and paired-debris burst origin in balcony logical view 17.
///
/// Signed game-coordinate XYZ relative to the ambient task's coordinate.
/// Spawning copies XYZ and retains a borrowed vector pointer; keep the overlay
/// loaded for its effects. Storage follows the spawner's mutable-vector interface.
static SVECTOR _gDryfieldNightMotelBalconyView17DebrisBurstOffset = { -6930, -3826, -3150, 0 };

/// Base offset of the expanding flame region in balcony logical view 21.
///
/// Random signed-halfword offsets are added to XYZ in game-coordinate units.
static SVECTOR _gDryfieldNightMotelBalconyExpandingFlameBaseOffset = { -3840, -1024, 1536, 0 };

/// Scattered-puff and debris origin for the balcony break in logical view 23.
///
/// Signed game-coordinate XYZ relative to the ambient task's coordinate.
/// Spawning copies XYZ and retains a borrowed vector pointer; keep the overlay
/// loaded for its effects. Storage follows the spawner's mutable-vector interface.
static SVECTOR _gDryfieldNightMotelBalconyView23BreakOffset = { -6930, -4096, -1230, 0 };

/// Visibility masks for the balcony's twenty glow-point slots.
///
/// Row 0 contains bits for logical views 0..31; row 1 covers 32..63. Six
/// shafts test their first-point slots 0, 2, 4, 6, 8 and 18; flares test slots
/// 10..17. The other pair slots have no drawing reader. Breaking the lamp
/// permanently clears both words for slots 2 and 3 during this overlay load.
static u32 _gDryfieldNightMotelBalconyGlowViewMasks[2][20] = {
    { 0x80000, 0, 0x80000, 0, 0x20040000, 0, 0xE083E70, 512, 0x20056000, 0x40000, 0x40081000, 0x40080000, 0x40080000, 0, 0xCED09E70u, 0x4CC04668, 0x4CC04668, 0x9024640, 132, 0 },
    { 0, 0, 128, 128, 129, 128, 24, 0, 128, 0, 0, 0, 129, 128, 25, 105, 109, 72, 2, 0 },
};

/// Texture rows for the balcony's three debris styles.
///
/// Initialized task row 0..2 selects eight square frames: row 0 uses 16-texel
/// frames at page X=704, V=240; rows 1 and 2 use 32-texel frames at page
/// X=768, V=0 and 32. The same row selects the palette at X=row*16, Y=271.
/// Page X and palette X use VRAM words; V and frame sides use texels.
static _DryfieldNightMotelBalconyDebrisTextureRow _gDryfieldNightMotelBalconyDebrisTextureRows[3] = {
    { 704, 16, 240, 0 },
    { 768, 32, 0, 0 },
    { 768, 32, 32, 0 },
};

/// First-frame palette origins for the balcony's three drift-puff styles.
///
/// Rows 0..2 select (64,271), (0,263) and (0,264) in VRAM words/scanlines.
/// Each of twelve animation frames advances X by 16 words for its 4-bit CLUT;
/// the puff drawer borrows the selected origin during drawing.
static const _ClutOrigin _gDryfieldNightMotelBalconyDriftPuffClutOrigins[3] SECTION(".data") = {
    { 64, 271 },
    { 0, 263 },
    { 0, 264 },
};

WorldCoordRoomLighting gDryfieldNightMotelBalconyRoomLighting[3] = {
    { .lights = &_gDryfieldNightMotelBalconyRooms1And3Lights, .ambientTable = _gDryfieldNightMotelBalconyRooms1And3AmbientByView },
    { .lights = &_gDryfieldNightMotelBalconyRoom2Lights, .ambientTable = _gDryfieldNightMotelBalconyRoom2AmbientByView },
    { .lights = &_gDryfieldNightMotelBalconyRooms1And3Lights, .ambientTable = _gDryfieldNightMotelBalconyRooms1And3AmbientByView },
};

WorldCollisionRoomResources gDryfieldNightMotelBalconyRoomCollisionResources[3] = {
    { .grid = &_gDryfieldNightMotelBalconyRooms1And2CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMotelBalconyRoom1ViewBoundaryTriggers, .actionTriggers = _gDryfieldNightMotelBalconyRoom1ActionTriggers, .occluders = _gDryfieldNightMotelBalconySightOccluders },
    { .grid = &_gDryfieldNightMotelBalconyRooms1And2CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMotelBalconyRoom2ViewBoundaryTriggers, .actionTriggers = _gDryfieldNightMotelBalconyRoom2ActionTriggers, .occluders = _gDryfieldNightMotelBalconySightOccluders },
    { .grid = &_gDryfieldNightMotelBalconyRoom3CollisionGrid, .viewBoundaryTriggers = _gDryfieldNightMotelBalconyRoom3ViewBoundaryTriggers, .actionTriggers = _gDryfieldNightMotelBalconyRoom3ActionTriggers, .occluders = _gDryfieldNightMotelBalconySightOccluders },
};

/// Logical-view to camera/image/sprite indices for balcony room 2.
///
/// Logical views 1..39 index bytes at view minus one; values are 1-based mapped
/// indices in 1..39. The first six select 1, 7, 8, 25, 26, 10, and views 7..9
/// select 2, 3, 9. Multiple logical views share an index. The final stored zero
/// is outside the 39-view lookup/search domain, not a terminator. The room
/// overlay owns this writable 40-byte array for its loaded lifetime.
static u8 _gDryfieldNightMotelBalconyRoom2ViewMap[40] = {
    1,
    7,
    8,
    25,
    26,
    10,
    2,
    3,
    9,
    6,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    0,
};

/// Logical-view to camera/image/sprite indices for balcony room 3.
///
/// Logical views 1..39 index bytes at view minus one; values are 1-based mapped
/// indices in 1..39. The first six select 1, 33, 34, 31, 32, 35, and views 7..9
/// select 2, 3, 9. Multiple logical views share an index. The final stored zero
/// is outside the 39-view lookup/search domain, not a terminator. The room
/// overlay owns this writable 40-byte array for its loaded lifetime.
static u8 _gDryfieldNightMotelBalconyRoom3ViewMap[40] = {
    1,
    33,
    34,
    31,
    32,
    35,
    2,
    3,
    9,
    6,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    0,
};

u8* gDryfieldNightMotelBalconyViewMaps[3] = {
    gViewIdentityMap,
    _gDryfieldNightMotelBalconyRoom2ViewMap,
    _gDryfieldNightMotelBalconyRoom3ViewMap,
};

/// Balcony view-count, transition and interaction constants.
///
/// Warp-slot bytes store the current-area departure in the high nibble and
/// the destination arrival in the low nibble. Sound IDs select this room's
/// nighttime Dryfield bank; map markers are independent of door-unlock flags.
enum {
    DRYFIELD_NIGHT_MOTEL_BALCONY_LOGICAL_VIEW_COUNT          = 39,
    DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_ARRIVAL_SOUND   = 0x531D0006,
    DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_DEPARTURE_SOUND = 0x531D0005,
    DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_ARRIVAL_SOUND          = 0x531D0002,
    DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_DEPARTURE_SOUND        = 0x531D0001,
    DRYFIELD_NIGHT_MOTEL_BALCONY_BLOCKED_WARP_SOUND          = 0x531D0009,
    DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM5_MAP_FLAG              = 464,
    DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM6_MAP_FLAG              = 465,
    DRYFIELD_NIGHT_MOTEL_BALCONY_LOFT_MAP_FLAG               = 466,
    DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_WARP_SLOTS      = (1 << 4) | 5,
    DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM5_WARP_SLOTS            = (2 << 4) | 1,
    DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM6_WARP_SLOTS            = (3 << 4) | 1,
    DRYFIELD_NIGHT_MOTEL_BALCONY_LOFT_WARP_SLOTS             = (4 << 4) | 1,
    DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM2_EXIT_CAP_COMMAND      = 8,
    DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM3_BOUNDARY_CAP_COMMAND  = 9,
};

ViewCount gDryfieldNightMotelBalconyRoomViewCounts[3] = { DRYFIELD_NIGHT_MOTEL_BALCONY_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_MOTEL_BALCONY_LOGICAL_VIEW_COUNT, DRYFIELD_NIGHT_MOTEL_BALCONY_LOGICAL_VIEW_COUNT };

DirectionWarpEntry gDryfieldNightMotelBalconyWarpEntries[5] = {
    { { { .word = 0 }, -0x315E, -3198, 1396 }, { 0, 0, 0, 0 }, { { .word = 0 }, -0x315E, -3198, 1396 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_ARRIVAL_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_DEPARTURE_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_BLOCKED_WARP_SOUND, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ONE / 4 }, -6572, -3200, -2869 }, { 0, 0, 0, 0 }, { { .word = ONE / 4 }, -6572, -3200, -2869 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_ARRIVAL_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_DEPARTURE_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_BLOCKED_WARP_SOUND, 6, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM5_MAP_FLAG },
    { { { .word = ONE / 4 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, { { .word = ONE / 4 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_ARRIVAL_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_DEPARTURE_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_BLOCKED_WARP_SOUND, 5, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM6_MAP_FLAG },
    { { { .word = ONE / 2 }, -1990, -3200, 0x29A8 }, { 0, 0, 0, 0 }, { { .word = ONE / 2 }, -1990, -3200, 0x29A8 }, { 0, 0, 0, 0 }, DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_ARRIVAL_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_DOOR_DEPARTURE_SOUND, DRYFIELD_NIGHT_MOTEL_BALCONY_BLOCKED_WARP_SOUND, 4, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_MOTEL_BALCONY_LOFT_MAP_FLAG },
    { { { .word = ONE / 4 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, { { .word = ONE / 4 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM6_MAP_FLAG },
};

/// Room-space face normals for the rooms 1 and 2 collision grid; 4096 represents unit length.
static SVECTOR _gDryfieldNightMotelBalconyRooms1And2CollisionGridNormals[7] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_normals.inc"
};

/// Room-space vertices for the rooms 1 and 2 collision mesh, in signed game-coordinate units.
static SVECTOR _gDryfieldNightMotelBalconyRooms1And2CollisionGridVerts[69] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_verts.inc"
};

/// Indexed triangles and quads for the rooms 1 and 2 collision mesh.
static WorldCollisionGridFace _gDryfieldNightMotelBalconyRooms1And2CollisionGridFaces[32] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_faces.inc"
};

/// Signed face-index lists for the rooms 1 and 2 collision cells, each ending with `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldNightMotelBalconyRooms1And2CollisionGridCells[342] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelBalconyRooms1And2CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..341 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_BALCONY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelBalconyRooms1And2CollisionGridCells[(halfwordOffset)])
/// X-major 8-by-8 cell directory into the rooms 1 and 2 collision face-index lists.
static s16* _gDryfieldNightMotelBalconyRooms1And2CollisionGridTable[64] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_BALCONY_CELL_FACE_LIST_AT_OFFSET

/// Collision grid shared by nighttime motel balcony rooms 1 and 2.
///
/// The 8 by 8 XZ cells have sides of 4000 game units and origin (-12990, -13340).
/// Activation binds the writable descriptor to the current view transform.
/// The descriptor and its mesh/index pools stay live with the loaded room overlay.
static WorldCollisionGrid _gDryfieldNightMotelBalconyRooms1And2CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelBalconyRooms1And2CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelBalconyRooms1And2CollisionGridVerts,
    .faces       = _gDryfieldNightMotelBalconyRooms1And2CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelBalconyRooms1And2CollisionGridTable,
    .xBias       = 12990,
    .zBias       = 13340,
    .cellCountX  = 8,
    .cellCountZ  = 8,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelBalconyRooms1And2CollisionGridFaces),
};

/// Room-space collision normals for room 3, with 4096 representing unit length.
static SVECTOR _gDryfieldNightMotelBalconyRoom3CollisionGridNormals[11] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_normals.inc"
};

/// Room-space collision vertices for room 3, in signed game-coordinate units.
static SVECTOR _gDryfieldNightMotelBalconyRoom3CollisionGridVerts[81] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_verts.inc"
};

/// Indexed triangles and quads for the room 3 collision mesh.
static WorldCollisionGridFace _gDryfieldNightMotelBalconyRoom3CollisionGridFaces[37] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_faces.inc"
};

/// Signed face-index lists for the room 3 collision cells, each ending with `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gDryfieldNightMotelBalconyRoom3CollisionGridCells[360] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightMotelBalconyRoom3CollisionGridCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..359 to a list's
/// first entry, evaluated once; the list ends at WORLD_COLLISION_GRID_CELL_END.
/// Returns a borrowed s16* without modifying the pool. `cell_face_list_macro`
/// selects this binding for the table include; it is undefined afterwards.
#define DRYFIELD_NIGHT_MOTEL_BALCONY_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightMotelBalconyRoom3CollisionGridCells[(halfwordOffset)])
/// X-major 8-by-8 cell directory into the room 3 collision face-index lists.
static s16* _gDryfieldNightMotelBalconyRoom3CollisionGridTable[64] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_table.inc"
};
#undef DRYFIELD_NIGHT_MOTEL_BALCONY_CELL_FACE_LIST_AT_OFFSET

/// Collision grid for nighttime motel balcony room 3.
///
/// The 8 by 8 XZ cells have sides of 4000 game units and origin (-12990, -13340).
/// Activation binds the writable descriptor to the current view transform.
/// The descriptor and its mesh/index pools stay live with the loaded room overlay.
static WorldCollisionGrid _gDryfieldNightMotelBalconyRoom3CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightMotelBalconyRoom3CollisionGridNormals,
    .vertices    = _gDryfieldNightMotelBalconyRoom3CollisionGridVerts,
    .faces       = _gDryfieldNightMotelBalconyRoom3CollisionGridFaces,
    .cellFaceIds = _gDryfieldNightMotelBalconyRoom3CollisionGridTable,
    .xBias       = 12990,
    .zBias       = 13340,
    .cellCountX  = 8,
    .cellCountZ  = 8,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightMotelBalconyRoom3CollisionGridFaces),
};

ViewCamera gDryfieldNightMotelBalconyViewCameras[39] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 4112, 0x88B8, -2748 } }, 380 },
    { { { { -559, 0, -4057 }, { -963, 3978, 132 }, { 3941, 972, -543 } }, { 0x4437, 4754, -1733 } }, 230 },
    { { { { 1192, 0, 3918 }, { 526, 4058, -160 }, { -3883, 550, 1181 } }, { 3977, 4660, -1257 } }, 230 },
    { { { { 3968, 0, -1013 }, { -310, 3898, -1217 }, { 964, 1256, 3777 } }, { 5913, 5512, -3603 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6137, 4498, 2582 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 257, 0, -4087 }, { -569, 4056, -35 }, { 4048, 570, 255 } }, { 0x3CEF, 4634, -1649 } }, 289 },
    { { { { 543, 0, -4059 }, { 386, 4077, 51 }, { 4041, -389, 540 } }, { 0x2D6F, 4037, -1767 } }, 289 },
    { { { { -1629, 0, 3757 }, { 362, 4076, 157 }, { -3740, 395, -1622 } }, { -2180, 4664, -0x298C } }, 230 },
    { { { { 3791, 0, -1548 }, { 139, 4079, 341 }, { 1542, -369, 3776 } }, { 6606, 4183, 7528 } }, 230 },
    { { { { 3674, 0, -1810 }, { -1285, 2883, -2609 }, { 1274, 2908, 2586 } }, { 5542, 9403, -3532 } }, 230 },
    { { { { 3475, 0, -2168 }, { -803, 3804, -1288 }, { 2013, 1518, 3227 } }, { 4659, 7233, 2080 } }, 230 },
    { { { { 2289, 0, -3396 }, { -1745, 3513, -1176 }, { 2913, 2104, 1963 } }, { 6624, 8520, 6055 } }, 230 },
    { { { { -3895, 0, -1265 }, { 436, 3844, -1344 }, { 1187, -1413, -3656 } }, { 6016, 3919, -0x2A09 } }, 230 },
    { { { { 3347, 0, -2360 }, { 1258, 3465, 1784 }, { 1997, -2183, 2831 } }, { 6606, 3324, 1736 } }, 230 },
    { { { { 3538, 0, -2062 }, { 822, 3756, 1411 }, { 1891, -1633, 3244 } }, { 6146, 3144, 8006 } }, 230 },
    { { { { -4087, 0, 270 }, { -89, 3867, -1344 }, { -255, -1347, -3859 } }, { 5661, 3313, -134 } }, 289 },
    { { { { -3824, 0, 1466 }, { -419, 3924, -1094 }, { -1405, -1172, -3664 } }, { 1367, 798, 24 } }, 230 },
    { { { { 3637, 0, -1883 }, { 328, 4033, 634 }, { 1854, -714, 3581 } }, { 6173, 491, 4712 } }, 230 },
    { { { { 3950, 0, -1083 }, { 204, 4022, 747 }, { 1064, -774, 3878 } }, { 6813, 3650, -7925 } }, 230 },
    { { { { 4092, 0, -163 }, { -89, 3427, -2240 }, { 137, 2241, 3425 } }, { 4200, 2740, 740 } }, 329 },
    { { { { 4006, 0, 850 }, { -182, 4001, 857 }, { -831, -876, 3913 } }, { 5560, 3880, -2000 } }, 329 },
    { { { { 4006, 0, 850 }, { -182, 4001, 857 }, { -831, -876, 3913 } }, { 6042, 3553, 3407 } }, 329 },
    { { { { 3135, 0, -2634 }, { 1166, 3672, 1388 }, { 2362, -1813, 2811 } }, { 6210, 6110, -4190 } }, 289 },
    { { { { 3968, 0, -1013 }, { -310, 3898, -1217 }, { 964, 1256, 3777 } }, { 5913, 5512, -3603 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6137, 4498, 2582 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 611, 0, -4050 }, { -580, 4053, -87 }, { 4008, 587, 605 } }, { 0x34E4, 5130, -1530 } }, 329 },
    { { { { -3824, 0, 1466 }, { -419, 3924, -1094 }, { -1405, -1172, -3664 } }, { 1367, 798, 24 } }, 230 },
    { { { { 3685, 0, 1788 }, { 67, 4093, -139 }, { -1786, 155, 3682 } }, { 4500, 4300, 2470 } }, 230 },
    { { { { 4021, 0, -776 }, { -94, 4065, -487 }, { 770, 496, 3991 } }, { 5620, 4940, -4510 } }, 230 },
    { { { { 4065, 0, 497 }, { -10, 4095, 82 }, { -497, -82, 4064 } }, { 5670, 4140, -930 } }, 230 },
    { { { { -559, 0, -4057 }, { -963, 3978, 132 }, { 3941, 972, -543 } }, { 0x4437, 4754, -1733 } }, 230 },
    { { { { 960, 0, 3981 }, { 404, 4074, -97 }, { -3961, 416, 955 } }, { 3520, 4170, -1056 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 2854, 0, -2937 }, { -843, 3923, -819 }, { 2813, 1176, 2734 } }, { 6920, 4745, 740 } }, 230 },
    { { { { 4085, 0, 295 }, { 258, 1993, -3568 }, { -144, 3578, 1988 } }, { 2970, 0x2D32, 2685 } }, 289 },
    { { { { -3608, 0, -1938 }, { 627, 3875, -1168 }, { 1833, -1326, -3413 } }, { 6830, 3500, -9600 } }, 230 },
    { { { { -3526, 0, -2083 }, { 695, 3861, -1176 }, { 1963, -1366, -3324 } }, { 6640, 510, -9760 } }, 230 },
};

/// Empty sprite batch list for mapped balcony view 1, retaining its decoded background.
///
/// The zero-count opener selects background image strips; the second record
/// ends the list. The descriptor also retains this list in its empty source
/// slot, without reading it as source elements. The room owns the writable
/// list for the loaded overlay's lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView1SpriteBatches[2] = {
    { 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Texture format and neutral RGB modulation of the view sprite sources below.
enum {
    DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT = 1,
    DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB  = 128,
};

/// Depth-sorted room sprites for mapped balcony view 2.
///
/// Batch ranges select these 185 elements. Positions/dimensions are drawing
/// pixels, UVs are page-relative texels and depth is an unscaled OT sorting
/// value. Pages use 8-bit textures and RGB 128 is neutral modulation. Packet
/// creation snapshots geometry and texture state; cached linking rereads depth.
/// The room overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelBalconyView2SpriteSources[185] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -120, 1690, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -120, 2261, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -120, 2394, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -112, 1319, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -112, 1326, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -112, 1332, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -112, 1338, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -112, 1344, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -112, 1351, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -112, 1357, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -112, 1364, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -112, 1370, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -112, 1377, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -112, 1383, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -104, 1341, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -104, 1337, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -104, 1353, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -104, 1360, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -104, 1366, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -104, 1373, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -104, 1379, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -104, 1386, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -104, 1393, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -104, 1398, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -104, 1405, { .fields = { 72, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -96, 1352, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -96, 1357, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -96, 1364, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -96, 1371, { .fields = { 8, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -96, 1377, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -96, 1384, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -96, 1391, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -96, 1397, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -96, 1404, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -96, 1411, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -96, 1418, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -88, 1359, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -88, 1369, { .fields = { 40, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -88, 1376, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -88, 1382, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -88, 1389, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -88, 1396, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -88, 1402, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -88, 1409, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -88, 1416, { .fields = { 24, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -88, 1423, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -88, 1430, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -80, 1371, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -80, 1380, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -80, 1387, { .fields = { 24, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -80, 1394, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -80, 1400, { .fields = { 24, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -80, 1407, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -80, 1410, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -80, 1416, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -80, 1428, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -80, 1435, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -80, 1443, { .fields = { 16, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -72, 1388, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -72, 1392, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -72, 1399, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -72, 1405, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -72, 1412, { .fields = { 16, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -72, 1419, { .fields = { 16, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -72, 1432, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -72, 1443, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -72, 1450, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -72, 1456, { .fields = { 16, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -72, 1455, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -64, 1399, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -64, 1404, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -64, 1410, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -64, 1417, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -64, 1424, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -64, 1431, { .fields = { 16, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -64, 1446, { .fields = { 16, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -64, 1456, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -64, 1463, { .fields = { 16, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -64, 1468, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -64, 1468, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -56, 1406, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -56, 1415, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -56, 1422, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -56, 1429, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -56, 1437, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -56, 1444, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -56, 1461, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -56, 1468, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -56, 1476, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -56, 1478, { .fields = { 8, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -56, 1481, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -48, 1415, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -48, 1427, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -48, 1435, { .fields = { 8, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -48, 1442, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -48, 1449, { .fields = { 8, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -48, 1456, { .fields = { 8, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -48, 1483, { .fields = { 8, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -48, 1491, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -48, 1499, { .fields = { 8, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -48, 1487, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -48, 1494, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -40, 1433, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -40, 1440, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -40, 1447, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -40, 1454, { .fields = { 8, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -40, 1462, { .fields = { 0, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -40, 1469, { .fields = { 0, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -40, 1496, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -40, 1504, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -40, 1512, { .fields = { 0, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -40, 1495, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -40, 1508, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -32, 1380, { .fields = { 0, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -32, 1452, { .fields = { 0, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -32, 1460, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -32, 1467, { .fields = { 0, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -32, 1475, { .fields = { 0, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -32, 1482, { .fields = { 0, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -32, 1451, { .fields = { 0, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -32, 1427, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -32, 1456, { .fields = { 0, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -32, 1513, { .fields = { 0, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -32, 1521, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -24, 1392, { .fields = { 0, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -24, 1465, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -24, 1472, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -24, 1480, { .fields = { 120, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -24, 1488, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -24, 1495, { .fields = { 120, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -24, 1502, { .fields = { 8, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -24, 1418, { .fields = { 120, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -24, 1511, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -24, 1519, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -24, 1535, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, -16, 1470, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 80, -16, 1478, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -16, 1486, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -16, 1493, { .fields = { 120, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -16, 1501, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -16, 1509, { .fields = { 120, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -16, 1516, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -16, 1524, { .fields = { 120, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -16, 1533, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -16, 1541, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -16, 1550, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, -8, 1499, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -8, 1507, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, -8, 1515, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, -8, 1522, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, -8, 1527, { .fields = { 112, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, -8, 1532, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, -8, 1537, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, -8, 1543, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, -8, 1552, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 0, 1506, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, 0, 1513, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, 0, 1518, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, 0, 1524, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, 0, 1533, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, 0, 1541, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, 0, 1549, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 0, 1558, { .fields = { 112, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 0, 1566, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 8, 1526, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, 8, 1533, { .fields = { 112, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 104, 8, 1532, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 112, 8, 1547, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 120, 8, 1555, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 128, 8, 1555, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 136, 8, 1571, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 144, 8, 1578, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, 152, 8, 1579, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -64, -88, 2408, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -24, -88, 2400, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, -40, 2400, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 112 } }, 0, -120, 1400, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, -8, -120, 1510, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 112 } }, -16, -120, 2087, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 112 } }, -160, -120, 1250, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -88, -112, 1500, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -80, -104, 1750, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -72, -96, 2077, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 104, 72 } }, -104, -32, 1000, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 0, -32, 1000, { .fields = { 24, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite batches for mapped balcony view 2, retaining its decoded background.
///
/// The zero-count opener selects background image strips and is skipped when
/// linking cached sprites. Nonterminal ranges partition the associated source
/// array in elements; the last record ends the list. The room overlay owns
/// the list and its writable visibility bytes for its loaded lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView2SpriteBatches[4] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = 183, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 183, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyView2SpriteSources) - 183, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Clip rectangle and full-screen restoration depth for mapped balcony view 2.
///
/// The rectangle is in draw-buffer pixels; restoreDepth is the unscaled OT
/// sorting value where full-screen drawing resumes. The second record ends
/// the list. Commands affect scene drawing as well as sprites; the room-owned
/// storage stays loaded while packets or frame-capture clips are built.
static SpriteDrawArea _gDryfieldNightMotelBalconyView2SpriteDrawAreas[2] = {
    { .clipRect = { 0, 0, 317, 87 }, .restoreDepth = 3000 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// Empty sprite batch list for mapped balcony view 3, retaining its decoded background.
///
/// The zero-count opener selects background image strips; the second record
/// ends the list. The descriptor also retains this list in its empty source
/// slot, without reading it as source elements. The room owns the writable
/// list for the loaded overlay's lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView3SpriteBatches[2] = {
    { 0 },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Depth-sorted room sprites for mapped balcony view 4.
///
/// Batch ranges select these 12 elements. Positions/dimensions are drawing
/// pixels, UVs are page-relative texels and depth is an unscaled OT sorting
/// value. Pages use 8-bit textures and RGB 128 is neutral modulation. Packet
/// creation snapshots geometry and texture state; cached linking rereads depth.
/// The room overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelBalconyView4SpriteSources[12] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, 0, 1479, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -24, 0, 1295, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -8, 0, 1481, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 8, -8, 1508, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 32, -8, 1537, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 48, -8, 1567, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 64, -8, 1600, { .fields = { 112, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 80, -8, 1633, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 96, -8, 1669, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 112, -16, 1682, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 128, -16, 1719, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 144, -8, 1755, { .fields = { 104, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite batches for mapped balcony view 4, retaining its decoded background.
///
/// The zero-count opener selects background image strips and is skipped when
/// linking cached sprites. Nonterminal ranges partition the associated source
/// array in elements; the last record ends the list. The room overlay owns
/// the list and its writable visibility bytes for its loaded lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView4SpriteBatches[3] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyView4SpriteSources), .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Depth-sorted room sprites for mapped balcony view 5.
///
/// Batch ranges select these 54 elements. Positions/dimensions are drawing
/// pixels, UVs are page-relative texels and depth is an unscaled OT sorting
/// value. Pages use 8-bit textures and RGB 128 is neutral modulation. Packet
/// creation snapshots geometry and texture state; cached linking rereads depth.
/// The room overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelBalconyView5SpriteSources[54] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, 112, 428, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 24, 2957, { .fields = { 40, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -40, 16, 2861, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 16, 2286, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -64, 32, 2560, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -24, 24, 1808, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 40, 2079, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -16, 32, 1517, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -8, 32, 1365, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 48, 1652, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -40, 48, 1652, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -64, 56, 1426, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -32, 56, 1426, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 0, 40, 1194, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 8, 48, 1067, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 16, 48, 980, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -64, 64, 1272, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -24, 64, 1272, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -64, 72, 1202, { .fields = { 16, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -24, 72, 1202, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 80, 1055, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -16, 80, 1055, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 88, 975, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, 88, 975, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 24, 56, 901, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 96, 908, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -16, 96, 908, { .fields = { 24, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 16, 96, 900, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 32, 64, 833, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 40, 72, 770, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 48, 72, 734, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 104, 851, { .fields = { 8, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -16, 104, 851, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 24, 104, 852, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -64, 112, 802, { .fields = { 8, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, 112, 802, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 32, 112, 808, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 56, 80, 694, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 64, 88, 658, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 72, 96, 625, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 80, 96, 611, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 88, 104, 444, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 88 } }, -160, -120, 700, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -128, -120, 750, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 80 } }, -160, -32, 700, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, -128, -32, 750, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -160, 48, 700, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 72 } }, -128, 48, 750, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, -120, 495, { .fields = { 80, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -144, -112, 501, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -128, -96, 526, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -40, 16, 2861, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -8, 16, 2860, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, 24, 16, 2860, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite batches for mapped balcony view 5, retaining its decoded background.
///
/// The zero-count opener selects background image strips and is skipped when
/// linking cached sprites. Nonterminal ranges partition the associated source
/// array in elements; the last record ends the list. The room overlay owns
/// the list and its writable visibility bytes for its loaded lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView5SpriteBatches[6] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = 42, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 42, .spriteCount = 6, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 48, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 51, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyView5SpriteSources) - 51, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Depth-sorted room sprites for mapped balcony view 6.
///
/// Batch ranges select these 38 elements. Positions/dimensions are drawing
/// pixels, UVs are page-relative texels and depth is an unscaled OT sorting
/// value. Pages use 8-bit textures and RGB 128 is neutral modulation. Packet
/// creation snapshots geometry and texture state; cached linking rereads depth.
/// The room overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelBalconyView6SpriteSources[38] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -160, 80, 351, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 48 } }, -104, 72, 369, { .fields = { 0, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 48 } }, -40, 72, 394, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 56 } }, 24, 64, 427, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 96, 72, 450, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -48, 24, 3491, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -40, 24, 2533, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, 24, 2009, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -24, 24, 1667, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, 32, 1417, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -8, 32, 1239, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 0, 40, 1107, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 16, 40, 971, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 8, 40, 997, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 24, 48, 902, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 32, 48, 833, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 40, 48, 787, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 56, 742, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 56, 724, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 64, 56, 675, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -56, 24, 3724, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -56, 32, 3740, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -56, 40, 2800, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -56, 48, 1729, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -56, 56, 1513, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -56, 64, 1193, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -56, 72, 1050, { .fields = { 16, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, 72, 1050, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 176 } }, -160, -120, 1176, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 176 } }, -120, -120, 1219, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 168 } }, -112, -112, 1216, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 152 } }, -104, -96, 1514, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 128 } }, -96, -72, 1714, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -88, -48, 1794, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -80, -32, 1632, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -48, 24, 3724, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -16, 24, 3720, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 8, 24, 3720, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite batches for mapped balcony view 6, retaining its decoded background.
///
/// The zero-count opener selects background image strips and is skipped when
/// linking cached sprites. Nonterminal ranges partition the associated source
/// array in elements; the last record ends the list. The room overlay owns
/// the list and its writable visibility bytes for its loaded lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView6SpriteBatches[7] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = 5, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 5, .spriteCount = 15, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 20, .spriteCount = 8, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = 28, .spriteCount = 7, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 35, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyView6SpriteSources) - 35, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Depth-sorted room sprites for mapped balcony view 7.
///
/// Batch ranges select these 74 elements. Positions/dimensions are drawing
/// pixels, UVs are page-relative texels and depth is an unscaled OT sorting
/// value. Pages use 8-bit textures and RGB 128 is neutral modulation. Packet
/// creation snapshots geometry and texture state; cached linking rereads depth.
/// The room overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelBalconyView7SpriteSources[74] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -32, -16, 2580, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -32, -8, 2577, { .fields = { 16, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -32, 0, 2244, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, 8, 575, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -128, 64, 575, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -96, 8, 575, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -96, 64, 575, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 40, 56 } }, -64, 8, 575, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 56 } }, -64, 64, 575, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -24, 8, 575, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, -24, 64, 575, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, 8, 8, 575, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, 8, 64, 575, { .fields = { 16, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, 40, 8, 575, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 56 } }, 72, 8, 575, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 32, 56 } }, 40, 64, 575, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 24, 56 } }, 72, 64, 575, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -80, -64, 1037, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, -72, 1133, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -64, -64, 1249, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 8, 56 } }, -56, -64, 1391, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -48, -72, 1569, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -40, -72, 1800, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -32, -72, 2064, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 40, -56, 1838, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 48, -64, 1402, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, -24, 1126, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 64, -72, 872, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 72, -64, 880, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 24, 1625, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 56 } }, -80, 64, 1125, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 8, 16, 1875, { .fields = { 64, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -48, 24, 1625, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -56, 32, 1524, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 0, 32, 1524, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -64, 40, 1375, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -72, 48, 1325, { .fields = { 40, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -8, 48, 1325, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, -80, 56, 1125, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, -8, 56, 1125, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 56 } }, -8, 64, 1125, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 88, 56 } }, 72, 64, 1125, { .fields = { 16, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 48, 8 } }, 0, 8, 2000, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -40, 8, 2000, { .fields = { 0, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, -48, 16, 1875, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 64, 8 } }, 0, 40, 1375, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 0, 24, 1625, { .fields = { 80, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 16, 24, 1625, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 32, 24, 1625, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -144, 64, 1125, { .fields = { 80, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 16, 56 } }, -160, 64, 1125, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 16, 56 } }, -128, 64, 1125, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 16, 56 } }, -112, 64, 1125, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 56 } }, -96, 64, 1125, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -32, -16, 2601, { .fields = { 16, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -32, -8, 2602, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -32, 0, 2269, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 32, 0, 2294, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 144 } }, 40, -112, 1843, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 168 } }, 48, -120, 1427, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 184 } }, 56, -120, 1160, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 192 } }, 64, -120, 970, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 96 } }, 72, -120, 862, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 96 } }, 72, -24, 862, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 112 } }, -32, -96, 2110, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 128 } }, -40, -104, 1825, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 144 } }, -48, -112, 1594, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 160 } }, -56, -120, 1416, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 168 } }, -64, -120, 1274, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 184 } }, -72, -120, 1160, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 192 } }, -80, -120, 1064, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 192 } }, -88, -120, 982, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 96 } }, -160, -120, 950, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 96 } }, -160, -24, 950, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite batches for mapped balcony view 7, retaining its decoded background.
///
/// The zero-count opener selects background image strips and is skipped when
/// linking cached sprites. Nonterminal ranges partition the associated source
/// array in elements; the last record ends the list. The room overlay owns
/// the list and its writable visibility bytes for its loaded lifetime.
static SpriteBatch _gDryfieldNightMotelBalconyView7SpriteBatches[8] = {
    { 0 },
    { .firstSprite = 0, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 3, 0 } },
    { .firstSprite = 3, .spriteCount = 14, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 0, 0 } },
    { .firstSprite = 17, .spriteCount = 12, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 5, 0 } },
    { .firstSprite = 29, .spriteCount = 25, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 1, 0 } },
    { .firstSprite = 54, .spriteCount = 3, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 4, 0 } },
    { .firstSprite = 57, .spriteCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyView7SpriteSources) - 57, .hidden = 0, .skipCachedPackets = 0, .field_6 = { 2, 0 } },
    { .firstSprite = SPRITE_BATCH_END },
};

/// Clip rectangle and full-screen restoration depth for mapped balcony view 7.
///
/// The rectangle is in draw-buffer pixels; restoreDepth is the unscaled OT
/// sorting value where full-screen drawing resumes. The second record ends
/// the list. Commands affect scene drawing as well as sprites; the room-owned
/// storage stays loaded while packets or frame-capture clips are built.
static SpriteDrawArea _gDryfieldNightMotelBalconyView7SpriteDrawAreas[2] = {
    { .clipRect = { 0, 0, 237, 239 }, .restoreDepth = 1125 },
    { .restoreDepth = SPRITE_DRAW_AREA_END },
};

/// Depth-sorted room sprites for mapped balcony view 8.
///
/// Batch ranges select these 74 elements. Positions/dimensions are drawing
/// pixels, UVs are page-relative texels and depth is an unscaled OT sorting
/// value. Pages use 8-bit textures and RGB 128 is neutral modulation. Packet
/// creation snapshots geometry and texture state; cached linking rereads depth.
/// The room overlay owns this writable array while its sprite resources are used.
static SpriteSource _gDryfieldNightMotelBalconyView8SpriteSources[74] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 216 } }, -40, -96, 1170, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 232 } }, -48, -112, 1088, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, -56, -120, 996, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, -64, -120, 945, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, -72, -120, 856, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, -80, -120, 826, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, -88, -120, 779, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 240 } }, -104, -120, 970, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 240 } }, -120, -120, 677, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 240 } }, -136, -120, 626, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 240 } }, -160, -120, 551, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 96 } }, 80, -120, 1043, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, 88, -120, 977, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, 80, 64, 755, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 240 } }, 96, -120, 893, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 80, 8 } }, -96, 112, 760, { .fields = { 24, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, -16, 112, 760, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, 56, 112, 760, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, -88, 104, 807, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 64, 8 } }, -16, 104, 807, { .fields = { 40, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, 48, 104, 807, { .fields = { 32, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, -72, 96, 913, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 0, 96, 913, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 64, 96, 913, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, -56, 88, 1100, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 56, 8 } }, 0, 88, 1100, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 48, 8 } }, 56, 88, 1100, { .fields = { 64, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 48, 24 } }, -40, 48, 1243, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, 56, 1243, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 24 } }, 24, 48, 1243, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 64, 8 } }, -40, 72, 1237, { .fields = { 40, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, 24, 72, 1237, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, -48, 80, 1212, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 255), { .fields = { 72, 8 } }, 24, 80, 1212, { .fields = { 32, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 80, 8 } }, -56, 80, 1085, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 72, 8 } }, 24, 80, 1080, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 64, 8 } }, -40, 72, 1215, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 64, 8 } }, 24, 72, 1215, { .fields = { 40, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 64, 8 } }, -40, 64, 1217, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 72, 8 } }, 24, 64, 1217, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 64, 24 } }, -40, 40, 1225, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 72, 24 } }, 24, 40, 1225, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -160, 32, 507, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -152, 88, 525, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 152 } }, -40, -64, 1145, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 160 } }, -48, -72, 1080, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 160 } }, -56, -80, 986, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 168 } }, -64, -88, 910, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 8, 128 } }, -80, -96, 769, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 8, 128 } }, -88, -104, 747, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 16, 128 } }, -128, -96, 596, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 128 } }, -96, -104, 706, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 128 } }, -112, -104, 654, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 152 } }, -136, -88, 560, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 144 } }, -144, -80, 540, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 8, 120 } }, -152, -72, 521, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 56 } }, -160, -64, 379, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 24 } }, -88, 40, 774, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 160 } }, -72, -80, 846, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 24 } }, -88, 88, 738, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 48 } }, -104, 72, 664, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 48 } }, -120, 72, 622, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 48 } }, -136, 64, 579, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 48 } }, -144, 64, 532, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, 104, 514, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -160, 56, 390, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 144 } }, 152, -120, 549, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 144 } }, 144, -104, 573, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 144 } }, 136, -104, 601, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 112 } }, 128, -72, 659, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 832, 0), getClut(0, 256), { .fields = { 8, 120 } }, 120, -80, 696, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 144 } }, 112, -80, 767, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 152 } }, 104, -64, 823, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 48 } }, 96, -32, 887, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 8.
///
/// Ranges cover all 74 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView8SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 19, 0, 0, { 0, 0 } },
    { 34, 8, 0, 0, { 2, 0 } },
    { 42, 32, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clipping rectangle and full-screen restore boundary for mapped balcony view 8.
///
/// The rectangle uses draw-buffer pixels; the restore boundary is an
/// unscaled OT sorting depth. The second record terminates the list.
/// The clip also affects scene primitives drawn before the restore command.
static SpriteDrawArea _gDryfieldNightMotelBalconyView8SpriteDrawAreas[2] = {
    { { 0, 0, 263, 239 }, 875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Depth-sorted room sprites for mapped balcony view 9.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView9SpriteSources[102] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -160, 72, 511, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -144, 80, 433, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 88, 484, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -112, 104, 485, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -64, 64, 622, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 56 } }, -56, 64, 576, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 8, 72, 516, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, 48, 80, 474, { .fields = { 16, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 32 } }, 96, 88, 449, { .fields = { 0, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 144, 96, 427, { .fields = { 16, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -88, 80, 721, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -80, 80, 716, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -72, 88, 708, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -64, 96, 713, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -56, 104, 627, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -48, 96, 715, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 16, 24, 1189, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 8, 24, 1085, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 0, 32, 1000, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -8, 32, 925, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, 40, 862, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -24, 40, 601, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, 48, 608, { .fields = { 8, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 24, 48, 1175, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 16, 56, 1067, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, 8, 64, 978, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 56, 80, 839, { .fields = { 88, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 8, 72, 903, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 64, 72, 903, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 8, 1830, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 8, 1672, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 32, 8, 1469, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 24, 16, 1314, { .fields = { 16, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 48, 24, 1701, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 40, 32, 1478, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 32, 40, 1328, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, 88, 16, 2205, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 96, 24, 2125, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -48, 0, 2108, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -32, 0, 2040, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -16, 0, 1977, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 0, 8, 1932, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 24, 8, 1877, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -16, -16, 2703, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 0, -8, 2631, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, 16, -24, 2534, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, 0, 2402, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -88, 0, 2317, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -72, 0, 2238, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -160, -8, 2701, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 0, 2595, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 0, 2517, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 0, 2447, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -72, -40, 2915, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -120, -40, 3206, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -112, -48, 3147, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -96, -48, 3264, { .fields = { 112, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -80, -48, 3562, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -136, -24, 3345, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 88, 8 } }, 24, 32, 1535, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 104, 8 } }, 8, 40, 1359, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 112, 8 } }, 0, 48, 1225, { .fields = { 8, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 120, 8 } }, -8, 56, 1117, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 96, 8 } }, 32, 80, 889, { .fields = { 24, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 72, 88, 615, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -16, 72, 953, { .fields = { 48, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, 48, 72, 953, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, -16, 64, 1028, { .fields = { 40, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 56, 64, 1028, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 40, 16, 2055, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, 32, 24, 1726, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 48, 8, 1871, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 40, 8, 1692, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 16, 24, 1239, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, 8, 24, 1135, { .fields = { 72, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 0, 40, 1038, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -8, 40, 978, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -16, 48, 910, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -24, 48, 746, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -32, 48, 746, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -40, 56, 757, { .fields = { 40, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -48, 56, 767, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -56, 64, 685, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -64, 64, 695, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, 32, 8, 1500, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 24, 16, 1350, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -80, 8, 2326, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -160, -8, 2805, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -152, 0, 2725, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -136, 0, 2618, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -120, 0, 2520, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -104, 0, 2427, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -88, 0, 2363, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -72, 0, 2263, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -56, 0, 2193, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 16 } }, -40, 8, 2127, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 16 } }, -24, 8, 2062, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 16 } }, -8, 8, 2001, { .fields = { 112, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 8, 8, 1943, { .fields = { 0, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 24, 8, 1889, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 16 } }, 40, 8, 1845, { .fields = { 120, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -136, -24, 3352, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 9.
///
/// Ranges cover all 102 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView9SpriteBatches[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 6, 0 } },
    { 4, 6, 0, 0, { 9, 0 } },
    { 10, 6, 0, 0, { 2, 0 } },
    { 16, 13, 0, 0, { 10, 0 } },
    { 29, 7, 0, 0, { 0, 0 } },
    { 36, 2, 0, 0, { 11, 0 } },
    { 38, 5, 0, 0, { 1, 0 } },
    { 43, 3, 0, 0, { 12, 0 } },
    { 46, 3, 0, 0, { 8, 0 } },
    { 49, 4, 0, 0, { 13, 0 } },
    { 53, 1, 0, 0, { 4, 0 } },
    { 54, 4, 0, 0, { 14, 0 } },
    { 58, 1, 0, 0, { 3, 0 } },
    { 59, 12, 0, 0, { 15, 0 } },
    { 71, 15, 0, 0, { 7, 0 } },
    { 86, 15, 0, 0, { 16, 0 } },
    { 101, 1, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 10.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView10SpriteSources[62] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -104, 0, 4829, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -32, 24, 4295, { .fields = { 48, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -64, 24, 4323, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, -104, 24, 3777, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, -104, 24, 2457, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -64, 32, 2451, { .fields = { 16, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -112, 40, 2433, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -112, 48, 1821, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -112, 56, 1512, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -88, 56, 1493, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, -120, 64, 1237, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -128, 72, 1062, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -128, 80, 1012, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -40, 40, 1522, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 40, 1342, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, 40, 1197, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, 40, 1081, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -8, 48, 984, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 0, 48, 905, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 8, 48, 838, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 16, 56, 779, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 24, 56, 743, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, 56, 721, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 56, 706, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 48, 64, 647, { .fields = { 32, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -120, 0, 1349, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -96, 56, 1517, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -96, 48, 1620, { .fields = { 8, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, -112, 0, 1534, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -112, 56, 1517, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -112, 64, 1517, { .fields = { 24, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 88, 40 } }, -160, 80, 403, { .fields = { 32, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 88, 48 } }, -72, 72, 469, { .fields = { 40, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 56 } }, 16, 64, 594, { .fields = { 64, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -104, 32, 3778, { .fields = { 120, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -112, 40, 2458, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -112, 48, 1846, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 104, 8 } }, -120, 56, 1518, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 120, 8 } }, -120, 64, 1237, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -128, 80, 1023, { .fields = { 104, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 88, 8 } }, -80, 72, 1063, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 8 } }, -128, 72, 1088, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -96, 16, 4829, { .fields = { 64, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -80, 32, 4027, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -72, 24, 3762, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -64, 24, 2460, { .fields = { 72, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -56, 32, 2462, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -48, 32, 2435, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -40, 32, 1522, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -32, 40, 1493, { .fields = { 96, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -24, 56, 1251, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -16, 56, 1081, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -8, 64, 985, { .fields = { 104, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 0, 64, 907, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 8, 64, 838, { .fields = { 88, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 16, 64, 825, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 24, 64, 742, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 32, 56, 721, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 40, 64, 624, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 48, 64, 633, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 56, 64, 639, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, 64, 72, 626, { .fields = { 32, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 10.
///
/// Ranges cover all 62 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView10SpriteBatches[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 7, 0 } },
    { 1, 1, 0, 0, { 2, 0 } },
    { 2, 1, 0, 0, { 6, 0 } },
    { 3, 1, 0, 0, { 1, 0 } },
    { 4, 1, 0, 0, { 9, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { 9, 16, 0, 0, { 10, 0 } },
    { 25, 6, 0, 0, { 5, 0 } },
    { 31, 3, 0, 0, { 8, 0 } },
    { 34, 8, 0, 0, { 4, 0 } },
    { 42, 1, 0, 0, { 11, 0 } },
    { 43, 19, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 11.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView11SpriteSources[54] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, -56, 2590, { .fields = { 32, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 56, -24, 2409, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 16, -48, 2679, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, -40, 2519, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -40, 2519, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 16, -40, 2519, { .fields = { 16, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 32, -40, 2519, { .fields = { 16, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 48, -40, 2371, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -8, -24, 2345, { .fields = { 8, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 8, -24, 2169, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 24, -24, 2249, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 40, -24, 2772, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 0, -8, 2225, { .fields = { 0, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 16, -8, 2879, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -32, -32, 2423, { .fields = { 120, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -64, -24, 2334, { .fields = { 104, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -80, -16, 2251, { .fields = { 120, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, -8, 2174, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -48, -8, 2174, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, -16, 2251, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, -16, 2127, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 0, -24, 2313, { .fields = { 8, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 0, -8, 2207, { .fields = { 8, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -72, 0, 2119, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, -64, 8, 1938, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -48, 8, 2073, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -32, 0, 2082, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -16, 0, 2979, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 24 } }, -56, 96, 1479, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 24 } }, -128, 96, 1480, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 32 } }, -128, 64, 1619, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 32 } }, -72, 64, 1619, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, -128, 40, 1778, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, -80, 40, 1778, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -112, 24, 1912, { .fields = { 32, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -72, 24, 1874, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -72, 8, 1913, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -120, 16, 2077, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -152, -48, 2005, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 16 } }, -144, -32, 2121, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 16 } }, -136, -16, 2251, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, -128, 0, 2177, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 24 } }, -128, 96, 1530, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 24 } }, -64, 96, 1532, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 56, 32 } }, -128, 64, 1669, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 56, 32 } }, -72, 64, 1669, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 24 } }, -128, 40, 1828, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 24 } }, -80, 40, 1828, { .fields = { 40, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 16 } }, -112, 24, 1962, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, -72, 24, 1924, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -72, 8, 1963, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 24 } }, -128, -40, 2189, { .fields = { 88, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 24 } }, -128, -16, 2333, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -120, 8, 2218, { .fields = { 56, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 11.
///
/// Ranges cover all 54 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView11SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 14, 0, 0, { 0, 0 } },
    { 28, 9, 0, 0, { 5, 0 } },
    { 37, 5, 0, 0, { 1, 0 } },
    { 42, 9, 0, 0, { 4, 0 } },
    { 51, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 12.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView12SpriteSources[40] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -72, -24, 3382, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -88, -16, 2947, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -80, -8, 2941, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 24 } }, -136, -16, 2646, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 48, 1762, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, -8, 2497, { .fields = { 112, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 0, 2580, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 8, 2338, { .fields = { 104, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 16, 2173, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, 24, 1928, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, 32, 1863, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, 40, 1816, { .fields = { 80, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -144, 32, 1747, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, 40, 1811, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, 48, 1687, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -160, 56, 1599, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 64, 1555, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 72, 1660, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 80, 1629, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 80, 1634, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 88, 2412, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 96, 2312, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -160, 104, 2220, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -160, 112, 2135, { .fields = { 80, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -160, -8, 2775, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -160, 0, 2605, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -160, 8, 2363, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -160, 16, 2198, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -160, 24, 1953, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -160, 32, 1888, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -160, 40, 1836, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -160, 48, 1712, { .fields = { 96, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -160, 56, 1624, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 64, 1580, { .fields = { 88, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 72, 1685, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 80, 1654, { .fields = { 88, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 88, 2437, { .fields = { 88, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 96, 2337, { .fields = { 88, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -160, 104, 2245, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 8 } }, -160, 112, 2160, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 12.
///
/// Ranges cover all 40 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView12SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 1, 0, 0, { 0, 0 } },
    { 4, 8, 0, 0, { 5, 0 } },
    { 12, 7, 0, 0, { 1, 0 } },
    { 19, 5, 0, 0, { 4, 0 } },
    { 24, 16, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped balcony view 13.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list. Its address also occupies the unused source slot in the view
/// descriptor and is never read as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView13SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 14.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView14SpriteSources[40] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 88, 3522, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, 88, 2356, { .fields = { 104, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 64, 96, 2694, { .fields = { 72, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 96, 0x32D6, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 88, 0x32E5, { .fields = { 104, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 88, 2476, { .fields = { 104, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, 88, 2489, { .fields = { 104, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, 64, 96, 2421, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 56, 104, 2102, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 24, 96, 0x3969, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 96, 1198, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 96, 1349, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 48, 96, 2450, { .fields = { 96, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, 56, 104, 2035, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 8 } }, 48, 112, 1311, { .fields = { 40, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -48, 112, 625, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -16, 104, 750, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 8, 96, 950, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 16, 96, 1018, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 24, 96, 1107, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, 96, 1129, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 104, 1238, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, 48, 112, 1254, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 104, 16, 2229, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, 112, 16, 1550, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 120, 72, 1458, { .fields = { 112, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 96, 24, 2356, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 104, 24, 2148, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, 112, 48, 1625, { .fields = { 120, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 64, 88, 3647, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, 0, 112, 950, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 16, 104, 1053, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 24, 96, 1132, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 32, 96, 1154, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 40, 96, 1374, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 48, 88, 2501, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 56, 88, 2539, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, 64, 96, 2744, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 8 } }, 56, 104, 2137, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 8 } }, 48, 112, 1336, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 14.
///
/// Ranges cover all 40 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView14SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 6, 0, 0, { 4, 0 } },
    { 9, 6, 0, 0, { 1, 0 } },
    { 15, 8, 0, 0, { 5, 0 } },
    { 23, 3, 0, 0, { 0, 0 } },
    { 26, 3, 0, 0, { 6, 0 } },
    { 29, 11, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped balcony view 15.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list. Its address also occupies the unused source slot in the view
/// descriptor and is never read as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView15SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped balcony view 16.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list. Its address also occupies the unused source slot in the view
/// descriptor and is never read as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView16SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 17.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView17SpriteSources[44] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, -16, 975, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, 40, -40, 975, { .fields = { 72, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 160 } }, 48, -40, 975, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, 64, -104, 950, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 48, -72, 975, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -120, 40, 425, { .fields = { 88, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -112, 40, 425, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 72 } }, -104, 48, 425, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 64 } }, -96, 56, 525, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -88, 56, 525, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 56 } }, -80, 64, 775, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 56 } }, -72, 64, 775, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 48 } }, -64, 72, 775, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, -56, 80, 975, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, -32, 80, 975, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 40 } }, -8, 80, 975, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 16, 80, 975, { .fields = { 72, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 40 } }, 32, 80, 975, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -128, 48, 357, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 56 } }, -80, 64, 774, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 64 } }, -96, 56, 524, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 40 } }, -56, 80, 974, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 160 } }, 80, -40, 980, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 72 } }, 48, 48, 980, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 88 } }, 72, -40, 980, { .fields = { 120, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 8 } }, -136, 112, 337, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 8 } }, -128, 112, 337, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 64, 160 } }, 48, -40, 999, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 48, 40 } }, 0, 80, 974, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 24, 56 } }, -80, 64, 774, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 40 } }, -56, 80, 974, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 64 } }, -96, 56, 524, { .fields = { 104, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 24, 80 } }, -120, 40, 424, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 40, 40 } }, -40, 80, 996, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -136, 96, 313, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -128, 96, 337, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -136, 80, 312, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -128, 80, 336, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -136, 64, 330, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -128, 64, 356, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -136, 32, 378, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -128, 32, 407, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -136, 48, 329, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -128, 48, 355, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 17.
///
/// Ranges cover all 44 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Scene cues hide batch 2 or 3; the zero-length batch 1 retains its slot.
static SpriteBatch _gDryfieldNightMotelBalconyView17SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { 19, 6, 0, 0, { 2, 0 } },
    { 25, 19, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clipping rectangle and full-screen restore boundary for mapped balcony view 17.
///
/// The rectangle uses draw-buffer pixels; the restore boundary is an
/// unscaled OT sorting depth. The second record terminates the list.
/// The clip also affects scene primitives drawn before the restore command.
static SpriteDrawArea _gDryfieldNightMotelBalconyView17SpriteDrawAreas[2] = {
    { { 0, 0, 257, 239 }, 999 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Depth-sorted room sprites for mapped balcony view 18.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
/// Sources 38..72 are the scene-controlled breakaway pieces; the last 29
/// sources enable semitransparency.
static SpriteSource _gDryfieldNightMotelBalconyView18SpriteSources[102] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, -32, 1425, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 160 } }, 48, -56, 1425, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 56 } }, 80, -64, 1334, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 80, 56, 1418, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 64 } }, 96, -8, 1425, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -16, 88, 1425, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -88, 24, 1425, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -88, 48, 1425, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -88, 64, 1425, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -88, 88, 1425, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, -56, 16, 1425, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, -64, 40, 1425, { .fields = { 40, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 56 } }, -56, 56, 1425, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -80, 80, 1425, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -32, 48, 1425, { .fields = { 0, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 80 } }, 16, 24, 1425, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 32, 32, 1425, { .fields = { 16, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 32, 48, 1425, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 32, 64, 1425, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 32, 80, 1897, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 72, 48, 1425, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 16 } }, 72, 32, 1425, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 80, 1425, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -16, 80, 1425, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -40, 64, 1425, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -8, 64, 1557, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 24, 1425, { .fields = { 40, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -16, 24, 1425, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 16 } }, -160, 104, 1088, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, -128, 104, 1088, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, -96, 96, 1149, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, -64, 96, 1149, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, 24, 96, 1128, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, 48, 96, 1128, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, -32, 96, 1149, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, 0, 96, 1128, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 16 } }, 72, 104, 1043, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 112, 112, 797, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 16 } }, -120, 80, 2775, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -16, 64, 2775, { .fields = { 32, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 0, 72, 2775, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 16 } }, 32, 80, 2775, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, -80, 72, 2775, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, -56, 72, 2775, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 24 } }, -32, 72, 2775, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -72, 104, 1171, { .fields = { 16, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 16 } }, -136, 96, 1195, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 32 } }, 16, 72, 1424, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 56, 72, 1424, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 40 } }, -48, 80, 1513, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 40 } }, -16, 80, 1513, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -104, 80, 1171, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 24 } }, -88, 80, 1171, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, -72, 80, 1171, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -160, 80, 1048, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -136, 80, 2775, { .fields = { 8, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -112, 80, 2775, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 24 } }, -88, 80, 2775, { .fields = { 32, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, -56, 80, 2775, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, -24, 80, 2775, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 8, 80, 2775, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, 40, 80, 2775, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 8 } }, 48, 88, 1424, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 16 } }, -128, 88, 1210, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -96, 80, 1163, { .fields = { 8, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, -72, 72, 1405, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 16 } }, -48, 88, 1341, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 24 } }, 32, 80, 1424, { .fields = { 96, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 16 } }, 40, -32, 1694, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 152 } }, 48, -56, 1424, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 16 } }, 56, 96, 1000, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 24 } }, 104, 96, 1000, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 48 } }, 80, -64, 1000, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 8 } }, -16, 32, 1437, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 8 } }, 0, 32, 1437, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 8 } }, -88, 24, 1437, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 24, 8 } }, -88, 48, 1437, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 16, 32 } }, -88, 80, 1437, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, -56, 16, 1437, { .fields = { 88, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 16 } }, -64, 40, 1437, { .fields = { 64, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 32, 8 } }, -88, 64, 1437, { .fields = { 0, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 16, 40 } }, -56, 56, 1437, { .fields = { 80, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 16 } }, -72, 96, 1437, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 16, 8 } }, -72, 80, 1437, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 48, 8 } }, -32, 48, 1437, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 56, 8 } }, -40, 24, 1437, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 16, 80 } }, 16, 24, 1437, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 8 } }, 32, 32, 1437, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 8 } }, 32, 48, 1437, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 8 } }, 32, 64, 1437, { .fields = { 24, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 16 } }, 32, 72, 1437, { .fields = { 56, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, 72, 48, 1437, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, 88, 48, 1437, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, 104, 48, 1437, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, 72, 24, 1437, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, 88, 24, 1437, { .fields = { 96, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 24 } }, 104, 24, 1437, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 8 } }, -40, 80, 1437, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 16, 16 } }, -24, 80, 1437, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 24, 8 } }, -8, 80, 1437, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 32, 8 } }, -40, 64, 1437, { .fields = { 16, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 257), { .fields = { 24, 8 } }, -8, 64, 1437, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

/// Sprite ranges for mapped balcony view 18.
///
/// Ranges cover all 102 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Scene cues show or hide breakaway batches 6..10. Zero-length batches
/// 1 and 3 retain their slots so later batch indices remain stable.
static SpriteBatch _gDryfieldNightMotelBalconyView18SpriteBatches[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 0, 0, 0, { 2, 0 } },
    { 5, 23, 0, 0, { 7, 0 } },
    { 28, 10, 0, 0, { 5, 0 } },
    { 38, 7, 0, 0, { 8, 0 } },
    { 45, 9, 0, 0, { 4, 0 } },
    { 54, 8, 0, 0, { 9, 0 } },
    { 62, 6, 0, 0, { 0, 0 } },
    { 68, 5, 0, 0, { 10, 0 } },
    { 73, 29, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Clipping rectangle and full-screen restore boundary for mapped balcony view 18.
///
/// The rectangle uses draw-buffer pixels; the restore boundary is an
/// unscaled OT sorting depth. The second record terminates the list.
/// The clip also affects scene primitives drawn before the restore command.
static SpriteDrawArea _gDryfieldNightMotelBalconyView18SpriteDrawAreas[2] = {
    { { 0, 0, 272, 239 }, 1425 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Depth-sorted room sprites for mapped balcony view 19.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
static SpriteSource _gDryfieldNightMotelBalconyView19SpriteSources[54] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -80, 112, 750, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 104, 750, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 96, 750, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 88, 750, { .fields = { 104, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -112, 88, 750, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -120, 80, 750, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 80, 750, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -120, 72, 730, { .fields = { 120, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -96, 72, 730, { .fields = { 16, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -72, 64, 750, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -48, 64, 700, { .fields = { 32, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -24, 56, 675, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -72, 80, 750, { .fields = { 32, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -72, 96, 750, { .fields = { 32, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 40 } }, -48, 80, 750, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, 8, 64, 625, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, 48, 72, 450, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 96, 72, 450, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 136, 64, 507, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -72, -96, 1110, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -40, -80, 1114, { .fields = { 104, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -72, -40, 1088, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, -80, 32, 1028, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -120, -120, 896, { .fields = { 0, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -72, -120, 896, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -120, -112, 929, { .fields = { 112, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -72, -112, 929, { .fields = { 112, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -128, -104, 1017, { .fields = { 112, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -80, -104, 1017, { .fields = { 112, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 8 } }, -136, -96, 1081, { .fields = { 8, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -88, -96, 1081, { .fields = { 112, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -136, -88, 1154, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -88, -88, 1154, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -136, -80, 1343, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -96, -80, 1343, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -136, -72, 1366, { .fields = { 120, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -96, -72, 1366, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -136, -64, 1558, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -96, -64, 1558, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, -56, 1694, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -104, -56, 1694, { .fields = { 104, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, -48, 1644, { .fields = { 112, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -104, -48, 1644, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, -32, 2242, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -104, -32, 2242, { .fields = { 112, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -136, -40, 2019, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -104, -40, 2019, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -80, 40, 958, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 88 } }, -72, -40, 1100, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, -160, 32, 293, { .fields = { 24, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 16 } }, -160, 48, 160, { .fields = { 8, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, -160, 64, 109, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -160, 80, 83, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -160, 96, 64, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 19.
///
/// Ranges cover all 54 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Scene cues select visibility of batches 1 and 2.
static SpriteBatch _gDryfieldNightMotelBalconyView19SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 4, 0, 0, { 0, 0 } },
    { 23, 24, 0, 0, { 5, 0 } },
    { 47, 1, 0, 0, { 1, 0 } },
    { 48, 1, 0, 0, { 4, 0 } },
    { 49, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 20.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// Depth is an unscaled OT sorting value. Texture pages use 8-bit pixels;
/// palette rows are in VRAM. The loaded room overlay owns this writable array.
/// The two four-sprite ranges are selected by section visibility state.
static SpriteSource _gDryfieldNightMotelBalconyView20SpriteSources[8] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 40 } }, -80, -64, 1000, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 112, 48 } }, -80, -24, 1000, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 120, 88 } }, -88, 24, 1000, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 80 } }, 32, 24, 1000, { .fields = { 80, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 40 } }, -80, -64, 1025, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 120, 64 } }, -80, -24, 1025, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 112, 72 } }, -88, 40, 1025, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 56, 64 } }, 24, 40, 1025, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 20.
///
/// Ranges cover all 8 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView20SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped balcony view 21.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list. Its address also occupies the unused source slot in the view
/// descriptor and is never read as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView21SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 22.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896 and 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView22SpriteSources[16] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 104, 240 } }, -160, -120, 709, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 56 } }, -40, -88, 1192, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -40, -32, 1150, { .fields = { 8, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 40 } }, -40, 0, 1150, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 32 } }, -40, 40, 1150, { .fields = { 8, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -40, 72, 1150, { .fields = { 0, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 24 } }, -40, 96, 1150, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 224 } }, -56, -104, 1038, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 104, 240 } }, -160, -120, 708, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 224 } }, -56, -104, 1038, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 32 } }, -40, -88, 1149, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 32 } }, -40, -56, 1149, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 40 } }, -40, -24, 1149, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 40 } }, -40, 16, 1149, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 32 } }, -40, 56, 1149, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 32 } }, -40, 88, 1149, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 22.
///
/// Ranges cover all 16 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Scene cues select visibility of the two eight-sprite ranges.
static SpriteBatch _gDryfieldNightMotelBalconyView22SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 23.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896 and 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView23SpriteSources[32] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 48, 72, 3500, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 88, 72, 3500, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, 56, 48, 3500, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 64 } }, -96, -80, 875, { .fields = { 72, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 32 } }, -104, 40, 875, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 88, 48 } }, -120, 72, 875, { .fields = { 16, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 8 } }, -32, 112, 875, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, 112, 875, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 56 } }, -104, -16, 875, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 48 } }, 40, 32, 3475, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, 64, 80, 3475, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 80, 72 } }, -128, -120, 862, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 112, 96 } }, -136, -48, 862, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 112, 64 } }, -144, 48, 862, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 48, 72, 3457, { .fields = { 24, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, 32, 80, 2334, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, 72, 80, 2150, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, 88, 72, 2674, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, 96, 64, 2927, { .fields = { 72, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 24, 80, 1944, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 72, 80, 1853, { .fields = { 8, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, 40, 88, 1698, { .fields = { 8, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 16 } }, 104, 64, 1927, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 24 } }, 120, 56, 3312, { .fields = { 32, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, 96, 80, 2849, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 32 } }, -40, 88, 791, { .fields = { 16, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 24 } }, 24, 88, 952, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 24 } }, 96, 88, 925, { .fields = { 8, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 128, 112, 645, { .fields = { 32, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 128 } }, -24, -56, 1125, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, 0, -8, 1640, { .fields = { 0, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 80 } }, 16, -8, 1758, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 23.
///
/// Ranges cover all 32 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView23SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 6, 0 } },
    { 3, 6, 0, 0, { 1, 0 } },
    { 9, 2, 0, 0, { 4, 0 } },
    { 11, 3, 0, 0, { 0, 0 } },
    { 14, 5, 0, 0, { 5, 0 } },
    { 19, 4, 0, 0, { 3, 0 } },
    { 23, 6, 0, 0, { 7, 0 } },
    { 29, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Retained source sprites for mapped balcony view 24, whose batches select none.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896, Y 0, with palette X 0 and rows 255.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView24SpriteSources[2] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 152, 64 } }, 8, 56, 2250, { .fields = { 104, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 168, 184 } }, -160, -64, 2250, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Empty sprite batch list for mapped balcony view 24.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list.
/// The two retained source sprites are not selected by either record.
static SpriteBatch _gDryfieldNightMotelBalconyView24SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 25.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896 and 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView25SpriteSources[72] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 72, -8, 1595, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 80, -8, 1650, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 112, -8, 1702, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 136, -16, 1729, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 64, 8, 1603, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, -16, 0, 1481, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, -8, 1516, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 32, -8, 1552, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -32, 0, 1396, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 16, 1446, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -24, 16, 1305, { .fields = { 96, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -16, 16, 1187, { .fields = { 32, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -16, 32, 1099, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -8, 32, 1027, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -8, 48, 861, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -8, 64, 873, { .fields = { 80, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 0, 88, 777, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 8, 64, 779, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 24 } }, 16, 88, 646, { .fields = { 32, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, 112, 616, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 72 } }, -96, -64, 1875, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -48, -24, 1875, { .fields = { 96, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, -64, 1875, { .fields = { 56, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 72, 0, 2000, { .fields = { 96, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 64, -8, 2000, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 8 } }, -8, -8, 1964, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -8, 0, 1950, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -8, 8, 1950, { .fields = { 96, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -160, 16, 816, { .fields = { 112, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -152, 0, 872, { .fields = { 120, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -144, 24, 925, { .fields = { 112, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -136, 40, 960, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -136, -24, 1080, { .fields = { 48, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 120 } }, -128, -24, 1075, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, -120, -8, 1202, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, -112, 0, 1354, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, -104, 24, 1330, { .fields = { 56, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 16, 1499, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -104, 32, 1333, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -104, 40, 1248, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -112, 48, 1179, { .fields = { 40, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 112, 8 } }, -112, 56, 1104, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -112, 64, 1052, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 120, 8 } }, -112, 72, 1000, { .fields = { 40, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, -120, 80, 953, { .fields = { 32, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 128, 8 } }, -120, 88, 911, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -128, 96, 873, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -128, 104, 838, { .fields = { 8, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 144, 8 } }, -128, 112, 806, { .fields = { 8, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 24 } }, -32, 0, 1531, { .fields = { 8, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 32, 0, 1596, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 24 } }, 48, -8, 1666, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 112, -8, 1743, { .fields = { 16, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, 128, -16, 1754, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -32, 0, 1479, { .fields = { 56, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -24, 0, 1400, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -24, 16, 1312, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -24, 24, 1325, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -16, 24, 1200, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -16, 40, 1101, { .fields = { 56, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -8, 40, 1050, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -8, 56, 1025, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, 0, 56, 939, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, 0, 72, 875, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 0, 88, 867, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 8, 88, 787, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 8, 96, 783, { .fields = { 56, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 16 } }, 16, 96, 655, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, 16, 112, 624, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 48 } }, -88, -40, 1887, { .fields = { 64, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -88, 8, 1887, { .fields = { 0, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -96, -8, 1887, { .fields = { 24, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 25.
///
/// Ranges cover all 72 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView25SpriteBatches[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 6, 0 } },
    { 4, 4, 0, 0, { 1, 0 } },
    { 8, 12, 0, 0, { 8, 0 } },
    { 20, 3, 0, 0, { 0, 0 } },
    { 23, 2, 0, 0, { 5, 0 } },
    { 25, 3, 0, 0, { 2, 0 } },
    { 28, 21, 0, 0, { 9, 0 } },
    { 49, 5, 0, 0, { 4, 0 } },
    { 54, 15, 0, 0, { 7, 0 } },
    { 69, 3, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 26.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896 and 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView26SpriteSources[93] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -160, -120, 392, { .fields = { 24, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -160, -104, 433, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, -160, -88, 644, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, -72, -8, 4018, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 16 } }, 8, 16, 3085, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 16 } }, -24, 16, 2956, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -80, 24, 2880, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, 16, 2927, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -32, 24, 2987, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -88, 32, 2535, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, -32, 24, 2625, { .fields = { 80, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, -32, 32, 2087, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -48, 32, 1988, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -88, 40, 2087, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, 40, 2075, { .fields = { 8, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -88, 48, 1700, { .fields = { 96, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -24, 40, 2075, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -96, 48, 1703, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -16, 48, 1725, { .fields = { 120, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, -96, 56, 1498, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, -8, 56, 1513, { .fields = { 120, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 0, 64, 1387, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, 72, 1053, { .fields = { 112, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 8, 80, 980, { .fields = { 104, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 16, 88, 902, { .fields = { 104, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -128, 96, 795, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -48, 96, 787, { .fields = { 88, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 24, 96, 795, { .fields = { 96, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -136, 104, 750, { .fields = { 72, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -56, 104, 750, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 24, 104, 750, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, 32, 112, 715, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -120, 88, 900, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -48, 88, 902, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 72, 8 } }, -120, 80, 987, { .fields = { 80, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -48, 80, 962, { .fields = { 96, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -112, 72, 1050, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -48, 72, 1052, { .fields = { 96, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -104, 64, 1350, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -48, 64, 1368, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -144, 112, 715, { .fields = { 96, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -88, 112, 715, { .fields = { 96, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -32, 112, 715, { .fields = { 72, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -120, -72, 800, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 160 } }, -152, -80, 722, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 160 } }, -136, -80, 747, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 160 } }, -128, -80, 772, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 104 } }, -120, -40, 1538, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -96, -40, 1685, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 240 } }, -160, -120, 725, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 240 } }, -144, -120, 750, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 240 } }, -136, -120, 796, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 240 } }, -128, -120, 800, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 232 } }, -120, -112, 822, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, -72, -8, 4022, { .fields = { 64, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -56, 0, 4073, { .fields = { 48, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -32, 32, 2545, { .fields = { 56, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, -24, 40, 2081, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -40, 16, 2928, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -32, 24, 2997, { .fields = { 80, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -80, 24, 2882, { .fields = { 48, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -88, 32, 2535, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -88, 40, 2093, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -96, 48, 1705, { .fields = { 16, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 8 } }, -16, 48, 1729, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, -8, 56, 1514, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 0, 64, 1392, { .fields = { 56, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 8 } }, 0, 72, 1055, { .fields = { 56, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, 8, 80, 980, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 24, 88, 910, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 32, 96, 795, { .fields = { 24, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, 32, 104, 754, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, 32, 112, 721, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, -120, 88, 900, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, -48, 88, 903, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -128, 96, 796, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, -48, 96, 793, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, -120, 80, 988, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -48, 80, 975, { .fields = { 24, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -112, 72, 1052, { .fields = { 16, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -48, 72, 1062, { .fields = { 24, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -104, 64, 1354, { .fields = { 16, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -48, 64, 1369, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -104, 56, 1500, { .fields = { 16, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, -48, 56, 1500, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -144, 112, 715, { .fields = { 32, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -80, 112, 718, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -16, 112, 719, { .fields = { 64, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -136, 104, 767, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, -72, 104, 761, { .fields = { 64, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, -16, 104, 760, { .fields = { 72, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -24, 32, 2525, { .fields = { 104, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 8 } }, -16, 40, 2091, { .fields = { 120, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 26.
///
/// Ranges cover all 93 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView26SpriteBatches[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 7, 0 } },
    { 3, 1, 0, 0, { 2, 0 } },
    { 4, 1, 0, 0, { 6, 0 } },
    { 5, 1, 0, 0, { 1, 0 } },
    { 6, 4, 0, 0, { 9, 0 } },
    { 10, 6, 0, 0, { 0, 0 } },
    { 16, 27, 0, 0, { 10, 0 } },
    { 43, 4, 0, 0, { 5, 0 } },
    { 47, 2, 0, 0, { 8, 0 } },
    { 49, 5, 0, 0, { 4, 0 } },
    { 54, 2, 0, 0, { 11, 0 } },
    { 56, 37, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped balcony view 27.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list.
/// Its address also occupies the unused source slot in the view descriptor;
/// that slot is never dereferenced as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView27SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Empty sprite batch list for mapped balcony view 28.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list.
/// Its address also occupies the unused source slot in the view descriptor;
/// that slot is never dereferenced as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView28SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 29.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView29SpriteSources[43] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 72, 104, 1267, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 16 } }, -32, 88, 1426, { .fields = { 8, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 16, 80, 1537, { .fields = { 40, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 40, 72, 1422, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, -32, 1694, { .fields = { 48, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, 48, -40, 1572, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, 56, -40, 1495, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 144 } }, 64, -40, 1507, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, 72, 24, 1350, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 80, 56, 1302, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 88, 88, 1281, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 72, -48, 1415, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 80, -48, 1336, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 88, -56, 1279, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 96, -56, 1225, { .fields = { 64, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 104, -56, 1199, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 64, -40, 1463, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 80, -48, 1416, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 88 } }, 88, -88, 1400, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 104 } }, 96, -104, 1500, { .fields = { 104, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 120, -24, 1694, { .fields = { 80, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, 104, 0, 1705, { .fields = { 80, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 48 } }, 64, 56, 1522, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 56, 48, 1425, { .fields = { 16, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, 56, 64, 1424, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, 56, 80, 1396, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 80, 88, 1276, { .fields = { 40, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, 40, 0x61A7, { .fields = { 32, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, 56, 1551, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, 72, 1516, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, 32, 1728, { .fields = { 40, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, 48, 1713, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, 64, 1520, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, 80, 1487, { .fields = { 56, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 152, -112, 1000, { .fields = { 64, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 128, -96, 1125, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 40, -32, 1625, { .fields = { 64, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 136, -104, 1050, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 112, -88, 1175, { .fields = { 48, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 80, -64, 1318, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, 96, -80, 1250, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 64, -56, 1300, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 48, -48, 1460, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 29.
///
/// Ranges cover all 43 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
/// Zero-length intermediate ranges retain their batch indices.
static SpriteBatch _gDryfieldNightMotelBalconyView29SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 7, 0, 0, { 4, 0 } },
    { 23, 0, 0, 0, { 1, 0 } },
    { 23, 0, 0, 0, { 5, 0 } },
    { 23, 11, 0, 0, { 0, 0 } },
    { 34, 0, 0, 0, { 6, 0 } },
    { 34, 9, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 30.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView30SpriteSources[52] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 48 } }, 56, -32, 3251, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 104, 0, 2578, { .fields = { 16, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 120, 0, 2509, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 136, 0, 2444, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 152, 0, 2398, { .fields = { 24, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, 88, 0, 2515, { .fields = { 24, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 32, -8, 2142, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 0, 2393, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 48, -16, 2704, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 56, -8, 2897, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 40, 16, 2382, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 56, 16, 2382, { .fields = { 0, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, 16, 2329, { .fields = { 8, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 56, 8, 3166, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 72, 8, 2882, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 120 } }, -160, -112, 764, { .fields = { 104, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 96 } }, -160, 8, 764, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 88 } }, -144, 8, 774, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -128, 8, 757, { .fields = { 56, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -120, 8, 755, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -112, 8, 755, { .fields = { 72, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -104, 8, 757, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, 8, 787, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -144, 96, 762, { .fields = { 32, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 104, 771, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 96 } }, -144, -80, 791, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, -120, -72, 751, { .fields = { 48, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 48 } }, 56, -32, 3276, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 120 } }, -160, -112, 789, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 96 } }, -160, 8, 789, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, 104, 796, { .fields = { 16, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 88 } }, -144, 8, 799, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 80 } }, -128, 8, 782, { .fields = { 64, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 64 } }, -120, 8, 780, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 40 } }, -112, 8, 780, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -104, 8, 782, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -96, 8, 787, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -144, 96, 787, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -128, 88, 799, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -128, 104, 796, { .fields = { 0, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -104, 88, 796, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -104, 104, 796, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -80, 88, 799, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -80, 104, 796, { .fields = { 0, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -56, 96, 788, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -48, 96, 788, { .fields = { 40, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -40, 104, 784, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -32, 112, 492, { .fields = { 0, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -120, 80, 776, { .fields = { 0, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -104, 80, 776, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -88, 80, 776, { .fields = { 0, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -72, 80, 776, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 30.
///
/// Ranges cover all 52 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView30SpriteBatches[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 6, 0 } },
    { 1, 4, 0, 0, { 1, 0 } },
    { 5, 1, 0, 0, { 4, 0 } },
    { 6, 9, 0, 0, { 0, 0 } },
    { 15, 10, 0, 0, { 5, 0 } },
    { 25, 2, 0, 0, { 3, 0 } },
    { 27, 1, 0, 0, { 7, 0 } },
    { 28, 24, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 31.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896 and 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView31SpriteSources[54] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -104, -40, 1537, { .fields = { 120, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 64, 48 } }, -104, -32, 1594, { .fields = { 16, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -96, 16, 1599, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 24 } }, -64, 16, 1632, { .fields = { 120, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -40, 8, 1667, { .fields = { 40, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -24, 48, 1243, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -16, 48, 1047, { .fields = { 16, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 104, 831, { .fields = { 48, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, 112, 682, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -152, 112, 684, { .fields = { 40, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -144, 112, 687, { .fields = { 24, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -136, 112, 681, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -80, 112, 695, { .fields = { 0, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 112, 695, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 8 } }, -64, 112, 695, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 8, 112, 596, { .fields = { 0, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 16, 112, 592, { .fields = { 0, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 24, 112, 604, { .fields = { 0, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 32, 112, 1841, { .fields = { 0, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 64 } }, -136, 24, 1398, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -32, 40, 1262, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -16, 40, 1099, { .fields = { 120, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, -8, 48, 1126, { .fields = { 120, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -160, -16, 1218, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 96 } }, -152, -8, 1273, { .fields = { 120, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -144, 8, 1335, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 24 } }, -160, 88, 1268, { .fields = { 112, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 72 } }, -128, 48, 1372, { .fields = { 72, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -80, 56, 1392, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 72 } }, -56, 48, 1378, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -24, 56, 1323, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 48 } }, 8, 64, 998, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 16, 80, 795, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 24, 96, 817, { .fields = { 8, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, -8, 32, 1128, { .fields = { 16, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 16, 32, 1151, { .fields = { 32, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 40, 24, 1167, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 64, 24, 1176, { .fields = { 16, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 88, 40, 1271, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 96, 24, 1249, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, 136, 24, 1287, { .fields = { 8, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 24 } }, 16, 32, 1193, { .fields = { 96, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 120, 32, 1286, { .fields = { 80, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, 128, 32, 1309, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 152, 24, 1337, { .fields = { 8, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 72, 24, 1231, { .fields = { 56, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 32 } }, 88, 24, 1259, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, -8, 32, 1167, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -24, 32, 1192, { .fields = { 0, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -104, -40, 1562, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 64, 48 } }, -104, -32, 1619, { .fields = { 16, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 32 } }, -96, 16, 1624, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, -64, 16, 1657, { .fields = { 120, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -40, 8, 1692, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 31.
///
/// Ranges cover all 54 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
/// Zero-length intermediate ranges retain their batch indices.
static SpriteBatch _gDryfieldNightMotelBalconyView31SpriteBatches[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 29, 0, 0, { 4, 0 } },
    { 34, 5, 0, 0, { 1, 0 } },
    { 39, 0, 0, 0, { 5, 0 } },
    { 39, 2, 0, 0, { 0, 0 } },
    { 41, 8, 0, 0, { 6, 0 } },
    { 49, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 32.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 960, Y 0, with palette X 0 and rows 255, 256.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView32SpriteSources[25] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 144, 24, 1832, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 104, 16, 1865, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 24 } }, 64, 16, 1896, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 16, 1918, { .fields = { 80, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 16, 1875, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 40, 16, 1867, { .fields = { 88, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 56 } }, -8, -24, 3250, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 32, 8, 3000, { .fields = { 80, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 56 } }, -8, -24, 3000, { .fields = { 88, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 32, 8, 3000, { .fields = { 112, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 24 } }, 120, 16, 1831, { .fields = { 88, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 48, 16, 1850, { .fields = { 112, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 56, 16, 1883, { .fields = { 80, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, 64, 16, 1892, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 24 } }, 80, 16, 1864, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 40, 16, 1850, { .fields = { 120, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 48, 16, 1800, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 56, 24, 1875, { .fields = { 104, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 48, 32, 1875, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 40, 32, 1862, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 8 } }, 8, 48, 2250, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 24 } }, 8, 24, 2500, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -40, -8, 2500, { .fields = { 112, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 56 } }, -32, -8, 2500, { .fields = { 120, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 48 } }, -24, 8, 2875, { .fields = { 96, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 32.
///
/// Ranges cover all 25 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView32SpriteBatches[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 2, 0, 0, { 0, 0 } },
    { 8, 2, 0, 0, { 5, 0 } },
    { 10, 1, 0, 0, { 1, 0 } },
    { 11, 4, 0, 0, { 4, 0 } },
    { 15, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 33.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 896 and 960, Y 0, with palette X 0 and rows 255.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView33SpriteSources[19] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 144 } }, -160, -120, 1250, { .fields = { 56, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 136 } }, -88, -112, 1500, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 120 } }, -80, -104, 1750, { .fields = { 40, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 104 } }, -72, -96, 2077, { .fields = { 120, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 80 } }, -64, -88, 2408, { .fields = { 88, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -24, -88, 2400, { .fields = { 104, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, -40, 2400, { .fields = { 112, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 120 } }, -16, -120, 2087, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 136 } }, -8, -120, 1510, { .fields = { 48, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 160, 136 } }, 0, -120, 1400, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 104, 72 } }, -104, -32, 1000, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 88 } }, 0, -32, 1000, { .fields = { 96, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -24, -32, 2469, { .fields = { 96, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, -64, -24, 2519, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, -72, -16, 2283, { .fields = { 104, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, -80, -8, 1978, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 88, 8 } }, -88, 0, 1753, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 96, 8 } }, -88, 8, 1579, { .fields = { 72, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 104, 8 } }, -96, 16, 1399, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 33.
///
/// Ranges cover all 19 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
static SpriteBatch _gDryfieldNightMotelBalconyView33SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 2, 0, 0, { 2, 0 } },
    { 12, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-limited clipping for mapped balcony view 33.
///
/// The 319 by 107 pixel rectangle begins at the draw buffer's upper-left
/// corner; the renderer adds its VRAM Y origin. Full-screen drawing resumes
/// at unscaled depth 2412. The second record terminates the list. The clip
/// also affects scene primitives drawn before the restore command.
static SpriteDrawArea _gDryfieldNightMotelBalconyView33SpriteDrawAreas[2] = {
    { { 0, 0, 319, 107 }, 2412 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Empty sprite batch list for mapped balcony view 34.
///
/// The zero-count opener keeps the decoded background; the next record ends
/// the list.
/// Its address also occupies the unused source slot in the view descriptor;
/// that slot is never dereferenced as source sprites.
static SpriteBatch _gDryfieldNightMotelBalconyView34SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-sorted room sprites for mapped balcony view 35.
///
/// Positions are pixels relative to the view centre; sizes are pixels and
/// depths are unscaled sorting values. RGB modulation is neutral. Textures are
/// 8-bit on VRAM page X 960, Y 0, with palette X 0 and rows 255, 256, 257.
/// The loaded room overlay owns the writable sources for packet allocation
/// and drawing; borrowed pointers must not outlive it.
static SpriteSource _gDryfieldNightMotelBalconyView35SpriteSources[77] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -32, 32, 1822, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -24, 32, 1544, { .fields = { 40, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -16, 32, 1317, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, -8, 32, 1139, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, 0, 48, 987, { .fields = { 80, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 8, 56, 925, { .fields = { 64, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 16, 56, 811, { .fields = { 48, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 24, 64, 800, { .fields = { 40, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 32, 64, 500, { .fields = { 104, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, 40, 64, 500, { .fields = { 40, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 56, 500, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 56, 500, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 40 } }, -160, 80, 351, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 48 } }, -104, 72, 370, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 48 } }, -32, 72, 396, { .fields = { 56, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 72, 56 } }, 24, 64, 402, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, 96, 72, 425, { .fields = { 112, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 48, 1504, { .fields = { 56, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 56, 1255, { .fields = { 24, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -64, 56, 1255, { .fields = { 32, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -72, 64, 1068, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, 72, 875, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 0, 72, 0x9E59, { .fields = { 24, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, 16, 72, 875, { .fields = { 24, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -32, 64, 1043, { .fields = { 16, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 8 } }, -8, 64, 1043, { .fields = { 8, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 56, 1230, { .fields = { 16, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -16, 56, 1230, { .fields = { 16, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -48, 48, 1504, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -24, 48, 1504, { .fields = { 0, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -64, 40, 1944, { .fields = { 0, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -40, 40, 1944, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -64, 48, 1504, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -40, 72, 900, { .fields = { 8, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -56, 72, 900, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -72, 72, 900, { .fields = { 24, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -64, 64, 1068, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -48, 64, 1068, { .fields = { 16, 8 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -56, 56, 1255, { .fields = { 24, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -24, 32, 1517, { .fields = { 40, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -16, 32, 1269, { .fields = { 96, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, -8, 32, 1091, { .fields = { 48, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 0, 40, 955, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 8, 40, 852, { .fields = { 96, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 16, 40, 768, { .fields = { 48, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 24, 48, 699, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 32, 48, 683, { .fields = { 80, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 40, 48, 637, { .fields = { 88, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 48, 56, 592, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 56, 56, 553, { .fields = { 40, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 64, 56, 524, { .fields = { 40, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, 72, 64, 483, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -88, 48, 1454, { .fields = { 0, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -64, 48, 1454, { .fields = { 0, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -40, 48, 1454, { .fields = { 0, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -104, 56, 1194, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -72, 56, 1180, { .fields = { 8, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -40, 56, 1180, { .fields = { 8, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 8 } }, -112, 64, 978, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -72, 64, 993, { .fields = { 8, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -40, 64, 993, { .fields = { 16, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -16, 64, 993, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -40, 72, 850, { .fields = { 88, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -16, 72, 850, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -72, 72, 900, { .fields = { 24, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -56, 72, 900, { .fields = { 32, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 8 } }, -112, 72, 900, { .fields = { 16, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -88, 72, 900, { .fields = { 24, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 72 } }, -104, -8, 1460, { .fields = { 112, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, -88, 48, 1403, { .fields = { 32, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 16 } }, -80, 24, 3516, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 32 } }, -80, 0, 4433, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -40, 24, 4000, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, -8, 24, 4146, { .fields = { 8, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 16, 72 } }, -104, -8, 1485, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 8, 16 } }, -88, 48, 1428, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 257), { .fields = { 32, 32 } }, -80, 0, 4458, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Sprite ranges for mapped balcony view 35.
///
/// Ranges cover all 77 source elements. The zero-count opener keeps
/// the decoded background; the terminal record ends the list.
/// Section-state streams address the writable hidden bytes by batch index.
static SpriteBatch _gDryfieldNightMotelBalconyView35SpriteBatches[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 5, 0, 0, { 6, 0 } },
    { 17, 22, 0, 0, { 2, 0 } },
    { 39, 29, 0, 0, { 7, 0 } },
    { 68, 2, 0, 0, { 5, 0 } },
    { 70, 1, 0, 0, { 8, 0 } },
    { 71, 1, 0, 0, { 4, 0 } },
    { 72, 1, 0, 0, { 9, 0 } },
    { 73, 1, 0, 0, { 0, 0 } },
    { 74, 2, 0, 0, { 10, 0 } },
    { 76, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-limited scene clipping for balcony mapped view 35.
///
/// The (73,0,233,239) rectangle uses draw-buffer pixels. Depth 1700 restores
/// full-screen drawing; the second record ends the list with SPRITE_DRAW_AREA_END.
/// The sprite-view descriptor borrows this writable room-owned list.
static SpriteDrawArea _gDryfieldNightMotelBalconyView35SpriteDrawAreas[2] = {
    { { 73, 0, 233, 239 }, 1700 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Writable scenery sprite sources for balcony mapped view 36.
///
/// The 36 elements have no terminator; this view's batch ranges cover all of
/// them. Positions and dimensions use screen pixels relative to the drawing
/// origin, UVs use page-relative texels, and depth is an unscaled sorting value.
/// Cached packet setup and drawing borrow this room-owned array.
static SpriteSource _gDryfieldNightMotelBalconyView36SpriteSources[36] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, -160, 80, 533, { .fields = { 72, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, -112, 80, 546, { .fields = { 32, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 48 } }, -64, 72, 550, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, -40, 64, 573, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 56 } }, -8, 64, 561, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, 16, 72, 550, { .fields = { 72, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 56 } }, 48, 64, 529, { .fields = { 80, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 48 } }, 96, 72, 451, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 40 } }, 152, 80, 417, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -160, -40, 2012, { .fields = { 64, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, -88, 112, 914, { .fields = { 80, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 8 } }, -160, -32, 1818, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -160, 8, 1031, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -136, -24, 1524, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -136, 8, 1079, { .fields = { 16, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, -8, 1123, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -104, 8, 1079, { .fields = { 32, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -128, 24, 910, { .fields = { 24, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -128, 40, 912, { .fields = { 24, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -120, 56, 918, { .fields = { 24, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 16 } }, -160, 40, 787, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -120, 72, 912, { .fields = { 16, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 16 } }, -120, 88, 907, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, -112, 104, 904, { .fields = { 48, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -152, -24, 1637, { .fields = { 56, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 32 } }, -160, -24, 1600, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, -144, -32, 1512, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -128, -32, 1413, { .fields = { 72, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -160, -8, 1390, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -160, 0, 1237, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, -40, 1707, { .fields = { 48, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, -24, 1567, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -160, -40, 2100, { .fields = { 88, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 16 } }, -144, -40, 2196, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, -120, -48, 2351, { .fields = { 48, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 48, 16 } }, -88, -48, 2709, { .fields = { 16, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Writable drawing batches for balcony mapped view 36.
///
/// A zero-count first record requests decoded background strips. Source-element
/// ranges are 0+9, 9+17, 26+6, 32+3 and 35+1; the final record has
/// firstSprite=SPRITE_BATCH_END and contributes no sprites. Visibility updates
/// and cached drawing borrow this list while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelBalconyView36SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 17, 0, 0, { 3, 0 } },
    { 26, 6, 0, 0, { 2, 0 } },
    { 32, 3, 0, 0, { 4, 0 } },
    { 35, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Writable scenery sprite sources for balcony mapped view 37.
///
/// The 39 elements have no terminator; this view's batch ranges cover all of
/// them. Positions and dimensions use screen pixels relative to the drawing
/// origin, UVs use page-relative texels, and depth is an unscaled sorting value.
/// Cached packet setup and drawing borrow this room-owned array.
static SpriteSource _gDryfieldNightMotelBalconyView37SpriteSources[39] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -160, 32, 1921, { .fields = { 88, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 56, 1894, { .fields = { 88, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 56, 2059, { .fields = { 88, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 72, 1872, { .fields = { 80, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 72, 2052, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 88, 1875, { .fields = { 72, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -144, 88, 2018, { .fields = { 72, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -160, 104, 1887, { .fields = { 72, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -144, 104, 1980, { .fields = { 72, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 64, 2101, { .fields = { 80, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -112, 64, 2100, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 80, 2046, { .fields = { 72, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -128, 96, 1992, { .fields = { 80, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 24 } }, -112, 80, 2027, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -96, 72, 2052, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 32 } }, -96, 88, 2013, { .fields = { 96, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 24 } }, -120, 96, 2005, { .fields = { 104, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, -136, 104, 1992, { .fields = { 72, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 8 } }, -152, 112, 1980, { .fields = { 72, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, -96, -112, 2989, { .fields = { 112, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -80, -120, 3008, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -64, -120, 2980, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -48, -120, 2975, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -32, -120, 3025, { .fields = { 104, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, -160, 32, 1946, { .fields = { 88, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, 56, 1919, { .fields = { 96, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, 72, 1897, { .fields = { 96, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, 88, 1900, { .fields = { 96, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -160, 104, 1912, { .fields = { 88, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -144, 56, 2084, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -144, 72, 2077, { .fields = { 80, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -144, 88, 2043, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -128, 64, 2126, { .fields = { 88, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -128, 80, 2071, { .fields = { 88, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -128, 96, 2017, { .fields = { 72, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, -144, 104, 2005, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 16 } }, -112, 64, 2125, { .fields = { 80, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, -96, 72, 2077, { .fields = { 104, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 24 } }, -112, 80, 2052, { .fields = { 96, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Writable drawing batches for balcony mapped view 37.
///
/// A zero-count first record requests decoded background strips. Source-element
/// ranges are 0+15, 15+4, 19+5 and 24+15; the final record has
/// firstSprite=SPRITE_BATCH_END and contributes no sprites. Visibility updates
/// and cached drawing borrow this list while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelBalconyView37SpriteBatches[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 4, 0, 0, { 0, 0 } },
    { 19, 5, 0, 0, { 2, 0 } },
    { 24, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Depth-limited scene clipping for balcony mapped view 37.
///
/// The (0,0,2,1) rectangle uses draw-buffer pixels. Depth 3500 restores
/// full-screen drawing; the second record ends the list with SPRITE_DRAW_AREA_END.
/// The sprite-view descriptor borrows this writable room-owned list.
static SpriteDrawArea _gDryfieldNightMotelBalconyView37SpriteDrawAreas[2] = {
    { { 0, 0, 2, 1 }, 3500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

/// Writable scenery sprite sources for balcony mapped view 38.
///
/// The 88 elements have no terminator; this view's batch ranges cover all of
/// them. Positions and dimensions use screen pixels relative to the drawing
/// origin, UVs use page-relative texels, and depth is an unscaled sorting value.
/// Cached packet setup and drawing borrow this room-owned array.
static SpriteSource _gDryfieldNightMotelBalconyView38SpriteSources[88] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 8 } }, 96, -64, 781, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 64 } }, -160, 56, 386, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -144, 72, 402, { .fields = { 80, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -128, 80, 423, { .fields = { 8, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -112, 72, 453, { .fields = { 56, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -96, 72, 486, { .fields = { 56, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 48 } }, -80, 72, 523, { .fields = { 64, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -64, 80, 565, { .fields = { 48, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, -48, 80, 617, { .fields = { 40, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -32, 72, 696, { .fields = { 120, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 32 } }, -16, 72, 768, { .fields = { 120, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 40 } }, 0, 64, 886, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 32 } }, 16, 64, 1024, { .fields = { 0, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 24 } }, 32, 72, 1184, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 48, 72, 1350, { .fields = { 0, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 56, 72, 1500, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 64, 72, 1764, { .fields = { 56, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, 72, 72, 1929, { .fields = { 0, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 80, 72, 2326, { .fields = { 112, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, 88, 72, 3121, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 16 } }, 96, 72, 3175, { .fields = { 88, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -32, 104, 607, { .fields = { 64, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -32, 112, 438, { .fields = { 64, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 96, 827, { .fields = { 48, 24 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 104, 606, { .fields = { 48, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 16, 112, 477, { .fields = { 48, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 80, 88, 1297, { .fields = { 56, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 80, 96, 827, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 80, 104, 575, { .fields = { 40, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 80, 8 } }, 80, 112, 415, { .fields = { 40, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, -112, 602, { .fields = { 104, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, -96, 651, { .fields = { 112, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 88, -80, 717, { .fields = { 72, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 64 } }, 104, -120, 370, { .fields = { 72, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, 112, -56, 445, { .fields = { 8, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 40 } }, 120, -16, 486, { .fields = { 8, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 24 } }, 112, 24, 895, { .fields = { 80, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 40 } }, 128, 48, 748, { .fields = { 32, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 8 } }, 136, 88, 572, { .fields = { 88, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 16, 8 } }, 144, 96, 486, { .fields = { 96, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 136, -64, 361, { .fields = { 56, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, -160, 56, 356, { .fields = { 112, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, -144, 56, 379, { .fields = { 112, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, -128, 56, 405, { .fields = { 112, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, -112, 56, 434, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, -96, 56, 468, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, -80, 56, 504, { .fields = { 96, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 48 } }, -64, 64, 552, { .fields = { 64, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 48 } }, -48, 64, 606, { .fields = { 64, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, -32, 64, 674, { .fields = { 24, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, -16, 64, 750, { .fields = { 24, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, 0, 64, 851, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 32 } }, 16, 64, 989, { .fields = { 120, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 32, 80, 985, { .fields = { 64, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 8 } }, 40, 88, 1039, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 16, 24 } }, 112, -120, 517, { .fields = { 112, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 40 } }, 120, -96, 465, { .fields = { 16, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 40 } }, 128, -56, 384, { .fields = { 8, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 48 } }, 136, -16, 379, { .fields = { 40, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 64 } }, 144, 32, 472, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -64, 112, 399, { .fields = { 48, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, 0, 112, 399, { .fields = { 32, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 80, 8 } }, 80, 112, 385, { .fields = { 32, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, -32, 104, 527, { .fields = { 48, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 72, 8 } }, 32, 104, 541, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 104, 104, 527, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 16, 96, 792, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 64, 96, 797, { .fields = { 64, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 48, 8 } }, 112, 96, 797, { .fields = { 72, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 80, 88, 1146, { .fields = { 32, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 16 } }, 56, 72, 1518, { .fields = { 120, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 24 } }, 64, 72, 1701, { .fields = { 120, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 72, 72, 1867, { .fields = { 0, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 88, 88, 1146, { .fields = { 64, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 56, 8 } }, 88, 80, 1155, { .fields = { 64, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 80, 72, 2314, { .fields = { 0, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 24, 64, 1139, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 32, 64, 1116, { .fields = { 0, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 32 } }, 40, 64, 1155, { .fields = { 72, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 24 } }, 48, 72, 1312, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 24 } }, 56, 72, 1487, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 16 } }, 64, 80, 1664, { .fields = { 112, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 8 } }, 72, 88, 1284, { .fields = { 80, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 64, 8 } }, 40, 96, 990, { .fields = { 56, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 16 } }, 80, 72, 2373, { .fields = { 112, 16 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 16 } }, 88, 72, 3108, { .fields = { 112, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 96, 72, 2987, { .fields = { 8, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 8 } }, 104, 80, 2250, { .fields = { 16, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Writable drawing batches for balcony mapped view 38.
///
/// A zero-count first record requests decoded background strips. Source-element
/// ranges are 0+40, 40+29, 69+7, 76+8 and 84+4; the final record has
/// firstSprite=SPRITE_BATCH_END and contributes no sprites. Visibility updates
/// and cached drawing borrow this list while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelBalconyView38SpriteBatches[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 0, 0 } },
    { 40, 29, 0, 0, { 3, 0 } },
    { 69, 7, 0, 0, { 2, 0 } },
    { 76, 8, 0, 0, { 4, 0 } },
    { 84, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// Writable scenery sprite sources for balcony mapped view 39.
///
/// The 75 elements have no terminator; this view's batch ranges cover all of
/// them. Positions and dimensions use screen pixels relative to the drawing
/// origin, UVs use page-relative texels, and depth is an unscaled sorting value.
/// Cached packet setup and drawing borrow this room-owned array.
static SpriteSource _gDryfieldNightMotelBalconyView39SpriteSources[75] = {
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 56, 56 } }, -160, -120, 358, { .fields = { 40, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 8, 16 } }, -104, -120, 412, { .fields = { 120, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 40 } }, -160, -64, 339, { .fields = { 48, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, -160, -24, 325, { .fields = { 24, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 8, 24 } }, -80, 16, 446, { .fields = { 32, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 32, -24, 1275, { .fields = { 32, 72 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 104, -16, 1457, { .fields = { 24, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 32, -16, 1273, { .fields = { 32, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 32 } }, 40, -8, 1251, { .fields = { 56, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 56 } }, 40, 24, 1171, { .fields = { 96, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 40, 32 } }, 40, 80, 1101, { .fields = { 16, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -40, -120, 837, { .fields = { 80, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, -16, -120, 837, { .fields = { 88, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 8, -120, 837, { .fields = { 88, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 32, -120, 837, { .fields = { 88, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 56, -120, 837, { .fields = { 88, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 24, 16 } }, 80, -120, 837, { .fields = { 80, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 16, 16 } }, 104, -120, 837, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 24, 16 } }, 120, -120, 837, { .fields = { 24, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -24, -104, 846, { .fields = { 40, 192 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 64, 8 } }, 24, -104, 846, { .fields = { 40, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 88, -104, 846, { .fields = { 48, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, -96, 935, { .fields = { 56, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 32, -96, 935, { .fields = { 24, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 88, -96, 935, { .fields = { 24, 48 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -16, -88, 980, { .fields = { 40, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 32, -88, 980, { .fields = { 32, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 88, -88, 980, { .fields = { 8, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, -8, -80, 1028, { .fields = { 16, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 40, -80, 1028, { .fields = { 16, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 88, -80, 1028, { .fields = { 8, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 0, -72, 1039, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 40, -72, 1039, { .fields = { 32, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 96, -72, 1039, { .fields = { 40, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, -64, 1147, { .fields = { 56, 160 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 56, -64, 1147, { .fields = { 40, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 104, -64, 1147, { .fields = { 48, 40 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 8, -56, 1082, { .fields = { 40, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 56, -56, 1082, { .fields = { 64, 144 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 104, -56, 1082, { .fields = { 112, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 48, 8 } }, 16, -48, 1161, { .fields = { 56, 112 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 64, -48, 1161, { .fields = { 64, 88 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 104, -48, 1161, { .fields = { 48, 136 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 24, -40, 1241, { .fields = { 64, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 64, -40, 1241, { .fields = { 64, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 104, -40, 1241, { .fields = { 48, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 32, -32, 1454, { .fields = { 48, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 40, 8 } }, 72, -32, 1454, { .fields = { 48, 240 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 32, 8 } }, 112, -32, 1454, { .fields = { 56, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 255), { .fields = { 56, 8 } }, 88, -24, 1452, { .fields = { 32, 208 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 64 } }, -160, 8, 383, { .fields = { 80, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 64 } }, -112, 8, 383, { .fields = { 96, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 48, 48 } }, -160, 72, 365, { .fields = { 48, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 255), { .fields = { 32, 48 } }, -112, 72, 365, { .fields = { 64, 168 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 8 } }, 0, -64, 1069, { .fields = { 120, 248 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 32, 64 } }, 40, 16, 1125, { .fields = { 96, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 40, 32 } }, 80, 40, 2223, { .fields = { 8, 120 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 8, 16 } }, 8, -64, 1069, { .fields = { 8, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 8, 24 } }, 16, -64, 1069, { .fields = { 112, 224 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, 24, -64, 1069, { .fields = { 24, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, 48, -64, 1069, { .fields = { 16, 64 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 32 } }, 72, -64, 1069, { .fields = { 0, 184 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 32 } }, 96, -64, 1069, { .fields = { 120, 216 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 16, 32 } }, 128, -64, 1069, { .fields = { 8, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 88, 72, 3139, { .fields = { 88, 200 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 32, -32, 1125, { .fields = { 104, 104 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 64, -32, 1125, { .fields = { 104, 152 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 24, 24 } }, 40, -8, 1250, { .fields = { 112, 128 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 24 } }, 64, -8, 2102, { .fields = { 88, 176 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 40, 24 } }, 104, -8, 2102, { .fields = { 104, 56 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 80, 16, 3067, { .fields = { 104, 80 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 32, 24 } }, 112, 16, 3067, { .fields = { 112, 32 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, 96, -32, 1125, { .fields = { 8, 96 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 960, 0), getClut(0, 256), { .fields = { 24, 24 } }, 120, -32, 1125, { .fields = { 24, 232 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
    { getTPage(DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_TEXTURE_8BIT, GPU_BLEND_AVERAGE, 896, 0), getClut(0, 256), { .fields = { 208, 32 } }, -48, 88, 2000, { .fields = { 0, 0 } }, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_NEUTRAL_RGB, 0 },
};

/// Writable drawing batches for balcony mapped view 39.
///
/// A zero-count first record requests decoded background strips. Source-element
/// ranges are 0+54, 54+20 and 74+1; the final record has
/// firstSprite=SPRITE_BATCH_END and contributes no sprites. Visibility updates
/// and cached drawing borrow this list while the room overlay is loaded.
static SpriteBatch _gDryfieldNightMotelBalconyView39SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 54, 0, 0, { 1, 0 } },
    { 54, 20, 0, 0, { 2, 0 } },
    { 74, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gDryfieldNightMotelBalconySpriteViews[39] = {
    { { .empty = _gDryfieldNightMotelBalconyView1SpriteBatches }, _gDryfieldNightMotelBalconyView1SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView2SpriteSources }, _gDryfieldNightMotelBalconyView2SpriteBatches, _gDryfieldNightMotelBalconyView2SpriteDrawAreas },
    { { .empty = _gDryfieldNightMotelBalconyView3SpriteBatches }, _gDryfieldNightMotelBalconyView3SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView4SpriteSources }, _gDryfieldNightMotelBalconyView4SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView5SpriteSources }, _gDryfieldNightMotelBalconyView5SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView6SpriteSources }, _gDryfieldNightMotelBalconyView6SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView7SpriteSources }, _gDryfieldNightMotelBalconyView7SpriteBatches, _gDryfieldNightMotelBalconyView7SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelBalconyView8SpriteSources }, _gDryfieldNightMotelBalconyView8SpriteBatches, _gDryfieldNightMotelBalconyView8SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelBalconyView9SpriteSources }, _gDryfieldNightMotelBalconyView9SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView10SpriteSources }, _gDryfieldNightMotelBalconyView10SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView11SpriteSources }, _gDryfieldNightMotelBalconyView11SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView12SpriteSources }, _gDryfieldNightMotelBalconyView12SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelBalconyView13SpriteBatches }, _gDryfieldNightMotelBalconyView13SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView14SpriteSources }, _gDryfieldNightMotelBalconyView14SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelBalconyView15SpriteBatches }, _gDryfieldNightMotelBalconyView15SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelBalconyView16SpriteBatches }, _gDryfieldNightMotelBalconyView16SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView17SpriteSources }, _gDryfieldNightMotelBalconyView17SpriteBatches, _gDryfieldNightMotelBalconyView17SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelBalconyView18SpriteSources }, _gDryfieldNightMotelBalconyView18SpriteBatches, _gDryfieldNightMotelBalconyView18SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelBalconyView19SpriteSources }, _gDryfieldNightMotelBalconyView19SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView20SpriteSources }, _gDryfieldNightMotelBalconyView20SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelBalconyView21SpriteBatches }, _gDryfieldNightMotelBalconyView21SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView22SpriteSources }, _gDryfieldNightMotelBalconyView22SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView23SpriteSources }, _gDryfieldNightMotelBalconyView23SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView24SpriteSources }, _gDryfieldNightMotelBalconyView24SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView25SpriteSources }, _gDryfieldNightMotelBalconyView25SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView26SpriteSources }, _gDryfieldNightMotelBalconyView26SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelBalconyView27SpriteBatches }, _gDryfieldNightMotelBalconyView27SpriteBatches, NULL },
    { { .empty = _gDryfieldNightMotelBalconyView28SpriteBatches }, _gDryfieldNightMotelBalconyView28SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView29SpriteSources }, _gDryfieldNightMotelBalconyView29SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView30SpriteSources }, _gDryfieldNightMotelBalconyView30SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView31SpriteSources }, _gDryfieldNightMotelBalconyView31SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView32SpriteSources }, _gDryfieldNightMotelBalconyView32SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView33SpriteSources }, _gDryfieldNightMotelBalconyView33SpriteBatches, _gDryfieldNightMotelBalconyView33SpriteDrawAreas },
    { { .empty = _gDryfieldNightMotelBalconyView34SpriteBatches }, _gDryfieldNightMotelBalconyView34SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView35SpriteSources }, _gDryfieldNightMotelBalconyView35SpriteBatches, _gDryfieldNightMotelBalconyView35SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelBalconyView36SpriteSources }, _gDryfieldNightMotelBalconyView36SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView37SpriteSources }, _gDryfieldNightMotelBalconyView37SpriteBatches, _gDryfieldNightMotelBalconyView37SpriteDrawAreas },
    { { .elements = _gDryfieldNightMotelBalconyView38SpriteSources }, _gDryfieldNightMotelBalconyView38SpriteBatches, NULL },
    { { .elements = _gDryfieldNightMotelBalconyView39SpriteSources }, _gDryfieldNightMotelBalconyView39SpriteBatches, NULL },
};

/// Twenty-two point lights shared by nighttime motel balcony room variants 1 and 3.
///
/// Every light contributes in every view. Positions and inner/outer falloff radii
/// use integer world units; RGB intensities have 12 fractional bits (`ONE` is 1.0).
/// Strength is full through the inner radius, then falls with squared distance
/// to zero at the outer radius. The loaded room overlay owns this writable array:
/// coordinate updates parent and compose its transforms, and lighting queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldNightMotelBalconyRooms1And3PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, 2032 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 316 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2611, -5200, 0x2A75 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 3833 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 5310 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, -3212 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2FD2, -5200, 424 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1381,
        .outer = 3827,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, 7089 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, -2343 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6829, -6200, -5876 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4259, -6200, 2340 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, -1229 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 9970 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 8124 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, -2607 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3686, ONE, 3686 },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, 7297 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3686, ONE, 3686 },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4253, -2000, 0x2A75 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 9652 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 6623 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1664, -2000, 9386 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, -3140 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, -5930 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 3000,
        .outer = 5500,
    },
};

/// Authored point-light collection for balcony rooms 1 and 3.
///
/// Borrows all 22 writable room-owned point lights; there are no directional
/// or cone lights. The collection and light arrays stay live with this overlay.
static WorldCoordRoomLights _gDryfieldNightMotelBalconyRooms1And3Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelBalconyRooms1And3PointLights),
    .pointLights           = _gDryfieldNightMotelBalconyRooms1And3PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Authored point lights for balcony room 2, admitted in every logical view.
///
/// Positions and inner/outer falloff radii use integer world-coordinate units;
/// RGB intensity and unit matrix coefficients use ONE (Q12). These differ from
/// rooms 1 and 3 in placement and falloff. Coordinate setup borrows the live
/// view parent and shading overwrites attenuation, so the 22 room-owned records
/// remain writable for the loaded overlay's lifetime.
static WorldCoordPointLight _gDryfieldNightMotelBalconyRoom2PointLights[22] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, 2032 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 316 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2611, -5200, 0x2A75 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 3833 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 5310 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, -3212 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2FD2, -5200, 424 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1339,
        .outer = 4141,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3755, -6200, 7089 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 8921,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, -2343 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 7181,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6829, -6200, -5876 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4259, -6200, 2340 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, -1229 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 9970 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 8124 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, -2607 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3686, ONE, 3686 },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, 7297 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3686, ONE, 3686 },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4253, -2000, 0x2A75 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 9652 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 6623 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1664, -2000, 9386 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, -3140 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, -5930 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 3000,
        .outer = 5500,
    },
};

/// Authored point-light collection for balcony room 2.
///
/// Borrows all 22 writable room-owned point lights; there are no directional
/// or cone lights. The collection and light arrays stay live with this overlay.
static WorldCoordRoomLights _gDryfieldNightMotelBalconyRoom2Lights = {
    .directionalLightCount = 0,
    .directionalLights     = NULL,
    .pointLightCount       = ARRAY_SIZE(_gDryfieldNightMotelBalconyRoom2PointLights),
    .pointLights           = _gDryfieldNightMotelBalconyRoom2PointLights,
    .coneLightCount        = 0,
    .coneLights            = NULL,
};

/// Directed view-boundary quads for balcony room 1.
///
/// The 8 records request logical view transitions 5<->6, 5<->3, 4<->5 and 3<->2.
/// Positions/radii use room game units and normals use 4096 per unit. Activation
/// binds coordinates and mutates list state; the final included record carries
/// WORLD_COLLISION_TRIGGER_LAST. Keep this room-owned array live until unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelBalconyRoom1ViewBoundaryTriggers[8] = {
    { NULL, NULL, NULL, { -6032, -4416, 320, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 480, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7168, -4000, 1952, 0 }, { { 66, 2304, 1358, 0 }, { -67, 2304, -1359, 0 }, { 66, -2304, 1358, 0 }, { -67, -2304, -1359, 0 } }, { 4102, 0, -202, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7073, -4096, 1855, 0 }, { { -66, 2528, -1358, 0 }, { 67, 2528, 1359, 0 }, { -66, -2528, -1358, 0 }, { 67, -2528, 1359, 0 } }, { -4091, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -4400, 6206, 0 }, { { -1323, 2576, 327, 0 }, { 1316, 2576, -332, 0 }, { -1323, -2576, 327, 0 }, { 1316, -2576, -332, 0 } }, { 992, 0, 3980, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6180, -4656, 6363, 0 }, { { 1320, 2704, -329, 0 }, { -1319, 2704, 330, 0 }, { 1320, -2704, -329, 0 }, { -1319, -2704, 330, 0 } }, { -996, 0, -3982, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2EB7, -4032, 2043, 0 }, { { 1, 2480, 1361, 0 }, { 0, 2480, -1361, 0 }, { 1, -2480, 1361, 0 }, { 0, -2480, -1361, 0 } }, { 4103, 0, -3, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2E15, -3936, 2013, 0 }, { { 0, 2480, -1361, 0 }, { 1, 2480, 1361, 0 }, { 0, -2480, -1361, 0 }, { 1, -2480, 1361, 0 } }, { -4106, 0, 1, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Directed view-boundary quads for balcony room 2.
///
/// The 10 records request logical view transitions 5<->6, 5<->3, 4<->5, 3<->2 and 4<->9.
/// Positions/radii use room game units and normals use 4096 per unit. Activation
/// binds coordinates and mutates list state; the final included record carries
/// WORLD_COLLISION_TRIGGER_LAST. Keep this room-owned array live until unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelBalconyRoom2ViewBoundaryTriggers[10] = {
    { NULL, NULL, NULL, { -6032, -4416, 320, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 480, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7168, -4000, 1952, 0 }, { { 66, 2304, 1358, 0 }, { -67, 2304, -1359, 0 }, { 66, -2304, 1358, 0 }, { -67, -2304, -1359, 0 } }, { 4102, 0, -202, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7073, -4096, 1855, 0 }, { { -66, 2528, -1358, 0 }, { 67, 2528, 1359, 0 }, { -66, -2528, -1358, 0 }, { 67, -2528, 1359, 0 } }, { -4091, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -4400, 6206, 0 }, { { -1323, 2576, 327, 0 }, { 1316, 2576, -332, 0 }, { -1323, -2576, 327, 0 }, { 1316, -2576, -332, 0 } }, { 992, 0, 3980, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6180, -4656, 6331, 0 }, { { 1320, 2704, -329, 0 }, { -1319, 2704, 330, 0 }, { 1320, -2704, -329, 0 }, { -1319, -2704, 330, 0 } }, { -996, 0, -3982, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9463, -4032, 2043, 0 }, { { -266, 2480, 1333, 0 }, { 263, 2480, -1337, 0 }, { -266, -2480, 1333, 0 }, { 263, -2480, -1337, 0 } }, { 4025, 0, 796, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9301, -3936, 2013, 0 }, { { 198, 2480, -1348, 0 }, { -200, 2480, 1345, 0 }, { 198, -2480, -1348, 0 }, { -200, -2480, 1345, 0 } }, { -4062, 0, -601, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5969, -4800, 0x2780, 0 }, { { 1196, 2480, -1184, 0 }, { -1195, 2480, 1184, 0 }, { 1196, -2480, -1184, 0 }, { -1195, -2480, 1184, 0 } }, { -2885, 0, -2913, 0 }, { 0, 0, 4096, 0 }, 2996, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6096, -4736, 9968, 0 }, { { -1156, 2480, 1136, 0 }, { 1157, 2480, -1136, 0 }, { -1156, -2480, 1136, 0 }, { 1157, -2480, -1136, 0 } }, { 2870, 0, 2921, 0 }, { 0, 0, 4096, 0 }, 2952, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Directed view-boundary quads for balcony room 3.
///
/// The 6 records request logical view transitions 3<->6, 4<->5 and 3<->2.
/// Positions/radii use room game units and normals use 4096 per unit. Activation
/// binds coordinates and mutates list state; the final included record carries
/// WORLD_COLLISION_TRIGGER_LAST. Keep this room-owned array live until unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelBalconyRoom3ViewBoundaryTriggers[6] = {
    { NULL, NULL, NULL, { -6032, -4416, 736, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 864, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6181, -4400, 8571, 0 }, { { -1360, 2576, -1, 0 }, { 1360, 2576, 1, 0 }, { -1360, -2576, -1, 0 }, { 1360, -2576, 1, 0 } }, { -4, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6149, -4656, 8698, 0 }, { { 1361, 2704, -19, 0 }, { -1360, 2704, 20, 0 }, { 1361, -2704, -19, 0 }, { -1360, -2704, 20, 0 } }, { -60, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9207, -4032, 2043, 0 }, { { -133, 2480, 1354, 0 }, { 132, 2480, -1356, 0 }, { -133, -2480, 1354, 0 }, { 132, -2480, -1356, 0 } }, { 4085, 0, 398, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9046, -3936, 2011, 0 }, { { 132, 2480, -1355, 0 }, { -133, 2480, 1355, 0 }, { 132, -2480, -1355, 0 }, { -133, -2480, 1355, 0 } }, { -4087, 0, -400, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Scavenger resource recipes for balcony placement variant 1.
///
/// Entry 16 selects global-library file 16 in base group 10 and task index 0.
/// The five placements share this recipe; AREA_PLACEMENT_END ends the two-record
/// list. Loading and spawning borrow the table while this room is loaded.
static AreaResource _gDryfieldNightMotelBalconyVariant1Resources[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor101600ScavengerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Actor resource recipes for balcony placement variant 2.
///
/// Entries 31, 106 and 114 select Burner, Gary Douglas and Flint task
/// recipes, each at task index 0. File/base-group pairs are 31/40, 358/30 and
/// 358/60. AREA_PLACEMENT_END ends the four-record list; loading and spawning
/// borrow the room-owned storage.
static AreaResource _gDryfieldNightMotelBalconyVariant2Resources[4] = {
    { 31, 31, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, gActor403100TaskDescriptors },
    { 106, 358, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, gActor335800GaryDouglasTasks },
    { 114, 358, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &gActor335800FlintTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Zebra Stalker resource recipes for balcony placement variant 3.
///
/// Entry 6 selects global-library file 6 in base group 40 and task index 0.
/// Two placements share the recipe and request additional file low component 1.
/// AREA_PLACEMENT_END ends the two-record list; loading and spawning borrow it.
static AreaResource _gDryfieldNightMotelBalconyVariant3Resources[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &gActor400600ZebraStalkerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

/// Mind Suckler resource recipes for balcony placement variant 11.
///
/// Entry 15 selects global-library file 15 in base group 10 and task index 0.
/// Five placements share the recipe and request additional file low component 2.
/// AREA_PLACEMENT_END ends the two-record list; loading and spawning borrow it.
static AreaResource _gDryfieldNightMotelBalconyVariant11Resources[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor01500MindSucklerTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant gDryfieldNightMotelBalconyAreaVariants[13] = {
    [1]  = { .placements = gMapDryfieldFullMotelBalconyVariant1Placements, .resources = _gDryfieldNightMotelBalconyVariant1Resources },
    [2]  = { .placements = gMapDryfieldFullMotelBalconyVariant2Placements, .resources = _gDryfieldNightMotelBalconyVariant2Resources },
    [3]  = { .placements = gMapDryfieldFullMotelBalconyVariant3Placements, .resources = _gDryfieldNightMotelBalconyVariant3Resources },
    [11] = { .placements = gMapDryfieldFullMotelBalconyVariant11Placements, .resources = _gDryfieldNightMotelBalconyVariant11Resources },
};

/// Interaction and exit triggers for balcony room 1.
///
/// Four facing quads request the parking lot, motel rooms 5/6 and loft.
/// Packed departure/arrival warp slots are 1/5, 2/1, 3/1 and 4/1. Positions/radii use room game units; normals use 4096 per
/// unit. Activation binds coordinates and mutates links, enable flags and hits.
/// The final included trigger carries WORLD_COLLISION_TRIGGER_LAST; retain this
/// room-owned array until it is unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelBalconyRoom1ActionTriggers[4] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM5_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6752, -3280, 4448, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM6_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1872, -3248, 0x2A2F, 0 }, { { 848, 0, 432, 0 }, { -848, 0, 432, 0 }, { 848, 0, -432, 0 }, { -848, 0, -432, 0 } }, { 0, 4102, 0, 0 }, { -51, 0, -4096, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, DRYFIELD_NIGHT_MOTEL_BALCONY_LOFT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and exit triggers for balcony room 2.
///
/// Four facing quads request CAP command 8 instead of allowing the four exits. Positions/radii use room game units; normals use 4096 per
/// unit. Activation binds coordinates and mutates links, enable flags and hits.
/// The final included trigger carries WORLD_COLLISION_TRIGGER_LAST; retain this
/// room-owned array until it is unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelBalconyRoom2ActionTriggers[4] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM2_EXIT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM2_EXIT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1920, -3248, 0x2950, 0 }, { { 832, 0, 336, 0 }, { -832, 0, 336, 0 }, { 832, 0, -336, 0 }, { -832, 0, -336, 0 } }, { 0, 4117, 0, 0 }, { -51, 0, -4096, 0 }, 896, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM2_EXIT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6784, -3254, 3872, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM2_EXIT_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Interaction and exit triggers for balcony room 3.
///
/// Four facing quads request the same exits as room 1; three further quads
/// request CAP command 9. Positions/radii use room game units; normals use 4096 per
/// unit. Activation binds coordinates and mutates links, enable flags and hits.
/// The final included trigger carries WORLD_COLLISION_TRIGGER_LAST; retain this
/// room-owned array until it is unlinked.
static WorldCollisionTrigger _gDryfieldNightMotelBalconyRoom3ActionTriggers[7] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_PARKING_LOT, DRYFIELD_NIGHT_MOTEL_BALCONY_PARKING_LOT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM5_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6752, -3280, 4448, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_6, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM6_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1872, -3248, 0x2A2F, 0 }, { { 848, 0, 432, 0 }, { -848, 0, 432, 0 }, { 848, 0, -432, 0 }, { -848, 0, -432, 0 } }, { 0, 4102, 0, 0 }, { -51, 0, -4096, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_WARP, GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOFT, DRYFIELD_NIGHT_MOTEL_BALCONY_LOFT_WARP_SLOTS, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6032, -3264, 4272, 0 }, { { -1232, 0, -848, 0 }, { 1232, 0, 80, 0 }, { -1232, 0, -80, 0 }, { 1232, 0, 848, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM3_BOUNDARY_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7632, -3264, 2176, 0 }, { { -2688, 0, 560, 0 }, { 2688, 0, -1328, 0 }, { -2688, 0, 1328, 0 }, { 2688, 0, -560, 0 } }, { 0, 4095, 0, 0 }, { -3919, 0, -1189, 0 }, 2996, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM3_BOUNDARY_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7648, -3264, 2176, 0 }, { { -2688, 0, 560, 0 }, { 2688, 0, -1328, 0 }, { -2688, 0, 1328, 0 }, { 2688, 0, -560, 0 } }, { 0, 4095, 0, 0 }, { 401, 0, -4076, 0 }, 2996, WORLD_COLLISION_TRIGGER_ACTION_CAP, DRYFIELD_NIGHT_MOTEL_BALCONY_ROOM3_BOUNDARY_CAP_COMMAND, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Two sight-blocking quads shared by all three balcony rooms.
///
/// Room-space origins, corner offsets and radii use game units; normals use
/// 4096 per unit. Room activation mutates list links and enable flags. The
/// second included quad carries WORLD_COLLISION_OCCLUDER_LAST; storage must
/// remain loaded until both records are unlinked.
static WorldCollisionOccluder _gDryfieldNightMotelBalconySightOccluders[2] = {
    { NULL, NULL, { -0x2840, -4064, 7440, 0 }, { { -3104, -2624, 4336, 0 }, { 3104, -2624, -4336, 0 }, { -3104, 2624, 4336, 0 }, { 3104, 2624, -4336, 0 } }, { -3332, 0, -2385, 0 }, 5926, 1, 0 },
    { NULL, NULL, { -0x2B61, -4016, -2561, 0 }, { { 4011, -2640, 3514, 0 }, { -4010, -2640, -3513, 0 }, { 4011, 2640, 3514, 0 }, { -4010, 2640, -3513, 0 } }, { -2701, 0, 3082, 0 }, 5948, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Minimum ambient model colours by logical view for balcony rooms 1 and 3.
///
/// Entry zero counts 39 views; entries 1..39 hold RGB minima at 16 units per
/// 8-bit colour level, plus stored luma. These use logical views directly,
/// independently of the camera/image remap. The lighting table borrows this
/// read-only room-owned storage while the overlay is loaded.
static const WorldCoordRoomAmbientEntry _gDryfieldNightMotelBalconyRooms1And3AmbientByView[40] SECTION(".data") = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyRooms1And3AmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 618, 618, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Minimum ambient model colours by logical view for balcony room 2.
///
/// Entry zero counts 39 views; entries 1..39 hold RGB minima at 16 units per
/// 8-bit colour level, plus stored luma. These use logical views directly,
/// independently of the camera/image remap. The lighting table borrows this
/// read-only room-owned storage while the overlay is loaded.
static const WorldCoordRoomAmbientEntry _gDryfieldNightMotelBalconyRoom2AmbientByView[40] SECTION(".data") = {
    { .viewCount = ARRAY_SIZE(_gDryfieldNightMotelBalconyRoom2AmbientByView) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 618, 618, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

/// Cue-2 entry numbers in the currently loaded type-1 footstep bank.
enum {
    DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_1_WALK_ENTRY = 0x49,
    DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_1_RUN_ENTRY  = 0x4B,
    DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_2_WALK_ENTRY = 0x2D,
    DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_2_RUN_ENTRY  = 0x2F,
    DRYFIELD_NIGHT_MOTEL_BALCONY_DEFAULT_WALK_ENTRY = 0x51,
    DRYFIELD_NIGHT_MOTEL_BALCONY_DEFAULT_RUN_ENTRY  = 0x53,
    DRYFIELD_NIGHT_MOTEL_BALCONY_DEFAULT_JUMP_ENTRY = 0x55,
};

/// Footstep script requests for balcony surface class 1.
///
/// Stores walk, run and scripted-jump cue-2 bases; cue 1 adds one and companion
/// steps add 100. Surface records borrow this read-only room-owned storage while
/// the overlay is loaded. The type-1 request selects the currently loaded bank.
/// Its entries 0x49..0x4C are absent from this room's bank, so these retained
/// requests produce no playback.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelBalconyClass1FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_1_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_1_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_1_WALK_ENTRY,
};

/// Footstep script requests for balcony surface class 2.
///
/// Stores walk, run and scripted-jump cue-2 bases; cue 1 adds one and companion
/// steps add 100. Surface records borrow this read-only room-owned storage while
/// the overlay is loaded. The type-1 request selects the currently loaded bank.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelBalconyClass2FootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_2_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_2_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_CLASS_2_WALK_ENTRY,
};

/// Footstep script requests for balcony surface classes 0 and 3..7.
///
/// Stores walk, run and scripted-jump cue-2 bases; cue 1 adds one and companion
/// steps add 100. Surface records borrow this read-only room-owned storage while
/// the overlay is loaded. The type-1 request selects the currently loaded bank.
/// The stored jump entries 0x55/0x56 are absent from this room's bank.
static const WorldCollisionFootstepSounds _gDryfieldNightMotelBalconyDefaultFootstepSounds SECTION(".data") = {
    .walk         = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_DEFAULT_WALK_ENTRY,
    .run          = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_DEFAULT_RUN_ENTRY,
    .scriptedJump = SOUND_SCRIPT_REQUEST_TYPE_1 | DRYFIELD_NIGHT_MOTEL_BALCONY_DEFAULT_JUMP_ENTRY,
};

/// Blocking, impact-enabled, pushback-applying surface properties for balcony classes 0 and 4..7.
///
/// Borrows the selected footstep requests. The eight-class directory reaches
/// this single record while the room overlay is loaded; the stored field_0
/// byte remains zero and its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelBalconyDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelBalconyDefaultFootstepSounds,
};

/// Blocking, impact-enabled, pushback-applying surface properties for balcony class 1.
///
/// Borrows the selected footstep requests. The eight-class directory reaches
/// this single record while the room overlay is loaded; the stored field_0
/// byte remains zero and its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelBalconyClass1SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelBalconyClass1FootstepSounds,
};

/// Blocking, impact-enabled, pushback-applying surface properties for balcony class 2.
///
/// Borrows the selected footstep requests. The eight-class directory reaches
/// this single record while the room overlay is loaded; the stored field_0
/// byte remains zero and its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelBalconyClass2SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelBalconyClass2FootstepSounds,
};

/// Blocking, impact-enabled, pushback-applying surface properties for balcony class 3.
///
/// Borrows the selected footstep requests. The eight-class directory reaches
/// this single record while the room overlay is loaded; the stored field_0
/// byte remains zero and its role is unproven.
static WorldCollisionSurfaceProperties _gDryfieldNightMotelBalconyClass3SurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gDryfieldNightMotelBalconyDefaultFootstepSounds,
};

WorldCollisionSurfaceProperties* gDryfieldNightMotelBalconySurfacePropertiesByClass[8] = {
    &_gDryfieldNightMotelBalconyDefaultSurfaceProperties,
    &_gDryfieldNightMotelBalconyClass1SurfaceProperties,
    &_gDryfieldNightMotelBalconyClass2SurfaceProperties,
    &_gDryfieldNightMotelBalconyClass3SurfaceProperties,
    &_gDryfieldNightMotelBalconyDefaultSurfaceProperties,
    &_gDryfieldNightMotelBalconyDefaultSurfaceProperties,
    &_gDryfieldNightMotelBalconyDefaultSurfaceProperties,
    &_gDryfieldNightMotelBalconyDefaultSurfaceProperties,
};

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);
static void _glowDrawShaft(const SVECTOR worldPoints[2], s32 radiusScale);

void dryfieldNightMotelBalconyAmbientEffectsTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_MASK_BITS                   = 32,
        DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE               = 0x180,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLARE_RADIUS_SCALE               = 0x380,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FIRST_FLARE_POINT                = 10,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLARE_POINT_END                  = 18,
        DRYFIELD_NIGHT_MOTEL_BALCONY_LAMP_BREAK_PENDING               = 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_LAMP_BREAK_DONE                  = 2,
        DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_17_BURST_TICK               = 92,
        DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_18_BREAK_TICK               = 63,
        DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_19_BREAK_TICK               = 43,
        DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_20_23_BREAK_TICK            = 12,
        DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_29_BURST_TICK               = 65,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_EXPANSION_START            = 71,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_EMISSION_PERIOD            = 6,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_PAIR_COUNT                = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_SCATTERED_PUFF_ARGS              = 0x40000300,
        DRYFIELD_NIGHT_MOTEL_BALCONY_LARGE_DEBRIS_ARGS                = 0x80010100,
        DRYFIELD_NIGHT_MOTEL_BALCONY_SMALL_DEBRIS_ARGS                = 0x80000080,
        DRYFIELD_NIGHT_MOTEL_BALCONY_EXPANDING_FLAME_ARGS             = 0x40000600,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_SCATTERED_PUFFS_AND_DEBRIS = 2,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_PUFFS_AND_LARGE_DEBRIS     = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_PUFFS_ONLY                 = 4,
        DRYFIELD_NIGHT_MOTEL_BALCONY_LARGE_DEBRIS_SIZE_MASK           = 0xFF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_SMALL_DEBRIS_SIZE_MASK           = 0x7F,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_SIZE_STEP                  = 40,
    };
    EffectWork* work;
    GfxCoord*   coord;
    s32         viewMaskWord;
    u32         viewBit;
    s32         effectIndex;
    s32         expansionFrames;
    s16         elapsedFrames;
    SVECTOR     spawnOffset;
    SVECTOR     randomOffset;

    work                                = task->spawnArg2.pointer;
    coord                               = task->extra.coordBody->coord;
    gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_MAX_SHADE;
    // Select this view in the low or high 32-bit mask word.
    viewMaskWord = 0;
    if (gGameSession->location.view < DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_MASK_BITS) {
        viewBit = 1u << gGameSession->location.view;
    } else {
        viewBit      = 1u << (gGameSession->location.view - DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_MASK_BITS);
        viewMaskWord = 1;
    }
    if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][0]) {
        _glowDrawShaft(&gDryfieldNightMotelBalconyGlowPoints[0], DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE);
    }
    if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][2]) {
        _glowDrawShaft(&gDryfieldNightMotelBalconyLampShaftStart, DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE);
    }
    if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][4]) {
        _glowDrawShaft(&gDryfieldNightMotelBalconyShaft3Start, DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE);
    }
    if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][6]) {
        _glowDrawShaft(&gDryfieldNightMotelBalconyShaft4Start, DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE);
    }
    if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][8]) {
        _glowDrawShaft(&_gDryfieldNightMotelBalconyShaft5Start, DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE);
    }
    for (effectIndex = DRYFIELD_NIGHT_MOTEL_BALCONY_FIRST_FLARE_POINT; effectIndex < DRYFIELD_NIGHT_MOTEL_BALCONY_FLARE_POINT_END; effectIndex++) {
        if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][effectIndex]) {
            _glowDrawFlare(&_gDryfieldNightMotelBalconyShaft5EndAndFlarePoints[effectIndex - (DRYFIELD_NIGHT_MOTEL_BALCONY_FIRST_FLARE_POINT - 1)], 1, DRYFIELD_NIGHT_MOTEL_BALCONY_FLARE_RADIUS_SCALE);
        }
    }
    if (viewBit & _gDryfieldNightMotelBalconyGlowViewMasks[viewMaskWord][18]) {
        _glowDrawShaft(&_gDryfieldNightMotelBalconyShaft6Start, DRYFIELD_NIGHT_MOTEL_BALCONY_SHAFT_RADIUS_SCALE);
    }
    // Keep a broken lamp dark across subsequent visits.
    if (gameFlagGetNibble(GAME_FLAG_07F) == DRYFIELD_NIGHT_MOTEL_BALCONY_LAMP_BREAK_PENDING) {
        _dryfieldNightMotelBalconyDisableBrokenLamp();
        effectSpawn(EFFECT_NIGHT_MOTEL_BALCONY_LAMP_BURST, coord, 0, &gDryfieldNightMotelBalconyLampShaftStart);
        gameFlagSetNibble(GAME_FLAG_07F, DRYFIELD_NIGHT_MOTEL_BALCONY_LAMP_BREAK_DONE);
    } else if (gameFlagGetNibble(GAME_FLAG_07F) == DRYFIELD_NIGHT_MOTEL_BALCONY_LAMP_BREAK_DONE) {
        _dryfieldNightMotelBalconyDisableBrokenLamp();
    }
    // These counters continue across the coordinated scene views.
    switch (gGameSession->location.view) {
        case 17:
            if (++work->angle == DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_17_BURST_TICK) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, DRYFIELD_NIGHT_MOTEL_BALCONY_SCATTERED_PUFF_ARGS, &_gDryfieldNightMotelBalconyView17DebrisBurstOffset);
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, DRYFIELD_NIGHT_MOTEL_BALCONY_SCATTERED_PUFF_ARGS, &_gDryfieldNightMotelBalconyView17DebrisBurstOffset);
                for (effectIndex = 0; effectIndex < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_PAIR_COUNT; effectIndex++) {
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_LARGE_DEBRIS_SIZE_MASK) | DRYFIELD_NIGHT_MOTEL_BALCONY_LARGE_DEBRIS_ARGS,
                                &_gDryfieldNightMotelBalconyView17DebrisBurstOffset);
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_SMALL_DEBRIS_SIZE_MASK) | DRYFIELD_NIGHT_MOTEL_BALCONY_SMALL_DEBRIS_ARGS,
                                &_gDryfieldNightMotelBalconyView17DebrisBurstOffset);
                }
            }
            break;
        case 18:
            if (++work->scale == DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_18_BREAK_TICK) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_PUFFS_AND_LARGE_DEBRIS, &_gDryfieldNightMotelBalconyView18BreakOffset);
                work->angle = 0;
            }
            break;
        case 21:
            elapsedFrames = ++work->angle;
            if (elapsedFrames >= DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_EXPANSION_START) {
                // Every sixth tick scatters a flame within an expanding region.
                expansionFrames = elapsedFrames - (DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_EXPANSION_START - 1);
                if ((s16)(elapsedFrames % DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_EMISSION_PERIOD) == 0) {
                    memset(&randomOffset, 0, sizeof(randomOffset));
                    RANDOM_LCG_ADVANCE();
                    randomOffset.vx = -((s32)RANDOM_LCG_DRAW() % (expansionFrames * 10));
                    RANDOM_LCG_ADVANCE();
                    randomOffset.vy = (s32)RANDOM_LCG_DRAW() % (expansionFrames * 10);
                    RANDOM_LCG_ADVANCE();
                    randomOffset.vz = (s32)RANDOM_LCG_DRAW() % (expansionFrames * 10);
                    spawnOffset     = randomOffset;
                    spawnOffset.vx += _gDryfieldNightMotelBalconyExpandingFlameBaseOffset.vx;
                    spawnOffset.vy += _gDryfieldNightMotelBalconyExpandingFlameBaseOffset.vy;
                    spawnOffset.vz += _gDryfieldNightMotelBalconyExpandingFlameBaseOffset.vz;
                    effectSpawn(EFFECT_NIGHT_MOTEL_BALCONY_FLAME, coord, expansionFrames * DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_SIZE_STEP + DRYFIELD_NIGHT_MOTEL_BALCONY_EXPANDING_FLAME_ARGS, &spawnOffset);
                }
                work->scale = 0;
            }
            break;
        case 19:
            if (++work->scale == DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_19_BREAK_TICK) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_PUFFS_ONLY, &_gDryfieldNightMotelBalconyView19BreakOffset);
            }
            break;
        case 20:
            if (++work->scale == DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_20_23_BREAK_TICK) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_SCATTERED_PUFFS_AND_DEBRIS, &_gDryfieldNightMotelBalconyView20BreakOffset);
            }
            break;
        case 23:
            if (++work->scale == DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_20_23_BREAK_TICK) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, DRYFIELD_NIGHT_MOTEL_BALCONY_BREAK_SCATTERED_PUFFS_AND_DEBRIS, &_gDryfieldNightMotelBalconyView23BreakOffset);
            }
            break;
        case 29:
            if (++work->scale == DRYFIELD_NIGHT_MOTEL_BALCONY_VIEW_29_BURST_TICK) {
                for (effectIndex = 0; effectIndex < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_PAIR_COUNT; effectIndex++) {
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_SMALL_DEBRIS_SIZE_MASK) | DRYFIELD_NIGHT_MOTEL_BALCONY_SMALL_DEBRIS_ARGS,
                                &_gDryfieldNightMotelBalconyView29DebrisOffset);
                }
            }
            break;
        default:
            work->scale = 0;
            work->angle = 0;
            break;
    }
}

#include "../../shared/effects/glow_draw_shaft.inc.c"

#include "../../shared/effects/glow_draw_flare.inc.c"

void dryfieldNightMotelBalconyDrawBreathFlame(s32 packedScreenPosition, s16 otIndex, s16 projectedScaleQ12, s16 animationStep)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_FIRST_TEXTURE_FRAME = 2,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_QUAD_CODE           = 0x2F,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_EXTENT_NUMERATOR    = 31,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_SCALE_SHIFT         = 12,
    };
    POLY_FT4*                       quad;
    const EffectSpriteTextureFrame* textureFrame;
    s16                             textureFrameIndex;
    const EffectSpriteTextureFrame* textureFrames;
    s32                             screenExtent;
    s32                             screenY;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, sizeof(*quad) / sizeof(quad->tag) - 1);
    setcode(quad, DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_QUAD_CODE);
    quad->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;

    // Repeat the atlas tail, skipping the two opening frames.
    textureFrameIndex = animationStep % (ARRAY_SIZE(gEffectSpriteAtlasFrames) - DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_FIRST_TEXTURE_FRAME) + DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_FIRST_TEXTURE_FRAME;
    textureFrames     = gEffectSpriteAtlasFrames;
    textureFrame      = &textureFrames[textureFrameIndex];
    _dryfieldNightMotelBalconySetBreathFlameTexture(quad, textureFrame);

    // Narrow packed X at the stores; reuse the extent for the vertical anchor.
    screenExtent = (projectedScaleQ12 * DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_EXTENT_NUMERATOR) >> DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_SCALE_SHIFT;
    quad->x2     = packedScreenPosition - screenExtent;
    quad->x0     = packedScreenPosition - screenExtent;
    quad->x3     = packedScreenPosition + screenExtent;
    quad->x1     = packedScreenPosition + screenExtent;

    screenExtent = (projectedScaleQ12 * DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_EXTENT_NUMERATOR) >> (DRYFIELD_NIGHT_MOTEL_BALCONY_BREATH_SCALE_SHIFT + 1);
    screenY      = packedScreenPosition >> 16;
    quad->y1     = screenY - screenExtent * 3;
    quad->y0     = screenY - screenExtent * 3;
    quad->y3     = screenY + screenExtent;
    quad->y2     = screenY + screenExtent;

    addPrim(&gGpuCurrentOt[otIndex], quad);
}

void dryfieldNightMotelBalconyDebrisTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_NEW               = 0,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_MOVING            = 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_SETTLED           = 2,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_SIZE_MASK         = 0xFFF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_ROW_MASK          = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FADE_START        = 60,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_LIFETIME          = 90,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FADE_STEP         = 4,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_REST_INTERVAL     = 8,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_REST_SPEED        = 0x20,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_INITIAL_SPEED     = 0xA0,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_GRAVITY_NUMERATOR = 0xA000,
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     localTransform;
    s32         randomCentre;
    SVECTOR     localDisplacement;
    SVECTOR     probeEndOrHitPoint;
    SVECTOR     probeStartOrNormal;
    u8          fadeRgb[3];

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }

    // Probe and draw the composed starting position; local movement dirties the next cache.
    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->state) {
        case DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_NEW:
            // Discard inherited rotation; velocity remains in the local world frame.
            localTransform = &coord->coord;
            gfxSetRotIdentity(localTransform);
            work->pos.vx = (u16)task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_SIZE_MASK;
            work->scale  = DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_INITIAL_SPEED;
            RANDOM_LCG_ADVANCE();
            work->pos.vy = RANDOM_LCG_DRAW() & 7;
            RANDOM_LCG_ADVANCE();
            work->index = RANDOM_LCG_DRAW() & 7;
            RANDOM_LCG_ADVANCE();
            work->pos.vz = RANDOM_LCG_DRAW() & (ONE - 1);
            RANDOM_LCG_ADVANCE();
            work->period = 0x200 - (RANDOM_LCG_DRAW() & 0x3FF);
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value < 0) {
                    randomCentre = 0x40;
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = randomCentre - (RANDOM_LCG_DRAW() & 0x7F);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = (RANDOM_LCG_DRAW() & 0xFF) + 0x20;
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = randomCentre - (RANDOM_LCG_DRAW() & 0x7F);
                } else {
                    randomCentre = 0x80;
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = randomCentre - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = randomCentre - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = randomCentre - (RANDOM_LCG_DRAW() & 0xFF);
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            coord->composeStamp   = GRAPHICS_COORD_DIRTY;
            task->state           = DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_MOVING;
            task->spawnArg1.value = (s16)(task->spawnArg1.value >> 16) & DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_ROW_MASK;
            break;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_MOVING:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->pos.vz += work->period;
                if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                    work->index++;
                }
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&localDisplacement);
                coord->coord.t[0]  += localDisplacement.vx;
                coord->coord.t[1]  += localDisplacement.vy;
                coord->coord.t[2]  += localDisplacement.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                // Rotate the local step into composed view space; outputs reuse the endpoints.
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&localDisplacement);
                gte_rtv0();
                gte_stsv(&probeEndOrHitPoint);
                probeStartOrNormal.vx  = coord->workm.t[0];
                probeStartOrNormal.vy  = coord->workm.t[1];
                probeStartOrNormal.vz  = coord->workm.t[2];
                probeEndOrHitPoint.vx += probeStartOrNormal.vx;
                probeEndOrHitPoint.vy += probeStartOrNormal.vy;
                probeEndOrHitPoint.vz += probeStartOrNormal.vz;
                if (worldCollisionProbeGridSegment(&probeEndOrHitPoint, &probeStartOrNormal, &probeEndOrHitPoint, &probeStartOrNormal) == 1) {
                    // Undo the trial move and blend toward the room-space surface normal.
                    coord->coord.t[0] -= localDisplacement.vx;
                    coord->coord.t[1] -= localDisplacement.vy;
                    coord->coord.t[2] -= localDisplacement.vz;
                    work->move.vx      = (probeStartOrNormal.vx >> 1) + (work->move.vx >> 1);
                    work->move.vy      = probeStartOrNormal.vy + (work->move.vy >> 1);
                    work->move.vz      = (probeStartOrNormal.vz >> 1) + (work->move.vz >> 1);
                    VectorNormalSS(&work->move, &work->move);
                    work->scale  = work->scale >> 1;
                    work->period = work->period >> 1;
                    gte_lddp(work->scale);
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&localDisplacement);
                    coord->coord.t[0] += localDisplacement.vx;
                    coord->coord.t[1] += localDisplacement.vy;
                    coord->coord.t[2] += localDisplacement.vz;
                    if (work->age - work->step < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_REST_INTERVAL && work->scale < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_REST_SPEED) {
                        task->state = DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_SETTLED;
                    } else {
                        work->step = work->age;
                    }
                } else if (work->scale > 0) {
                    work->move.vy += DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_GRAVITY_NUMERATOR / work->scale;
                }
            } else {
                work->age--;
            }
            if (work->age < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FADE_START) {
                _dryfieldNightMotelBalconyDrawDebris(task, NULL, task->spawnArg1.value);
            } else if (work->age < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_LIFETIME) {
                fadeRgb[0] = fadeRgb[1] = fadeRgb[2] = (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_LIFETIME - work->age) * DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FADE_STEP;
                _dryfieldNightMotelBalconyDrawDebris(task, fadeRgb, task->spawnArg1.value);
            } else {
                effectKillTask(work, task);
            }
            break;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_SETTLED:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            }
            if (work->age < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FADE_START) {
                _dryfieldNightMotelBalconyDrawDebris(task, NULL, task->spawnArg1.value);
            } else if (work->age < DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_LIFETIME) {
                fadeRgb[0] = fadeRgb[1] = fadeRgb[2] = (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_LIFETIME - work->age) * DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FADE_STEP;
                _dryfieldNightMotelBalconyDrawDebris(task, fadeRgb, task->spawnArg1.value);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Draws a spinning debris billboard, optionally modulated by three RGB bytes.
///
/// The live task supplies an `EffectWork` and a composed coordinate body. Its
/// initialized `spawnArg1.value` selects texture/palette row 0..2; `index` wraps
/// across eight frames. `pos.vx` is the sizing numerator and `pos.vz` the angle
/// (4096 per turn). `tintRgb` is borrowed for this call; NULL selects raw opaque
/// texture, otherwise the quad uses additive semitransparency. The ignored
/// `unusedRowIndex` is retained in the call signature. Uses and releases one
/// `EffectShapeScratch` block; changes GTE state and queues at projected depth.
static void _dryfieldNightMotelBalconyDrawDebris(const Task* task, const u8* tintRgb, s32 unusedRowIndex)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW     = 8,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_WORDS         = 16,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_Y             = 271,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_TRIG_FRACTION_BITS = 12,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_QUARTER_TURN       = ONE / 4,
    };
    const EffectWork*   work  = task->spawnArg2.pointer;
    const GfxCoord*     coord = task->extra.coordBody->coord;
    EffectShapeScratch* projection;
    POLY_FT4*           quad;
    s16                 uvSpan;

    uvSpan = _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].frameSize - 1;
    SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    projection = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    // Project the composed view-space centre before constructing screen-space corners.
    _dryfieldNightMotelBalconyProjectDebrisCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        if (tintRgb != NULL) {
            quad->r0 = tintRgb[0];
            quad->g0 = tintRgb[1];
            quad->b0 = tintRgb[2];
            setSemiTrans(quad, 1);
        } else {
            setShadeTex(quad, 1);
        }
        quad->tpage                 = getTPage(0, GPU_BLEND_ADD, _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].vramX, 0);
        quad->clut                  = getClut(task->spawnArg1.value * DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_WORDS, DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_Y);
        quad->u0                    = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].frameSize;
        quad->v0                    = _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].v;
        quad->u1                    = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].frameSize + uvSpan;
        quad->v1                    = _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].v;
        quad->u2                    = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].frameSize;
        quad->v2                    = _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].v + uvSpan;
        quad->u3                    = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].frameSize + uvSpan;
        quad->v3                    = _gDryfieldNightMotelBalconyDebrisTextureRows[task->spawnArg1.value].v + uvSpan;
        projection->extent.corner.x = (((uvSpan * work->pos.vx) / projection->depth) * rsin(work->pos.vz)) >> DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_TRIG_FRACTION_BITS;
        projection->extent.corner.y = (((uvSpan * work->pos.vx) / projection->depth) * rcos(work->pos.vz)) >> DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_TRIG_FRACTION_BITS;
        quad->x0                    = projection->screenX + projection->extent.corner.x;
        quad->x3                    = projection->screenX - projection->extent.corner.x;
        quad->y0                    = projection->screenY - projection->extent.corner.y;
        quad->y3                    = projection->screenY + projection->extent.corner.y;
        projection->extent.corner.x = (((uvSpan * work->pos.vx) / projection->depth) * rsin(work->pos.vz + DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_QUARTER_TURN)) >> DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_TRIG_FRACTION_BITS;
        projection->extent.corner.y = (((uvSpan * work->pos.vx) / projection->depth) * rcos(work->pos.vz + DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_QUARTER_TURN)) >> DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_TRIG_FRACTION_BITS;
        quad->x1                    = projection->screenX + projection->extent.corner.x;
        quad->x2                    = projection->screenX - projection->extent.corner.x;
        quad->y1                    = projection->screenY - projection->extent.corner.y;
        quad->y2                    = projection->screenY + projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(projection->depth), quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void dryfieldNightMotelBalconyBreakBurstTask(Task* task)
{
    enum {
        NEW                    = 0,
        LARGE_DEBRIS           = 1,
        LARGE_DEBRIS_AND_PUFFS = 2,
        SMALL_DEBRIS           = 3,
        SMALL_DEBRIS_AND_PUFFS = 4,
        SCATTERED_PUFFS        = 5,
        DOWNWARD_DEBRIS        = 6,
        ROW_ONE_PUFFS          = 7,
        FOLLOWUP_DEBRIS        = 8,
        PUFFS_ONLY             = 9,
        FINISH                 = 10,
        LARGE_SIZE_MASK        = 0xFF,
        SMALL_SIZE_MASK        = 0x7F,
        LARGE_ROW_ZERO_ARGS    = 0x100,
        LARGE_ROW_ONE_ARGS     = 0x10100,
        SMALL_ROW_ZERO_ARGS    = 0x80,
        SMALL_ROW_ONE_ARGS     = 0x10080,
        DOWNWARD_ROW_ZERO_ARGS = 0x80000080,
        DOWNWARD_ROW_ONE_ARGS  = 0x80010080,
        ROW_ZERO_PUFF_ARGS     = 0x400,
        SCATTERED_PUFF_ARGS    = 0x40000300,
        ROW_ONE_PUFF_ARGS      = 0x10400,
    };
/// Emits a batch of independently spawned debris with one LCG size draw each.
///
/// Captures `coord`, `particleIndex` and `gRandomLcgState`. Arguments must be
/// side-effect-free: count is tested each iteration; sizeMask and packedArgs
/// are evaluated for each particle. The spawn copies placement and retains
/// coord as its borrowed parent. Expands to a block and changes particleIndex.
#define DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH(count, sizeMask, packedArgs)                                             \
    {                                                                                                                           \
        for (particleIndex = 0; particleIndex < (count); particleIndex++) {                                                     \
            RANDOM_LCG_ADVANCE();                                                                                               \
            effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, (RANDOM_LCG_DRAW() & (sizeMask)) | (packedArgs), NULL); \
        }                                                                                                                       \
    }

    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         particleIndex;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    switch (task->state) {
        // Each variant selects one or two emission ticks followed by release.
        case NEW:
            task->state = task->spawnArg1.value * 2 + 1;
            break;
        case LARGE_DEBRIS:
            DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH(8, LARGE_SIZE_MASK, LARGE_ROW_ZERO_ARGS);
            task->state = LARGE_DEBRIS_AND_PUFFS;
            break;
        case LARGE_DEBRIS_AND_PUFFS:
            for (particleIndex = 0; particleIndex < 4; particleIndex++) {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, (RANDOM_LCG_DRAW() & LARGE_SIZE_MASK) | LARGE_ROW_ONE_ARGS, NULL);
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ROW_ZERO_PUFF_ARGS, NULL);
            }
            task->state = FINISH;
            break;
        case SMALL_DEBRIS:
            DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH(6, SMALL_SIZE_MASK, SMALL_ROW_ZERO_ARGS);
            task->state = SMALL_DEBRIS_AND_PUFFS;
            break;
        case SMALL_DEBRIS_AND_PUFFS:
            for (particleIndex = 0; particleIndex < 3; particleIndex++) {
                RANDOM_LCG_ADVANCE();
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, (RANDOM_LCG_DRAW() & SMALL_SIZE_MASK) | SMALL_ROW_ONE_ARGS, NULL);
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ROW_ZERO_PUFF_ARGS, NULL);
            }
            task->state = FINISH;
            break;
        case SCATTERED_PUFFS:
            for (particleIndex = 0; particleIndex < 4; particleIndex++) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, SCATTERED_PUFF_ARGS, NULL);
            }
            task->state = DOWNWARD_DEBRIS;
            break;
        case DOWNWARD_DEBRIS:
            DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH(4, SMALL_SIZE_MASK, DOWNWARD_ROW_ZERO_ARGS);
            DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH(2, SMALL_SIZE_MASK, DOWNWARD_ROW_ONE_ARGS);
            task->state = FINISH;
            break;
        case ROW_ONE_PUFFS:
            for (particleIndex = 0; particleIndex < 8; particleIndex++) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ROW_ONE_PUFF_ARGS, NULL);
            }
            task->state = FOLLOWUP_DEBRIS;
            break;
        case FOLLOWUP_DEBRIS:
            DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH(8, LARGE_SIZE_MASK, LARGE_ROW_ZERO_ARGS);
            task->state = FINISH;
            break;
        case PUFFS_ONLY:
            for (particleIndex = 0; particleIndex < 8; particleIndex++) {
                effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ROW_ONE_PUFF_ARGS, NULL);
            }
            task->state = FINISH;
            break;
        case FINISH:
            effectKillTask(work, task);
            break;
    }
}

#undef DRYFIELD_NIGHT_MOTEL_BALCONY_EMIT_DEBRIS_BATCH

void dryfieldNightMotelBalconyFallingParticleTask(Task* task)
{
    enum {
        NEW                     = 0,
        FALLING                 = 1,
        SETTLED                 = 2,
        SIZE_MASK               = 0xFFF,
        INITIAL_SCALE           = 160,
        START_FRAME_MASK        = 7,
        INITIAL_FALL_SPEED_MASK = 0xFF,
        GRAVITY_PER_TICK        = 6,
        FADE_START              = 20,
        LIFETIME                = 30,
        FADE_STEP               = 12,
        FLOOR_PUFF_ARGS         = 0x20010400, // Size base 1024, palette row 1, slow drift
    };
/// Draws this task's falling particle at its current age, or releases it on expiry.
///
/// Captures `task`, `work`, `drawAge`, `fadeRgb` and the local fade constants.
/// Age narrows to a signed halfword. The borrowed RGB bytes last through the
/// draw call. Expands to a block; evaluates each captured pointer repeatedly.
#define DRYFIELD_NIGHT_MOTEL_BALCONY_DRAW_OR_RETIRE_FALLING_PARTICLE()               \
    {                                                                                \
        drawAge = work->age;                                                         \
        if (drawAge < FADE_START) {                                                  \
            _dryfieldNightMotelBalconyDrawFallingParticle(task, NULL, 0);            \
        } else if (drawAge < LIFETIME) {                                             \
            fadeRgb[0] = fadeRgb[1] = fadeRgb[2] = (LIFETIME - drawAge) * FADE_STEP; \
            _dryfieldNightMotelBalconyDrawFallingParticle(task, fadeRgb, 0);         \
        } else {                                                                     \
            effectKillTask(work, task);                                              \
        }                                                                            \
    }

    EffectWork* work;
    GfxCoord*   coord;
    s16         effectControl;
    u16         previousAge;
    s16         drawAge;
    u8          fadeRgb[3];

    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    // Draw the composed starting position; movement dirties the next cache.
    actorRenderComposeCoord(coord);
    previousAge = work->age;
    work->age   = previousAge + 1;
    switch (task->state) {
        case NEW:
            gfxSetRotIdentity(&coord->coord);
            work->pos.vx = task->spawnArg1.halves.low & SIZE_MASK;
            work->scale  = INITIAL_SCALE;
            RANDOM_LCG_ADVANCE();
            work->index = RANDOM_LCG_DRAW() & START_FRAME_MASK;
            RANDOM_LCG_ADVANCE();
            work->move.vy = RANDOM_LCG_DRAW() & INITIAL_FALL_SPEED_MASK;
            task->state   = FALLING;
            break;
        case FALLING:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->index++;
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (coord->coord.t[1] > 0) {
                    // Crossing local Y=0 stops descent and emits one slow floor puff.
                    if (work->age < LIFETIME) {
                        effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, work->pos.vx + FLOOR_PUFF_ARGS, NULL);
                    }
                    task->state = SETTLED;
                } else if (work->scale > 0) {
                    work->move.vy += GRAVITY_PER_TICK;
                }
            } else {
                work->age = previousAge;
            }
            DRYFIELD_NIGHT_MOTEL_BALCONY_DRAW_OR_RETIRE_FALLING_PARTICLE();
            break;
        case SETTLED:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = previousAge;
            }
            DRYFIELD_NIGHT_MOTEL_BALCONY_DRAW_OR_RETIRE_FALLING_PARTICLE();
            break;
    }
}

#undef DRYFIELD_NIGHT_MOTEL_BALCONY_DRAW_OR_RETIRE_FALLING_PARTICLE

/// Draws the falling particle's six-frame billboard, optionally tinted.
///
/// Requires a live `EffectWork` and composed coordinate body. `index` wraps across
/// six 40-texel cells; `pos.vx` * 39 / (SZ3 / 4) is the half-side in screen pixels.
/// `tintRgb` borrows three RGB bytes, enabling additive semitransparency; NULL
/// draws the raw opaque texture. `unusedDrawArg` is ignored (callers supply 0).
/// Uses and releases one `EffectCentreScratch` block, changes GTE state and queues
/// one `POLY_FT4` when projection succeeds. No caller pointers are retained.
static void _dryfieldNightMotelBalconyDrawFallingParticle(const Task* task, const u8* tintRgb, s32 unusedDrawArg)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_FRAME_COUNT = 6,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_CELL_SIZE   = 40,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_UV_SPAN     = DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_CELL_SIZE - 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_TOP_V       = 0x40,
    };
    const EffectWork*    work;
    const GfxCoord*      coord;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    const DisplayState*  display;
    s16                  screenEdge;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    // Project the composed view-space centre before constructing screen-space corners.
    _dryfieldNightMotelBalconyProjectParticleCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        if (tintRgb != NULL) {
            quad->r0 = tintRgb[0];
            quad->g0 = tintRgb[1];
            quad->b0 = tintRgb[2];
            setSemiTrans(quad, 1);
        } else {
            setShadeTex(quad, 1);
        }
        quad->tpage              = getTPage(0, GPU_BLEND_ADD, 768, 0);
        quad->clut               = getClut(48, 271);
        quad->u0                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_FRAME_COUNT * DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_CELL_SIZE;
        quad->v0                 = DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_TOP_V;
        quad->u1                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_FRAME_COUNT * DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_UV_SPAN;
        quad->v1                 = DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_TOP_V;
        quad->u2                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_FRAME_COUNT * DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_CELL_SIZE;
        quad->v2                 = DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_TOP_V + DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_UV_SPAN;
        quad->u3                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_FRAME_COUNT * DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_UV_SPAN;
        quad->v3                 = DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_TOP_V + DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_UV_SPAN;
        projection->screenExtent = work->pos.vx * DRYFIELD_NIGHT_MOTEL_BALCONY_FALLING_UV_SPAN / projection->depth;
        screenEdge               = projection->screenX - projection->screenExtent;
        quad->x2                 = screenEdge;
        quad->x0                 = screenEdge;
        screenEdge               = projection->screenX + projection->screenExtent;
        quad->x3                 = screenEdge;
        quad->x1                 = screenEdge;
        screenEdge               = projection->screenY - projection->screenExtent;
        quad->y1                 = screenEdge;
        quad->y0                 = screenEdge;
        screenEdge               = projection->screenY + projection->screenExtent;
        quad->y3                 = screenEdge;
        quad->y2                 = screenEdge;
        display                  = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(projection->depth, display->otDepthShift),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Chooses a lamp particle's signed-halfword offset with three ordered LCG draws.
///
/// Writes XYZ in -255..256 local coordinate units; retains the fourth halfword.
/// `effectSpawn` copies the offset, so this work can be reused for the next draw.
static inline void _dryfieldNightMotelBalconyChooseLampParticleOffset(EffectWork* work)
{
    enum { DRYFIELD_NIGHT_MOTEL_BALCONY_PARTICLE_OFFSET_RANDOM_MASK = 0x1FF };
    RANDOM_LCG_ADVANCE();
    work->move.vx = 0x100 - (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_PARTICLE_OFFSET_RANDOM_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vy = 0x100 - (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_PARTICLE_OFFSET_RANDOM_MASK);
    RANDOM_LCG_ADVANCE();
    work->move.vz = 0x100 - (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_PARTICLE_OFFSET_RANDOM_MASK);
}

void dryfieldNightMotelBalconyLampBurstTask(Task* task)
{
    enum {
        CONTINUOUS_VIEW           = 39,
        AFTERGLOW_LIFETIME        = 150,
        HIGH_PUFF_EMISSION_WINDOW = 120,
        DOWNWARD_DEBRIS_ARGS      = 0x80000100, // Size base 256; random texture row added
        RISING_PUFF_ARGS          = 0xA0000400, // Size base 1024, negative launch, slow drift
        AFTERGLOW_PUFF_ARGS       = 0x80000400, // Size base 1024, negative launch
        HIGH_PUFF_ARGS            = 0x20010400, // Size base 1024, palette row 1, slow drift
        SIZE_RANDOM_MASK          = 0x1FF,
        FALLING_SIZE_BASE         = 0x100,
        HIGH_PUFF_HEIGHT          = 0xC00,
    };

    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         randomSize;
    s32         debrisArgs;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }
    // The close view emits continuously without advancing the afterglow age.
    if (gGameSession->location.view == CONTINUOUS_VIEW) {
        RANDOM_LCG_ADVANCE();
        if ((RANDOM_LCG_DRAW() & 3) == 0) {
            _dryfieldNightMotelBalconyChooseLampParticleOffset(work);
            RANDOM_LCG_ADVANCE();
            randomSize = RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK;
            RANDOM_LCG_ADVANCE();
            debrisArgs = ((RANDOM_LCG_DRAW() % 3) << 16) + DOWNWARD_DEBRIS_ARGS;
            effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, randomSize + debrisArgs, &work->move);
        }
        RANDOM_LCG_ADVANCE();
        if ((u16)(RANDOM_LCG_DRAW() % 3U) == 0) {
            _dryfieldNightMotelBalconyChooseLampParticleOffset(work);
            RANDOM_LCG_ADVANCE();
            effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_FALLING, coord, (RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK) + FALLING_SIZE_BASE, &work->move);
        }
        RANDOM_LCG_ADVANCE();
        if ((u16)(RANDOM_LCG_DRAW() % 7U) == 0) {
            RANDOM_LCG_ADVANCE();
            work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            work->move.vy = 0;
            RANDOM_LCG_ADVANCE();
            work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
            RANDOM_LCG_ADVANCE();
            effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, (RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK) | RISING_PUFF_ARGS, &work->move);
        }
    } else {
        // Away from the close view, two emission windows taper out before release.
        work->age++;
        if (work->age < AFTERGLOW_LIFETIME) {
            RANDOM_LCG_ADVANCE();
            if (work->age < (u16)(RANDOM_LCG_DRAW() % (u32)AFTERGLOW_LIFETIME)) {
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 3) == 0) {
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = RANDOM_LCG_DRAW() & 0x7FF;
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, (RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK) | AFTERGLOW_PUFF_ARGS,
                                &work->move);
                }
            }
            RANDOM_LCG_ADVANCE();
            if (work->age < (u16)(RANDOM_LCG_DRAW() % (u32)HIGH_PUFF_EMISSION_WINDOW)) {
                RANDOM_LCG_ADVANCE();
                if ((RANDOM_LCG_DRAW() & 3) == 0) {
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = 0x100 - (RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK);
                    work->move.vy = HIGH_PUFF_HEIGHT;
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = 0x100 - (RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK);
                    RANDOM_LCG_ADVANCE();
                    effectSpawn(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, (RANDOM_LCG_DRAW() & SIZE_RANDOM_MASK) | HIGH_PUFF_ARGS,
                                &work->move);
                }
            }
        } else {
            effectKillTask(work, task);
        }
    }
}

void dryfieldNightMotelBalconyDriftPuffTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_NEW         = 0,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_MOVING      = 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_SIZE_MASK   = 0xFFF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_ORIGIN_MASK = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_FRAME_COUNT = 12,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_SCATTER     = 0x40000000,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_SLOW        = 0x20000000,
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     localTransform;
    s32         driftRange;

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }

    // Draw the composed starting position; local movement dirties the next cache.
    actorRenderComposeCoord(coord);
    driftRange = 0x80;
    work->age++;

    switch (task->state) {
        case DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_NEW:
            // Normalize the chosen world-frame drift once, then retain displacement per frame.
            localTransform = &coord->coord;
            gfxSetRotIdentity(localTransform);
            work->pos.vx = (u16)task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_SIZE_MASK;
            work->scale  = driftRange;
            RANDOM_LCG_ADVANCE();
            work->pos.vy = (RANDOM_LCG_DRAW() & 3) + 1;
            RANDOM_LCG_ADVANCE();
            work->pos.vz = RANDOM_LCG_DRAW() & (ONE - 1);
            work->index  = 0;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_SCATTER) {
                    RANDOM_LCG_ADVANCE();
                    work->move.vx = driftRange - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vy = driftRange - (RANDOM_LCG_DRAW() & 0xFF);
                    RANDOM_LCG_ADVANCE();
                    work->move.vz = driftRange - (RANDOM_LCG_DRAW() & 0xFF);
                    work->scale   = 0x40;
                } else {
                    if (task->spawnArg1.value < 0) {
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = -(RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = 0x10 - (RANDOM_LCG_DRAW() & 0x1F);
                    } else {
                        RANDOM_LCG_ADVANCE();
                        work->move.vx = driftRange - (RANDOM_LCG_DRAW() & 0xFF);
                        RANDOM_LCG_ADVANCE();
                        work->move.vy = RANDOM_LCG_DRAW() & 0xF;
                        RANDOM_LCG_ADVANCE();
                        work->move.vz = driftRange - (RANDOM_LCG_DRAW() & 0xFF);
                    }
                    if (task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_SLOW) {
                        work->scale = 0x40;
                    }
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_MOVING;
            // Replace the packed spawn word with the palette-origin index (0..2).
            task->spawnArg1.value = (s16)(task->spawnArg1.value >> 16) & DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_ORIGIN_MASK;
            break;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_MOVING:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if (work->age % work->pos.vy == 0) {
                    work->index++;
                }
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->move.vy--;
            } else {
                work->age--;
            }
            if (work->index < DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_FRAME_COUNT) {
                _dryfieldNightMotelBalconyDrawDriftPuff(task, task->spawnArg1.value);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Draws a centred drifting puff with its frame's palette, using additive blending.
///
/// Requires a live `EffectWork` and composed coordinate body. `clutOriginIndex`
/// selects palette-origin row 0..2; `index` must be 0..11 and selects a 48-texel
/// cell and a palette 16 VRAM words further along the row per frame. The raw
/// texture supplies colour. `pos.vx` * 47 / (SZ3 / 4 + 1) gives the half-side in
/// screen pixels. Uses and releases one `EffectCentreScratch` block, changes GTE
/// state and queues one `POLY_FT4` when projection succeeds; retains no pointers.
static void _dryfieldNightMotelBalconyDrawDriftPuff(const Task* task, s32 clutOriginIndex)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS    = 5,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE  = 48,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_UV_SPAN    = DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE - 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_TOP_V      = 0x68,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CLUT_WORDS = 16,
    };
    const EffectWork*    work;
    const GfxCoord*      coord;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    const DisplayState*  display;
    const _ClutOrigin*   selectedOrigin;
    s16                  screenEdge;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    // Project the composed view-space centre before constructing screen-space corners.
    _dryfieldNightMotelBalconyProjectParticleCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        projection->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setShadeTex(quad, 1);
        setSemiTrans(quad, 1);
        quad->tpage              = getTPage(0, GPU_BLEND_ADD, 768, 0);
        selectedOrigin           = &_gDryfieldNightMotelBalconyDriftPuffClutOrigins[clutOriginIndex];
        quad->clut               = getClut(selectedOrigin->x + work->index * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CLUT_WORDS, selectedOrigin->y);
        quad->u0                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE;
        quad->v0                 = work->index / DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_TOP_V;
        quad->u1                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_UV_SPAN;
        quad->v1                 = work->index / DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_TOP_V;
        quad->u2                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE;
        quad->v2                 = work->index / DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_TOP_V + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_UV_SPAN;
        quad->u3                 = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_UV_SPAN;
        quad->v3                 = work->index / DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_CELL_SIZE + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_TOP_V + DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_UV_SPAN;
        projection->screenExtent = work->pos.vx * DRYFIELD_NIGHT_MOTEL_BALCONY_PUFF_UV_SPAN / projection->depth;
        screenEdge               = projection->screenX - (u16)projection->screenExtent;
        quad->x2                 = screenEdge;
        quad->x0                 = screenEdge;
        screenEdge               = projection->screenX + (u16)projection->screenExtent;
        quad->x3                 = screenEdge;
        quad->x1                 = screenEdge;
        screenEdge               = projection->screenY - (u16)projection->screenExtent;
        quad->y1                 = screenEdge;
        quad->y0                 = screenEdge;
        screenEdge               = projection->screenY + (u16)projection->screenExtent;
        quad->y3                 = screenEdge;
        quad->y2                 = screenEdge;
        display                  = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(projection->depth, display->otDepthShift),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void dryfieldNightMotelBalconyFlameTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_NEW         = 0,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_MOVING      = 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_SIZE_MASK   = 0xFFF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FAST        = 0x40000000,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FADE_FRAMES = 10,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FADE_STEP   = 12,
    };
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     localTransform;
    s32         randomState;
    s16         ageFrames;
    s16         lifetimeFrames;
    u8          fadeRgb[3];

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }

    // Draw the composed starting position; local movement dirties the next cache.
    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->state) {
        case DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_NEW:
            // Randomize the sprite phase and lifetime before selecting and scaling drift.
            randomState    = RANDOM_LCG_NEXT(gRandomLcgState);
            localTransform = &coord->coord;
            gfxSetRotIdentity(localTransform);
            work->pos.vx    = task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_SIZE_MASK;
            gRandomLcgState = randomState;
            work->index     = RANDOM_LCG_DRAW() % 10;
            RANDOM_LCG_ADVANCE();
            work->angle = RANDOM_LCG_DRAW() % 10 + 5;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                RANDOM_LCG_ADVANCE();
                work->move.vx = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                RANDOM_LCG_ADVANCE();
                work->move.vy = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                RANDOM_LCG_ADVANCE();
                work->move.vz = 0x80 - (RANDOM_LCG_DRAW() & 0xFF);
                if (task->spawnArg1.value < 0) {
                    work->scale = 8;
                } else if (task->spawnArg1.value & DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FAST) {
                    work->scale = 0x80;
                } else {
                    work->scale = 0x20;
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_MOVING;
            break;
        case DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_MOVING:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->index++;
                work->move.vy--;
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            } else {
                work->age--;
            }
            ageFrames      = work->age;
            lifetimeFrames = work->angle;
            if (ageFrames < lifetimeFrames - DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FADE_FRAMES) {
                _dryfieldNightMotelBalconyDrawFlame(task, NULL, ageFrames);
            } else if (ageFrames < lifetimeFrames) {
                fadeRgb[0] = fadeRgb[1] = fadeRgb[2] = (lifetimeFrames - ageFrames) * DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FADE_STEP;
                _dryfieldNightMotelBalconyDrawFlame(task, fadeRgb, ageFrames);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Draws an additive flame billboard with three quarters above its anchor.
///
/// Requires a live `EffectWork` and composed coordinate body. `index` wraps across
/// ten 48-texel cells and selects the matching 16-colour palette. `pos.vx` * 47 /
/// (SZ3 / 4) gives the half-width in screen pixels; integer halving sets the
/// vertical extents in a 3:1 ratio. `tintRgb` borrows three RGB bytes, or NULL
/// selects raw texture colour; both modes blend additively. `unusedAge` is ignored.
/// Uses and releases one `EffectCentreScratch` block, changes GTE state and queues
/// one `POLY_FT4` when projection succeeds. No caller pointers are retained.
static void _dryfieldNightMotelBalconyDrawFlame(const Task* task, const u8* tintRgb, s16 unusedAge)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FRAME_COUNT = 10,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_COLUMNS     = 5,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_CELL_SIZE   = 48,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_UV_SPAN     = DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_CELL_SIZE - 1,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_TOP_V       = 0x28,
        DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_CLUT_WORDS  = 16,
    };
    const EffectWork*    work = task->spawnArg2.pointer;
    const GfxCoord*      coord;
    EffectCentreScratch* projection;
    POLY_FT4*            quad;
    const DisplayState*  display;
    s16                  frame;
    s32                  leftU;
    s32                  rightU;
    s32                  topV;
    s32                  bottomV;
    s16                  screenEdge;

    frame = work->index % DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_FRAME_COUNT;
    coord = task->extra.coordBody->coord;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    projection = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    // Project the composed view-space centre before constructing screen-space corners.
    _dryfieldNightMotelBalconyProjectParticleCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        if (tintRgb != NULL) {
            quad->r0 = tintRgb[0];
            quad->g0 = tintRgb[1];
            quad->b0 = tintRgb[2];
        } else {
            setShadeTex(quad, 1);
        }
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(frame * DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_CLUT_WORDS + 0x40, 0x10E);
        setSemiTrans(quad, 1);
        leftU                    = frame % DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_CELL_SIZE;
        topV                     = frame / DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_COLUMNS * DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_CELL_SIZE;
        rightU                   = leftU + DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_UV_SPAN;
        bottomV                  = topV + DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_TOP_V + DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_UV_SPAN;
        topV                     = topV + DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_TOP_V;
        quad->u0                 = leftU;
        quad->v0                 = topV;
        quad->u1                 = rightU;
        quad->v1                 = topV;
        quad->u2                 = leftU;
        quad->v2                 = bottomV;
        quad->u3                 = rightU;
        quad->v3                 = bottomV;
        projection->screenExtent = work->pos.vx * DRYFIELD_NIGHT_MOTEL_BALCONY_FLAME_UV_SPAN / projection->depth;
        screenEdge               = projection->screenX - projection->screenExtent;
        quad->x2                 = screenEdge;
        quad->x0                 = screenEdge;
        screenEdge               = projection->screenX + projection->screenExtent;
        quad->x3                 = screenEdge;
        quad->x1                 = screenEdge;
        // Keep three quarters of the billboard above its projected anchor.
        projection->screenExtent >>= 1;
        screenEdge                 = projection->screenY - projection->screenExtent * 3;
        quad->y1                   = screenEdge;
        quad->y0                   = screenEdge;
        screenEdge                 = projection->screenY + projection->screenExtent;
        quad->y3                   = screenEdge;
        quad->y2                   = screenEdge;
        display                    = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH_SCALED(projection->depth, display->otDepthShift),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void dryfieldNightMotelBalconySpawnFlameBurst(void)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_MODEL_PART  = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_FLAME_COUNT = 8,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SMOKE_COUNT = 6,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SIZE_MASK   = 0x1FF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SIZE_BASE   = 0x300,
        DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SMOKE_ARGS  = 0xC0033800,
    };
    Task*     sceneTask;
    GfxCoord* emissionCoord;
    SVECTOR   spawnOffset;
    s32       particleIndex;

    sceneTask     = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
    emissionCoord = sceneTask->firstChild->extra.tmd->coords + DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_MODEL_PART;

    for (particleIndex = 0; particleIndex < DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_FLAME_COUNT; particleIndex++) {
        DRYFIELD_NIGHT_MOTEL_BALCONY_CHOOSE_FLAME_BURST_OFFSET(&spawnOffset);
        RANDOM_LCG_ADVANCE();
        effectSpawn(EFFECT_NIGHT_MOTEL_BALCONY_FLAME, emissionCoord, (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SIZE_MASK) + DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SIZE_BASE, &spawnOffset);
    }

    for (particleIndex = 0; particleIndex < DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SMOKE_COUNT; particleIndex++) {
        DRYFIELD_NIGHT_MOTEL_BALCONY_CHOOSE_FLAME_BURST_OFFSET(&spawnOffset);
        effectSpawn(EFFECT_SMOKE_PUFF, emissionCoord, DRYFIELD_NIGHT_MOTEL_BALCONY_BURST_SMOKE_ARGS, &spawnOffset);
    }
}

void dryfieldNightMotelBalconyTrySpawnFlame(void)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_MODEL_PART     = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_CHANCE_DIVISOR = 3,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_SIZE_MASK      = 0x1FF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_SLOW_SIZE_BASE = 0x80000100,
    };
    Task* sceneTask;

    RANDOM_LCG_ADVANCE();
    if ((u16)(RANDOM_LCG_DRAW() % (u32)DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_CHANCE_DIVISOR) == 0) {
        sceneTask = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
        RANDOM_LCG_ADVANCE();
        // Bit 31 selects slow drift; the random bits alter only the size.
        effectSpawn(EFFECT_NIGHT_MOTEL_BALCONY_FLAME, sceneTask->firstChild->extra.tmd->coords + DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_MODEL_PART,
                    (RANDOM_LCG_DRAW() & DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_SIZE_MASK) + DRYFIELD_NIGHT_MOTEL_BALCONY_PERIODIC_FLAME_SLOW_SIZE_BASE,
                    &_gDryfieldNightMotelBalconyPeriodicFlameOffset);
    }
}
