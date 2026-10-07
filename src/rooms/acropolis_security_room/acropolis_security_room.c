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
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
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
static s32 _actionPromptHitTestDefault(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
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

/// Prompt and exit states selected by the monitor's hotspot scan.
enum {
    ACROPOLIS_SECURITY_ROOM_MONITOR_STATE_OPEN_PROMPT = 3,
    ACROPOLIS_SECURITY_ROOM_MONITOR_STATE_CLOSE       = 5,
};

/// Prompt and exit states selected by the power-supply hotspot scan.
enum {
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_OPEN_PROMPT = 3,
    ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_CLOSE       = 5,
};

/// Shared start, playback-wait and result-handoff states of the unlock-scene tasks.
enum {
    ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_START  = 0,
    ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_WAIT   = 1,
    ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_FINISH = 2,
};

/// Input updates before room interactions can be triggered after a panel closes.
enum { ACROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES = 10 };

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

/// The whole-unit world displacement the last `_actorContactApplyGridPushback`
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
/// `_actionPromptHitTestDefault`.
extern ActionPromptHotspot D_acropolis_security_room_80182648[];

/// The monitor's grey-wash detents, darkest first. The first five are the
/// levels `GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA` indexes. The sixth value
/// is part of this symbol and is never read; its role is unproven.
extern s16 D_acropolis_security_room_801826B4[];

/// The single-entry `TaskDesc` table the script spawns its child task from:
/// `_acropolisSecurityRoomPowerSupplyCursorTask`.
extern TaskDesc D_acropolis_security_room_801826C0[];
/// The script's message table, parked in `Task::msgTable`.
extern TaskMessageEntry D_acropolis_security_room_801826CC[];

/// The script's hotspot table, terminated by `ACTION_PROMPT_HOTSPOT_END`.
extern ActionPromptHotspot D_acropolis_security_room_801826DC[];

/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `_acropolisSecurityRoomRightUnlockSoundTask`, index 1 is
/// `_acropolisSecurityRoomLeftUnlockMovieTask`.
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

static void _actionPromptResetDefault(Task* task);
static void func_acropolis_security_room_8017D930(Task* task);
static void _acropolisSecurityRoomMessageIdle(Task* task);
static void func_acropolis_security_room_8017D9DC(Task* task);
static void _acropolisSecurityRoomMonitorScanHotspots(Task* task);
static void func_acropolis_security_room_8017DC7C(Task* task);
static void _acropolisSecurityRoomDrawMonitorWash(s16 washLevel);
static void _acropolisSecurityRoomDrawMonitorSweep(Task* task);
static void _acropolisSecurityRoomMonitorArmCursor(Task* task);
static void func_acropolis_security_room_8017EA5C(Task* task);
static void _acropolisSecurityRoomMonitorClose(Task* task);
static void func_acropolis_security_room_8017EB9C(Task* task);
static void _acropolisSecurityRoomPowerSupplyScanHotspots(Task* task);
static void _actionPromptMoveCursors(Task* task);
static void _actionPromptDrawCursor(s32 cursorX, s32 cursorY, s32 cursorMode);
static void func_acropolis_security_room_8017FA18(Task* task);
static void _acropolisSecurityRoomPowerSupplyArmCursor(Task* task);
static void func_acropolis_security_room_8017FB54(Task* task);
static void func_acropolis_security_room_8017FBA4(Task* task);
static void _acropolisSecurityRoomPowerSupplyClose(Task* task);
static s32  _actionPromptHitTest(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
static void _acropolisSecurityRoomShowReleasedLocks(s32 releasedLocks);
static void _acropolisSecurityRoomPowerSupplyFadeToRightUnlock(Task* task);
static void func_acropolis_security_room_8017FF0C(Task* task);
static void _acropolisSecurityRoomPowerSupplyWaitRightUnlock(Task* task);
static void func_acropolis_security_room_8017FFD0(Task* task);
static void _acropolisSecurityRoomPowerSupplyRestoreRoomView(Task* task);
static void func_acropolis_security_room_80180030(Task* task);
static void func_acropolis_security_room_801800A4(Task* task);
static void func_acropolis_security_room_8018014C(Task* task);
static void func_acropolis_security_room_801801C4(Task* task);
static void func_acropolis_security_room_80180218(Task* task);
static void _actionPromptReset(Task* task);
static void _acropolisSecurityRoomDrawSweepLine(Task* task);

static void _acropolisSecurityRoomMonitorCursorTask(Task* task);
static void _acropolisSecurityRoomPowerSupplyCursorTask(Task* task);
static s32  _acropolisSecurityRoomPowerSupplyUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedPayload);
static void _acropolisSecurityRoomRightUnlockSoundTask(Task* task);
static void _acropolisSecurityRoomLeftUnlockMovieTask(Task* task);

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
static s32 _acropolisSecurityRoomForwardKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedPayload);
s32        func_acropolis_security_room_8017D708(Task*, s32, s32, s32);
s32        func_acropolis_security_room_8017D740(Task* task, s32 msgId, DirectionActionRequest* request, s32 arg3);

