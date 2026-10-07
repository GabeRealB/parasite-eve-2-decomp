#include "rooms/acropolis_security_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/item_menu.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/action_prompt.h"
#include "../../shared/actor_contacts.h"

/// Hotspot ids of the buttons that step the monitor's grey wash.
///
/// Brighter adds one detent to `screenLevel` and darker subtracts one. Both
/// are negative as signed hotspot ids, so confirming one does not change the
/// saved camera view. The handler compares these same bits as unsigned.
enum {
    ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_BRIGHTER = 0x8000,
    ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_DARKER   = 0x8001,
};

/// Encoding of `_AcropolisSecurityRoomMonitorWork::screenLevel`.
///
/// The five detents in `D_acropolis_security_room_801826B4` are the darkest
/// level plus a multiple of the step. The panel drawer subtracts the bias; a
/// negative result is a subtractive blend and a non-negative one an additive
/// blend. A step brighter is taken only when the prospective level is below
/// the next-step limit, which admits the brightest detent and rejects one
/// step past it. Scripted captions stay quiet on the darkest detent.
enum {
    ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS       = 0x7F,
    ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP       = 0x3E,
    ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST    = 4,
    ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_NEXT_LIMIT = 0xFE,
};

/// Camera view of the enemy seen on the monitor.
///
/// Selecting it while bit 1 of `GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN` is
/// clear arms `enemySceneArmed`. The caption starts only once the wash has
/// left the darkest detent.
enum {
    ACROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY = 0xA,
};

/// Frames in one pass of the monitor's overlay bar. The timer wraps to 0.
enum {
    ACROPOLIS_SECURITY_ROOM_MONITOR_SWEEP_PERIOD = 0x97,
};

/// CAP slot started once for the enemy's camera view.
enum {
    ACROPOLIS_SECURITY_ROOM_MONITOR_ENEMY_CAP_SLOT = 0xC,
};

/// Work block of the security-monitor task, held in `Task::work`.
///
/// The task allocates the block zeroed when it starts. `screenLevel` is the
/// grey wash on the monitor picture, one of five detents restored from
/// `GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA`. `sweepTimer` counts the frames of
/// the overlay bar. `hotspotId` is the hotspot the player confirmed, a camera
/// view or a wash button, and `promptKind` is the first row of the prompt
/// opened for it. `enemySceneArmed` and `enemyCaptionStarted` latch the
/// one-shot caption on the enemy's camera view. The block is 0xA bytes; the
/// byte after `enemyCaptionStarted` is the alignment tail and is never accessed.
typedef struct {
    u16 screenLevel;         // Wash detent: grey level plus ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS, stored unsigned
    s16 sweepTimer;          // Frames into the overlay bar's current pass
    s16 hotspotId;           // Confirmed hotspot (a camera view, or ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_*)
    s8  promptKind;          // First row of the prompt opened for that hotspot (0 "Examine", 1 "Push")
    s8  enemySceneArmed;     // 1 once the enemy's view was selected while its scene had not played
    s8  enemyCaptionStarted; // 1 once that visit's enemy caption has been started
} _AcropolisSecurityRoomMonitorWork;
STATIC_ASSERT_SIZEOF(_AcropolisSecurityRoomMonitorWork, 0xA);

/// `_AcropolisSecurityRoomPowerSupplyWork::usedKey` values.
enum {
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE  = 0, // No key item, or one the panel refuses
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT  = 1, // Item 0x104, which releases the left lock
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT = 2, // Item 0x103, which releases the right lock
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_OTHER = 3, // Item 0x101, accepted but releasing neither lock
};

/// `_AcropolisSecurityRoomPowerSupplyWork::hotspotId` values: the two locks of
/// the panel, as the ids of their hotspots.
enum {
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_LEFT  = 1,
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_RIGHT = 2,
};

/// Work block of the room's power-supply panel task, held in `Task::work`.
///
/// The panel is a close-up view with one hotspot per lock. Confirming a
/// hotspot opens the prompt that offers to examine it or to use a key item on
/// it; the answer runs that lock's caption, or, given the lock's own key,
/// releases the lock, fades the screen out and plays the unlock scene. The
/// task allocates the block zeroed when it starts.
typedef struct {
    s32   usedKey;    // Key item used from the prompt, cleared once acted on (ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_*)
    Task* sceneTask;  // Task playing the unlock scene, awaited until it stops
    u16   timer;      // Fade level while the screen fades out (0 to 0x100, 4 a frame), then frames since the view changed
    s16   hotspotId;  // Hotspot the player confirmed (ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_*)
    s8    promptKind; // First row of the prompt opened for that hotspot (0 "Examine", 1 "Push")
} _AcropolisSecurityRoomPowerSupplyWork;
STATIC_ASSERT_SIZEOF(_AcropolisSecurityRoomPowerSupplyWork, 0x10);

/// Work block of the task that sounds the movie loop over one of the
/// power-supply panel's two unlock scenes, held in `Task::work`.
///
/// The task starts `SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP` and fades it out
/// once: when the scene's movie reaches the cue frame, or, if it has not got
/// that far, when the player skips the scene. The block holds the latch that
/// keeps the fade from being queued twice. The task allocates the block zeroed
/// when it starts; the allocation and its clear are both four bytes long.
typedef struct {
    u16  fadeStarted; // Whether the loop's fade-out was queued at the cue frame (0 no, 1 yes)
    byte field_2[2];  // Allocated and cleared with the block but never accessed; role unproven
} _AcropolisSecurityRoomMovieLoopWork;
STATIC_ASSERT_SIZEOF(_AcropolisSecurityRoomMovieLoopWork, 0x4);

/// Where one camera feed's picture sits, in the texture page and on screen,
/// in the one camera view that draws the feeds as pictures.
///
/// The four pictures share one 8-bit texture page, a 128x128 quadrant each,
/// and differ in palette: each is drawn with its own feed's blended CLUT, which
/// is what brightens a feed as it comes on. A picture is a 128x128 quad, 0x40
/// to the left of and above its centre and 0x3F to the right and below. Only
/// the low byte of `u` and of `v` is ever read, so their signedness is
/// unproven; the high bytes are zero in every entry.
typedef struct {
    u16 clutY;   // VRAM row of the feed's 256-colour CLUT, which starts at X 0
    u16 u;       // Left texel column of the feed's quadrant of the page (0 or 128)
    u16 v;       // Top texel row of that quadrant (0 or 128)
    s16 centreX; // Centre of the picture on screen, in primitive coordinates
    s16 centreY;
} _AcropolisSecurityRoomMonitorFeedQuad;
STATIC_ASSERT_SIZEOF(_AcropolisSecurityRoomMonitorFeedQuad, 0xA);

/// Scratch-stack block of the sweep line the monitor-feed task draws in the
/// frame of its coordinate.
///
/// The line is a segment along that frame's X axis which travels along Y as
/// the frame counter advances, so its two endpoints differ only in X. Each
/// endpoint is written in the coordinate's frame, turned into a world position
/// in place, and then projected onto one vertex of a flat line primitive. The
/// world positions are 16-bit: only the low half of the coordinate's world
/// translation is added to the rotated point.
///
/// Reserve one whole block and release it once the primitive is linked;
/// nothing in it outlives the draw.
typedef struct {
    s32     depth; // SZ3 / 4 from projecting `end`: the near-cull test, the ordering-table depth and the blend-mode depth
    SVECTOR start; // Endpoint at the lower X, projected onto the primitive's vertex 0
    SVECTOR end;   // Endpoint at the higher X, projected onto vertex 1
} _AcropolisSecurityRoomSweepLineScratch;
STATIC_ASSERT_SIZEOF(_AcropolisSecurityRoomSweepLineScratch, 0x14);

/// The tasks `func_acropolis_security_room_8017D77C` and
/// `func_acropolis_security_room_8017D834` spawn and poll until they end;
/// message 0x13F1 is forwarded to the second while it is alive.
extern Task* D_acropolis_security_room_801855A8;
extern Task* D_acropolis_security_room_801855AC;

/// The whole-unit world displacement the last `ActorContact_PushContact`
/// call produced.
extern SVECTOR ActorContact_ScratchPosition;

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// The room task's message table, installed in `Task::msgTable`.
extern TaskMessageEntry D_acropolis_security_room_801825DC[];
extern TaskDesc         D_acropolis_security_room_80182618[];
extern TaskDesc         D_acropolis_security_room_8018263C;

/// The security monitor's own hotspot table, hit-tested by
/// `actionPromptHitTest`.
extern ActionPromptHotspot D_acropolis_security_room_80182648[];

/// The monitor's grey-wash detents, darkest first. The first five are the
/// levels `GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA` indexes. The sixth value
/// is part of this symbol and is never read; its role is unproven.
extern s16 D_acropolis_security_room_801826B4[];

/// The single-entry `TaskDesc` table the script spawns its child task from:
/// `func_acropolis_security_room_8017F9C8`.
extern TaskDesc D_acropolis_security_room_801826C0[];
/// The script's message table, parked in `Task::msgTable`.
extern TaskMessageEntry D_acropolis_security_room_801826CC[];

/// The script's hotspot table, terminated by `ACTION_PROMPT_HOTSPOT_END`.
extern ActionPromptHotspot D_acropolis_security_room_801826DC[];

/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `func_acropolis_security_room_80180368`, index 1 is
/// `func_acropolis_security_room_801804CC`.
extern TaskDesc D_acropolis_security_room_80182700[];

/// The 0x100-entry RGB555 palette every monitor-screen CLUT is blended
/// towards: the "off" colours of the four security-camera feeds.
extern u16 D_acropolis_security_room_80182718[];
/// The four lit palettes, one per camera feed, blended against
/// `D_acropolis_security_room_80182718` by the feed's own brightness.
extern u16 D_acropolis_security_room_80182918[];
extern u16 D_acropolis_security_room_80182B18[];
extern u16 D_acropolis_security_room_80182D18[];
extern u16 D_acropolis_security_room_80182F18[];
/// The four blend results, one 256-colour RGB555 CLUT row per camera feed.
///
/// Each is written only as colours; `D_acropolis_security_room_80183918`
/// borrows it as packed words because that is the form the GPU upload takes.
extern u16 D_acropolis_security_room_80183118[256];
extern u16 D_acropolis_security_room_80183318[256];
extern u16 D_acropolis_security_room_80183518[256];
extern u16 D_acropolis_security_room_80183718[256];
/// The upload records for the four blended CLUTs above.
extern GpuImageUpload D_acropolis_security_room_80183918[];
/// Camera-lit bitmask for each value of `gameFlagGetNibble(9)`; bit N is set
/// while feed N is showing something.
extern u16 D_acropolis_security_room_80183968[];

/// The picture placement of each of the four camera feeds, indexed by feed.
extern _AcropolisSecurityRoomMonitorFeedQuad D_acropolis_security_room_80183970[];

/// Spawn position and effect id of the flash each newly lit feed plays,
/// indexed by feed.
extern SVECTOR D_acropolis_security_room_80183998[];
extern s16     D_acropolis_security_room_801839B8[];

extern EffectUnitQuadCorner D_acropolis_security_room_801839C0[];

/// 0xFF-terminated area-record lists applied as the script ends.
extern AreaApplyRec D_acropolis_security_room_80184F50[];
extern AreaApplyRec D_acropolis_security_room_80184F78[];
extern AreaApplyRec D_acropolis_security_room_80184F7C[];
extern AreaApplyRec D_acropolis_security_room_80184F80[];

/// Initializes one reserved prompt edge from signed screen coordinates and RGB bytes.
///
/// The packet must be writable. Arguments must be side-effect-free; the packet
/// is evaluated repeatedly. Expands to standalone statements without linking.
#define ACROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, startX, startY, endX, endY, red, green, blue) \
    setLineF2(line);                                                                                 \
    (line)->x0 = (startX);                                                                           \
    (line)->y0 = (startY);                                                                           \
    (line)->x1 = (endX);                                                                             \
    (line)->y1 = (endY);                                                                             \
    (line)->r0 = (red);                                                                              \
    (line)->g0 = (green);                                                                            \
    (line)->b0 = (blue);

/// Initializes a reserved semitransparent grey quad over the monitor picture.
///
/// Requires the caller's ACROPOLIS_SECURITY_ROOM_MONITOR_* geometry constants.
/// The packet must be writable and both arguments side-effect-free; each is
/// evaluated repeatedly. Expands to standalone statements without linking.
#define ACROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH(quad, intensity) \
    setPolyF4(quad);                                               \
    setSemiTrans(quad, true);                                      \
    (quad)->r0 = (intensity);                                      \
    (quad)->g0 = (intensity);                                      \
    (quad)->b0 = (intensity);                                      \
    (quad)->x0 = ACROPOLIS_SECURITY_ROOM_MONITOR_LEFT;             \
    (quad)->y0 = ACROPOLIS_SECURITY_ROOM_MONITOR_TOP;              \
    (quad)->x1 = ACROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;            \
    (quad)->y1 = ACROPOLIS_SECURITY_ROOM_MONITOR_TOP;              \
    (quad)->x2 = ACROPOLIS_SECURITY_ROOM_MONITOR_LEFT;             \
    (quad)->y2 = ACROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;           \
    (quad)->x3 = ACROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;            \
    (quad)->y3 = ACROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;