TaskMessageEntry D_acropolis_security_room_801825DC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisSecurityRoomResolveTransition },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_security_room_8017D740 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_security_room_8017D708 },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisSecurityRoomForwardKeyItemUse },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_acropolis_security_room_80182604 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

TaskDesc D_acropolis_security_room_80182618[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_security_room_8017D77C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_security_room_8017D834, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc D_acropolis_security_room_8018263C = { { { TASK_BODY_NONE, 192 } }, _acropolisSecurityRoomMonitorCursorTask, { .value = 0 } };

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
    { { { TASK_BODY_NONE, 192 } }, _acropolisSecurityRoomPowerSupplyCursorTask, { .value = 0 } },
};

TaskMessageEntry D_acropolis_security_room_801826CC[2] = {
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisSecurityRoomPowerSupplyUseKeyItem },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActionPromptHotspot D_acropolis_security_room_801826DC[3] = {
    { -68, 20, 24, 24, 1, 0, 0 },
    { 54, 20, 24, 24, 2, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

TaskDesc D_acropolis_security_room_80182700[2] = {
    { { { TASK_BODY_NONE, 192 } }, _acropolisSecurityRoomRightUnlockSoundTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisSecurityRoomLeftUnlockMovieTask, { .value = 0 } },
};

u16 D_acropolis_security_room_80182718[256] = { 0 };

u16 D_acropolis_security_room_80182918[256] = {
#include "assets/acropolis_security_room_clut_05358.inc"
};

u16 D_acropolis_security_room_80182B18[256] = {
#include "assets/acropolis_security_room_clut_05558.inc"
};

u16 D_acropolis_security_room_80182D18[256] = {
#include "assets/acropolis_security_room_clut_05758.inc"
};

u16 D_acropolis_security_room_80182F18[256] = {
#include "assets/acropolis_security_room_clut_05958.inc"
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

/// Forwards a selected key item to the live power-supply panel.
///
/// Receives `ROOM_MESSAGE_USE_KEY_ITEM`; `itemId` is a collection catalogue id
/// and the second payload is unused by the panel. Returns its item-menu reply
/// unchanged, or `ROOM_KEY_ITEM_USE_REFUSED` while the panel is absent.
static s32 _acropolisSecurityRoomForwardKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedPayload)
{
    Task* panelTask;
    s32   useResult;

    panelTask = D_acropolis_security_room_801855AC;
    if (panelTask == NULL) {
        useResult = ROOM_KEY_ITEM_USE_REFUSED;
    } else {
        useResult = taskMessageDispatch(panelTask, messageId, itemId, unusedPayload);
    }
    return useResult;
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
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (taskPollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
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
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (taskPollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
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
    _acropolisSecurityRoomMonitorArmCursor,
    _acropolisSecurityRoomMonitorScanHotspots,
    func_acropolis_security_room_8017EA5C,
    func_acropolis_security_room_8017DC7C,
    _acropolisSecurityRoomMonitorClose,
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
    displayAcquireMenuHold();
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

/// Draws the monitor picture and scans its camera and brightness hotspots.
///
/// Requires live monitor work and the loaded, sentinel-terminated hotspot
/// table. CAP playback hides and stops the cursor. Confirm latches the first
/// hit and opens its commands; cancel selects monitor cleanup.
static void _acropolisSecurityRoomMonitorScanHotspots(Task* task)
{
    _AcropolisSecurityRoomMonitorWork* work;
    ActionPromptHotspot*               hotspot;
    ActionPrompt*                      prompt;

    hotspot = D_acropolis_security_room_80182648;
    prompt  = D_80114D28;
    work    = task->work;
    _acropolisSecurityRoomDrawMonitorWash(work->screenLevel - ACROPOLIS_SECURITY_ROOM_MONITOR_SCREEN_BIAS);
    _acropolisSecurityRoomDrawMonitorSweep(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    // Keep drawing the monitor while CAP temporarily owns input.
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if ((prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) && (hotspot->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = ACROPOLIS_SECURITY_ROOM_MONITOR_STATE_OPEN_PROMPT;
                    return;
                }
                hotspot++;
            } while (hotspot->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = ACROPOLIS_SECURITY_ROOM_MONITOR_STATE_CLOSE;
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
    _acropolisSecurityRoomDrawMonitorSweep(task);
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

/// Initializes the monitor sweep's semitransparent grey line and its horizontal endpoints.
///
/// Borrows one writable packet; the drawer supplies Y and ordering-table linkage.
static inline void _acropolisSecurityRoomInitMonitorSweepLine(LINE_F2* line)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_SWEEP_GREY  = 0x60,
        ACROPOLIS_SECURITY_ROOM_SWEEP_LEFT  = -0x66,
        ACROPOLIS_SECURITY_ROOM_SWEEP_RIGHT = 0x6C,
    };

    setLineF2(line);
    setSemiTrans(line, true);
    line->r0 = ACROPOLIS_SECURITY_ROOM_SWEEP_GREY;
    line->g0 = ACROPOLIS_SECURITY_ROOM_SWEEP_GREY;
    line->b0 = ACROPOLIS_SECURITY_ROOM_SWEEP_GREY;
    line->x0 = ACROPOLIS_SECURITY_ROOM_SWEEP_LEFT;
    line->x1 = ACROPOLIS_SECURITY_ROOM_SWEEP_RIGHT;
}

/// Draws the monitor's horizontal grey sweep and advances its repeating frame counter.
///
/// Requires live monitor work. The line runs from X -102 to 108 at Y -95..55,
/// in centre-origin screen pixels, wrapping after 151 draws. Reserves one
/// `LINE_F2` and one `DR_MODE` in the frame arena, linked at ordering-table slot 14.
static void _acropolisSecurityRoomDrawMonitorSweep(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_SWEEP_TOP            = 0x5F,
        ACROPOLIS_SECURITY_ROOM_SWEEP_OT_SLOT        = 0xE,
        ACROPOLIS_SECURITY_ROOM_SWEEP_TEXTURE_PAGE_X = 640,
    };
    _AcropolisSecurityRoomMonitorWork* work;
    LINE_F2*                           line;
    DR_MODE*                           drawMode;
    s16                                sweepY;

    line           = gGpuPrimCursor;
    work           = task->work;
    gGpuPrimCursor = line + 1;
    _acropolisSecurityRoomInitMonitorSweepLine(line);
    sweepY   = work->sweepTimer - ACROPOLIS_SECURITY_ROOM_SWEEP_TOP;
    line->y1 = sweepY;
    line->y0 = sweepY;
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_SWEEP_OT_SLOT, line);
    // Set the blend page before the line is consumed; OT insertion reverses order.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_AVERAGE, ACROPOLIS_SECURITY_ROOM_SWEEP_TEXTURE_PAGE_X, 0));
    addPrim(gGpuCurrentOt + ACROPOLIS_SECURITY_ROOM_SWEEP_OT_SLOT, drawMode);
    work->sweepTimer++;
    if (work->sweepTimer >= ACROPOLIS_SECURITY_ROOM_MONITOR_SWEEP_PERIOD) {
        work->sweepTimer = 0;
    }
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Runs the monitor cursor through reset and per-frame movement.
///
/// The descriptor starts at state 0; reset advances to state 1, which remains
/// active until the monitor kills this child. Both callbacks use the shared
/// per-port prompt state. No work block is required.
static void _acropolisSecurityRoomMonitorCursorTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptResetDefault,
        _actionPromptMoveCursorsDefault,
    };

    states[task->state](task);
}

/// Enables the monitor cursor and advances to hotspot scanning.
///
/// Clears only the derived pixel position. The movement task replaces it
/// from its fixed-point coordinates; the command menu copies those pixels
/// when a hotspot is confirmed.
static void _acropolisSecurityRoomMonitorArmCursor(Task* task)
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
    _acropolisSecurityRoomDrawMonitorSweep(task);
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Saves the monitor wash detent index, defaulting to the darkest detent.
///
/// `screenLevel` is the signed, word-sized view of the stored wash encoding.
/// Only the first five table entries are detents; the sixth is not searched.
static inline void _acropolisSecurityRoomSaveScreenLevel(s32 screenLevel)
{
    enum { ACROPOLIS_SECURITY_ROOM_SCREEN_DETENT_COUNT = 5 };
    s32 detentIndex;

    for (detentIndex = 0; detentIndex < ACROPOLIS_SECURITY_ROOM_SCREEN_DETENT_COUNT; detentIndex++) {
        if (screenLevel == D_acropolis_security_room_801826B4[detentIndex]) {
            gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA, detentIndex);
            return;
        }
    }
    gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA, 0);
}

/// Saves the monitor brightness and returns control to the room.
///
/// Requires live monitor work and its cursor child. Restores view 4, releases
/// the panel display hold, clears the HUD/event holds and kills the cursor.
/// Requests result 0 so the opener can poll and release this task and its work.
static void _acropolisSecurityRoomMonitorClose(Task* task)
{
    enum { ACROPOLIS_SECURITY_ROOM_MONITOR_RETURN_VIEW = 4 };
    _AcropolisSecurityRoomMonitorWork* work;

    work       = task->work;
    D_80114D08 = ACROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES;
    _acropolisSecurityRoomSaveScreenLevel((s16)work->screenLevel);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_SECURITY_ROOM_MONITOR_RETURN_VIEW;
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
    if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    capRunCommand(0xE, CAP_PLAYBACK_IN_PLACE);
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
    _acropolisSecurityRoomPowerSupplyArmCursor,
    _acropolisSecurityRoomPowerSupplyScanHotspots,
    func_acropolis_security_room_8017FB54,
    func_acropolis_security_room_8017FBA4,
    _acropolisSecurityRoomPowerSupplyClose,
    func_acropolis_security_room_801800A4,
    func_acropolis_security_room_8018014C,
    func_acropolis_security_room_801801C4,
    func_acropolis_security_room_80180218,
    _acropolisSecurityRoomPowerSupplyFadeToRightUnlock,
    func_acropolis_security_room_8017FF0C,
    _acropolisSecurityRoomPowerSupplyWaitRightUnlock,
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

/// Scans the power-supply panel's left and right lock hotspots.
///
/// Requires live panel work and the loaded, sentinel-terminated hotspot
/// table. CAP playback stops the cursor. Confirm latches the first hit and
/// opens its commands; a miss clears the pending key and cancel selects cleanup.
static void _acropolisSecurityRoomPowerSupplyScanHotspots(Task* task)
{
    ActionPrompt*                          prompt  = D_80114D28;
    ActionPromptHotspot*                   hotspot = D_acropolis_security_room_801826DC;
    _AcropolisSecurityRoomPowerSupplyWork* work    = task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    // CAP temporarily owns input; retain the pending key during playback.
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
            for (; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_OPEN_PROMPT;
                    return;
                }
            }
        }
    } else {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
        prompt->mode  = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_STATE_CLOSE;
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
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET, PAD_INPUT_SUPPRESS_GAMEPLAY);
            areaApplySavedUpdates(D_acropolis_security_room_80184F80);
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
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET, PAD_INPUT_SUPPRESS_GAMEPLAY);
            areaApplySavedUpdates(D_acropolis_security_room_80184F50);
            if (gameFlagGetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE) < 3) {
                areaApplySavedUpdates(D_acropolis_security_room_80184F78);
            } else {
                areaApplySavedUpdates(D_acropolis_security_room_80184F7C);
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

/// Selects the additional private cursor task, with signature `void(Task*)`.
#undef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursors
#undef ACTION_PROMPT_DRAW_CURSOR
/// Selects the second prompt's private drawer for both fragments.
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursor
#include "../../shared/action_prompt_move_cursors.inc.c"
#undef ACTION_PROMPT_MOVE_CURSORS_TASK
#define ACTION_PROMPT_MOVE_CURSORS_TASK _actionPromptMoveCursorsDefault

#include "../../shared/action_prompt_draw_cursor.inc.c"
#undef ACTION_PROMPT_DRAW_CURSOR
#define ACTION_PROMPT_DRAW_CURSOR _actionPromptDrawCursorDefault

/// Runs the power-supply cursor through reset and per-frame movement.
///
/// The descriptor starts at state 0; reset advances to state 1, which remains
/// active until the panel kills this child. Both callbacks use the shared
/// per-port prompt state. No work block is required.
static void _acropolisSecurityRoomPowerSupplyCursorTask(Task* task)
{
    TaskFunc states[] = {
        _actionPromptReset,
        _actionPromptMoveCursors,
    };

    states[task->state](task);
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
    displayAcquireMenuHold();
    for (hs = D_acropolis_security_room_801826DC; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
        hs->hit = 0;
    }
}

/// Enables the power-supply cursor and advances to lock hotspot scanning.
///
/// Clears only the derived pixel position. The movement task replaces it
/// from its fixed-point coordinates; the command menu copies those pixels
/// when a lock is confirmed.
static void _acropolisSecurityRoomPowerSupplyArmCursor(Task* task)
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

/// Closes the power-supply panel without playing an unlock scene.
///
/// Requires the live cursor child. Restores the player model and view 3,
/// releases the display hold and clears the HUD/event holds. Kills the cursor
/// and requests result 0 for the opener to poll and release the panel work.
static void _acropolisSecurityRoomPowerSupplyClose(Task* task)
{
    enum { ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RETURN_VIEW = 3 };
    D_80114D08 = ACROPOLIS_SECURITY_ROOM_PANEL_REARM_UPDATES;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_RETURN_VIEW;
    displayReleaseMenuHold();
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, 0);
}