/// Converts a sweep endpoint from the task's local frame to 16-bit projection-input coordinates.
///
/// `endpoint` is the member token start or end; line is a live scratch block,
/// and coord has a composed workm. Pointer arguments are evaluated repeatedly
/// and must be side-effect-free. Changes GTE rotation/V0/IR state and expands
/// to standalone statements, retaining XYZ translation order.
#define ACROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT(line, endpoint, coord) \
    gte_SetRotMatrix(&(coord)->workm);                                          \
    gte_ldv0(&(line)->endpoint);                                                \
    gte_rtv0();                                                                 \
    gte_stsv(&(line)->endpoint);                                                \
    (line)->endpoint.vx += (coord)->workm.t[0];                                 \
    (line)->endpoint.vy += (coord)->workm.t[1];                                 \
    (line)->endpoint.vz += (coord)->workm.t[2];

/// Seeds the falling quad's tumble rates and parent-space displacement per frame.
///
/// `work` is borrowed, writable effect state. `period` receives the X tumble
/// rate (-240..256) and `step` the Z rate (-112..128), in multiples of 16
/// angle units per frame; one turn is 4096 units. Each `move` component
/// receives -15..16 parent-coordinate units per frame.
/// Advances the shared random sequence exactly five times, ordered X/Z
/// tumble then X/Y/Z displacement. Other fields, including `move.pad`, stay
/// intact; this call does not allocate or retain the work block.
static inline void _acropolisSecurityRoomSeedQuadMotion(EffectWork* work)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_BIAS = 256,
        ACROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_MASK = 0x1F0,
        ACROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_BIAS = 128,
        ACROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_MASK = 0xF0,
        ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS     = 16,
        ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK     = 0x1F,
    };
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->period    = ACROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_BIAS - ((gRandomLcgState >> 16) & ACROPOLIS_SECURITY_ROOM_QUAD_X_TUMBLE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->step      = ACROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_BIAS - ((gRandomLcgState >> 16) & ACROPOLIS_SECURITY_ROOM_QUAD_Z_TUMBLE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vx   = ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS - ((gRandomLcgState >> 16) & ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vy   = ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS - ((gRandomLcgState >> 16) & ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->move.vz   = ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_BIAS - ((gRandomLcgState >> 16) & ACROPOLIS_SECURITY_ROOM_QUAD_MOVE_MASK);
}

/// Places one corner of the falling quad's local XZ square in its composed coordinate space.
///
/// `cornerIndex` is 0..3 in GPU quad strip order; `work->scale` is the signed
/// half-side in coordinate units (the task supplies 32). `quadScratch` must
/// be a live, word-aligned block, and `coord->workm` must already be composed
/// with 12-fractional-bit rotation.
/// Local products narrow to signed 16 bits, rotation stores the GTE's signed
/// IR results, and unsigned translation sums retain their low 16 bits.
/// Only the selected vertex's XYZ changes; its fourth halfword stays intact.
/// Loads GTE rotation/V0 and overwrites IR/FLAG, without loading translation
/// or projecting. All pointers are borrowed for this call.
static inline void _acropolisSecurityRoomTransformQuadCorner(EffectQuadCornersScratch* quadScratch, s32 cornerIndex, const EffectWork* work, const GfxCoord* coord)
{
    quadScratch->vertices[cornerIndex].vx     = D_acropolis_security_room_801839C0[cornerIndex].axis0Sign * work->scale;
    (&quadScratch->vertices[cornerIndex])->vy = 0;
    (&quadScratch->vertices[cornerIndex])->vz = D_acropolis_security_room_801839C0[cornerIndex].axis1Sign * work->scale;
    // Rotate the local corner before adding the composition-root translation.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&quadScratch->vertices[cornerIndex]);
    gte_rtv0();
    gte_stsv(&quadScratch->vertices[cornerIndex]);
    quadScratch->vertices[cornerIndex].vx     += (u32)coord->workm.t[0];
    (&quadScratch->vertices[cornerIndex])->vy += (u32)coord->workm.t[1];
    (&quadScratch->vertices[cornerIndex])->vz += (u32)coord->workm.t[2];
}

/// Initializes one monitor-glow wedge's Gouraud quad with a lit centre and black outer vertices.
///
/// `quad` is a borrowed, writable packet. `brightness` is 64..176 in steps
/// of 16; `redFactor` and `greenFactor` enable their channel with 0 or 1.
/// Vertex 2 is the lit centre; vertices 0, 1 and 3 are black, and blue is zero
/// throughout. Sets packet length/opcode and RGB bytes; the caller supplies
/// screen geometry, linkage and additive semitransparency. Does not allocate
/// or retain the packet.
static inline void _acropolisSecurityRoomInitGlowQuad(POLY_G4* quad, s16 brightness, s16 redFactor, s16 greenFactor)
{
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, brightness * redFactor, greenFactor * brightness, 0);
    setRGB3(quad, 0, 0, 0);
}

/// Initializes one monitor-glow streak's Gouraud polyline with a lit centre and black ends.
///
/// `line` is a borrowed, writable three-vertex packet. `brightness` is 64..176
/// in steps of 16; `redFactor` and `greenFactor` enable their channel with 0
/// or 1. Vertex 1 is the lit centre; vertices 0 and 2 are black, and blue is
/// zero throughout. Sets packet length/opcode, RGB bytes, the GPU polyline
/// terminator and cleared `p2` byte. The caller supplies screen geometry,
/// linkage and additive semitransparency. Does not allocate or retain the packet.
static inline void _acropolisSecurityRoomInitGlowLine(LINE_G3* line, s16 brightness, s16 redFactor, s16 greenFactor)
{
    setLineG3(line);
    setRGB0(line, 0, 0, 0);
    setRGB1(line, brightness * redFactor, greenFactor * brightness, 0);
    setRGB2(line, 0, 0, 0);
}

static void func_acropolis_security_room_8017D930(Task* task);
static void _acropolisSecurityRoomMessageIdle(Task* task);
static void func_acropolis_security_room_8017D9DC(Task* task);
static void func_acropolis_security_room_8017DB30(Task* task);
static void func_acropolis_security_room_8017DC7C(Task* task);
static void _acropolisSecurityRoomDrawMonitorWash(s16 washLevel);
static void func_acropolis_security_room_8017E37C(Task* task);
static void func_acropolis_security_room_8017EA28(Task* task);
static void func_acropolis_security_room_8017EA5C(Task* task);
static void func_acropolis_security_room_8017EADC(Task* task);
static void func_acropolis_security_room_8017EB9C(Task* task);
static void func_acropolis_security_room_8017EE44(Task* task);
static void func_acropolis_security_room_8017F480(Task* task);
static void _actionPromptDrawCursor(s32 cursorX, s32 cursorY, s32 cursorMode);
static void func_acropolis_security_room_8017FA18(Task* task);
static void func_acropolis_security_room_8017FB20(Task* task);
static void func_acropolis_security_room_8017FB54(Task* task);
static void func_acropolis_security_room_8017FBA4(Task* task);
static void func_acropolis_security_room_8017FC30(Task* task);
static s32  func_acropolis_security_room_8017FCB0(ActionPromptHotspot* table, s16 x, s16 y);
static void _acropolisSecurityRoomShowReleasedLocks(s32 releasedLocks);
static void func_acropolis_security_room_8017FE6C(Task* task);
static void func_acropolis_security_room_8017FF0C(Task* task);
static void func_acropolis_security_room_8017FF84(Task* task);
static void func_acropolis_security_room_8017FFD0(Task* task);
static void _acropolisSecurityRoomPowerSupplyRestoreRoomView(Task* task);
static void func_acropolis_security_room_80180030(Task* task);
static void func_acropolis_security_room_801800A4(Task* task);
static void func_acropolis_security_room_8018014C(Task* task);
static void func_acropolis_security_room_801801C4(Task* task);
static void func_acropolis_security_room_80180218(Task* task);
static void func_acropolis_security_room_80180308(Task* task);
static void _acropolisSecurityRoomDrawSweepLine(Task* task);

void func_acropolis_security_room_8017E9D8(Task*);
void func_acropolis_security_room_8017F9C8(Task*);
s32  func_acropolis_security_room_8017FE24(Task*, s32, s32, s32);
void func_acropolis_security_room_80180368(Task*);
void func_acropolis_security_room_801804CC(Task*);

void func_acropolis_security_room_8017D77C(Task*);
void func_acropolis_security_room_8017D834(Task*);

extern WorldCollisionGrid    D_acropolis_security_room_80183D94[1];
extern WorldCollisionTrigger D_acropolis_security_room_80183DB8[4];
extern WorldCollisionTrigger D_acropolis_security_room_80183EE8[5];
extern WorldCoordRoomLights  D_acropolis_security_room_801841C8[1];

extern SpriteBatch  D_acropolis_security_room_801841E0[2];
extern SpriteBatch  D_acropolis_security_room_80184358[3];
extern SpriteBatch  D_acropolis_security_room_80184370[2];
extern SpriteBatch  D_acropolis_security_room_80184380[2];
extern SpriteBatch  D_acropolis_security_room_80184458[3];
extern SpriteBatch  D_acropolis_security_room_80184498[4];
extern SpriteBatch  D_acropolis_security_room_801844B8[2];
extern SpriteSource D_acropolis_security_room_801841F0[18];
extern SpriteSource D_acropolis_security_room_80184390[10];
extern SpriteSource D_acropolis_security_room_80184470[2];

static s32 _acropolisSecurityRoomResolveTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
s32        func_acropolis_security_room_8017D6D4(Task*, s32, s32, s32);
s32        func_acropolis_security_room_8017D708(Task*, s32, s32, s32);
s32        func_acropolis_security_room_8017D740(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3);

TaskMessageEntry D_acropolis_security_room_801825DC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisSecurityRoomResolveTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_security_room_8017D740 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_security_room_8017D708 },
    { 5105, func_acropolis_security_room_8017D6D4 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_acropolis_security_room_80182604 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

TaskDesc D_acropolis_security_room_80182618[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_security_room_8017D77C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_security_room_8017D834, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc D_acropolis_security_room_8018263C = { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_8017E9D8, { .value = 0 } };

ActionPromptHotspot D_acropolis_security_room_80182648[9] = {
    { -30, 81, 12, 11, 8, 1, 0 },
    { -7, 81, 12, 11, 9, 1, 0 },
    { 16, 81, 12, 11, ACROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY, 1, 0 },
    { 39, 81, 12, 11, 11, 1, 0 },
    { 61, 81, 12, 11, 12, 1, 0 },
    { 82, 81, 12, 11, 13, 1, 0 },
    { -141, -76, 12, 11, (s16)ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_BRIGHTER, 1, 0 },
    { -141, -58, 12, 11, (s16)ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_DARKER, 1, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

s16 D_acropolis_security_room_801826B4[6] = {
    4,
    66,
    128,
    190,
    252,
    8481,
};

TaskDesc D_acropolis_security_room_801826C0[1] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_8017F9C8, { .value = 0 } },
};

TaskMessageEntry D_acropolis_security_room_801826CC[2] = {
    { 5105, func_acropolis_security_room_8017FE24 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActionPromptHotspot D_acropolis_security_room_801826DC[3] = {
    { -68, 20, 24, 24, 1, 0, 0 },
    { 54, 20, 24, 24, 2, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

TaskDesc D_acropolis_security_room_80182700[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_80180368, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_801804CC, { .value = 0 } },
};

u16 D_acropolis_security_room_80182718[256] = { 0 };

u16 D_acropolis_security_room_80182918[256] = {
    0,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8421,
    0x8422,
    0x8422,
    0x8422,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8443,
    0x8443,
    0x8443,
    0x8443,
    0x8463,
    0x8463,
    0x8463,
    0x8463,
    0x8463,
    0x8463,
    0x8863,
    0x8863,
    0x8863,
    0x8463,
    0x8863,
    0x8484,
    0x8864,
    0x8864,
    0x8884,
    0x8884,
    0x8484,
    0x8884,
    0x8884,
    0x8884,
    0x8885,
    0x8885,
    0x8885,
    0x84A5,
    0x88A5,
    0x88A5,
    0x88A5,
    0x88A5,
    0x8CA5,
    0x84C6,
    0x8CA5,
    0x8CA6,
    0x8CA6,
    0x8CA6,
    0x8CC6,
    0x88C6,
    0x84C6,
    0x8CC6,
    0x8CC7,
    0x8CC7,
    0x84E7,
    0x8CC7,
    0x8CE6,
    0x8CE7,
    0x8CE7,
    0x8CE7,
    0x8CE7,
    0x8CE7,
    0x88E7,
    0x90E7,
    0x90E7,
    0x90E8,
    0x9108,
    0x9108,
    0x9108,
    0x8908,
    0x9108,
    0x9108,
    0x9108,
    0x8908,
    0x9108,
    0x9108,
    0x9128,
    0x9129,
    0x8929,
    0x9129,
    0x9129,
    0x9129,
    0x9129,
    0x8929,
    0x9529,
    0x9529,
    0x9529,
    0x954A,
    0x894A,
    0x954A,
    0x954A,
    0x954A,
    0x894A,
    0x954A,
    0x954A,
    0x954A,
    0x896B,
    0x954A,
    0x956B,
    0x956B,
    0x896B,
    0x956B,
    0x956B,
    0x996B,
    0x996B,
    0x8D8C,
    0x996B,
    0x996B,
    0x998C,
    0x8D8C,
    0x998C,
    0x998C,
    0x998C,
    0x8D8C,
    0x998C,
    0x998C,
    0x8DAD,
    0x998C,
    0x99AD,
    0x99AD,
    0x8DAD,
    0x99AD,
    0x99AD,
    0x8DCE,
    0x9DAD,
    0x9DAD,
    0x8DCE,
    0x9DAE,
    0x9DCE,
    0x8DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x8DEF,
    0x9DCE,
    0x9DEF,
    0x8DEF,
    0x9DEF,
    0x9DEF,
    0x9DEF,
    0x9DEF,
    0x9210,
    0xA210,
    0x9210,
    0x9231,
    0x9231,
    0xA231,
    0xA231,
    0x9252,
    0xA231,
    0x9252,
    0x9252,
    0x9E52,
    0x9273,
    0xA652,
    0x9673,
    0x9294,
    0x9694,
    0xAA73,
    0xAA94,
    0x9694,
    0xAA94,
    0x96B5,
    0xA2B5,
    0x96B5,
    0xAAB5,
    0x96D6,
    0xAEB5,
    0xA6D6,
    0x96D6,
    0xAED6,
    0x96F7,
    0xAEF7,
    0x9717,
    0xAEF7,
    0xAEF7,
    0xB2F7,
    0xB318,
    0x9B39,
    0xB318,
    0x9F39,
    0xB339,
    0xB739,
    0x9F5A,
    0xB75A,
    0x9F7B,
    0xB75A,
    0xA37B,
    0xB77A,
    0xB77B,
    0xA39C,
    0xBB9C,
    0xAB9C,
    0xBB9C,
    0xBBBC,
    0xBFBD,
    0xB3DE,
    0xBFDE,
    0xB3DE,
    0xC3FF,
    0xC3FF,
    0xCBFF,
    0xD3FF,
    0xDBFF,
    0xFFFF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80182B18[256] = {
    0,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8421,
    0x8421,
    0x8421,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8462,
    0x8462,
    0x8462,
    0x8862,
    0x8862,
    0x8862,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88C2,
    0x88C2,
    0x88C2,
    0x88C2,
    0x88C3,
    0x8CC3,
    0x8CC3,
    0x8CC3,
    0x8CC3,
    0x8CC3,
    0x88C3,
    0x8CC3,
    0x8CC3,
    0x8CE3,
    0x8CE3,
    0x8CE3,
    0x88E2,
    0x8CE3,
    0x8CE3,
    0x8902,
    0x8CE3,
    0x8CE3,
    0x8CE3,
    0x8903,
    0x8CE4,
    0x90E4,
    0x9104,
    0x8923,
    0x9104,
    0x9104,
    0x9104,
    0x9104,
    0x8924,
    0x9104,
    0x9104,
    0x9104,
    0x9124,
    0x8942,
    0x9124,
    0x9124,
    0x9124,
    0x9143,
    0x9124,
    0x8D42,
    0x9525,
    0x9545,
    0x8D62,
    0x9545,
    0x9545,
    0x9545,
    0x9545,
    0x8D82,
    0x9545,
    0x9545,
    0x9545,
    0x9545,
    0x9565,
    0x8D83,
    0x9565,
    0x9565,
    0x9565,
    0x9565,
    0x91A3,
    0x9565,
    0x9565,
    0x9566,
    0x9966,
    0x9986,
    0x9986,
    0x91A3,
    0x9986,
    0x9986,
    0x9986,
    0x9986,
    0x91C3,
    0x9986,
    0x9986,
    0x99A6,
    0x99A6,
    0x91E3,
    0x99A6,
    0x99A7,
    0x91E4,
    0x9DA7,
    0x9DC7,
    0x9DC7,
    0x9DC7,
    0x9204,
    0x9DC7,
    0x9DC7,
    0x9DC7,
    0x9624,
    0x9DC7,
    0x9DE7,
    0x9DE7,
    0x9624,
    0x9DE7,
    0x9DE8,
    0xA1E8,
    0x9644,
    0xA1E8,
    0xA208,
    0xA208,
    0x9644,
    0xA208,
    0xA208,
    0xA208,
    0x9664,
    0xA228,
    0xA228,
    0x9A65,
    0xA228,
    0x9A85,
    0xA649,
    0x9A85,
    0x9AA5,
    0xA649,
    0x9AA5,
    0xAA6A,
    0x9AC5,
    0x9AC5,
    0xAA8A,
    0x9AE5,
    0xAA8A,
    0xAE8B,
    0x9F06,
    0x9F06,
    0xAEAB,
    0x9F26,
    0xAEEB,
    0xB2EC,
    0xB30C,
    0xA367,
    0xB70D,
    0xB32C,
    0xA788,
    0xB74D,
    0xBB2E,
    0xB76D,
    0xBB4E,
    0xB76D,
    0xAFCA,
    0xBB8E,
    0xC370,
    0xBB8E,
    0xB7CC,
    0xC390,
    0xBFAF,
    0xBFCF,
    0xC3B0,
    0xBFCF,
    0xBFCF,
    0xBFEF,
    0xC3F0,
    0xBFF0,
    0xC3F0,
    0xC7F1,
    0xC7F1,
    0xC7F1,
    0xCBF2,
    0xCFF3,
    0xCFF3,
    0xD7F4,
    0xFFFF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80182D18[256] = {
    0,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8421,
    0x8421,
    0x8422,
    0x8422,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8443,
    0x8443,
    0x8443,
    0x8443,
    0x8463,
    0x8463,
    0x8863,
    0x8863,
    0x8863,
    0x8864,
    0x8864,
    0x8884,
    0x8884,
    0x8884,
    0x8884,
    0x8884,
    0x8884,
    0x8885,
    0x8885,
    0x8885,
    0x88A5,
    0x88A5,
    0x84A5,
    0x88A5,
    0x88A5,
    0x8CA5,
    0x8CA5,
    0x8CA6,
    0x8CA6,
    0x8CA6,
    0x8CA6,
    0x8CC6,
    0x8CC6,
    0x88C6,
    0x8CC6,
    0x8CC6,
    0x8CC6,
    0x84E7,
    0x8CE7,
    0x8CE7,
    0x90E7,
    0x90E7,
    0x90E7,
    0x8508,
    0x90E7,
    0x9107,
    0x9108,
    0x9108,
    0x9108,
    0x9108,
    0x8908,
    0x9108,
    0x9108,
    0x9108,
    0x8929,
    0x9129,
    0x9129,
    0x9129,
    0x9129,
    0x8929,
    0x9129,
    0x9529,
    0x9529,
    0x9529,
    0x894A,
    0x9529,
    0x954A,
    0x954A,
    0x954A,
    0x954A,
    0x894A,
    0x954A,
    0x954A,
    0x896B,
    0x954A,
    0x954A,
    0x956B,
    0x956B,
    0x996B,
    0x8D6B,
    0x996B,
    0x996B,
    0x996B,
    0x8D8C,
    0x996B,
    0x998C,
    0x998C,
    0x998C,
    0x998C,
    0x998C,
    0x918C,
    0x8DAC,
    0x998C,
    0x998C,
    0x8DAD,
    0x998C,
    0x99AD,
    0x99AD,
    0x8DAD,
    0x99AD,
    0x99AD,
    0x9DAD,
    0x9DAD,
    0x8DCE,
    0x9DAD,
    0x9DAE,
    0x9DCE,
    0x8DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x91EF,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DEE,
    0x9DEF,
    0x91EF,
    0x9DEF,
    0x9DEF,
    0xA1EF,
    0xA1EF,
    0xA1EF,
    0x9210,
    0xA20F,
    0xA210,
    0x9210,
    0xA210,
    0xA210,
    0x9211,
    0xA210,
    0xA210,
    0x9231,
    0xA210,
    0xA211,
    0xA231,
    0xA231,
    0x9231,
    0xA231,
    0xA631,
    0x9252,
    0xA651,
    0x9252,
    0xA652,
    0x9273,
    0xA673,
    0x9693,
    0x9694,
    0xAA73,
    0x9694,
    0x96B5,
    0xAA94,
    0x96B5,
    0xAE95,
    0x96D6,
    0xAAB5,
    0x96D6,
    0xAED6,
    0x96D6,
    0x96F7,
    0xAED6,
    0x9AF7,
    0x9B18,
    0xB2F7,
    0x9B39,
    0xB318,
    0xB739,
    0x9F5A,
    0xB739,
    0x9F5A,
    0xB75A,
    0xB75A,
    0x9F7B,
    0xA39C,
    0xBB7B,
    0xBB9C,
    0xA7BD,
    0xBB9C,
    0xBFBD,
    0xC39D,
    0xC3BD,
    0xBFBD,
    0xBFDE,
    0xB3DE,
    0xC7DE,
    0xBFDE,
    0xBFFF,
    0xCBFF,
    0xC3FF,
    0xC7FF,
    0xCBFF,
    0xCFFF,
    0xDBFF,
    0xEFFF,
    0xFFFF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80182F18[256] = {
    0,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8004,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8005,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8006,
    0x8005,
    0x8005,
    0x8005,
    0x8005,
    0x8005,
    0x8006,
    0x8005,
    0x8005,
    0x8005,
    0x8007,
    0x8006,
    0x8006,
    0x8006,
    0x8007,
    0x8006,
    0x8006,
    0x8006,
    0x8006,
    0x8006,
    0x8007,
    0x8008,
    0x8006,
    0x8007,
    0x8008,
    0x8007,
    0x8007,
    0x8007,
    0x8009,
    0x8007,
    0x8007,
    0x8008,
    0x8009,
    0x8008,
    0x8008,
    0x8008,
    0x8008,
    0x800A,
    0x8008,
    0x8008,
    0x8428,
    0x8009,
    0x8009,
    0x800B,
    0x8009,
    0x8009,
    0x800B,
    0x8029,
    0x8029,
    0x800A,
    0x8029,
    0x800C,
    0x840A,
    0x802A,
    0x802A,
    0x8429,
    0x800C,
    0x802A,
    0x800A,
    0x800D,
    0x842A,
    0x800B,
    0x802A,
    0x800D,
    0x802B,
    0x802B,
    0x800B,
    0x842B,
    0x802B,
    0x800E,
    0x842B,
    0x802B,
    0x802C,
    0x842C,
    0x800F,
    0x842C,
    0x842C,
    0x800D,
    0x842C,
    0x800F,
    0x842C,
    0x842C,
    0x842C,
    0x800F,
    0x842D,
    0x842D,
    0x842D,
    0x8010,
    0x842D,
    0x842D,
    0x8010,
    0x842D,
    0x842E,
    0x8010,
    0x842E,
    0x8011,
    0x842E,
    0x842F,
    0x8012,
    0x8012,
    0x844E,
    0x8013,
    0x842F,
    0x8013,
    0x8430,
    0x8014,
    0x8014,
    0x8450,
    0x8431,
    0x884F,
    0x8015,
    0x8013,
    0x8432,
    0x8016,
    0x8014,
    0x8851,
    0x8016,
    0x8433,
    0x8453,
    0x8017,
    0x8017,
    0x8434,
    0x8853,
    0x8435,
    0x8018,
    0x8854,
    0x8019,
    0x8436,
    0x8855,
    0x8436,
    0x801A,
    0x8019,
    0x8457,
    0x8439,
    0x8875,
    0x8856,
    0x8458,
    0x8C76,
    0x8858,
    0x8C77,
    0x843A,
    0x8859,
    0x843B,
    0x8C78,
    0x845B,
    0x843D,
    0x885B,
    0x8879,
    0x885C,
    0x843E,
    0x885D,
    0x883F,
    0x8C7B,
    0x885E,
    0x8C7C,
    0x8C7C,
    0x885F,
    0x8C9B,
    0x885E,
    0x885F,
    0x8C7F,
    0x909D,
    0x8C7F,
    0x909E,
    0x8C7F,
    0x909F,
    0x909F,
    0x90BF,
    0x98DF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80183118[256] = { 0 };

u16 D_acropolis_security_room_80183318[256] = { 0 };

u16 D_acropolis_security_room_80183518[256] = { 0 };

u16 D_acropolis_security_room_80183718[256] = { 0 };

GpuImageUpload D_acropolis_security_room_80183918[5] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 270, 256, 1 }, (u_long*)D_acropolis_security_room_80183118 },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 271, 256, 1 }, (u_long*)D_acropolis_security_room_80183318 },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 263, 256, 1 }, (u_long*)D_acropolis_security_room_80183518 },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 264, 256, 1 }, (u_long*)D_acropolis_security_room_80183718 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u16 D_acropolis_security_room_80183968[4] = {
    5,
    6,
    9,
    10,
};

_AcropolisSecurityRoomMonitorFeedQuad D_acropolis_security_room_80183970[4] = {
    { 270, 0, 0, -89, -14 },
    { 271, 0, 128, -21, -13 },
    { 263, 128, 0, 29, -13 },
    { 264, 128, 128, 100, -15 },
};

SVECTOR D_acropolis_security_room_80183998[4] = {
    { 720, -2450, 420, 0 },
    { 720, -2450, 280, 0 },
    { 720, -2450, 350, 0 },
    { 720, -2450, 210, 0 },
};

s16 D_acropolis_security_room_801839B8[4] = {
    3,
    1,
    3,
    2,
};

EffectUnitQuadCorner D_acropolis_security_room_801839C0[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

WorldCollisionRoomResources D_acropolis_security_room_801839D0[1] = {
    { D_acropolis_security_room_80183D94, D_acropolis_security_room_80183DB8, D_acropolis_security_room_80183EE8, NULL },
};

u8* D_acropolis_security_room_801839E0[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_security_room_801839E4[1] = { 16 };

WorldCoordRoomLighting D_acropolis_security_room_801839E8[1] = {
    { D_acropolis_security_room_801841C8, NULL },
};

DirectionWarpEntry D_acropolis_security_room_801839F0[1] = {
    { { { .word = 0 }, -46, -935, -2877 }, { 0, 0, 0, 0 }, { { .word = 0 }, -46, -935, -2877 }, { 0, 0, 0, 0 }, 0x51060005, 0x51060004, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 498 },
};

static SVECTOR _gAcropolisSecurityRoomCollision067D4Normals[11] = {
#include "assets/acropolis_security_room_collision_067D4_normals.inc"
};

static SVECTOR _gAcropolisSecurityRoomCollision067D4Verts[50] = {
#include "assets/acropolis_security_room_collision_067D4_verts.inc"
};

static WorldCollisionGridFace _gAcropolisSecurityRoomCollision067D4Faces[24] = {
#include "assets/acropolis_security_room_collision_067D4_faces.inc"
};

static s16 _gAcropolisSecurityRoomCollision067D4Cells[46] = {
#include "assets/acropolis_security_room_collision_067D4_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSecurityRoomCollision067D4Cells[i])
static s16* _gAcropolisSecurityRoomCollision067D4Table[2] = {
#include "assets/acropolis_security_room_collision_067D4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_security_room_80183D94[1] = {
    { NULL, _gAcropolisSecurityRoomCollision067D4Normals, _gAcropolisSecurityRoomCollision067D4Verts, _gAcropolisSecurityRoomCollision067D4Faces, _gAcropolisSecurityRoomCollision067D4Table, 1250, 3250, 1, 2, 4000, 24 },
};

WorldCollisionTrigger D_acropolis_security_room_80183DB8[4] = {
    { NULL, NULL, NULL, { -64, -2080, -513, 0 }, { { -1472, -1664, 0, 0 }, { 1472, -1664, 0, 0 }, { -1472, 1664, 0, 0 }, { 1472, 1664, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 0, 0 }, 2217, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2, -2016, 1215, 0 }, { { -1471, -1664, 0, 0 }, { 1472, -1664, 0, 0 }, { -1471, 1664, 0, 0 }, { 1472, 1664, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 0, 0 }, 2217, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1, -2176, 1087, 0 }, { { -1473, 1664, 0, 0 }, { 1472, 1664, 1, 0 }, { -1473, -1664, 0, 0 }, { 1472, -1664, 1, 0 } }, { -2, 0, 4103, 0 }, { 0, 0, 0, 0 }, 2217, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -2208, -1024, 0 }, { { -1472, 1664, 0, 0 }, { 1472, 1664, 0, 0 }, { -1472, -1664, 0, 0 }, { 1472, -1664, 0, 0 } }, { 0, 0, 4103, 0 }, { 0, 0, 0, 0 }, 2217, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_security_room_80183EE8[5] = {
    { NULL, NULL, NULL, { 128, -1024, -2896, 0 }, { { -544, 0, -240, 0 }, { 544, 0, -240, 0 }, { -544, 0, 240, 0 }, { 544, 0, 240, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 593, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1088, -1088, -704, 0 }, { { -464, 0, -752, 0 }, { 464, 0, -752, 0 }, { -464, 0, 752, 0 }, { 464, 0, 752, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 882, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -1088, 144, 0 }, { { -272, 0, -416, 0 }, { 272, 0, -416, 0 }, { -272, 0, 416, 0 }, { 272, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { -4097, 0, -27, 0 }, 496, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -448, -1056, 1568, 0 }, { { -784, 0, -272, 0 }, { 784, 0, -272, 0 }, { -784, 0, 272, 0 }, { 784, 0, 272, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 829, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -880, -1056, -2880, 0 }, { { -432, 0, -304, 0 }, { 432, 0, -304, 0 }, { -432, 0, 304, 0 }, { 432, 0, 304, 0 } }, { 0, 4096, 0, 0 }, { 1189, 0, 3920, 0 }, 527, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_security_room_80184064[3] = {
    { 102, 119, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_311900_8016EC0C },
    { 110, 119, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_311900_8016EC00 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_security_room_80184088[4] = {
    { NULL, NULL },
    { D_map_akropolis_8017B01C, D_acropolis_security_room_80184064 },
    { NULL, NULL },
    { NULL, NULL },
};

/// The security room's three white point lights, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensity uses
/// 12 fractional bits (`ONE` is full strength). Each light is full strength
/// within 1280 units and fades to zero at 2128 units. The loaded room overlay
/// owns these writable records: coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not outlive the overlay.
static WorldCoordPointLight _gAcropolisSecurityRoomPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2267, -2365 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1280,
        .outer = 2128,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2267, -472 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1280,
        .outer = 2128,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2993, 1623 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1280,
        .outer = 2128,
    },
};

WorldCoordRoomLights D_acropolis_security_room_801841C8[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisSecurityRoomPointLights), _gAcropolisSecurityRoomPointLights, 0, NULL },
};

SpriteBatch D_acropolis_security_room_801841E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_801841F0[18] = {
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -160, -112, 550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, -16, 550, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, 64, 550, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -120, -112, 575, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -120, -16, 575, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -120, 64, 575, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -104, 600, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -48, 600, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 8, 600, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 64, 600, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -104, 612, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -48, 612, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, 8, 612, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 64, 612, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -64, 625, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -8, 625, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 48, 625, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 40, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184358[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_80184370[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_80184380[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_80184390[10] = {
    { 143, 0x3FC0, { .fields = { 56, 240 } }, -160, -120, 129, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 240 } }, 104, -120, 129, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -104, -120, 129, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -48, -120, 129, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 8, -120, 129, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 56, -120, 129, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -104, 56, 129, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -48, 56, 129, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, 8, 56, 129, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, 56, 56, 129, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184458[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_80184470[2] = {
    { 143, 0x3FC0, { .fields = { 120, 120 } }, -128, 0, 50, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 120 } }, 8, 0, 50, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184498[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_801844B8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_801844C8[16] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_80184548[84] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 375, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 375, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, 64, 375, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 56, 375, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 56, 375, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 96, 375, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 375, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 72, 375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, 80, 375, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 375, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 375, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -96, 375, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, -16, 375, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 56, 375, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -96, 375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -48, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 24, -120, 375, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 56, -120, 375, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 88, -120, 375, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, -120, 375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 0, 375, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -120, -120, 375, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, -80, 681, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -160, -48, 1883, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 24, 1475, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 32, 1425, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 1415, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, 32, 1312, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 72, 1312, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -40, 32, 1312, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -40, 72, 1312, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 32, 1400, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, 72, 1400, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 32, 1350, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 72, 1350, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 32, 72, 1312, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -8, 72, 1312, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, 32, 1312, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, 32, 1312, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 16, 1500, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -80, 1500, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, -80, 1500, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -56, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -16, 1500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 1500, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -72, 24, 1500, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, -24, 1875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -16, -16, 1875, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -16, 1875, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -16, -48, 1875, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 1875, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 24, 1875, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1875, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, -104, 1212, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, -104, 1212, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 40, 1212, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 128, 64, 1212, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 32, 1212, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 0, 1212, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -40, 1212, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -80, 1212, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 88, -112, 1212, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 24, 1625, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, -8, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -40, 1625, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 88, -72, 1625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 375, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, 48, 104, 375, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -32, 104, 375, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 8, 104, 375, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 104, 375, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -144, 88, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 375, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -72, -104, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 2212, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 2162, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 2100, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184BD8[15] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 5, 0 } },
    { 8, 5, 0, 0, { 7, 0 } },
    { 13, 7, 0, 0, { 1, 0 } },
    { 20, 4, 0, 0, { 8, 0 } },
    { 24, 5, 0, 0, { 0, 0 } },
    { 29, 15, 0, 0, { 9, 0 } },
    { 44, 7, 0, 0, { 6, 0 } },
    { 51, 7, 0, 0, { 10, 0 } },
    { 58, 9, 0, 0, { 3, 0 } },
    { 67, 4, 0, 0, { 11, 0 } },
    { 71, 9, 0, 0, { 2, 0 } },
    { 80, 1, 0, 0, { 12, 0 } },
    { 81, 3, 0, 0, { 4, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_security_room_80184C50[16] = {
    { { .empty = D_acropolis_security_room_801841E0 }, D_acropolis_security_room_801841E0, NULL },
    { { .elements = D_acropolis_security_room_801841F0 }, D_acropolis_security_room_80184358, NULL },
    { { .empty = D_acropolis_security_room_80184370 }, D_acropolis_security_room_80184370, NULL },
    { { .empty = D_acropolis_security_room_80184380 }, D_acropolis_security_room_80184380, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184470 }, D_acropolis_security_room_80184498, NULL },
    { { .empty = D_acropolis_security_room_801844B8 }, D_acropolis_security_room_801844B8, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .empty = D_acropolis_security_room_80184380 }, D_acropolis_security_room_80184380, NULL },
    { { .empty = D_acropolis_security_room_80184380 }, D_acropolis_security_room_80184380, NULL },
    { { .elements = D_acropolis_security_room_80184548 }, D_acropolis_security_room_80184BD8, NULL },
};

ViewCamera D_acropolis_security_room_80184D10[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 911 },
    { { { { -3929, 0, -1154 }, { -300, 3954, 1023 }, { 1114, 1067, -3794 } }, { 670, 2800, -1160 } }, 207 },
    { { { { 3895, 0, -1265 }, { -445, 3833, -1371 }, { 1184, 1441, 3646 } }, { 780, 3090, 2600 } }, 207 },
    { { { { 3698, 0, -1759 }, { -1442, 2345, -3032 }, { 1007, 3357, 2118 } }, { 770, 3450, -750 } }, 207 },
    { { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 780, 2260, -1930 } }, 207 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { -490, 2430, -320 } }, 207 },
    { { { { 0, 0, 4096 }, { 0, 4096, 0 }, { -4096, 0, 0 } }, { 910, 2500, 880 } }, 207 },
    { { { { 505, 0, 4064 }, { 822, 4011, -102 }, { -3980, 829, 495 } }, { -7580, 2600, 290 } }, 230 },
    { { { { -4073, 0, 425 }, { 115, 3943, 1101 }, { -409, 1107, -3922 } }, { -4735, 2711, -4651 } }, 257 },
    { { { { -1392, 0, 3852 }, { 1053, 3939, 380 }, { -3705, 1120, -1339 } }, { -1390, 2540, 250 } }, 297 },
    { { { { -3750, 0, 1646 }, { 1161, 2905, 2643 }, { -1168, 2887, -2659 } }, { 3260, 3610, 2030 } }, 230 },
    { { { { -169, 0, 4092 }, { 1090, 3948, 45 }, { -3944, 1091, -163 } }, { -5964, 2300, 6357 } }, 230 },
    { { { { -1332, 0, -3873 }, { -1542, 3757, 530 }, { 3552, 1631, -1222 } }, { -2580, 5310, -0x3660 } }, 230 },
    { { { { -3, 0, -4095 }, { 199, 4091, 0 }, { 4091, -199, -3 } }, { -6100, 1240, 5740 } }, 235 },
    { { { { 3815, 0, 1489 }, { 981, 3080, -2514 }, { -1120, 2699, 2869 } }, { -920, 3010, 0x48A8 } }, 230 },
    { { { { 3982, 0, 958 }, { -16, 4095, 69 }, { -958, -71, 3981 } }, { -1500, 1380, 7850 } }, 225 },
};

AreaApplyRec D_acropolis_security_room_80184F50[10] = {
    { 1, 8, 2, 17 },
    { 1, 8, 7, 33 },
    { 1, 9, 4, 16 },
    { 1, 9, 8, 32 },
    { 1, 10, 3, 0 },
    { 1, 11, 3, 17 },
    { 1, 11, 8, 33 },
    { 1, 13, 2, 17 },
    { 1, 13, 8, 33 },
    { 255, 0, 0, 0 },
};

AreaApplyRec D_acropolis_security_room_80184F78[1] = {
    { 255, 0, 0, 0 },
};

AreaApplyRec D_acropolis_security_room_80184F7C[1] = {
    { 255, 0, 0, 0 },
};

AreaApplyRec D_acropolis_security_room_80184F80[1] = {
    { 255, 0, 0, 0 },
};

WorldCollisionFootstepSounds D_acropolis_security_room_80184F84 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionSurfaceProperties D_acropolis_security_room_80184F90[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_security_room_80184F98[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_security_room_80184F84 },
};

WorldCollisionSurfaceProperties* D_acropolis_security_room_80184FA0[8] = {
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F98,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
};

static TmdBone _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gAcropolisSecurityRoomAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0PartVerts,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Verts,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Normals,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Skeleton,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Stream,
};

Task* D_acropolis_security_room_801855A8;

Task* D_acropolis_security_room_801855AC;

SVECTOR ActorContact_ScratchPosition;

static void _acropolisSecurityRoomOutlinePromptRect(ActionPromptRect* rect, u8 red, u8 green, u8 blue);
static void func_acropolis_security_room_8017F1BC(Task* task);
static void func_acropolis_security_room_8017F300(Task* task);
static void func_acropolis_security_room_80182574(Task* task);

/// Accepts a room transition by copying the complete request to its reply.
///
/// Both records must be live for this synchronous message; they may alias.
/// Returns 1 without changing the destination or applying transition effects.
static s32 _acropolisSecurityRoomResolveTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return 1;
}

/// Message 0x13F1 handler: forwards the message unchanged to the task
/// `func_acropolis_security_room_8017D834` spawns and keeps in
/// `D_acropolis_security_room_801855AC`, answering 0 while it is not alive.
s32 func_acropolis_security_room_8017D6D4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Task* target;
    s32   ret;

    target = D_acropolis_security_room_801855AC;
    if (target == NULL) {
        ret = 0;
    } else {
        ret = taskMessageDispatch(target, msgId, arg2, arg3);
    }
    return ret;
}

s32 func_acropolis_security_room_8017D708(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        taskSpawnFromTable(D_acropolis_security_room_80182618, 1, 0, 0);
    }
    return 0;
}

s32 func_acropolis_security_room_8017D740(Task* arg0, s32 arg1, DirectionActionRequest* request, s32 arg3)
{
    if (request->actionId == 0) {
        taskSpawnFromTable(D_acropolis_security_room_80182618, 0, 0, 0);
    }
}
/// State table of the room's message task: register the room's message table,
/// idle, then kill the task.
static const TaskFuncTable3 D_acropolis_security_room_8017D5C4 = { {
    func_acropolis_security_room_8017D930,
    _acropolisSecurityRoomMessageIdle,
    taskKill,
} };

void func_acropolis_security_room_8017D77C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf("monitor\n");
            D_acropolis_security_room_801855A8 = taskSpawn(2, 9, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (taskPollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                taskKill(arg0);
            }
            return;
    }
}

/// The debug line `func_acropolis_security_room_8017D834` prints. The two bytes
/// after its terminator are non-zero in the ROM (0x40, 0x11), so the array is
/// declared at 16 bytes to carry them.
static const char PowerSupplyMsg[16] = "power supply\n\0@\021";

void func_acropolis_security_room_8017D834(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf(PowerSupplyMsg);
            D_acropolis_security_room_801855AC = taskSpawn(2, 0xA, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(2);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (taskPollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                D_acropolis_security_room_801855AC = NULL;
                taskKill(arg0);
            }
            return;
    }
}

static void func_acropolis_security_room_8017D930(Task* arg0)
{
    arg0->msgTable = D_acropolis_security_room_801825DC;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state                        = arg0->state + 1;
    D_acropolis_security_room_801855AC = NULL;
}

/// Keeps the room-message task alive and available to receive messages.
static void _acropolisSecurityRoomMessageIdle(Task* task)
{
}

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`: set up the work block, run the
/// camera list, redraw the panel, confirm a camera, and leave the monitor.
static const TaskFuncTable7 D_acropolis_security_room_8017D5EC = { {
    func_acropolis_security_room_8017D9DC,
    func_acropolis_security_room_8017EA28,
    func_acropolis_security_room_8017DB30,
    func_acropolis_security_room_8017EA5C,
    func_acropolis_security_room_8017DC7C,
    func_acropolis_security_room_8017EADC,
    func_acropolis_security_room_8017EB9C,
} };

/// Runs the room's message task's current state through a stack copy of its
/// three-entry state table.
void func_acropolis_security_room_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_security_room_8017D5C4;
    sp.funcs[task->state](task);
}

/// Entry state of the security-monitor task: allocates the
/// `_AcropolisSecurityRoomMonitorWork` block into `Task::work`, spawns the
/// monitor's companion task, and seeds `screenLevel` from
/// `GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA` (the darkest detent when that
/// index is not one of the five). While observatory-route progress is below 3
/// the saved view becomes 8 and the task continues into the camera list; from
/// 3 on the view becomes 5 and the task goes to the idle hotspot state. Every
/// hotspot's `hit` flag is cleared so the first test starts clean.
static void func_acropolis_security_room_8017D9DC(Task* task)
{
    _AcropolisSecurityRoomMonitorWork* work;
    ActionPromptHotspot*               hs;
    s16                                flag;
    s32                                state;
    s16                                stateElse;

    work = memCalloc(sizeof(_AcropolisSecurityRoomMonitorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&D_acropolis_security_room_8018263C, 0, 1, 0);
    task->work              = work;
    work->sweepTimer        = 0;
    stateElse               = 6;
    flag                    = gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA);
    if ((u16)flag < 5) {
        work->screenLevel = D_acropolis_security_room_801826B4[flag];
    } else {
        work->screenLevel = D_acropolis_security_room_801826B4[0];
    }
    if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) < 3) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
        /* Without this the scheduler hoists the `task->state` load above the
           `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` byte store to fill its load-delay slot. */
        state = task->state;
        state++;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
        state                                                      = stateElse;
    }
    task->state = state;
    Display_AcquireRef();
    gGameSession->hideHud      = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->eventState   = 1;
    hs                         = D_acropolis_security_room_80182648;
    if (hs->id != ACTION_PROMPT_HOTSPOT_END) {
        do {
            hs->hit = 0;
            hs++;
        } while (hs->id != ACTION_PROMPT_HOTSPOT_END);
    }
}

/// Runs the hotspot-hit state of the security monitor: redraws the panel and
/// its overlay bar, then hit-tests the action cursor against the room's hotspot table.
/// A miss leaves the prompt's idle cursor; a hit with the prompt
/// confirmed (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`) scans the table for the raised entry and hands its
/// `id` / `promptKind` to the work block, advancing to state 3. Otherwise the
/// task advances to state 5 once the prompt has been dismissed.
static void func_acropolis_security_room_8017DB30(Task* task)
{
    _AcropolisSecurityRoomMonitorWork* work;
    ActionPromptHotspot*               hs;
    ActionPrompt*                      prompt;

    hs     = D_acropolis_security_room_80182648;
    prompt = D_80114D28;
    work   = (_AcropolisSecurityRoomMonitorWork*)task->work;
    _acropolisSecurityRoomDrawMonitorWash(work->screenLevel - ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    func_acropolis_security_room_8017E37C(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if ((prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) && (hs->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 3;
                    return;
                }
                hs++;
            } while (hs->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

/// Applies a confirmed monitor hotspot. A non-negative id is a camera view:
/// it replaces the saved view, with a click, and arms the enemy caption when
/// the enemy's view is selected while that scene has not played. The brighter
/// and darker ids step the grey wash by one detent. When the wash is not on
/// its darkest detent, two one-shot captions can start: view 0xB's, and the
/// enemy view's once this visit armed it and the caption has not already been
/// started. What view 0xB shows is not established in this task. The panel
/// and its overlay bar are redrawn and the task returns to the hotspot scan.
static void func_acropolis_security_room_8017DC7C(Task* task)
{
    _AcropolisSecurityRoomMonitorWork* work;
    McSaveData*                        save;
    s16                                confirmedId;
    u16                                confirmedBits;

    work                      = (_AcropolisSecurityRoomMonitorWork*)task->work;
    D_80114D28[0].mode        = ACTION_PROMPT_MODE_HIDDEN;
    D_80114D28[0].cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        confirmedId = work->hotspotId;
        if (confirmedId >= 0) {
            save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.location.loc.view != confirmedId) {
                save->state.location.loc.view = work->hotspotId;
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_SELECT, 0, 0);
                // Arm the enemy caption when this confirm is what first shows that view.
                if ((work->hotspotId == ACROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY) && !(gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) & 2)) {
                    work->enemySceneArmed = 1;
                }
            }
        }
        confirmedBits = work->hotspotId;
        if (confirmedBits == ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_BRIGHTER) {
            if (((s16)work->screenLevel + ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP) < ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_NEXT_LIMIT) {
                work->screenLevel += ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP;
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTER, 0, 0);
            }
        } else if (confirmedBits == ACROPOLIS_SECURITY_ROOM_MONITOR_HOTSPOT_DARKER) {
            if (((s16)work->screenLevel - ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP) > 0) {
                work->screenLevel -= ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_STEP;
                sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_DARKER, 0, 0);
            }
        }
        if (((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 0xB) &&
            ((s16)work->screenLevel != ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST) && !(gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) & 1)) {
            capStartSequenceSlot(0xD, 0, 0);
            gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN, gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) | 1);
        }
        if (((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == ACROPOLIS_SECURITY_ROOM_MONITOR_VIEW_ENEMY) &&
            ((s16)work->screenLevel != ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_DARKEST) && (work->enemySceneArmed != 0) &&
            (work->enemyCaptionStarted == 0) && (gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_CAM_A_SCENE_DONE) == 0)) {
            capStartSequenceSlot(ACROPOLIS_SECURITY_ROOM_MONITOR_ENEMY_CAP_SLOT, 0, 0);
            work->enemyCaptionStarted = 1;
        }
    }
    _acropolisSecurityRoomDrawMonitorWash(work->screenLevel - ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    func_acropolis_security_room_8017E37C(task);
    task->state = 2;
}

/// Queues the four edges of a prompt rectangle in the supplied RGB byte colour.
///
/// `rect` is borrowed screen-space geometry; edges include x + w and y + h.
/// Consumes four LINE_F2 packets in the frame arena at ordering-table slot 3.
/// This retained drawer has no caller in the room.
static void _acropolisSecurityRoomOutlinePromptRect(ActionPromptRect* rect, u8 red, u8 green, u8 blue)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT = 3,
    };
    LINE_F2* line;

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x, rect->y, rect->x + rect->w, rect->y, red, green, blue);
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x + rect->w, rect->y, rect->x + rect->w, rect->y + rect->h, red, green, blue);
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x + rect->w, rect->y + rect->h, rect->x, rect->y + rect->h, red, green, blue);
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    ACROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE(line, rect->x, rect->y + rect->h, rect->x, rect->y, red, green, blue);
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_PROMPT_OUTLINE_OT_SLOT, line);
}