/// Selects the private hotspot tester for the power-supply prompt.
///
/// The static instance uses the signature documented in `action_prompt.h`.
#undef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTest
#include "../../shared/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault

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

/// Latches a selected key for the power-supply panel's confirmed lock.
///
/// Receives `ROOM_MESSAGE_USE_KEY_ITEM` with a collection catalogue `itemId`
/// and unused second payload. Red, blue and Parthenon keys request the menu
/// used notice; other items clear the latch and are refused. Acceptance does
/// not consume the item: the later lock handler checks and consumes its own key.
static s32 _acropolisSecurityRoomPowerSupplyUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedPayload)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;

    if (itemId == INVENTORY_COLLECTION_ID_PARTHENON_KEY) {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_OTHER;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    if (itemId == INVENTORY_COLLECTION_ID_RED_KEY) {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_RIGHT;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    if (itemId == INVENTORY_COLLECTION_ID_BLUE_KEY) {
        work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_LEFT;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    work->usedKey = ACROPOLIS_SECURITY_ROOM_POWER_SUPPLY_KEY_NONE;
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Fades the panel to black before the right-lock unlock movie.
///
/// `timer` starts at zero and increases by four per callback, selecting view
/// 16 after 64 callbacks. The subtractive RGB level is its low byte; the
/// unlock fade sound starts halfway through. Resets the timer for scene setup.
static void _acropolisSecurityRoomPowerSupplyFadeToRightUnlock(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_STEP        = 4,
        ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_SOUND_LEVEL = 0x80,
        ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_END         = 0x100,
        ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_VIEW       = 0x10,
        ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_SOUND       = SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 2),
    };
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    u8                                     fadeLevel;

    fadeLevel = work->timer;
    fadeDrawOverlay(fadeLevel, fadeLevel, fadeLevel, GPU_BLEND_SUBTRACT);
    work->timer = work->timer + ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_STEP;
    if (work->timer == ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_SOUND_LEVEL) {
        sndEvtRequestScriptStart(ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_SOUND, 0, 0);
    }
    if (work->timer >= ACROPOLIS_SECURITY_ROOM_UNLOCK_FADE_END) {
        work->timer                                                = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_VIEW;
        task->state                                                = task->state + 1;
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

/// Waits for the right-lock sound task's result handoff before restoring the room.
///
/// Requires the live `sceneTask` spawned for view 16. Polling dispatches its
/// exit and releases its work; advances once and does not inspect the result.
static void _acropolisSecurityRoomPowerSupplyWaitRightUnlock(Task* task)
{
    _AcropolisSecurityRoomPowerSupplyWork* work = task->work;
    s32                                    sceneResult;

    if (taskPollKill(work->sceneTask, &sceneResult) != 0) {
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8017FFD0(Task* arg0)
{
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
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
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    displayReleaseMenuHold();
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_INPUT_SUPPRESS_GAMEPLAY);
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
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_80180218(Task* task)
{
    D_80114D08                                                 = 0xA;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    displayReleaseMenuHold();
    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_INPUT_SUPPRESS_GAMEPLAY);
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

// Bind the additional private reset callback, with signature void(Task*).
#undef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptReset
#include "../../shared/action_prompt_reset.inc.c"
#undef ACTION_PROMPT_RESET_TASK
#define ACTION_PROMPT_RESET_TASK _actionPromptResetDefault

/// Plays the sound loop over the right-lock movie and hands completion to the panel.
///
/// View 16 starts its movie through view-stream loading; this task supplies sound.
/// Owns a four-byte zeroed work block, released by the panel's poll.
/// Fades the loop once at movie frame 70 or on Start; stop control 20 sets
/// the gain step per audio update. Queue idle or Start advances to handoff.
static void _acropolisSecurityRoomRightUnlockSoundTask(Task* task)
{
    enum {
        ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_FADE_CUE_FRAME = 70,
        ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_STOP_CONTROL   = 20,
    };
    CdCmdQueue*                          queue;
    _AcropolisSecurityRoomMovieLoopWork* work;
    _AcropolisSecurityRoomMovieLoopWork* allocatedWork;

    queue = &gCdCmdQueue;
    work  = task->work;
    switch (task->state) {
        case ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_START:
            allocatedWork = memCalloc(sizeof(_AcropolisSecurityRoomMovieLoopWork), 0);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
            sndEvtRequestScriptStart(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0, 0);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_WAIT:
            if (queue->movieFrame >= ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_FADE_CUE_FRAME && work->fadeStarted == 0) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_STOP_CONTROL);
                work->fadeStarted = 1;
            }
            // Idle and Start advance independently, even when both occur together.
            if (cdCmdIsIdle()) {
                task->state = task->state + 1;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            if (work->fadeStarted == 0) {
                sndEvtRequestScriptStop(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, ACROPOLIS_SECURITY_ROOM_RIGHT_UNLOCK_STOP_CONTROL);
            }
            task->state = task->state + 1;
            return;
        case ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_FINISH:
            taskRequestKill(task, 0);
            return;
    }
}

/// Starts the left-lock movie and hands completion or skip back to the panel.
///
/// Runs in view 14, whose movie is not selected by view-stream loading. The
/// location must resolve to a loaded stream slot (0..14); the signed lookup
/// result narrows to one byte without a failure check. Queue idle or Start
/// advances to result 0 handoff; this task does not request CD cancellation.
static void _acropolisSecurityRoomLeftUnlockMovieTask(Task* task)
{
    u8          commandArgs[sizeof(((CdCmdEntry*)0)->args)];
    CdCmdQueue* queue;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_START:
            queue->movieFrame = 1;
            // Enqueue reads four bytes; playback interprets only the slot byte.
            commandArgs[0] = streamFindMovieSlot(&gGameSession->location.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, commandArgs);
            task->state = task->state + 1;
            return;
        case ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_WAIT:
            if (cdCmdIsIdle() || padIsStartPressed() != 0) {
                task->state = task->state + 1;
            }
            return;
        case ACROPOLIS_SECURITY_ROOM_UNLOCK_STATE_FINISH:
            taskRequestKill(task, 0);
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