#undef ACROPOLIS_SECURITY_ROOM_INIT_PROMPT_EDGE

/// Draws the monitor's signed grey wash and the opaque black strip at its bottom.
///
/// `washLevel` is the stored screen detent minus the screen bias: nonnegative
/// levels add the low seven bits, negative levels subtract the low byte of
/// their magnitude. Queues a quad and draw-mode packet at slot 12, then the
/// black border and its average-blend draw mode at slot 11.
static void _acropolisSecurityRoomDrawMonitorWash(s16 washLevel)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT   = 12,
        ACROPOLIS_SECURITY_ROOM_MONITOR_BORDER_OT_SLOT = 11,
        ACROPOLIS_SECURITY_ROOM_MONITOR_LEFT           = -102,
        ACROPOLIS_SECURITY_ROOM_MONITOR_RIGHT          = 108,
        ACROPOLIS_SECURITY_ROOM_MONITOR_TOP            = -95,
        ACROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM         = 60,
        ACROPOLIS_SECURITY_ROOM_MONITOR_BORDER_TOP     = 56,
    };
    POLY_F4* quad;
    DR_MODE* drawMode;
    u16      intensity;

    if (washLevel >= 0) {
        intensity      = washLevel & ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        ACROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH(quad, intensity);
        addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, quad);

        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_ADD, 640, 0));
        addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, drawMode);
    } else {
        intensity      = (~washLevel + 1) & 0xFF;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        ACROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH(quad, intensity);
        addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, quad);

        drawMode       = gGpuPrimCursor;
        gGpuPrimCursor = drawMode + 1;
        setlen(drawMode, 1);
        drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));
        addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_MONITOR_WASH_OT_SLOT, drawMode);
    }

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyF4(quad);
    quad->r0 = 0;
    quad->g0 = 0;
    quad->b0 = 0;
    quad->x0 = ACROPOLIS_SECURITY_ROOM_MONITOR_LEFT;
    quad->y0 = ACROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;
    quad->x1 = ACROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;
    quad->y1 = ACROPOLIS_SECURITY_ROOM_MONITOR_BOTTOM;
    quad->x2 = ACROPOLIS_SECURITY_ROOM_MONITOR_LEFT;
    quad->y2 = ACROPOLIS_SECURITY_ROOM_MONITOR_BORDER_TOP;
    quad->x3 = ACROPOLIS_SECURITY_ROOM_MONITOR_RIGHT;
    quad->y3 = ACROPOLIS_SECURITY_ROOM_MONITOR_BORDER_TOP;
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_MONITOR_BORDER_OT_SLOT, quad);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_AVERAGE, 640, 0));
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_MONITOR_BORDER_OT_SLOT, drawMode);
}

#undef ACROPOLIS_SECURITY_ROOM_INIT_MONITOR_WASH

/// Draws the overlay bar on the monitor panel: a 0x6C-wide grey `TILE` whose
/// top and height are both `sweepTimer` minus the panel top (0x5F), followed
/// by the drawing-mode packet that restores the panel's texture page. The
/// timer wraps at `ACROPOLIS_SECURITY_ROOM_MONITOR_SWEEP_PERIOD`, which
/// restarts the bar.
static void func_acropolis_security_room_8017E37C(Task* task)
{
    _AcropolisSecurityRoomMonitorWork* work;
    TILE*                              tile;
    DR_MODE*                           dr;
    s16                                y;

    tile           = gGpuPrimCursor;
    work           = (_AcropolisSecurityRoomMonitorWork*)task->work;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x42);
    tile->r0 = 0x60;
    tile->g0 = 0x60;
    tile->b0 = 0x60;
    tile->x0 = -0x66;
    tile->w  = 0x6C;
    y        = work->sweepTimer - 0x5F;
    tile->h  = y;
    tile->y0 = y;
    addPrim(gGpuCurrentOt + 0xE, tile);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(gGpuCurrentOt + 0xE, dr);
    work->sweepTimer++;
    if (work->sweepTimer >= ACROPOLIS_SECURITY_ROOM_MONITOR_SWEEP_PERIOD) {
        work->sweepTimer = 0;
    }
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Two-state dispatcher whose handler table is built on the stack rather than
/// read from `.data`: state 0 runs `actionPromptReset` and
/// state 1 runs `actionPromptMoveCursors`.
void func_acropolis_security_room_8017E9D8(Task* task)
{
    TaskFunc funcs[2] = {
        actionPromptReset,
        actionPromptMoveCursors,
    };

    funcs[task->state](task);
}

/// Arms the action prompt for the monitor's hotspot and steps the caller on one
/// state: sets the aiming speed and the idle cursor, and clears the prompt's
/// on-screen position. Cursor movement updates that position, which
/// `itemMenuOpenHotspotCommands` copies when opening the menu. The room carries a second copy at
/// `func_acropolis_security_room_8017FB20`.
static void func_acropolis_security_room_8017EA28(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Confirms the hotspot the player picked: clears the action prompt, redraws
/// the panel for the current wash plus its overlay bar, spawns the prompt at
/// the panel's coordinates with that hotspot's `promptKind` and advances.
static void func_acropolis_security_room_8017EA5C(Task* task)
{
    ActionPrompt*                      prompt = D_80114D28;
    _AcropolisSecurityRoomMonitorWork* work   = (_AcropolisSecurityRoomMonitorWork*)task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    _acropolisSecurityRoomDrawMonitorWash(work->screenLevel - ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    func_acropolis_security_room_8017E37C(task);
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Records which of the five screen detents `screenLevel` is in
/// `GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA`; a level that is none of them
/// records the first.
static inline void _acropolisSecurityRoomSaveScreenLevel(s32 screenLevel)
{
    s32 index;

    for (index = 0; index < 5; index++) {
        if (screenLevel == D_acropolis_security_room_801826B4[index]) {
            gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA, index);
            return;
        }
    }
    gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA, 0);
}

/// Leaves the security monitor: records the current wash detent as
/// `GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA` (its index among the first five
/// table entries, or 0 when it is not one of them), restores the room's normal
/// display state and kills the monitor task along with the child it spawned.
static void func_acropolis_security_room_8017EADC(Task* task)
{
    _AcropolisSecurityRoomMonitorWork* work;

    work       = (_AcropolisSecurityRoomMonitorWork*)task->work;
    D_80114D08 = 0xA;
    _acropolisSecurityRoomSaveScreenLevel((s16)work->screenLevel);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
    displayReleaseMenuHold();
    gGameSession->cutsceneHold = 0;
    gGameSession->hideHud      = 0;
    gGameSession->eventState   = 0;
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Idle state of the security monitor: hit-tests the action cursor against the
/// monitor's hotspot table and mirrors the result into the room's action
/// prompt. A hit that the player confirms (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`) on a raised hotspot
/// clears the prompt and runs cap command 0xE; `buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED` leaves the
/// monitor by advancing to state 5.
static void func_acropolis_security_room_8017EB9C(Task* task)
{
    ActionPrompt*        prompt  = D_80114D28;
    ActionPromptHotspot* hotspot = D_acropolis_security_room_80182648;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    Gp_RunCapCmd(0xE, 0);
                    return;
                }
            }
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// The sixteen state handlers of the room's cap script.
static const TaskFuncTable16 D_acropolis_security_room_8017D63C = { {
    func_acropolis_security_room_8017FA18,
    func_acropolis_security_room_8017FB20,
    func_acropolis_security_room_8017EE44,
    func_acropolis_security_room_8017FB54,
    func_acropolis_security_room_8017FBA4,
    func_acropolis_security_room_8017FC30,
    func_acropolis_security_room_801800A4,
    func_acropolis_security_room_8018014C,
    func_acropolis_security_room_801801C4,
    func_acropolis_security_room_80180218,
    func_acropolis_security_room_8017FE6C,
    func_acropolis_security_room_8017FF0C,
    func_acropolis_security_room_8017FF84,
    func_acropolis_security_room_8017FFD0,
    _acropolisSecurityRoomPowerSupplyRestoreRoomView,
    func_acropolis_security_room_80180030,
} };

/// Runs the security-monitor task's current state. The seven handlers are
/// copied onto the stack first, so the call goes through a local table rather
/// than through `.rodata`.
void func_acropolis_security_room_8017ED68(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_acropolis_security_room_8017D5EC;
    sp.funcs[task->state](task);
}

#include "../../shared/action_prompt_reset.inc.c"

/// Idle state of the security room's cap script: the same hotspot scan
/// `func_acropolis_security_room_8017EB9C` runs for the monitor, but against
/// the script's own table and with the hit recorded in the task's work block
/// instead of dispatched as a cap command. A confirmed
/// (`buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED`) hit copies the hotspot's `id` and `promptKind` into the
/// work block's `hotspotId` and `promptKind` and advances to state 3; with
/// nothing under the cursor `usedKey` is cleared and the prompt keeps its idle
/// cursor.
/// `buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED` leaves the scan by advancing to state 5.
static void func_acropolis_security_room_8017EE44(Task* task)
{
    ActionPrompt*                          prompt = D_80114D28;
    ActionPromptHotspot*                   hs     = D_acropolis_security_room_801826DC;
    _AcropolisSecurityRoomPowerSupplyWork* work   = task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (func_acropolis_security_room_8017FCB0(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 3;
                    return;
                }
            }
        }
    } else {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
        prompt->mode  = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

static void func_acropolis_security_room_8017F1BC(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    flag;
    s32                                    usedKey;

    flag = gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED);
    if ((flag == 0) || (flag == 2)) {
        usedKey = work->usedKey;
        if (usedKey == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(3, 1, 0);
        } else if (usedKey == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_BLUE_KEY);
            sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK, 0, 0);
            gameFlagSetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED, gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) | 1);
            gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, 2);
            _acropolisSecurityRoomShowReleasedLocks(gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 0xFF);
            work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
            task->state   = 6;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F80);
            taskKill(task->spawnArg2.pointer);
            return;
        } else {
            capStartSequenceSlot(3, 1, 2);
        }
    } else if ((flag == 1) || (flag == 3)) {
        if (work->usedKey == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(3, 1, 1);
        } else {
            capStartSequenceSlot(3, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

static void func_acropolis_security_room_8017F300(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    flag;
    s32                                    usedKey;

    flag = gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED);
    if ((flag == 0) || (flag == 1)) {
        usedKey = work->usedKey;
        if (usedKey == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(4, 1, 0);
        } else if (usedKey == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT) {
            inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_RED_KEY);
            sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK, 0, 0);
            gameFlagSetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED, gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) | 2);
            _acropolisSecurityRoomShowReleasedLocks(gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 0xFF);
            work->usedKey            = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
            task->state              = 0xA;
            gGameSession->eventState = 1;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F50);
            if (gameFlagGetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE) < 3) {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F78);
            } else {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F7C);
            }
            taskKill(task->spawnArg2.pointer);
            return;
        } else {
            capStartSequenceSlot(4, 1, 2);
            task->state = 2;
            return;
        }
    } else if ((flag == 2) || (flag == 3)) {
        if (work->usedKey == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE) {
            capStartSequenceSlot(4, 1, 1);
        } else {
            capStartSequenceSlot(4, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

/// The second prompt's copy.
#define actionPromptMoveCursors func_acropolis_security_room_8017F480
#undef ACTION_PROMPT_DRAW_CURSOR
/// Selects the second prompt's private drawer for both fragments.
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursor
#include "../../shared/action_prompt_move_cursors.inc.c"
#undef actionPromptMoveCursors

#include "../../shared/action_prompt_draw_cursor.inc.c"
#undef ACTION_PROMPT_DRAW_CURSOR
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursorDefault

/// Task callback of the descriptor at `D_acropolis_security_room_801826C0`:
/// a two-state dispatcher whose handler table is built on the stack rather
/// than read from `.data`, so state 0 runs
/// `func_acropolis_security_room_80180308` and state 1 runs
/// `func_acropolis_security_room_8017F480`.
void func_acropolis_security_room_8017F9C8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_80180308,
        func_acropolis_security_room_8017F480,
    };

    funcs[task->state](task);
}

/// State 0 of the security-room cap script: allocates the work block into
/// `Task::work`, spawns the script's child task, publishes the message table
/// and the current pair-flag nibble, takes a display reference and clears every
/// hotspot's `hit` flag before the first cursor scan. A failed allocation kills
/// the task instead.
static void func_acropolis_security_room_8017FA18(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work;
    ActionPromptHotspot*                   hs;

    work = memCalloc(sizeof(_AcropolisSecurityRoomPowerSupplyWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = taskSpawnFromTable(D_acropolis_security_room_801826C0, 0, 1, 0);
    task->msgTable                                             = D_acropolis_security_room_801826CC;
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
    task->state++;
    work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    work->timer   = 0;
    _acropolisSecurityRoomShowReleasedLocks(gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 0xFF);
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    Display_AcquireRef();
    for (hs = D_acropolis_security_room_801826DC; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
        hs->hit = 0;
    }
}

/// Arms the action prompt for the script's hotspot and steps the caller on one
/// state: sets the aiming speed and the idle cursor, and clears the prompt's
/// on-screen position. Cursor movement updates that position, which
/// `itemMenuOpenHotspotCommands` copies when opening the menu. The room carries a second copy at
/// `func_acropolis_security_room_8017EA28`.
static void func_acropolis_security_room_8017FB20(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with this state's Examine/Push action choice.
static void func_acropolis_security_room_8017FB54(Task* task)
{
    ActionPrompt*                          prompt = D_80114D28;
    _AcropolisSecurityRoomPowerSupplyWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Acts on the answer to the prompt opened for the confirmed hotspot. When its
/// first row was confirmed (`itemMenuIsHotspotActionConfirmed`) or a key item was used
/// (`usedKey`), hands the task to the lock `hotspotId` names --
/// `func_acropolis_security_room_8017F1BC` for the left one,
/// `func_acropolis_security_room_8017F300` for the right -- and then forgets
/// the key. A prompt closed without either returns the task to state 2.
static void func_acropolis_security_room_8017FBA4(Task* task)
{
    ActionPrompt*                          prompt = D_80114D28;
    _AcropolisSecurityRoomPowerSupplyWork* work   = task->work;

    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if ((itemMenuIsHotspotActionConfirmed() != 0) || (work->usedKey != ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE)) {
        if (work->hotspotId == ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_HOTSPOT_LEFT) {
            func_acropolis_security_room_8017F1BC(task);
            work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
            return;
        }
        func_acropolis_security_room_8017F300(task);
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
        return;
    }
    task->state = 2;
}

static void func_acropolis_security_room_8017FC30(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    displayReleaseMenuHold();
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// The second prompt's copy.
#define actionPromptHitTest func_acropolis_security_room_8017FCB0
#include "../../shared/action_prompt_hit_test.inc.c"
#undef actionPromptHitTest

/// Shows the power-supply panel's released-lock pictures for the supplied flag value.
///
/// The low byte must be 0..3: bit 0 releases the left lock, bit 1 the right.
/// Other values leave visibility unchanged. Updates batches 1 and 2 in the
/// active stage/variant/area's view 6; those tables must be loaded.
static void _acropolisSecurityRoomShowReleasedLocks(s32 releasedLocks)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_LOCKS_NEITHER     = 0,
        ACROPOLIS_SECURITY_ROOM_LOCKS_LEFT        = 1,
        ACROPOLIS_SECURITY_ROOM_LOCKS_RIGHT       = 2,
        ACROPOLIS_SECURITY_ROOM_LOCKS_BOTH        = 3,
        ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_VIEW = 6,
    };
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location.loc;
    SpriteBatch*     batches;

    batches = Gp_SprtTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1][ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_VIEW - 1].batches;
    switch (releasedLocks & 0xFF) {
        case ACROPOLIS_SECURITY_ROOM_LOCKS_NEITHER:
            batches[1].hidden = 1;
            batches[2].hidden = 1;
            break;
        case ACROPOLIS_SECURITY_ROOM_LOCKS_LEFT:
            batches[1].hidden = 0;
            batches[2].hidden = 1;
            break;
        case ACROPOLIS_SECURITY_ROOM_LOCKS_RIGHT:
            batches[1].hidden = 1;
            batches[2].hidden = 0;
            break;
        case ACROPOLIS_SECURITY_ROOM_LOCKS_BOTH:
            batches[1].hidden = 0;
            batches[2].hidden = 0;
            break;
    }
}

/// `TaskMessageEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; each of the three ids this room accepts is recorded in
/// the work block's `usedKey` for the lock the player confirmed to act on. Any
/// other item clears `usedKey` and answers 0, which is the "cannot use that
/// now" reply.
s32 func_acropolis_security_room_8017FE24(Task* task, s32 msgId, s32 item, s32 arg3)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (item == 0x101) {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_OTHER;
        return 1;
    }
    if (item == 0x103) {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT;
        return 1;
    }
    if (item == 0x104) {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT;
        return 1;
    }
    work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    return 0;
}

/// Fades the screen to white over 0x40 frames, then steps the caller on one
/// state: `timer` is the fade level here, rising by 4 a frame and
/// driving `fadeDrawOverlay`'s three colour channels together. At the halfway
/// point (0x80) the door chime is queued; once the level passes 0xFF the
/// timer is reset for the next state and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` is set to 0x10.
static void func_acropolis_security_room_8017FE6C(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    u8                                     level;

    level = work->timer;
    fadeDrawOverlay(level, level, level, GPU_BLEND_SUBTRACT);
    work->timer = work->timer + 4;
    if (work->timer == 0x80) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 2), 0, 0);
    }
    if (work->timer >= 0x100) {
        work->timer                                                = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x10;
        /* Without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store, dropping the load-delay `nop`. */
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8017FF0C(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (work->timer == 1) {
        work->sceneTask = taskSpawnFromTable(D_acropolis_security_room_80182700, 0, 0, 0);
        task->state     = task->state + 1;
    }
    work->timer = work->timer + 1;
}

static void func_acropolis_security_room_8017FF84(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    killArg;

    if (taskPollKill(work->sceneTask, &killArg) != 0) {
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8017FFD0(Task* arg0)
{
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayer3F3(0);
    arg0->state = (s32)(arg0->state + 1);
}

/// Restores the normal room view after the right-lock scene and advances to cleanup.
static void _acropolisSecurityRoomPowerSupplyRestoreRoomView(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_NORMAL_VIEW = 3,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_SECURITY_ROOM_NORMAL_VIEW;
    task->state                                                = task->state + 1;
}

static void func_acropolis_security_room_80180030(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    displayReleaseMenuHold();
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
    func_800E9BDC(0, 0xF9FF);
    taskRequestKill(task, 0);
}

static void func_acropolis_security_room_801800A4(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    u8                                     level;

    gameFlagSetNibble(GAME_FLAG_MAP_MARK_SECURITY_ROOM, 0);
    level = work->timer;
    fadeDrawOverlay(level, level, level, GPU_BLEND_SUBTRACT);
    work->timer = work->timer + 4;
    if (work->timer == 0x80) {
        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 2), 0, 0);
    }
    if (work->timer >= 0x100) {
        work->timer                                                = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xE;
        /* Same load-delay shape as `_acropolisSecurityRoomPowerSupplyRestoreRoomView`:
         * without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store and drops the delay `nop`. */
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8018014C(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (work->timer == 1) {
        work->sceneTask = taskSpawnFromTable(D_acropolis_security_room_80182700, 1, 0, 0);
        task->state     = task->state + 1;
    }
    work->timer = work->timer + 1;
}

static void func_acropolis_security_room_801801C4(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    killArg;

    if (taskPollKill(work->sceneTask, &killArg) != 0) {
        Gp_MsgPlayer3F3(1);
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_80180218(Task* task)
{
    D_80114D08                                                 = 0xA;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    displayReleaseMenuHold();
    func_800E9BDC(0, 0xF9FF);
    taskRequestKill(task, 0);
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
}

/// Runs the cap script's current state. The sixteen handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_acropolis_security_room_80180294(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_acropolis_security_room_8017D63C;
    sp.funcs[task->state](task);
}

/// The second prompt's copy.
#define actionPromptReset func_acropolis_security_room_80180308
#include "../../shared/action_prompt_reset.inc.c"
#undef actionPromptReset

/// First `TaskDesc` of `D_acropolis_security_room_80182700`: starts the room's
/// looping ambience, then rides alongside the cutscene task
/// (`func_acropolis_security_room_801804CC`) until the CD queue reaches its cue
/// or the player skips, fading the loop out exactly once either way, and asks
/// the task system to kill itself.
void func_acropolis_security_room_80180368(Task* task)
{
    CdCmdQueue*                          queue;
    _AcropolisSecurityRoomMovieLoopWork* work;
    _AcropolisSecurityRoomMovieLoopWork* alloc;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case 0:
            alloc      = memCalloc(sizeof(_AcropolisSecurityRoomMovieLoopWork), 0);
            task->work = alloc;
            if (alloc == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(alloc, 0, sizeof(_AcropolisSecurityRoomMovieLoopWork));
            sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (queue->movieFrame >= 0x46 && work->fadeStarted == 0) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0x14);
                work->fadeStarted = 1;
            }
            if (cdCmdIsIdle() & 0xFFFF) {
                task->state = task->state + 1;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            if (work->fadeStarted == 0) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0x14);
            }
            task->state = task->state + 1;
            return;
        case 2:
            taskRequestKill(task, 0);
            return;
    }
}

/// Second `TaskDesc` of `D_acropolis_security_room_80182700`: kicks off the
/// streamed cutscene for the security room, waits for the CD queue to go idle
/// (or for the player to skip it), then asks the task system to kill itself.
void func_acropolis_security_room_801804CC(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (arg0->state) {
        case 0:
            queue->movieFrame = 1;
            slotParam[0]      = streamFindMovieSlot(&gGameSession->location.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if ((cdCmdIsIdle() & 0xFFFF) || padIsStartPressed() != 0) {
                arg0->state = arg0->state + 1;
            }
            return;
        case 2:
            taskRequestKill(arg0, 0);
            return;
    }
}

/// Per-frame update of the security-room's four monitor feeds: state 0 seeds
/// the four screen CLUTs from the unlit palette, state 1 re-blends each of
/// them towards its lit palette by that feed's brightness and spawns the
/// flash effects. `Task::spawnArg2` is the `EffectWork` holding the lit-feed
/// bitmask (`index`) and the four per-feed brightnesses
/// (`scale` .. `step`).
void func_acropolis_security_room_801805A4(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         i;

    work  = (EffectWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    switch (task->state) {
        case 0: {
            u16* base = D_acropolis_security_room_80182718;
            u16* pal  = D_acropolis_security_room_80182918;
            u16* out  = D_acropolis_security_room_80183118;

            for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                gpuBlendRgb555ClutRow(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182B18;
            out = D_acropolis_security_room_80183318;
            for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                gpuBlendRgb555ClutRow(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182D18;
            out = D_acropolis_security_room_80183518;
            for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                gpuBlendRgb555ClutRow(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182F18;
            out = D_acropolis_security_room_80183718;
            for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                gpuBlendRgb555ClutRow(&pal[i], &base[i], 0, &out[i]);
            }
            gpuUploadImages(D_acropolis_security_room_80183918);
            task->state = task->state + 1;
            break;
        }

        case 1:
            work->index = D_acropolis_security_room_80183968[gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED)];
            if ((viewGetMappedIndex() & 0xFF) == 6) {
                u16* pal  = D_acropolis_security_room_80182918;
                u16* base = D_acropolis_security_room_80182718;
                u16* out  = D_acropolis_security_room_80183118;
                s32  limit;

                // The cap flickers by one step every other frame.
                limit        = 0x1000 - ((gDisplayState.animFrame & 1) << 9);
                work->scale  = (work->index & 1) ? ((work->scale < limit) ? work->scale + 0x200 : limit) : 0;
                work->angle  = (work->index & 2) ? ((work->angle < limit) ? work->angle + 0x200 : limit) : 0;
                work->period = (work->index & 4) ? ((work->period < limit) ? work->period + 0x200 : limit) : 0;
                work->step   = (work->index & 8) ? ((work->step < limit) ? work->step + 0x200 : limit) : 0;

                for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                    gpuBlendRgb555ClutRow(&pal[i], &base[i], work->scale, &out[i]);
                }
                pal = D_acropolis_security_room_80182B18;
                out = D_acropolis_security_room_80183318;
                for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                    gpuBlendRgb555ClutRow(&pal[i], &base[i], work->angle, &out[i]);
                }
                pal = D_acropolis_security_room_80182D18;
                out = D_acropolis_security_room_80183518;
                for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                    gpuBlendRgb555ClutRow(&pal[i], &base[i], work->period, &out[i]);
                }
                pal = D_acropolis_security_room_80182F18;
                out = D_acropolis_security_room_80183718;
                for (i = 0; i < 0x100; i += GPU_RGB555_CLUT_ROW_COLORS) {
                    gpuBlendRgb555ClutRow(&pal[i], &base[i], work->step, &out[i]);
                }
                gpuUploadImages(D_acropolis_security_room_80183918);

                for (i = 0; i < 4; i++) {
                    effectSpawn(EFFECT_ACROPOLIS_SECURITY_MONITOR_FEED, coord, i, NULL);
                }
            } else if (((viewGetMappedIndex() & 0xFF) != 8) && ((viewGetMappedIndex() & 0xFF) != 0x10)) {
                for (i = 0; i < 4; i++) {
                    if ((work->index >> i) & 1) {
                        effectSpawn(EFFECT_ACROPOLIS_SECURITY_MONITOR_GLOW, coord, (s32)(D_acropolis_security_room_801839B8[i]),
                                    &D_acropolis_security_room_80183998[i]);
                    }
                }
            }
            break;
    }

    if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) < 3) {
        _acropolisSecurityRoomDrawSweepLine(task);
    }
}

/// Draws a subtractive horizontal segment travelling along the task coordinate's Y axis.
///
/// Requires a composed coordinate body and scratch/primitive capacity. Only
/// room views 3 and 4 draw it. Positions narrow to 16 bits after composed
/// translation; endpoint `end` supplies camera Z / 4 for the minimum depth 17
/// and OT sort. Reserves one packet even if culled, and releases the scratch.
static void _acropolisSecurityRoomDrawSweepLine(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_SWEEP_VIEW_MASK = 0xC,
        ACROPOLIS_SECURITY_ROOM_SWEEP_START_X   = -1063,
        ACROPOLIS_SECURITY_ROOM_SWEEP_END_X     = -496,
        ACROPOLIS_SECURITY_ROOM_SWEEP_SPEED     = 6,
        ACROPOLIS_SECURITY_ROOM_SWEEP_TRAVEL    = 406,
        ACROPOLIS_SECURITY_ROOM_SWEEP_BASE_Y    = 0xF633,
        ACROPOLIS_SECURITY_ROOM_SWEEP_Z         = 2479,
        ACROPOLIS_SECURITY_ROOM_SWEEP_MIN_DEPTH = 17,
        ACROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY = 16,
    };
    _AcropolisSecurityRoomSweepLineScratch* line;
    GfxCoord*                               coord;
    LINE_F2*                                prim;

    coord = task->extra.coordBody->coord;
    if ((ACROPOLIS_SECURITY_ROOM_SWEEP_VIEW_MASK >> (gGameSession->location.loc.view - 1)) & 1) {
        // Build the travelling segment in the task coordinate and transform it to the projection-input frame.
        line           = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisSecurityRoomSweepLineScratch);
        line->start.vx = ACROPOLIS_SECURITY_ROOM_SWEEP_START_X;
        line->start.vy = (gDisplayState.animFrame * ACROPOLIS_SECURITY_ROOM_SWEEP_SPEED) % ACROPOLIS_SECURITY_ROOM_SWEEP_TRAVEL + ACROPOLIS_SECURITY_ROOM_SWEEP_BASE_Y;
        line->start.vz = ACROPOLIS_SECURITY_ROOM_SWEEP_Z;
        ACROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT(line, start, coord);
        line->end.vx = ACROPOLIS_SECURITY_ROOM_SWEEP_END_X;
        line->end.vy = (gDisplayState.animFrame * ACROPOLIS_SECURITY_ROOM_SWEEP_SPEED) % ACROPOLIS_SECURITY_ROOM_SWEEP_TRAVEL + ACROPOLIS_SECURITY_ROOM_SWEEP_BASE_Y;
        line->end.vz = ACROPOLIS_SECURITY_ROOM_SWEEP_Z;
        ACROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT(line, end, coord);
        // Project both ends; the second endpoint supplies the culling and sorting depth.
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&line->start);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        gte_stsxy(&prim->x0);
        gte_ldv0(&line->end);
        gte_rtps();
        setSemiTrans(prim, true);
        gte_stsxy(&prim->x1);
        gte_stszotz(&line->depth);
        if (line->depth >= ACROPOLIS_SECURITY_ROOM_SWEEP_MIN_DEPTH) {
            setRGB0(prim, ACROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY, ACROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY, ACROPOLIS_SECURITY_ROOM_SWEEP_INTENSITY);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                        ((((u32)line->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_SUBTRACT, line->depth);
        }
        SCRATCH_STACK_RELEASE_BLOCK(_AcropolisSecurityRoomSweepLineScratch);
    }
}

#undef ACROPOLIS_SECURITY_ROOM_TRANSFORM_SWEEP_ENDPOINT

void acropolisSecurityRoomMonitorFeedTask(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_FEED_INDEX_MASK = 3,
        ACROPOLIS_SECURITY_ROOM_FEED_SIZE       = 128,
        ACROPOLIS_SECURITY_ROOM_FEED_SORT_DEPTH = 48,
    };
    EffectWork* work;
    GfxCoord*   coord;
    POLY_FT4*   quad;
    s16         edgeX;
    s16         edgeY;
    u16         centreXBits;
    u16         centreYBits;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    work->scale = task->spawnArg1.value & ACROPOLIS_SECURITY_ROOM_FEED_INDEX_MASK;
    quad->tpage = getTPage(1, GPU_BLEND_ADD, 704, 0);
    setSemiTrans(quad, true);
    setShadeTex(quad, true);
    quad->clut = getClut(0, D_acropolis_security_room_80183970[work->scale].clutY);
    // Preserve the placement coordinates as unsigned halfword bits before edge narrowing.
    centreXBits = D_acropolis_security_room_80183970[work->scale].centreX;
    centreYBits = D_acropolis_security_room_80183970[work->scale].centreY;
    quad->u0    = D_acropolis_security_room_80183970[work->scale].u;
    quad->v0    = D_acropolis_security_room_80183970[work->scale].v;
    quad->u1    = D_acropolis_security_room_80183970[work->scale].u + ACROPOLIS_SECURITY_ROOM_FEED_SIZE - 1;
    quad->v1    = D_acropolis_security_room_80183970[work->scale].v;
    quad->u2    = D_acropolis_security_room_80183970[work->scale].u;
    quad->v2    = D_acropolis_security_room_80183970[work->scale].v + ACROPOLIS_SECURITY_ROOM_FEED_SIZE - 1;
    quad->u3    = D_acropolis_security_room_80183970[work->scale].u + ACROPOLIS_SECURITY_ROOM_FEED_SIZE - 1;
    quad->v3    = D_acropolis_security_room_80183970[work->scale].v + ACROPOLIS_SECURITY_ROOM_FEED_SIZE - 1;
    edgeX       = centreXBits - ACROPOLIS_SECURITY_ROOM_FEED_SIZE / 2;
    quad->x2    = edgeX;
    quad->x0    = edgeX;
    edgeX       = centreXBits + (ACROPOLIS_SECURITY_ROOM_FEED_SIZE / 2 - 1);
    quad->x3    = edgeX;
    quad->x1    = edgeX;
    edgeY       = centreYBits - ACROPOLIS_SECURITY_ROOM_FEED_SIZE / 2;
    quad->y1    = edgeY;
    quad->y0    = edgeY;
    edgeY       = centreYBits + (ACROPOLIS_SECURITY_ROOM_FEED_SIZE / 2 - 1);
    quad->y3    = edgeY;
    quad->y2    = edgeY;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)ACROPOLIS_SECURITY_ROOM_FEED_SORT_DEPTH << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
    effectKillTask(work, task);
}

void acropolisSecurityRoomFallingQuadTask(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_QUAD_MOVING                  = 0,
        ACROPOLIS_SECURITY_ROOM_QUAD_SETTLED                 = 1,
        ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_HALF_SIZE       = 32,
        ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_MIN_DEPTH       = 17,
        ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_SPEED_THRESHOLD = 29,
        ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_STOP_Y          = -419,
        ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_VIEW            = 15,
        ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL      = 7,
    };
    EffectQuadCornersScratch* quadScratch;
    GfxCoord*                 coord;
    EffectWork*               work;
    POLY_FT4*                 quad;
    s32                       cornerIndex;
    s32                       fallSpeed;
    s32                       driftX;
    s32                       driftZ;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadCornersScratch);
    quadScratch = SCRATCH_STACK_CURSOR(EffectQuadCornersScratch);
    coord       = task->extra.coordBody->coord;
    work        = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    if (work->age == 0) {
        work->scale = ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_HALF_SIZE;
        _acropolisSecurityRoomSeedQuadMotion(work);
    }

    // Build the local XZ square, narrowing each transformed view-space corner to 16 bits.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_acropolis_security_room_801839C0); cornerIndex++) {
        _acropolisSecurityRoomTransformQuadCorner(quadScratch, cornerIndex, work, coord);
    }

    // Projection consumes a quad even when the last vertex fails the near-depth test.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    quad->u0 = 0;
    quad->v0 = 0;
    quad->u1 = ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    quad->v1 = 0;
    quad->u2 = 0;
    quad->v2 = ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    quad->u3 = ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    quad->v3 = ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_LAST_TEXEL;
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->depth);
    if (quadScratch->depth >= ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_MIN_DEPTH) {
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 832, 0);
        quad->clut  = getClut(256, 270);
        setShadeTex(quad, true);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadCornersScratch);

    // Draw the entry pose, then move for the next frame; settled quads keep their final pose.
    if (work->index == ACROPOLIS_SECURITY_ROOM_QUAD_MOVING) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        gfxRotMatrixX(&coord->coord, work->period, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&coord->coord, work->step, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;

        // Speed approaches 28/29; horizontal drift damps to zero, then reseeds.
        fallSpeed = work->move.vy;
        if (fallSpeed >= ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_SPEED_THRESHOLD) {
            fallSpeed--;
        } else {
            fallSpeed++;
        }
        work->move.vy = fallSpeed;

        driftX = work->move.vx;
        if (driftX == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx  += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
        } else {
            if (driftX > 0) {
                driftX--;
            } else {
                driftX++;
            }
            work->move.vx = driftX;
        }

        driftZ = work->move.vz;
        if (driftZ == 0) {
            work->move.vz  += work->step % 32;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz  += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
        } else {
            if (driftZ > 0) {
                driftZ--;
            } else {
                driftZ++;
            }
            work->move.vz = driftZ;
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->period   += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 16;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->step     += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 8;
        if (coord->coord.t[1] >= ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_STOP_Y) {
            work->index = ACROPOLIS_SECURITY_ROOM_QUAD_SETTLED;
        }
    }

    work->age = work->age + 1;
    if (gGameSession->location.loc.view != ACROPOLIS_SECURITY_ROOM_FALLING_QUAD_VIEW) {
        effectKillTask(work, task);
    }
}

void acropolisSecurityRoomMonitorGlowTask(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_MIN_DEPTH            = 17,
        ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_EXTENT_DEPTH_PRODUCT = 3072,
        ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_MASK      = 0x70,
        ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_BASE      = 64,
    };
    GfxCoord*              coord;
    EffectWork*            work;
    RoomGlowSpriteScratch* scratch;
    POLY_G4*               quad;
    LINE_G3*               line;
    s16                    brightness;
    s16                    redFactor;
    s16                    greenFactor;
    s32                    armIndex;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    // Project the coordinate origin; both drawing phases share this centre and depth.
    scratch              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
    scratch->worldPos.vx = coord->workm.t[0];
    scratch->worldPos.vy = coord->workm.t[1];
    scratch->worldPos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPos);
    gte_rtps();
    gte_stsxy(&scratch->screenPos);
    gte_stszotz(&scratch->otz);
    if (scratch->otz >= ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_MIN_DEPTH) {
        redFactor           = (task->spawnArg1.value >> 1) & 1;
        greenFactor         = task->spawnArg1.value & 1;
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        brightness          = ((gRandomLcgState >> 16) & ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_MASK) + ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_BRIGHTNESS_BASE;
        scratch->halfExtent = ACROPOLIS_SECURITY_ROOM_MONITOR_GLOW_EXTENT_DEPTH_PRODUCT / scratch->otz;
        for (armIndex = 0; armIndex < 2; armIndex++) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            _acropolisSecurityRoomInitGlowQuad(quad, brightness, redFactor, greenFactor);
            quad->x0 = scratch->screenPos.vx - scratch->halfExtent;
            quad->x1 = quad->x2 = scratch->screenPos.vx;
            quad->x3            = scratch->screenPos.vx + scratch->halfExtent;
            quad->y0 = quad->y2 = quad->y3 = scratch->screenPos.vy;
            quad->y1                       = (scratch->screenPos.vy - scratch->halfExtent) + scratch->halfExtent * (armIndex + armIndex);
            addPrim(&gGpuCurrentOt[((u32)scratch->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF], quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
        for (armIndex = 0; armIndex < 2; armIndex++) {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            _acropolisSecurityRoomInitGlowLine(line, brightness, redFactor, greenFactor);
            line->x0 = scratch->screenPos.vx + scratch->halfExtent * (armIndex * 2 - 1);
            line->y0 = scratch->screenPos.vy - scratch->halfExtent * (armIndex + 1);
            line->x1 = scratch->screenPos.vx;
            line->y1 = scratch->screenPos.vy;
            line->x2 = scratch->screenPos.vx - scratch->halfExtent * (armIndex * 2 - 1);
            line->y2 = scratch->screenPos.vy + scratch->halfExtent * (armIndex + 1);
            addPrim(&gGpuCurrentOt[((u32)scratch->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF], line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    effectKillTask(work, task);
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Per-frame visibility hook for a pick-up prop: the model is drawn with flags
/// 8 at OT offset 0 until the item's 2-bit flag reaches 2, after which it is
/// hidden (flags 0x80).
static void func_acropolis_security_room_80182574(Task* task)
{
    Enemy*     enemy;
    TmdObject* tmd;
    s32        flag;

    enemy = task->spawnArg2.pointer;
    tmd   = task->extra.tmd;
    flag  = areaGetCurrentObjectState((u8)enemy->placeKey);
    viewGetMappedIndex();
    if (flag == 2) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
