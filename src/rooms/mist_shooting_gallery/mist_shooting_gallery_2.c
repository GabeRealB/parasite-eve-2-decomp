#include "types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
s32 gMistShootingGalleryUnscaledBp;

#include "rooms/mist_shooting_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "mist_shooting_gallery_private.h"

#include "actors/actor_107600.h"
#include "actors/actor_215100.h"
#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"
/// Signed texture-frame counter for the gallery's repeating six-cell sprite strip.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/effects/sprite_quad.h"
#include "../../shared/effects/beam_strip.h"

#define ROOM_PART declarations_mist_shooting_gallery_2
#include "assets/rooms/mist/shooting_gallery/declarations.inc"

/// Marker values of `_MistShootingGallerySpawn::frame`, above every frame the
/// script clock can reach.
enum {
    MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT = 0xFFF0, // Where the script clock saturates, so it never equals a marker
    MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR  = 0xFFF1, // Hold the script here until no spawned target is left in play
    MIST_SHOOTING_GALLERY_SPAWN_END         = 0xFFFF, // End of the script; nothing further spawns
};

static void   _mistShootingGalleryExitCourse(Task* task);
static void   _mistShootingGalleryDrawCountdownClock(MistShootingGalleryWork* work);
static u16    _mistShootingGalleryTickCourseClock(MistShootingGalleryWork* work);
static void   _mistShootingGalleryShowCaption(s16 commandIndex, s16 captionKey, s16 dismissButtons);
static Enemy* _mistShootingGallerySpawnTarget(Task* controller, const _MistShootingGallerySpawn* spawn);
static void   _mistShootingGalleryDrawClockGlyph(s32 screenX, s16 screenY, s32 glyph);
static void   _mistShootingGalleryDrawRedFlash(u8 redIntensity);
static void   _mistShootingGalleryInitCourse(Task* controller);
static void   _mistShootingGalleryRunCourse0(Task* controller);
static void   _mistShootingGalleryRunCourse1(Task* controller);
static void   _mistShootingGalleryRunCourse2(Task* controller);
static void   _mistShootingGalleryRunCourse3(Task* controller);
static void   _mistShootingGalleryRunCourse4(Task* controller);
static void   _mistShootingGalleryFinishCourse(Task* controller);
static void   _mistShootingGalleryRunCourse(Task* controller);

/// Controller state handlers: 0 initialize, 1 run the course, 2 finish and show results.
///
/// Copied by value before dispatch; the selector is `Task.state` in 0..2.
/// Entries borrow code in the loaded gallery overlay and receive the live task.
static const TaskFuncTable3 _gMistShootingGalleryControllerStates = {
    { _mistShootingGalleryInitCourse, _mistShootingGalleryRunCourse, _mistShootingGalleryFinishCourse },
};

/// Course handlers indexed by the controller work's zero-based course, 0..4.
///
/// Copied by value before dispatch. Every entry receives the live initialized
/// controller; the gallery overlay must remain loaded throughout playback.
static const TaskFuncTable5 _gMistShootingGalleryCourseHandlers = {
    {
        _mistShootingGalleryRunCourse0,
        _mistShootingGalleryRunCourse1,
        _mistShootingGalleryRunCourse2,
        _mistShootingGalleryRunCourse3,
        _mistShootingGalleryRunCourse4,
    },
};

static void _mistShootingGalleryCaptionModeTask(Task* task);

static void _mistShootingGalleryCourseTask(Task* task);
static void _mistShootingGalleryRedFlashTask(Task* task);

/// Buttons that dismiss ordinary course captions; special prompts choose one button.
enum {
    MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS = PAD_BUTTON_START | PAD_BUTTON_CIRCLE | PAD_BUTTON_CROSS | PAD_BUTTON_SQUARE,
};

TaskDesc gMistShootingGalleryTaskDescs[MIST_SHOOTING_GALLERY_TASK_COUNT] = {
    [MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER] = { { { TASK_BODY_NONE, 192 } }, _mistShootingGalleryCourseTask, { .value = 0 } },
    [MIST_SHOOTING_GALLERY_TASK_RED_FLASH]         = { { { TASK_BODY_NONE, 192 } }, _mistShootingGalleryRedFlashTask, { .value = 0 } },
};

/// Modal caption task descriptor, with no body and priority 192.
///
/// Its first spawn argument is the button-dismissal mask. The display queue
/// borrows the descriptor until spawning; caption resources and the callback's
/// gallery overlay must remain loaded until the mode closes. No work is owned.
static TaskDesc _gMistShootingGalleryCaptionModeTaskDesc = { { { TASK_BODY_NONE, 192 } }, _mistShootingGalleryCaptionModeTask, { .value = 0 } };

#include "assets/rooms/mist/shooting_gallery/waves.inc"
/// One pointer slot selecting course 0's finite wave script.
///
/// Only index 0 is valid. The course borrows the script for its lifetime; this
/// slot is separate from the neighbouring course pointers. The one-element
/// array preserves ordering of its load against controller phase stores.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse0WaveScriptRefs[1] = {
    _gMistShootingGalleryCourse0WaveScript,
};

/// Borrowed script pointer for course 1's finite target waves.
///
/// Refers to the complete 66-record script, including clear and end markers;
/// both the slot and records remain live while the gallery overlay is loaded.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse1WaveScriptRef = _gMistShootingGalleryCourse1WaveScript;

/// Borrowed wave-script pointer for zero-based course 2's GPS tutorial.
///
/// Refers to 75 records including WAIT_CLEAR and END; seeds three targets and
/// repeats from record 14/frame 240 after the final clearance. The independent
/// slot and read-only records remain live throughout this gallery visit.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse2WaveScriptRef = _gMistShootingGalleryCourse2WaveScript;

/// Borrowed wave-script pointer for zero-based course 3's weapon-swap tutorial.
///
/// Refers to 106 records including WAIT_CLEAR and END; seeds two targets and
/// repeats from record 9/frame 144 after the final clearance. The independent
/// slot and read-only records remain live throughout this gallery visit.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse3WaveScriptRef = _gMistShootingGalleryCourse3WaveScript;

/// Borrowed wave-script pointer for zero-based course 4's combat tutorial.
///
/// Refers to 80 records including WAIT_CLEAR and END; repeats from record 11/
/// frame 688 after the final clearance. The independent slot and read-only
/// records remain live throughout this gallery visit.
static const _MistShootingGallerySpawn* _gMistShootingGalleryCourse4WaveScriptRef = _gMistShootingGalleryCourse4WaveScript;

static TmdBone _gMistShootingGalleryModel093FCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_093FC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel093FCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_093FC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel093FCVerts[10] = {
#include "assets/mist_shooting_gallery_model_093FC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel093FCNormals[6] = {
#include "assets/mist_shooting_gallery_model_093FC_normals.inc"
};

static u32 _gMistShootingGalleryModel093FCStream[73] = {
#include "assets/mist_shooting_gallery_model_093FC_stream.inc"
};

TmdSource gMistShootingGalleryModel093FC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel093FCPartVerts,
    _gMistShootingGalleryModel093FCVerts,
    _gMistShootingGalleryModel093FCNormals,
    _gMistShootingGalleryModel093FCSkeleton,
    _gMistShootingGalleryModel093FCStream,
};

static TmdBone _gMistShootingGalleryModel095ECSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_095EC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel095ECPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_095EC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel095ECVerts[10] = {
#include "assets/mist_shooting_gallery_model_095EC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel095ECNormals[6] = {
#include "assets/mist_shooting_gallery_model_095EC_normals.inc"
};

static u32 _gMistShootingGalleryModel095ECStream[73] = {
#include "assets/mist_shooting_gallery_model_095EC_stream.inc"
};

TmdSource gMistShootingGalleryModel095EC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel095ECPartVerts,
    _gMistShootingGalleryModel095ECVerts,
    _gMistShootingGalleryModel095ECNormals,
    _gMistShootingGalleryModel095ECSkeleton,
    _gMistShootingGalleryModel095ECStream,
};

static TmdBone _gMistShootingGalleryModel097DCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_097DC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel097DCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_097DC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel097DCVerts[10] = {
#include "assets/mist_shooting_gallery_model_097DC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel097DCNormals[6] = {
#include "assets/mist_shooting_gallery_model_097DC_normals.inc"
};

static u32 _gMistShootingGalleryModel097DCStream[73] = {
#include "assets/mist_shooting_gallery_model_097DC_stream.inc"
};

TmdSource gMistShootingGalleryModel097DC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel097DCPartVerts,
    _gMistShootingGalleryModel097DCVerts,
    _gMistShootingGalleryModel097DCNormals,
    _gMistShootingGalleryModel097DCSkeleton,
    _gMistShootingGalleryModel097DCStream,
};

static TmdBone _gMistShootingGalleryModel099CCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_099CC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel099CCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_099CC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel099CCVerts[10] = {
#include "assets/mist_shooting_gallery_model_099CC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel099CCNormals[6] = {
#include "assets/mist_shooting_gallery_model_099CC_normals.inc"
};

static u32 _gMistShootingGalleryModel099CCStream[73] = {
#include "assets/mist_shooting_gallery_model_099CC_stream.inc"
};

TmdSource gMistShootingGalleryModel099CC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel099CCPartVerts,
    _gMistShootingGalleryModel099CCVerts,
    _gMistShootingGalleryModel099CCNormals,
    _gMistShootingGalleryModel099CCSkeleton,
    _gMistShootingGalleryModel099CCStream,
};

static TmdBone _gMistShootingGalleryModel09BBCSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_09BBC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel09BBCPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_09BBC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel09BBCVerts[10] = {
#include "assets/mist_shooting_gallery_model_09BBC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel09BBCNormals[6] = {
#include "assets/mist_shooting_gallery_model_09BBC_normals.inc"
};

static u32 _gMistShootingGalleryModel09BBCStream[73] = {
#include "assets/mist_shooting_gallery_model_09BBC_stream.inc"
};

TmdSource gMistShootingGalleryModel09BBC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel09BBCPartVerts,
    _gMistShootingGalleryModel09BBCVerts,
    _gMistShootingGalleryModel09BBCNormals,
    _gMistShootingGalleryModel09BBCSkeleton,
    _gMistShootingGalleryModel09BBCStream,
};

static TmdBone _gMistShootingGalleryModel09DACSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_09DAC_skeleton.inc"
};

static u32 _gMistShootingGalleryModel09DACPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_09DAC_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel09DACVerts[10] = {
#include "assets/mist_shooting_gallery_model_09DAC_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel09DACNormals[6] = {
#include "assets/mist_shooting_gallery_model_09DAC_normals.inc"
};

static u32 _gMistShootingGalleryModel09DACStream[73] = {
#include "assets/mist_shooting_gallery_model_09DAC_stream.inc"
};

TmdSource gMistShootingGalleryModel09DAC = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel09DACPartVerts,
    _gMistShootingGalleryModel09DACVerts,
    _gMistShootingGalleryModel09DACNormals,
    _gMistShootingGalleryModel09DACSkeleton,
    _gMistShootingGalleryModel09DACStream,
};

static TmdBone _gMistShootingGalleryModel09F9CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_09F9C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel09F9CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_09F9C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel09F9CVerts[10] = {
#include "assets/mist_shooting_gallery_model_09F9C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel09F9CNormals[6] = {
#include "assets/mist_shooting_gallery_model_09F9C_normals.inc"
};

static u32 _gMistShootingGalleryModel09F9CStream[73] = {
#include "assets/mist_shooting_gallery_model_09F9C_stream.inc"
};

TmdSource gMistShootingGalleryModel09F9C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel09F9CPartVerts,
    _gMistShootingGalleryModel09F9CVerts,
    _gMistShootingGalleryModel09F9CNormals,
    _gMistShootingGalleryModel09F9CSkeleton,
    _gMistShootingGalleryModel09F9CStream,
};

static TmdBone _gMistShootingGalleryModel0A18CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A18C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A18CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A18C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A18CVerts[10] = {
#include "assets/mist_shooting_gallery_model_0A18C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A18CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0A18C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A18CStream[73] = {
#include "assets/mist_shooting_gallery_model_0A18C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A18C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0A18CPartVerts,
    _gMistShootingGalleryModel0A18CVerts,
    _gMistShootingGalleryModel0A18CNormals,
    _gMistShootingGalleryModel0A18CSkeleton,
    _gMistShootingGalleryModel0A18CStream,
};

static TmdBone _gMistShootingGalleryModel0A37CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A37C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A37CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A37C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A37CVerts[10] = {
#include "assets/mist_shooting_gallery_model_0A37C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A37CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0A37C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A37CStream[73] = {
#include "assets/mist_shooting_gallery_model_0A37C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A37C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0A37CPartVerts,
    _gMistShootingGalleryModel0A37CVerts,
    _gMistShootingGalleryModel0A37CNormals,
    _gMistShootingGalleryModel0A37CSkeleton,
    _gMistShootingGalleryModel0A37CStream,
};

static TmdBone _gMistShootingGalleryModel0A56CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A56C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A56CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A56C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A56CVerts[10] = {
#include "assets/mist_shooting_gallery_model_0A56C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A56CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0A56C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A56CStream[73] = {
#include "assets/mist_shooting_gallery_model_0A56C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A56C = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0A56CPartVerts,
    _gMistShootingGalleryModel0A56CVerts,
    _gMistShootingGalleryModel0A56CNormals,
    _gMistShootingGalleryModel0A56CSkeleton,
    _gMistShootingGalleryModel0A56CStream,
};

static TmdBone _gMistShootingGalleryModel0A81CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0A81C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0A81CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0A81C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A81CVerts[24] = {
#include "assets/mist_shooting_gallery_model_0A81C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0A81CNormals[16] = {
#include "assets/mist_shooting_gallery_model_0A81C_normals.inc"
};

static u32 _gMistShootingGalleryModel0A81CStream[150] = {
#include "assets/mist_shooting_gallery_model_0A81C_stream.inc"
};

TmdSource gMistShootingGalleryModel0A81C = {
    0,
    1056,
    0,
    1,
    _gMistShootingGalleryModel0A81CPartVerts,
    _gMistShootingGalleryModel0A81CVerts,
    _gMistShootingGalleryModel0A81CNormals,
    _gMistShootingGalleryModel0A81CSkeleton,
    _gMistShootingGalleryModel0A81CStream,
};

static TmdBone _gMistShootingGalleryModel0AB30Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0AB30_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0AB30PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0AB30_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AB30Verts[8] = {
#include "assets/mist_shooting_gallery_model_0AB30_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AB30Normals[6] = {
#include "assets/mist_shooting_gallery_model_0AB30_normals.inc"
};

static u32 _gMistShootingGalleryModel0AB30Stream[42] = {
#include "assets/mist_shooting_gallery_model_0AB30_stream.inc"
};

TmdSource gMistShootingGalleryModel0AB30 = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0AB30PartVerts,
    _gMistShootingGalleryModel0AB30Verts,
    _gMistShootingGalleryModel0AB30Normals,
    _gMistShootingGalleryModel0AB30Skeleton,
    _gMistShootingGalleryModel0AB30Stream,
};

static TmdBone _gMistShootingGalleryModel0AC94Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0AC94_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0AC94PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0AC94_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AC94Verts[8] = {
#include "assets/mist_shooting_gallery_model_0AC94_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AC94Normals[6] = {
#include "assets/mist_shooting_gallery_model_0AC94_normals.inc"
};

static u32 _gMistShootingGalleryModel0AC94Stream[42] = {
#include "assets/mist_shooting_gallery_model_0AC94_stream.inc"
};

TmdSource gMistShootingGalleryModel0AC94 = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0AC94PartVerts,
    _gMistShootingGalleryModel0AC94Verts,
    _gMistShootingGalleryModel0AC94Normals,
    _gMistShootingGalleryModel0AC94Skeleton,
    _gMistShootingGalleryModel0AC94Stream,
};

static TmdBone _gMistShootingGalleryModel0ADF8Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0ADF8_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0ADF8PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0ADF8_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0ADF8Verts[8] = {
#include "assets/mist_shooting_gallery_model_0ADF8_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0ADF8Normals[6] = {
#include "assets/mist_shooting_gallery_model_0ADF8_normals.inc"
};

static u32 _gMistShootingGalleryModel0ADF8Stream[42] = {
#include "assets/mist_shooting_gallery_model_0ADF8_stream.inc"
};

TmdSource gMistShootingGalleryModel0ADF8 = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0ADF8PartVerts,
    _gMistShootingGalleryModel0ADF8Verts,
    _gMistShootingGalleryModel0ADF8Normals,
    _gMistShootingGalleryModel0ADF8Skeleton,
    _gMistShootingGalleryModel0ADF8Stream,
};

static TmdBone _gMistShootingGalleryModel0AF5CSkeleton[1] = {
#include "assets/mist_shooting_gallery_model_0AF5C_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0AF5CPartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0AF5C_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AF5CVerts[8] = {
#include "assets/mist_shooting_gallery_model_0AF5C_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0AF5CNormals[6] = {
#include "assets/mist_shooting_gallery_model_0AF5C_normals.inc"
};

static u32 _gMistShootingGalleryModel0AF5CStream[42] = {
#include "assets/mist_shooting_gallery_model_0AF5C_stream.inc"
};

TmdSource gMistShootingGalleryModel0AF5C = {
    0,
    312,
    0,
    1,
    _gMistShootingGalleryModel0AF5CPartVerts,
    _gMistShootingGalleryModel0AF5CVerts,
    _gMistShootingGalleryModel0AF5CNormals,
    _gMistShootingGalleryModel0AF5CSkeleton,
    _gMistShootingGalleryModel0AF5CStream,
};

static TmdBone _gMistShootingGalleryModel0B0D0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B0D0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B0D0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B0D0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B0D0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B0D0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B0D0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B0D0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B0D0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B0D0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B0D0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B0D0PartVerts,
    _gMistShootingGalleryModel0B0D0Verts,
    _gMistShootingGalleryModel0B0D0Normals,
    _gMistShootingGalleryModel0B0D0Skeleton,
    _gMistShootingGalleryModel0B0D0Stream,
};

static TmdBone _gMistShootingGalleryModel0B2C0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B2C0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B2C0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B2C0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B2C0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B2C0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B2C0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B2C0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B2C0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B2C0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B2C0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B2C0PartVerts,
    _gMistShootingGalleryModel0B2C0Verts,
    _gMistShootingGalleryModel0B2C0Normals,
    _gMistShootingGalleryModel0B2C0Skeleton,
    _gMistShootingGalleryModel0B2C0Stream,
};

static TmdBone _gMistShootingGalleryModel0B4B0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B4B0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B4B0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B4B0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B4B0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B4B0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B4B0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B4B0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B4B0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B4B0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B4B0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B4B0PartVerts,
    _gMistShootingGalleryModel0B4B0Verts,
    _gMistShootingGalleryModel0B4B0Normals,
    _gMistShootingGalleryModel0B4B0Skeleton,
    _gMistShootingGalleryModel0B4B0Stream,
};

static TmdBone _gMistShootingGalleryModel0B6A0Skeleton[1] = {
#include "assets/mist_shooting_gallery_model_0B6A0_skeleton.inc"
};

static u32 _gMistShootingGalleryModel0B6A0PartVerts[1] = {
#include "assets/mist_shooting_gallery_model_0B6A0_partVerts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B6A0Verts[10] = {
#include "assets/mist_shooting_gallery_model_0B6A0_verts.inc"
};

static SVECTOR _gMistShootingGalleryModel0B6A0Normals[6] = {
#include "assets/mist_shooting_gallery_model_0B6A0_normals.inc"
};

static u32 _gMistShootingGalleryModel0B6A0Stream[73] = {
#include "assets/mist_shooting_gallery_model_0B6A0_stream.inc"
};

TmdSource gMistShootingGalleryModel0B6A0 = {
    0,
    528,
    0,
    1,
    _gMistShootingGalleryModel0B6A0PartVerts,
    _gMistShootingGalleryModel0B6A0Verts,
    _gMistShootingGalleryModel0B6A0Normals,
    _gMistShootingGalleryModel0B6A0Skeleton,
    _gMistShootingGalleryModel0B6A0Stream,
};

/// Initial colour modulation and code flags for gallery background sprites.
enum {
    MIST_SHOOTING_GALLERY_SPRITE_NEUTRAL_RGB = 128,
    MIST_SHOOTING_GALLERY_SPRITE_OPAQUE      = 0,
};

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    MIST_SHOOTING_GALLERY_RESOURCE_TARGET_MOUNT = 76,
    MIST_SHOOTING_GALLERY_FILE_TARGET_MOUNT     = 76,
    MIST_SHOOTING_GALLERY_RESOURCE_PIERCE       = 143,
    MIST_SHOOTING_GALLERY_FILE_PIERCE           = 151,
};

// Keep the read-only table at its original position among the overlay's data.

/// Paired cue-2 requests in the loaded type-1 sound bank; cue 1 uses base + 1.
enum {
    MIST_SHOOTING_GALLERY_SURFACE1_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 9,
    MIST_SHOOTING_GALLERY_SURFACE1_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 11,
    MIST_SHOOTING_GALLERY_SURFACE2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 65,
    MIST_SHOOTING_GALLERY_SURFACE2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 67,
};

#define ROOM_PART _gMistShootingGalleryCollisionGridNormals
#include "assets/rooms/mist/shooting_gallery/geometry.inc"
#include "assets/rooms/mist/shooting_gallery/cameras.inc"
#include "assets/rooms/mist/shooting_gallery/sprites.inc"
#include "assets/rooms/mist/shooting_gallery/view_boundaries.inc"
#include "assets/rooms/mist/shooting_gallery/action_triggers.inc"
#include "assets/rooms/mist/shooting_gallery/lights.inc"
#include "assets/rooms/mist/shooting_gallery/placements.inc"
#include "assets/rooms/mist/shooting_gallery/ambient.inc"
#include "assets/rooms/mist/shooting_gallery/surfaces.inc"
s32 gMistShootingGalleryUnscaledExp = 0;

Task* gMistShootingGalleryControllerTask;

void mistShootingGalleryTracerTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_TRACER_INIT               = 0,
        MIST_SHOOTING_GALLERY_TRACER_FADE               = 1,
        MIST_SHOOTING_GALLERY_TRACER_HELD_SIZE          = 0x600,
        MIST_SHOOTING_GALLERY_TRACER_RUNNING_SIZE       = 0x400,
        MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_MASK    = 0x3FF,
        MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_RADIUS  = 0x200,
        MIST_SHOOTING_GALLERY_TRACER_VERTICAL_OFFSET    = 0x800,
        MIST_SHOOTING_GALLERY_TRACER_INITIAL_BRIGHTNESS = 0x80,
        MIST_SHOOTING_GALLERY_TRACER_FADE_STEP          = 8,
    };

    EffectWork* work;
    GfxCoord*   coord;
    u8          rgb[3];
    u32         randomX;
    u32         randomZ;
    u32         randomAngle;

    /// Queues an additive blue tint at the effect's current brightness.
    ///
    /// Borrows an EffectWork and three writable colour bytes. Red and green
    /// receive half brightness, blue full brightness, narrowing at each store.
    /// Arguments are evaluated repeatedly and must have no side effects;
    /// captures no caller locals. Expands to four statements inside braces.
#define MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT(effectWork, colorBytes) \
    (colorBytes)[0] = (effectWork)->scale >> 1;                        \
    (colorBytes)[1] = (effectWork)->scale >> 1;                        \
    (colorBytes)[2] = (effectWork)->scale;                             \
    effectDrawScreenTint((colorBytes), GPU_BLEND_ADD)

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        spriteQuadDraw(coord, work->index, MIST_SHOOTING_GALLERY_TRACER_HELD_SIZE, work->angle);
        _beamStripDraw(coord, &work->pos, work->index, MIST_SHOOTING_GALLERY_TRACER_HELD_SIZE);
        MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT(work, rgb);
        return;
    }

    work->age++;
    switch (task->state) {
        case MIST_SHOOTING_GALLERY_TRACER_INIT:
            // Attach the origin, but seed the endpoint from the existing world cache.
            coord->parent       = work->parent;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = 0;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = MIST_SHOOTING_GALLERY_TRACER_FADE;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomX         = gRandomLcgState;
            work->pos.vx    = (u16)coord->workm.t[0] - ((randomX >> 16 & MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_MASK) - MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_RADIUS);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomZ         = gRandomLcgState;
            work->pos.vy    = coord->workm.t[1] - MIST_SHOOTING_GALLERY_TRACER_VERTICAL_OFFSET;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomAngle     = gRandomLcgState;
            work->pos.vz    = (u16)coord->workm.t[2] - ((randomZ >> 16 & MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_MASK) - MIST_SHOOTING_GALLERY_TRACER_HORIZONTAL_RADIUS);
            work->scale     = MIST_SHOOTING_GALLERY_TRACER_INITIAL_BRIGHTNESS;
            work->angle     = randomAngle >> 16 & ACTOR_TRANSFORM_ANGLE_MASK;
        case MIST_SHOOTING_GALLERY_TRACER_FADE:
            // Only odd running ticks advance the texture frame and draw the tracer.
            if (work->age & 1) {
                spriteQuadDraw(coord, ++work->index, MIST_SHOOTING_GALLERY_TRACER_RUNNING_SIZE, work->angle);
                _beamStripDraw(coord, &work->pos, work->index, MIST_SHOOTING_GALLERY_TRACER_RUNNING_SIZE);
            }
            MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT(work, rgb);
            work->scale -= MIST_SHOOTING_GALLERY_TRACER_FADE_STEP;
            if (work->scale < MIST_SHOOTING_GALLERY_TRACER_FADE_STEP) {
                effectKillTask(work, task);
            }
            return;
    }
#undef MIST_SHOOTING_GALLERY_DRAW_TRACER_TINT
}

/// Texel width and horizontal stride of each cell in the gallery flash's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the gallery flash's texture row, repeated by its frame counter.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the gallery flash strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
/// Inclusive bottom texel row of the gallery sprite's square cells, relative to its texture page.
#define SPRITE_QUAD_BOTTOM_V (SPRITE_QUAD_TOP_V + SPRITE_QUAD_CELL_WIDTH - 1)
/// Perspective-sizing multiplier for the gallery effect sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
/// Muzzle-flash palette: VRAM X=304 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(304, 266)
/// Uses the gallery flash's unmodified projected depth for sizing and sorting.
///
/// Zero disables the unit increment in the next sprite-quad drawer inclusion;
/// the fragment consumes and undefines this preprocessor switch.
#define SPRITE_QUAD_INCREMENT_DEPTH 0
#include "../../shared/effects/sprite_quad_draw.inc.c"

/// Uses the unmodified start depth for the gallery tracer's width and sorting.
///
/// Disables both unit increments in the next beam-strip drawer inclusion;
/// the fragment consumes and undefines this preprocessor boolean.
#define BEAM_STRIP_INCREMENT_DEPTH 0
#include "../../shared/effects/beam_strip_draw.inc.c"

/// Allocates and publishes one course's controller work and prepares player control.
///
/// The low spawn-argument nibble must select course 0..4. Zeroed primary-heap work
/// belongs to the controller until its exit callback. Courses 0..2 lock swapping;
/// 0..1 also hold movement and acquire a menu hold. Every course acquires a battle
/// reference, clears the result latch and starts its clock off the left edge.
static void _mistShootingGalleryInitCourse(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_COURSE_ARGUMENT_MASK  = 0xF,
        MIST_SHOOTING_GALLERY_CLOCK_INITIAL_X       = -220,
        MIST_SHOOTING_GALLERY_FIRST_MOVEMENT_COURSE = 2,
        MIST_SHOOTING_GALLERY_FIRST_SWAP_COURSE     = 3,
    };
    Task*                    playerTask;
    GameActor*               player;
    MistShootingGalleryWork* work;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    player     = playerTask->work;

    work             = memCalloc(sizeof(*work), false);
    controller->work = work;
    if (work == NULL) {
        taskKill(controller);
        return;
    }

    gMistShootingGalleryControllerTask = controller;
    controller->exitCallback           = _mistShootingGalleryExitCourse;
    controller->state++;
    work->course = controller->spawnArg1.value & MIST_SHOOTING_GALLERY_COURSE_ARGUMENT_MASK;
    work->clockX = MIST_SHOOTING_GALLERY_CLOCK_INITIAL_X;

    // Extend the weapon capsule in the firing lane before entering locomotion.
    player->weaponShape.ends[0].vz =
        (player->weaponShape.ends[1].vz + gPlayerActorWeaponReaches[gPlayerStatus.weapon]) << 1;
    playerActorEnterLocomotion(playerTask, 1);

    // The early courses restrict swapping; the first two also hold movement.
    if (work->course < MIST_SHOOTING_GALLERY_FIRST_SWAP_COURSE) {
        gAttachmentState.flags |= ATTACHMENT_FLAG_SWAP_LOCK;
        if (work->course < MIST_SHOOTING_GALLERY_FIRST_MOVEMENT_COURSE) {
            player->movementInputDisabled = 1;
            displayAcquireMenuHold();
        }
    }
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    gGameSession->battleResetPending            = 0;
    (sceneAcquireBattleRef)(0);
}

/// Runs zero-based course 0's lock-on tutorial and one-minute target sequence.
///
/// Borrows initialized controller work and course-0 script storage. Keyed CAP
/// text uses the saved button layout (0..2). Seeds records 0..11 through timed
/// and target-clear gates, observes target lock, then runs the finite script
/// from record 12. SELECT requests abort; low primary ammunition interrupts once
/// with caption keys 17..21 and resumes the interrupted phase. The 1800-frame
/// clock counts at 30 Hz only in the wave phase while combat actors run. Timeout
/// advances to the controller's closing state; all gallery resources stay loaded.
static void _mistShootingGalleryRunCourse0(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_COURSE_0_INITIALIZE                    = 0,
        MIST_SHOOTING_GALLERY_COURSE_0_INTRO_CAPTION                 = 1,
        MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_CAPTION                = 2,
        MIST_SHOOTING_GALLERY_COURSE_0_INSTRUCTIONS_CAPTION          = 3,
        MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_INSTRUCTIONS           = 4,
        MIST_SHOOTING_GALLERY_COURSE_0_READY_DELAY                   = 5,
        MIST_SHOOTING_GALLERY_COURSE_0_FIRST_TARGET                  = 6,
        MIST_SHOOTING_GALLERY_COURSE_0_WAIT_FOR_LOCK                 = 7,
        MIST_SHOOTING_GALLERY_COURSE_0_SECOND_TARGET                 = 8,
        MIST_SHOOTING_GALLERY_COURSE_0_TARGET_INSTRUCTIONS           = 9,
        MIST_SHOOTING_GALLERY_COURSE_0_SINGLE_TARGETS                = 10,
        MIST_SHOOTING_GALLERY_COURSE_0_WAIT_FOR_SINGLE_TARGETS       = 11,
        MIST_SHOOTING_GALLERY_COURSE_0_SEED_TARGETS                  = 12,
        MIST_SHOOTING_GALLERY_COURSE_0_WAVE_INSTRUCTIONS             = 13,
        MIST_SHOOTING_GALLERY_COURSE_0_WAIT_FOR_SEED_TARGETS         = 14,
        MIST_SHOOTING_GALLERY_COURSE_0_WAVES                         = 15,
        MIST_SHOOTING_GALLERY_COURSE_0_AMMO_CAPTIONS                 = 16,
        MIST_SHOOTING_GALLERY_COURSE_0_DURATION_FRAMES               = 1800,
        MIST_SHOOTING_GALLERY_COURSE_0_LONG_DELAY_TICKS              = 60,
        MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_DELAY_TICKS           = 30,
        MIST_SHOOTING_GALLERY_COURSE_0_TARGET_DELAY_TICKS            = 15,
        MIST_SHOOTING_GALLERY_COURSE_0_RAPID_SPAWN_DELAY_TICKS       = 10,
        MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND               = 17,
        MIST_SHOOTING_GALLERY_COURSE_0_INTRO_KEY                     = 0,
        MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_FIRST_KEY              = 1,
        MIST_SHOOTING_GALLERY_COURSE_0_INSTRUCTIONS_KEY              = 4,
        MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_INSTRUCTIONS_FIRST_KEY = 5,
        MIST_SHOOTING_GALLERY_COURSE_0_LOCK_INSTRUCTIONS_FIRST_KEY   = 8,
        MIST_SHOOTING_GALLERY_COURSE_0_TARGET_INSTRUCTIONS_FIRST_KEY = 11,
        MIST_SHOOTING_GALLERY_COURSE_0_WAVE_INSTRUCTIONS_FIRST_KEY   = 14,
        MIST_SHOOTING_GALLERY_COURSE_0_AMMO_FIRST_KEY                = 17,
        MIST_SHOOTING_GALLERY_COURSE_0_AMMO_LAST_KEY                 = 21,
        MIST_SHOOTING_GALLERY_COURSE_0_SINGLE_TARGET_END_INDEX       = 7,
        MIST_SHOOTING_GALLERY_COURSE_0_SEED_TARGET_END_INDEX         = 12,
        MIST_SHOOTING_GALLERY_COURSE_0_MIN_PRIMARY_LOAD              = 2,
        MIST_SHOOTING_GALLERY_COURSE_0_M93R_SELECTION                = 2,
        MIST_SHOOTING_GALLERY_COURSE_0_M93R_MIN_PRIMARY_LOAD         = 4,
    };
    MistShootingGalleryWork*         work;
    const _MistShootingGallerySpawn* spawn;
    s32                              minimumPrimaryLoad;
    s32                              buttonLayout;
    u16                              introFramesLeft;
    u16                              layoutFramesLeft;
    u16                              readyFramesBeforeDecrement;
    u16                              firstTargetFramesLeft;
    u16                              secondTargetFramesLeft;
    u16                              instructionsFramesLeft;
    u16                              seedFramesLeft;
    u16                              waveInstructionsFramesLeft;
    u16                              spawnFrame;
    u16                              scriptFrame;
    u16                              resumePhase;
    u8                               captionKey;

    work         = controller->work;
    buttonLayout = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
        actor215100RequestGalleryAbort();
        return;
    }

    switch (work->phase) {
        case MIST_SHOOTING_GALLERY_COURSE_0_INITIALIZE:
            work->timeLeft = MIST_SHOOTING_GALLERY_COURSE_0_DURATION_FRAMES;
            work->timer    = MIST_SHOOTING_GALLERY_COURSE_0_LONG_DELAY_TICKS;
            work->phase++;
        case MIST_SHOOTING_GALLERY_COURSE_0_INTRO_CAPTION:
            introFramesLeft = work->timer - 1;
            work->timer     = introFramesLeft;
            if ((s32)(introFramesLeft << 16) <= 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_DELAY_TICKS;
                work->phase++;
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_0_INTRO_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_CAPTION:
            layoutFramesLeft = work->timer - 1;
            work->timer      = layoutFramesLeft;
            if ((s32)(layoutFramesLeft << 16) <= 0) {
                work->phase++;
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_FIRST_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_INSTRUCTIONS_CAPTION:
            work->phase++;
            _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_0_INSTRUCTIONS_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_INSTRUCTIONS:
            work->timer = MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_DELAY_TICKS;
            work->phase++;
            _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_0_LAYOUT_INSTRUCTIONS_FIRST_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_READY_DELAY:
            readyFramesBeforeDecrement = work->timer;
            work->timer                = readyFramesBeforeDecrement - 1;
            if ((s32)(readyFramesBeforeDecrement << 16) <= 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_0_TARGET_DELAY_TICKS;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            }
            break;
        // Seed tutorial targets before starting the timed wave script.
        case MIST_SHOOTING_GALLERY_COURSE_0_FIRST_TARGET:
            firstTargetFramesLeft = work->timer - 1;
            work->timer           = firstTargetFramesLeft;
            if ((s32)(firstTargetFramesLeft << 16) <= 0) {
                work->phase++;
                spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                _mistShootingGallerySpawnTarget(controller, spawn);
                work->spawnIndex++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_WAIT_FOR_LOCK:
            if (work->targetLockMask != 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_0_TARGET_DELAY_TICKS;
                work->phase++;
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_0_LOCK_INSTRUCTIONS_FIRST_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            } else if (work->liveTargets == 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_0_TARGET_DELAY_TICKS;
                work->phase++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_SECOND_TARGET:
            if (work->liveTargets == 0) {
                secondTargetFramesLeft = work->timer - 1;
                work->timer            = secondTargetFramesLeft;
                if ((s32)(secondTargetFramesLeft << 16) <= 0) {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_0_LONG_DELAY_TICKS;
                    work->phase++;
                    spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                    _mistShootingGallerySpawnTarget(controller, spawn);
                    work->spawnIndex++;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_TARGET_INSTRUCTIONS:
            instructionsFramesLeft = work->timer - 1;
            work->timer            = instructionsFramesLeft;
            if ((s32)(instructionsFramesLeft << 16) <= 0) {
                work->phase++;
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_0_TARGET_INSTRUCTIONS_FIRST_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_SINGLE_TARGETS:
            if (work->liveTargets == 0) {
                spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                _mistShootingGallerySpawnTarget(controller, spawn);
                work->spawnIndex++;
                if (work->spawnIndex == MIST_SHOOTING_GALLERY_COURSE_0_SINGLE_TARGET_END_INDEX) {
                    work->phase++;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_WAIT_FOR_SINGLE_TARGETS:
            if (work->liveTargets == 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_DELAY_TICKS;
                work->phase++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_SEED_TARGETS:
            seedFramesLeft = work->timer - 1;
            work->timer    = seedFramesLeft;
            if ((s32)(seedFramesLeft << 16) <= 0) {
                spawn = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
                _mistShootingGallerySpawnTarget(controller, spawn);
                work->spawnIndex++;
                if (work->spawnIndex == MIST_SHOOTING_GALLERY_COURSE_0_SEED_TARGET_END_INDEX) {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_0_LONG_DELAY_TICKS;
                    work->phase++;
                } else {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_0_RAPID_SPAWN_DELAY_TICKS;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_WAVE_INSTRUCTIONS:
            waveInstructionsFramesLeft = work->timer - 1;
            work->timer                = waveInstructionsFramesLeft;
            if ((s32)(waveInstructionsFramesLeft << 16) <= 0) {
                work->phase++;
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_0_WAVE_INSTRUCTIONS_FIRST_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_WAIT_FOR_SEED_TARGETS:
            if (work->liveTargets != 0) {
                break;
            }
            work->phase++;
            displayReleaseMenuHold();
        // Hold at clear markers and at the finite script end until timeout.
        case MIST_SHOOTING_GALLERY_COURSE_0_WAVES:
            spawn      = &_gMistShootingGalleryCourse0WaveScriptRefs[0][work->spawnIndex];
            spawnFrame = spawn->frame;
            if (spawnFrame != MIST_SHOOTING_GALLERY_SPAWN_END) {
                if (spawnFrame != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
                    if (work->scriptFrame == spawnFrame) {
                        do {
                            _mistShootingGallerySpawnTarget(controller, spawn);
                            spawn++;
                            work->spawnIndex++;
                        } while (work->scriptFrame == spawn->frame);
                    }
                    scriptFrame = work->scriptFrame;
                    if (scriptFrame < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                        work->scriptFrame = scriptFrame + 1;
                    }
                } else if (work->liveTargets == 0) {
                    work->spawnIndex++;
                }
            }
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = MIST_SHOOTING_GALLERY_COURSE_0_INITIALIZE;
                controller->state++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_0_AMMO_CAPTIONS:
            _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_0_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            captionKey = work->captionStep;
            if (captionKey == MIST_SHOOTING_GALLERY_COURSE_0_AMMO_LAST_KEY) {
                work->phase = work->resumePhase;
                displayReleaseMenuHold();
            } else {
                work->captionStep = captionKey + 1;
            }
            break;
    }

    // The ammunition warning is one-shot and retains the interrupted phase.
    if (work->interrupted == 0) {
        minimumPrimaryLoad = MIST_SHOOTING_GALLERY_COURSE_0_MIN_PRIMARY_LOAD;
        if (gPlayerStatus.weapon == MIST_SHOOTING_GALLERY_COURSE_0_M93R_SELECTION) {
            minimumPrimaryLoad = MIST_SHOOTING_GALLERY_COURSE_0_M93R_MIN_PRIMARY_LOAD;
        }
        if (equipmentConsumeWeaponLoad(gPlayerStatus.weapon + (EQUIPMENT_WEAPON_ITEM_FIRST - 1), EQUIPMENT_WEAPON_LOAD_QUERY_PRIMARY) < minimumPrimaryLoad) {
            resumePhase       = work->phase;
            work->phase       = MIST_SHOOTING_GALLERY_COURSE_0_AMMO_CAPTIONS;
            work->interrupted = 1;
            work->captionStep = MIST_SHOOTING_GALLERY_COURSE_0_AMMO_FIRST_KEY;
            work->resumePhase = resumePhase;
        }
    }
}

/// Advances one tick of a finite course script, holding at its end marker.
///
/// Borrows initialized controller/work and read-only script storage; `work`
/// must be the controller's work because target spawning updates its live count.
/// Equal frame keys spawn together, WAIT_CLEAR advances only with no live
/// targets and END holds the cursor. Every equal-frame run must have a following
/// record in the script;
/// the cursor must start in bounds and the script must end before that extent.
/// Indices count records; script frames count active wave ticks and saturate
/// below every marker value. Spawn failure still consumes the record; WAIT_CLEAR
/// and END freeze the frame counter. No script pointer is retained.
static inline void _mistShootingGalleryStepFiniteWaves(Task* controller, MistShootingGalleryWork* work, const _MistShootingGallerySpawn* script)
{
    const _MistShootingGallerySpawn* spawn;
    u16                              spawnFrame;
    u16                              scriptFrame;

    spawn      = &script[work->spawnIndex];
    spawnFrame = spawn->frame;
    if (spawnFrame != MIST_SHOOTING_GALLERY_SPAWN_END) {
        if (spawnFrame != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
            if (work->scriptFrame == spawnFrame) {
                do {
                    _mistShootingGallerySpawnTarget(controller, spawn);
                    spawn++;
                    work->spawnIndex++;
                } while (work->scriptFrame == spawn->frame);
            }
            scriptFrame = work->scriptFrame;
            if (scriptFrame < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                work->scriptFrame = scriptFrame + 1;
            }
        } else if (work->liveTargets == 0) {
            work->spawnIndex++;
        }
    }
}

/// Runs zero-based course 1's introduction and two-minute finite target script.
///
/// Requires initialized controller work, script records and loaded CAP resources.
/// Shows command-18 caption keys 0..4, waits before engaging battle, then releases
/// the menu hold and starts timed waves. SELECT requests abort. Equal-frame
/// records spawn together and clear markers wait for live targets to reach zero.
/// The clock counts 3600 active combat frames at 30 Hz; timeout advances to the
/// controller's closing state.
static void _mistShootingGalleryRunCourse1(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_COURSE_1_INITIALIZE        = 0,
        MIST_SHOOTING_GALLERY_COURSE_1_INTRO_CAPTIONS    = 1,
        MIST_SHOOTING_GALLERY_COURSE_1_READY_DELAY       = 2,
        MIST_SHOOTING_GALLERY_COURSE_1_START_DELAY       = 3,
        MIST_SHOOTING_GALLERY_COURSE_1_WAVES             = 4,
        MIST_SHOOTING_GALLERY_COURSE_1_DURATION_FRAMES   = 3600,
        MIST_SHOOTING_GALLERY_COURSE_1_READY_DELAY_TICKS = 60,
        MIST_SHOOTING_GALLERY_COURSE_1_START_DELAY_TICKS = 10,
        MIST_SHOOTING_GALLERY_COURSE_1_CAPTION_COMMAND   = 18,
        MIST_SHOOTING_GALLERY_COURSE_1_INTRO_LAST_KEY    = 4,
    };
    MistShootingGalleryWork* work;
    u16                      introFramesLeft;
    u16                      readyFramesBeforeDecrement;
    u16                      startFramesBeforeDecrement;
    u8                       captionKey;

    work = controller->work;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_SELECT) != 0) {
        actor215100RequestGalleryAbort();
        return;
    }

    switch (work->phase) {
        case MIST_SHOOTING_GALLERY_COURSE_1_INITIALIZE:
            work->timeLeft = MIST_SHOOTING_GALLERY_COURSE_1_DURATION_FRAMES;
            work->timer    = MIST_SHOOTING_GALLERY_COURSE_1_READY_DELAY_TICKS;
            work->phase++;
        case MIST_SHOOTING_GALLERY_COURSE_1_INTRO_CAPTIONS:
            introFramesLeft = work->timer - 1;
            work->timer     = introFramesLeft;
            if ((s32)(introFramesLeft << 16) <= 0) {
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_1_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                captionKey = work->captionStep;
                if (captionKey == MIST_SHOOTING_GALLERY_COURSE_1_INTRO_LAST_KEY) {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_1_READY_DELAY_TICKS;
                    work->phase++;
                    return;
                }
                work->captionStep = captionKey + 1;
                return;
            }
        default:
            return;
        case MIST_SHOOTING_GALLERY_COURSE_1_READY_DELAY:
            readyFramesBeforeDecrement = work->timer;
            work->timer                = readyFramesBeforeDecrement - 1;
            if ((s32)(readyFramesBeforeDecrement << 16) <= 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_1_START_DELAY_TICKS;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                sceneEngageBattle(1);
                return;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_1_START_DELAY:
            startFramesBeforeDecrement = work->timer;
            work->timer                = startFramesBeforeDecrement - 1;
            if ((s32)(startFramesBeforeDecrement << 16) > 0) {
                break;
            }
            work->phase++;
            displayReleaseMenuHold();
            /* fallthrough */
        case MIST_SHOOTING_GALLERY_COURSE_1_WAVES:
            _mistShootingGalleryStepFiniteWaves(controller, work, _gMistShootingGalleryCourse1WaveScriptRef);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = MIST_SHOOTING_GALLERY_COURSE_1_INITIALIZE;
                controller->state++;
            }
            break;
    }
}

/// Advances a timed target script, repeating a selected suffix after wave clearance.
///
/// Borrows initialized controller/work and the script. After consuming a
/// WAIT_CLEAR marker, a cursor at least loopRestartThreshold restarts at
/// loopStartIndex and loopStartFrame. The threshold must be reached before
/// advancing outside the script. Equal frame keys spawn together; the script
/// must provide a following record for each equal-frame run. The clock saturates
/// below all markers; END holds the cursor. Storage stays live across spawns.
/// Indices count records, and frames count active script ticks. The restart
/// index/frame pair must select an in-bounds suffix of the same script.
static inline void _mistShootingGalleryStepRepeatingWaves(Task* controller, MistShootingGalleryWork* work, const _MistShootingGallerySpawn* script, s32 loopRestartThreshold, s32 loopStartIndex, u16 loopStartFrame)
{
    const _MistShootingGallerySpawn* spawn;
    u16                              spawnFrame;
    u16                              scriptFrame;

    spawn      = &script[work->spawnIndex];
    spawnFrame = spawn->frame;
    if (spawnFrame != MIST_SHOOTING_GALLERY_SPAWN_END) {
        if (spawnFrame != MIST_SHOOTING_GALLERY_SPAWN_WAIT_CLEAR) {
            if (work->scriptFrame == spawnFrame) {
                do {
                    _mistShootingGallerySpawnTarget(controller, spawn);
                    spawn++;
                    work->spawnIndex++;
                } while (work->scriptFrame == spawn->frame);
            }
            scriptFrame = work->scriptFrame;
            if (scriptFrame < MIST_SHOOTING_GALLERY_SPAWN_FRAME_LIMIT) {
                work->scriptFrame = scriptFrame + 1;
            }
        } else if (work->liveTargets == 0) {
            work->spawnIndex++;
            if (work->spawnIndex >= loopRestartThreshold) {
                work->spawnIndex  = loopStartIndex;
                work->scriptFrame = loopStartFrame;
            }
        }
    }
}

/// Runs zero-based course 2's GPS tutorial and three-minute target sequence.
///
/// Borrows initialized controller work with phase 0..10 and a valid script
/// cursor. Shows keyed CAP captions, waits for view 18 and a GPS attachment,
/// seeds three targets, then repeats script records 14..73 while counting
/// 5400 ticks at 30 Hz. Caption layout variants use the saved button layout.
/// Modal display and actor control gate the tutorial; timeout advances the
/// controller to its exit state. Requires the gallery and caption resources live.
static void _mistShootingGalleryRunCourse2(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_COURSE_2_INITIALIZE               = 0,
        MIST_SHOOTING_GALLERY_COURSE_2_READY_CAPTIONS           = 1,
        MIST_SHOOTING_GALLERY_COURSE_2_WAIT_FOR_VIEW            = 2,
        MIST_SHOOTING_GALLERY_COURSE_2_GPS_PROMPT               = 3,
        MIST_SHOOTING_GALLERY_COURSE_2_CHECK_GPS                = 4,
        MIST_SHOOTING_GALLERY_COURSE_2_WAIT_FOR_GPS             = 5,
        MIST_SHOOTING_GALLERY_COURSE_2_PREPARE_TARGETS          = 6,
        MIST_SHOOTING_GALLERY_COURSE_2_SEED_TARGETS             = 7,
        MIST_SHOOTING_GALLERY_COURSE_2_INSTRUCTIONS             = 8,
        MIST_SHOOTING_GALLERY_COURSE_2_START_ROUND              = 9,
        MIST_SHOOTING_GALLERY_COURSE_2_WAVES                    = 10,
        MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND          = 0x13,
        MIST_SHOOTING_GALLERY_COURSE_2_DURATION_FRAMES          = 5400,
        MIST_SHOOTING_GALLERY_COURSE_2_READY_DELAY_FRAMES       = 30,
        MIST_SHOOTING_GALLERY_COURSE_2_STEP_DELAY_FRAMES        = 15,
        MIST_SHOOTING_GALLERY_COURSE_2_TARGET_SETTLE_FRAMES     = 60,
        MIST_SHOOTING_GALLERY_COURSE_2_HANDOFF_VIEW             = 18,
        MIST_SHOOTING_GALLERY_COURSE_2_LOOP_RESTART_THRESHOLD   = ARRAY_SIZE(_gMistShootingGalleryCourse2WaveScript) - 2,
        MIST_SHOOTING_GALLERY_COURSE_2_LOOP_START_INDEX         = 14,
        MIST_SHOOTING_GALLERY_COURSE_2_LOOP_START_FRAME         = 240,
        MIST_SHOOTING_GALLERY_COURSE_2_INITIAL_TARGET_COUNT     = 3,
        MIST_SHOOTING_GALLERY_COURSE_2_READY_LAST_KEY           = 3,
        MIST_SHOOTING_GALLERY_COURSE_2_LAYOUT_FIRST_KEY         = 4,
        MIST_SHOOTING_GALLERY_COURSE_2_GPS_PROMPT_KEY           = 7,
        MIST_SHOOTING_GALLERY_COURSE_2_GPS_CHECK_FIRST_KEY      = 8,
        MIST_SHOOTING_GALLERY_COURSE_2_GPS_RETRY_KEY            = 9,
        MIST_SHOOTING_GALLERY_COURSE_2_TARGET_PREP_FIRST_KEY    = 10,
        MIST_SHOOTING_GALLERY_COURSE_2_TARGET_PREP_LAST_KEY     = 11,
        MIST_SHOOTING_GALLERY_COURSE_2_INSTRUCTIONS_LAST_KEY    = 19,
        MIST_SHOOTING_GALLERY_COURSE_2_ROUND_START_DELAY_FRAMES = 10,
    };

    enum { INVENTORY_ITEM_GPS = 0x40 };
    MistShootingGalleryWork* work;
    s32                      buttonLayout;
    s32                      hasGpsAttachment;
    u16                      timerBeforeDecrement;
    u8                       captionKey;

    work         = controller->work;
    buttonLayout = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->phase) {
        case MIST_SHOOTING_GALLERY_COURSE_2_INITIALIZE:
            work->timeLeft            = MIST_SHOOTING_GALLERY_COURSE_2_DURATION_FRAMES;
            work->timer               = MIST_SHOOTING_GALLERY_COURSE_2_READY_DELAY_FRAMES;
            gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
            work->phase++;
        case MIST_SHOOTING_GALLERY_COURSE_2_READY_CAPTIONS:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    captionKey = work->captionStep;
                    if (captionKey == MIST_SHOOTING_GALLERY_COURSE_2_READY_LAST_KEY) {
                        work->timer               = MIST_SHOOTING_GALLERY_COURSE_2_STEP_DELAY_FRAMES;
                        gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                        work->phase++;
                    } else {
                        work->captionStep = captionKey + 1;
                    }
                }
            } else {
                work->timer--;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_WAIT_FOR_VIEW:
            if (gGameSession->location.view == MIST_SHOOTING_GALLERY_COURSE_2_HANDOFF_VIEW) {
                if (work->timer <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->timer = 1;
                        work->phase++;
                        _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_2_LAYOUT_FIRST_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    }
                } else {
                    work->timer--;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_GPS_PROMPT:
            if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                work->actionTriggered = 0;
                work->captionStep     = MIST_SHOOTING_GALLERY_COURSE_2_GPS_CHECK_FIRST_KEY;
                work->phase++;
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_2_GPS_PROMPT_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_CHECK_GPS:
            if (work->actionTriggered != 0) {
                hasGpsAttachment = inventoryHasAttachedItem(INVENTORY_ITEM_GPS);
                if (hasGpsAttachment != 1) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_2_GPS_RETRY_KEY) {
                        work->actionTriggered = 0;
                        work->phase++;
                    }
                    work->captionStep++;
                } else {
                    work->phase       = MIST_SHOOTING_GALLERY_COURSE_2_PREPARE_TARGETS;
                    work->timer       = 1;
                    work->captionStep = MIST_SHOOTING_GALLERY_COURSE_2_TARGET_PREP_FIRST_KEY;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_WAIT_FOR_GPS:
            if (inventoryHasAttachedItem(INVENTORY_ITEM_GPS) == 1) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_2_STEP_DELAY_FRAMES;
                work->phase++;
            } else if (work->actionTriggered != 0) {
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_2_GPS_RETRY_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                work->actionTriggered = 0;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_PREPARE_TARGETS:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_2_TARGET_PREP_LAST_KEY) {
                        work->timer = MIST_SHOOTING_GALLERY_COURSE_2_STEP_DELAY_FRAMES;
                        work->phase++;
                        sceneEngageBattle(1);
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_SEED_TARGETS:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                _mistShootingGallerySpawnTarget(controller, &_gMistShootingGalleryCourse2WaveScriptRef[work->spawnIndex]);
                work->spawnIndex++;
                if (work->spawnIndex == MIST_SHOOTING_GALLERY_COURSE_2_INITIAL_TARGET_COUNT) {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_2_TARGET_SETTLE_FRAMES;
                    work->phase++;
                } else {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_2_STEP_DELAY_FRAMES;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_INSTRUCTIONS:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_2_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_2_INSTRUCTIONS_LAST_KEY) {
                        work->timer = MIST_SHOOTING_GALLERY_COURSE_2_STEP_DELAY_FRAMES;
                        work->phase++;
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_START_ROUND:
            timerBeforeDecrement = work->timer;
            work->timer          = timerBeforeDecrement - 1;
            if ((s32)(timerBeforeDecrement << 16) <= 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_2_ROUND_START_DELAY_FRAMES;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_2_WAVES:
            _mistShootingGalleryStepRepeatingWaves(controller, work, _gMistShootingGalleryCourse2WaveScriptRef, MIST_SHOOTING_GALLERY_COURSE_2_LOOP_RESTART_THRESHOLD, MIST_SHOOTING_GALLERY_COURSE_2_LOOP_START_INDEX, MIST_SHOOTING_GALLERY_COURSE_2_LOOP_START_FRAME);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = MIST_SHOOTING_GALLERY_COURSE_2_INITIALIZE;
                controller->state++;
            }
            break;
    }
}

/// Runs zero-based course 3's attachment-wheel tutorial and three-minute sequence.
///
/// Borrows initialized controller work with phase 0..9 and a valid script
/// cursor. After view 18 and the room-action trigger, seeds two targets, turns
/// the player to yaw 3072 of 4096, and suppresses R1/R2 during the wheel lesson.
/// The round repeats records 9..104 for 5400 ticks at 30 Hz. An attachment
/// effect request can interrupt once with caption keys 19..23, then restore
/// the saved phase and key. Timeout advances the controller to its exit state.
/// The player, gallery, attachment and caption resources must remain live.
static void _mistShootingGalleryRunCourse3(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_COURSE_3_INITIALIZE             = 0,
        MIST_SHOOTING_GALLERY_COURSE_3_READY_CAPTION          = 1,
        MIST_SHOOTING_GALLERY_COURSE_3_WAIT_FOR_VIEW          = 2,
        MIST_SHOOTING_GALLERY_COURSE_3_WAIT_FOR_ACTION        = 3,
        MIST_SHOOTING_GALLERY_COURSE_3_SEED_TARGETS           = 4,
        MIST_SHOOTING_GALLERY_COURSE_3_TURN_INSTRUCTIONS      = 5,
        MIST_SHOOTING_GALLERY_COURSE_3_WAIT_FOR_TURN          = 6,
        MIST_SHOOTING_GALLERY_COURSE_3_SWAP_INSTRUCTIONS      = 7,
        MIST_SHOOTING_GALLERY_COURSE_3_WAVES                  = 8,
        MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_CAPTIONS  = 9,
        MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND        = 0x14,
        MIST_SHOOTING_GALLERY_COURSE_3_DURATION_FRAMES        = 5400,
        MIST_SHOOTING_GALLERY_COURSE_3_READY_DELAY_FRAMES     = 30,
        MIST_SHOOTING_GALLERY_COURSE_3_STEP_DELAY_FRAMES      = 15,
        MIST_SHOOTING_GALLERY_COURSE_3_TARGET_SETTLE_FRAMES   = 60,
        MIST_SHOOTING_GALLERY_COURSE_3_HANDOFF_VIEW           = 18,
        MIST_SHOOTING_GALLERY_COURSE_3_LOOP_RESTART_THRESHOLD = ARRAY_SIZE(_gMistShootingGalleryCourse3WaveScript) - 2,
        MIST_SHOOTING_GALLERY_COURSE_3_LOOP_START_INDEX       = 9,
        MIST_SHOOTING_GALLERY_COURSE_3_LOOP_START_FRAME       = 144,
        MIST_SHOOTING_GALLERY_COURSE_3_INITIAL_TARGET_COUNT   = 2,
        MIST_SHOOTING_GALLERY_COURSE_3_READY_KEY              = 0,
        MIST_SHOOTING_GALLERY_COURSE_3_ACTION_PROMPT_KEY      = 7,
        MIST_SHOOTING_GALLERY_COURSE_3_TURN_FIRST_KEY         = 8,
        MIST_SHOOTING_GALLERY_COURSE_3_TURN_LAST_KEY          = 10,
        MIST_SHOOTING_GALLERY_COURSE_3_LAYOUT_FIRST_KEY       = 11,
        MIST_SHOOTING_GALLERY_COURSE_3_SWAP_FIRST_KEY         = 14,
        MIST_SHOOTING_GALLERY_COURSE_3_SWAP_LAST_KEY          = 18,
        MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_FIRST_KEY = 19,
        MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_LAST_KEY  = 23,
        MIST_SHOOTING_GALLERY_COURSE_3_TURN_YAW               = 3072,
        MIST_SHOOTING_GALLERY_COURSE_3_SWAP_DELAY_FRAMES      = 4,
    };

    MistShootingGalleryWork* work;
    ActorTransform           turnRequest;
    GameActor*               player;
    s32                      dismissButtons;
    s32                      buttonLayout;
    u16                      resumePhase;
    s32                      interruptRequest;
    u8                       captionKey;
    u8                       resumeCaptionKey;

    work         = controller->work;
    buttonLayout = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.buttonLayout;

    switch (work->phase) {
        case MIST_SHOOTING_GALLERY_COURSE_3_INITIALIZE:
            work->timeLeft            = MIST_SHOOTING_GALLERY_COURSE_3_DURATION_FRAMES;
            work->timer               = MIST_SHOOTING_GALLERY_COURSE_3_READY_DELAY_FRAMES;
            gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
            work->phase++;
        case MIST_SHOOTING_GALLERY_COURSE_3_READY_CAPTION:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_3_READY_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    work->timer               = MIST_SHOOTING_GALLERY_COURSE_3_STEP_DELAY_FRAMES;
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->phase++;
                }
            } else {
                work->timer--;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_WAIT_FOR_VIEW:
            if (gGameSession->location.view == MIST_SHOOTING_GALLERY_COURSE_3_HANDOFF_VIEW) {
                if (work->timer <= 0) {
                    if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                        work->timer = 1;
                        work->phase++;
                        _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_3_ACTION_PROMPT_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    }
                } else {
                    work->timer--;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_WAIT_FOR_ACTION:
            if (work->actionTriggered != 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_3_READY_DELAY_FRAMES;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                sceneEngageBattle(1);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_SEED_TARGETS:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                _mistShootingGallerySpawnTarget(controller, &_gMistShootingGalleryCourse3WaveScriptRef[work->spawnIndex]);
                work->spawnIndex++;
                if (work->spawnIndex == MIST_SHOOTING_GALLERY_COURSE_3_INITIAL_TARGET_COUNT) {
                    work->captionStep = MIST_SHOOTING_GALLERY_COURSE_3_TURN_FIRST_KEY;
                    work->timer       = MIST_SHOOTING_GALLERY_COURSE_3_TARGET_SETTLE_FRAMES;
                    work->phase++;
                } else {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_3_STEP_DELAY_FRAMES;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_TURN_INSTRUCTIONS:
            work->timer--;
            if ((s32)(work->timer << 16) <= 0) {
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_3_TURN_LAST_KEY) {
                    work->timer = MIST_SHOOTING_GALLERY_COURSE_3_READY_DELAY_FRAMES;
                    work->phase++;
                    padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_ALIAS, PAD_BUTTON_R1 | PAD_BUTTON_R2);
                    turnRequest.rot.vy = MIST_SHOOTING_GALLERY_COURSE_3_TURN_YAW;
                    playerActorTurnToYaw(
                        gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, &turnRequest, 0);
                }
                work->captionStep++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_WAIT_FOR_TURN:
            player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_ALIAS, PAD_BUTTON_R1 | PAD_BUTTON_R2);
            if (player->scriptedMotionPending == 0) {
                playerActorEndScripted(
                    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
                work->phase++;
                dismissButtons          = PAD_BUTTON_TRIANGLE;
                gAttachmentState.flags |= ATTACHMENT_FLAG_FORCE_OPEN_WHEEL;
                work->timer             = MIST_SHOOTING_GALLERY_COURSE_3_SWAP_DELAY_FRAMES;
                work->captionStep       = MIST_SHOOTING_GALLERY_COURSE_3_SWAP_FIRST_KEY;
                if (buttonLayout == 2) {
                    dismissButtons = PAD_BUTTON_R2;
                }
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND, buttonLayout + MIST_SHOOTING_GALLERY_COURSE_3_LAYOUT_FIRST_KEY, dismissButtons);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_SWAP_INSTRUCTIONS:
            padInputChangeSuppression(PAD_INPUT_SUPPRESSION_SET_ALIAS, PAD_BUTTON_R1 | PAD_BUTTON_R2);
            if (work->timer <= 0) {
                if ((u32)((u8)gAttachmentState.mode - ATTACHMENT_MODE_ARMED) >= 2) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_3_SWAP_LAST_KEY) {
                        work->phase++;
                        padInputChangeSuppression(PAD_INPUT_SUPPRESSION_CLEAR, PAD_BUTTON_R1 | PAD_BUTTON_R2);
                        gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
                    }
                    work->captionStep++;
                }
            } else {
                work->timer--;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_WAVES:
            _mistShootingGalleryStepRepeatingWaves(controller, work, _gMistShootingGalleryCourse3WaveScriptRef, MIST_SHOOTING_GALLERY_COURSE_3_LOOP_RESTART_THRESHOLD, MIST_SHOOTING_GALLERY_COURSE_3_LOOP_START_INDEX, MIST_SHOOTING_GALLERY_COURSE_3_LOOP_START_FRAME);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = MIST_SHOOTING_GALLERY_COURSE_3_INITIALIZE;
                controller->state++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_CAPTIONS:
            if (work->timer <= 0) {
                if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_3_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    captionKey = work->captionStep;
                    if (captionKey == MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_LAST_KEY) {
                        work->phase       = work->resumePhase;
                        work->captionStep = work->resumeCaptionStep;
                    } else {
                        work->captionStep = captionKey + 1;
                    }
                }
            } else {
                work->timer--;
            }
            break;
    }

    if (work->interrupted == 0 && work->captionStep >= MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_FIRST_KEY) {
        interruptRequest = gAttachmentState.effectPhase;
        if (interruptRequest == 1) {
            resumePhase             = work->phase;
            work->interrupted       = interruptRequest;
            resumeCaptionKey        = work->captionStep;
            work->phase             = MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_CAPTIONS;
            work->timer             = MIST_SHOOTING_GALLERY_COURSE_3_READY_DELAY_FRAMES;
            work->captionStep       = MIST_SHOOTING_GALLERY_COURSE_3_INTERRUPTION_FIRST_KEY;
            work->resumePhase       = resumePhase;
            work->resumeCaptionStep = resumeCaptionKey;
        }
    }
}

/// Runs zero-based course 4's action tutorial and three-minute repeating target waves.
///
/// Requires initialized controller work and loaded gallery/CAP resources. Waits
/// for modal display completion and view 18, then advances keyed captions through
/// two action-point gates. Releases swap locking and engages battle before the
/// 5400-frame, 30 Hz clock starts. Clear markers restart script records 11..78.
/// A lethal hit interrupts once: pauses player state, shows keys 17..18, requests
/// abort and releases battle/controller ownership after four more callback ticks.
/// Normal timeout instead advances to the controller's closing state.
static void _mistShootingGalleryRunCourse4(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_COURSE_4_INITIALIZE                 = 0,
        MIST_SHOOTING_GALLERY_COURSE_4_INTRO_CAPTION              = 1,
        MIST_SHOOTING_GALLERY_COURSE_4_WAIT_FOR_VIEW              = 2,
        MIST_SHOOTING_GALLERY_COURSE_4_FIRST_ACTION_CAPTIONS      = 3,
        MIST_SHOOTING_GALLERY_COURSE_4_SECOND_ACTION_CAPTIONS     = 4,
        MIST_SHOOTING_GALLERY_COURSE_4_READY_DELAY                = 5,
        MIST_SHOOTING_GALLERY_COURSE_4_START_DELAY                = 6,
        MIST_SHOOTING_GALLERY_COURSE_4_WAVES                      = 7,
        MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_HIT_CAPTIONS        = 8,
        MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_HIT_RELEASE         = 9,
        MIST_SHOOTING_GALLERY_COURSE_4_DURATION_FRAMES            = 5400,
        MIST_SHOOTING_GALLERY_COURSE_4_READY_DELAY_TICKS          = 30,
        MIST_SHOOTING_GALLERY_COURSE_4_START_DELAY_TICKS          = 15,
        MIST_SHOOTING_GALLERY_COURSE_4_HANDOFF_VIEW               = 18,
        MIST_SHOOTING_GALLERY_COURSE_4_CAPTION_COMMAND            = 21,
        MIST_SHOOTING_GALLERY_COURSE_4_INTRO_KEY                  = 0,
        MIST_SHOOTING_GALLERY_COURSE_4_ACTION_PROMPT_KEY          = 7,
        MIST_SHOOTING_GALLERY_COURSE_4_ACTION_FIRST_KEY           = 8,
        MIST_SHOOTING_GALLERY_COURSE_4_FIRST_ACTION_LAST_KEY      = 10,
        MIST_SHOOTING_GALLERY_COURSE_4_SECOND_ACTION_LAST_KEY     = 16,
        MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_FIRST_KEY           = 17,
        MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_LAST_KEY            = 18,
        MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_CAPTION_DELAY_TICKS = 3,
        MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_RELEASE_DELAY_TICKS = 4,
        MIST_SHOOTING_GALLERY_COURSE_4_LOOP_RESTART_THRESHOLD     = ARRAY_SIZE(_gMistShootingGalleryCourse4WaveScript) - 2,
        MIST_SHOOTING_GALLERY_COURSE_4_LOOP_START_INDEX           = 11,
        MIST_SHOOTING_GALLERY_COURSE_4_LOOP_START_FRAME           = 688,
    };
    MistShootingGalleryWork* work;
    u16                      framesLeft;
    u8                       captionKey;

    work = controller->work;

    switch (work->phase) {
        case MIST_SHOOTING_GALLERY_COURSE_4_INITIALIZE:
            work->timeLeft            = MIST_SHOOTING_GALLERY_COURSE_4_DURATION_FRAMES;
            work->timer               = MIST_SHOOTING_GALLERY_COURSE_4_READY_DELAY_TICKS;
            gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
            work->phase++;
        case MIST_SHOOTING_GALLERY_COURSE_4_INTRO_CAPTION:
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_4_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_4_INTRO_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    work->timer               = MIST_SHOOTING_GALLERY_COURSE_4_START_DELAY_TICKS;
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->phase++;
                }
            } else {
                work->timer = (u16)work->timer - 1;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_WAIT_FOR_VIEW:
            if (gGameSession->location.view != MIST_SHOOTING_GALLERY_COURSE_4_HANDOFF_VIEW) {
                break;
            }
            if (work->timer <= 0) {
                if (gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                    work->timer = 1;
                    work->phase++;
                    _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_4_CAPTION_COMMAND, MIST_SHOOTING_GALLERY_COURSE_4_ACTION_PROMPT_KEY, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                    work->captionStep = MIST_SHOOTING_GALLERY_COURSE_4_ACTION_FIRST_KEY;
                }
            } else {
                work->timer = (u16)work->timer - 1;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_FIRST_ACTION_CAPTIONS:
            if (work->actionTriggered == 0) {
                break;
            }
            _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_4_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_4_FIRST_ACTION_LAST_KEY) {
                work->actionTriggered = 0;
                work->phase++;
            }
            work->captionStep++;
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_SECOND_ACTION_CAPTIONS:
            if (work->actionTriggered == 0) {
                break;
            }
            _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_4_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
            if (work->captionStep == MIST_SHOOTING_GALLERY_COURSE_4_SECOND_ACTION_LAST_KEY) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_4_READY_DELAY_TICKS;
                work->phase++;
            }
            work->captionStep++;
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_READY_DELAY:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                work->timer = MIST_SHOOTING_GALLERY_COURSE_4_START_DELAY_TICKS;
                work->phase++;
                taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
                sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
                gAttachmentState.flags &= ATTACHMENT_FLAG_CLEAR_SWAP_LOCK;
                sceneEngageBattle(1);
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_START_DELAY:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                work->phase++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_WAVES:
            // Advance the script before drawing and ticking the combat clock.
            _mistShootingGalleryStepRepeatingWaves(controller, work, _gMistShootingGalleryCourse4WaveScriptRef, MIST_SHOOTING_GALLERY_COURSE_4_LOOP_RESTART_THRESHOLD, MIST_SHOOTING_GALLERY_COURSE_4_LOOP_START_INDEX, MIST_SHOOTING_GALLERY_COURSE_4_LOOP_START_FRAME);
            _mistShootingGalleryDrawCountdownClock(work);
            if (_mistShootingGalleryTickCourseClock(work) == 0) {
                work->phase = MIST_SHOOTING_GALLERY_COURSE_4_INITIALIZE;
                controller->state++;
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_HIT_CAPTIONS:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                _mistShootingGalleryShowCaption(MIST_SHOOTING_GALLERY_COURSE_4_CAPTION_COMMAND, work->captionStep, MIST_SHOOTING_GALLERY_CAPTION_DISMISS_BUTTONS);
                captionKey = work->captionStep;
                if (captionKey == MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_LAST_KEY) {
                    work->timer               = MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_RELEASE_DELAY_TICKS;
                    gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_RUNNING;
                    work->phase++;
                    actor215100RequestGalleryAbort();
                } else {
                    work->captionStep = captionKey + 1;
                }
            }
            break;
        case MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_HIT_RELEASE:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if ((s32)(framesLeft << 16) <= 0) {
                sceneReleaseBattleRefAndClearRewards(controller, 0);
                taskKill(controller);
                return;
            }
            break;
    }

    // A lethal hit diverts both the tutorial and active waves into shutdown.
    if (work->interrupted == 0 && work->lethalHit != 0) {
        work->phase               = MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_HIT_CAPTIONS;
        work->interrupted         = 1;
        work->timer               = MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_CAPTION_DELAY_TICKS;
        work->captionStep         = MIST_SHOOTING_GALLERY_COURSE_4_LETHAL_FIRST_KEY;
        gPlayerActorStateTickHeld = PLAYER_ACTOR_STATE_TICK_HELD;
    }
}

/// Restores movement and queues the player's collision passes and view triggers.
///
/// Borrows live player work. Queues the standard collision-enable request and
/// enables root view triggers. The player update applies root and part-1 grid
/// enables before consuming the request.
static inline void _mistShootingGalleryRestorePlayerControl(GameActor* player)
{
    player->movementInputDisabled                        = 0;
    player->pendingCollisionUpdates                      = GAME_ACTOR_COLLISION_REQUEST_MASK;
    player->collisionBodies[GAME_ACTOR_BODY_ROOT].flags |= WORLD_COLLISION_BODY_VIEW_TRIGGER_ENABLED;
}

/// Closes a completed gallery course after the remaining targets leave play.
///
/// Runs in controller state 2 with live player and course work. Acquires a menu
/// hold, waits 30 callback ticks, starts the red flash/sound and waits 90 more.
/// Restores player movement/collision, releases one battle reference and asks the
/// actor overlay to finish the course. Waits for battle-reset state to be observed
/// clear and then set before releasing the hold and task. Task teardown also runs
/// its exit callback, retaining the original second release ordering.
static void _mistShootingGalleryFinishCourse(Task* controller)
{
    enum {
        MIST_SHOOTING_GALLERY_FINISH_WAIT_FOR_TARGETS     = 0,
        MIST_SHOOTING_GALLERY_FINISH_PAUSE                = 1,
        MIST_SHOOTING_GALLERY_FINISH_FLASH                = 2,
        MIST_SHOOTING_GALLERY_FINISH_WAIT_FOR_RESET_CLEAR = 3,
        MIST_SHOOTING_GALLERY_FINISH_WAIT_FOR_RESET_SET   = 4,
        MIST_SHOOTING_GALLERY_FINISH_PAUSE_TICKS          = 30,
        MIST_SHOOTING_GALLERY_FINISH_FLASH_TICKS          = 90,
    };
    MistShootingGalleryWork* work;
    GameActor*               player;

    work   = controller->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    switch (work->phase) {
        case MIST_SHOOTING_GALLERY_FINISH_WAIT_FOR_TARGETS:
            if (work->liveTargets != 0) {
                return;
            }
            work->timer = MIST_SHOOTING_GALLERY_FINISH_PAUSE_TICKS;
            work->phase++;
            displayAcquireMenuHold();
        case MIST_SHOOTING_GALLERY_FINISH_PAUSE:
            if ((s16)work->timer-- > 0) {
                return;
            }
            work->timer = MIST_SHOOTING_GALLERY_FINISH_FLASH_TICKS;
            work->phase++;
            taskSpawnFromTable(gMistShootingGalleryTaskDescs, MIST_SHOOTING_GALLERY_TASK_RED_FLASH, 0, 0);
            sndEvtRequestScriptStart(SOUND_MIST_SHOOTING_GALLERY_ROUND_START, 0, 0);
            return;
        case MIST_SHOOTING_GALLERY_FINISH_FLASH:
            if ((s16)--work->timer > 0) {
                return;
            }
            work->phase++;
            // Restore control before releasing combat ownership and actor course state.
            _mistShootingGalleryRestorePlayerControl(player);
            sceneReleaseBattleRefAndClearRewards(controller, 0);
            actor215100FinishGalleryCourse();
            return;
        case MIST_SHOOTING_GALLERY_FINISH_WAIT_FOR_RESET_CLEAR:
            if (gGameSession->battleResetPending == 0) {
                work->phase++;
            }
            return;
        case MIST_SHOOTING_GALLERY_FINISH_WAIT_FOR_RESET_SET:
            if (gGameSession->battleResetPending == 1) {
                displayReleaseMenuHold();
                taskKill(controller);
            }
            return;
    }
}

s32 mistShootingGalleryGetBonusBp(s32 score)
{
    enum {
        MIST_SHOOTING_GALLERY_BONUS_BP_NONE   = 0,
        MIST_SHOOTING_GALLERY_BONUS_BP_LOW    = 100,
        MIST_SHOOTING_GALLERY_BONUS_BP_MEDIUM = 200,
        MIST_SHOOTING_GALLERY_BONUS_BP_HIGH   = 300,
    };

    const MistShootingGalleryWork* work    = gMistShootingGalleryControllerTask->work;
    s32                            bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_NONE;

    switch (work->course) {
        case 0:
            if (score >= 10000) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_HIGH;
            } else if (score >= 9000) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_MEDIUM;
            } else if (score >= 8000) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_LOW;
            }
            break;
        case 1:
            if (score >= 17400) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_HIGH;
            } else if (score >= 16800) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_MEDIUM;
            } else if (score >= 16000) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_LOW;
            }
            break;
        case 2:
            if (score > 49999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_HIGH;
            } else if (score > 45999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_MEDIUM;
            } else if (score > 38999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_LOW;
            }
            break;
        case 3:
            if (score > 59999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_HIGH;
            } else if (score > 55999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_MEDIUM;
            } else if (score > 51999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_LOW;
            }
            break;
        case 4:
            if (score > 54999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_HIGH;
            } else if (score > 52999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_MEDIUM;
            } else if (score > 49999) {
                bonusBp = MIST_SHOOTING_GALLERY_BONUS_BP_LOW;
            }
            break;
    }
    return bonusBp;
}

/// Slides the course countdown into view and draws its remaining time as MM:SS.
///
/// `timeLeft` is measured at 30 frames per second; subsecond frames are omitted.
/// The courses start at 1800, 3600 or 5400 frames, keeping all digits in 0..9.
/// Screen positions are pixels relative to the display's drawing origin.
static void _mistShootingGalleryDrawCountdownClock(MistShootingGalleryWork* work)
{
    enum {
        MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND = 30,
        MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_MINUTE = 60 * MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND,
        MIST_SHOOTING_GALLERY_CLOCK_COLON             = 10,
        MIST_SHOOTING_GALLERY_CLOCK_Y                 = 70,
    };

    s32 minuteTens;
    s32 minuteOnes;
    s32 secondTens;
    s32 secondOnes;
    s32 remainingFrames;

    /// Extracts and draws one decimal place of the remaining frame count.
    ///
    /// Captures work, remainingFrames and the clock Y constant. Pass a digit local,
    /// a positive constant framesPerPlace and a constant pixel offset. The first two
    /// arguments occur repeatedly; remainingFrames must be nonnegative and the
    /// quotient in 0..9. Leaves the sub-place remainder for the next digit and
    /// draws zero digits too. The glyph drawer allocates the sprite and page
    /// packets; this expansion only updates the digit and remaining frame count.
    /// Expands to several
    /// statements and must be invoked inside an explicit brace-delimited block.
#define MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(digit, framesPerPlace, xOffset) \
    (digit) = remainingFrames / (framesPerPlace);                              \
    if ((digit) != 0) {                                                        \
        remainingFrames %= (framesPerPlace);                                   \
    }                                                                          \
    _mistShootingGalleryDrawClockGlyph(work->clockX + (xOffset), MIST_SHOOTING_GALLERY_CLOCK_Y, (digit))

    remainingFrames = work->timeLeft;
    if (work->clockX < -0x78) {
        work->clockX += 0xA;
    }

    // Peel off each decimal place before submitting its glyph.
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(minuteTens, 10 * MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_MINUTE, 0);
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(minuteOnes, MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_MINUTE, 0xC);
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(secondTens, 10 * MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND, 0x24);
    MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE(secondOnes, MIST_SHOOTING_GALLERY_CLOCK_FRAMES_PER_SECOND, 0x30);

    _mistShootingGalleryDrawClockGlyph(work->clockX + 0x18, MIST_SHOOTING_GALLERY_CLOCK_Y, MIST_SHOOTING_GALLERY_CLOCK_COLON);
#undef MIST_SHOOTING_GALLERY_DRAW_CLOCK_PLACE
}

/// Queues one unmodulated 15 by 19 pixel clock glyph and its texture-page command.
///
/// `glyph` is 0..9 for a digit or 10 for the colon; cells begin every 16 texels.
/// X and Y are pixels relative to the drawing origin and narrow to signed 16 bits.
/// The current primitive buffer must hold an SPRT and a DR_TPAGE. The page command
/// is linked last so it executes before the sprite in the prepend-only table.
static void _mistShootingGalleryDrawClockGlyph(s32 screenX, s16 screenY, s32 glyph)
{
    enum {
        MIST_SHOOTING_GALLERY_CLOCK_GLYPH_STRIDE    = 16,
        MIST_SHOOTING_GALLERY_CLOCK_RAW_SPRITE_CODE = 0x65,
    };

    SPRT*     sprite;
    DR_TPAGE* drawMode;

    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    sprite->w      = 0xF;
    sprite->h      = 0x13;
    sprite->clut   = getClut(0, 261);
    setlen(sprite, 4);
    sprite->y0 = screenY;
    sprite->u0 = glyph * MIST_SHOOTING_GALLERY_CLOCK_GLYPH_STRIDE;
    sprite->v0 = 0;
    setcode(sprite, MIST_SHOOTING_GALLERY_CLOCK_RAW_SPRITE_CODE);
    sprite->x0 = screenX;
    addPrim(gGpuCurrentOt, sprite);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(0, GPU_BLEND_AVERAGE, 320, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

/// Queues an additive red tile covering the viewport, with intensity in 0..255.
///
/// The 384 by 256 pixel tile overhangs the centred viewport. The current primitive
/// buffer must hold a TILE and a DR_TPAGE; the draw-mode command is linked last
/// so additive blending is selected before the tile executes.
static void _mistShootingGalleryDrawRedFlash(u8 redIntensity)
{
    enum { MIST_SHOOTING_GALLERY_RED_FLASH_TILE_CODE = 0x62 };

    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    tile->x0       = -0xA8;
    tile->y0       = -0x7C;
    tile->w        = 0x180;
    tile->h        = 0x100;
    setlen(tile, 3);
    tile->r0 = redIntensity;
    tile->g0 = 0;
    tile->b0 = 0;
    setcode(tile, MIST_SHOOTING_GALLERY_RED_FLASH_TILE_CODE);
    addPrim(gGpuCurrentOt, tile);

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(0, GPU_BLEND_ADD, 320, 256));
    addPrim(gGpuCurrentOt, drawMode);
}

void mistShootingGallerySpawnDemoTarget(void)
{
    enum {
        MIST_SHOOTING_GALLERY_DEMO_TARGET_KIND     = 13,
        MIST_SHOOTING_GALLERY_DEMO_MOUNT_BEHAVIOUR = 2,
        MIST_SHOOTING_GALLERY_DEMO_BEHAVIOUR_SHIFT = 12,
        MIST_SHOOTING_GALLERY_DEMO_CLUT_ROW        = 2,
    };
    Enemy*     enemy;
    TmdObject* model;
    GfxCoord*  rootCoord;

    enemy = enemySpawnFromTable(gActor107600TaskDescriptors, ACTOR_107600_TASK_MOUNT, MIST_SHOOTING_GALLERY_DEMO_TARGET_KIND | (MIST_SHOOTING_GALLERY_DEMO_MOUNT_BEHAVIOUR << MIST_SHOOTING_GALLERY_DEMO_BEHAVIOUR_SHIFT), NULL);
    if (enemy != NULL) {
        model                    = enemy->task->extra.tmd;
        model->texturePageOffset = 0;
        model->clutRowOffset     = MIST_SHOOTING_GALLERY_DEMO_CLUT_ROW;
        // Both primitive-buffer halves must use the gallery palette.
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
        rootCoord             = enemy->task->extra.tmd->coords;
        rootCoord->coord.t[0] = 6000;
        rootCoord->coord.t[2] = 3000;
        rootCoord->coord.t[1] = 0;
        enemy->workType       = ENEMY_WORK_ACTOR;
    }
}

void mistShootingGallerySignalAction(void)
{
    MistShootingGalleryWork* work = gMistShootingGalleryControllerTask->work;

    work->actionTriggered = 1;
}

s32 mistShootingGalleryQualifiesForPrize(s32 bonusBp)
{
    enum {
        MIST_SHOOTING_GALLERY_FIRST_ADVANCED_PRIZE_COURSE = 3,
        MIST_SHOOTING_GALLERY_BASIC_PRIZE_BP              = 200,
        MIST_SHOOTING_GALLERY_ADVANCED_PRIZE_BP           = 300,
    };
    MistShootingGalleryWork* work      = gMistShootingGalleryControllerTask->work;
    s32                      qualifies = 0;

    if (work->course < MIST_SHOOTING_GALLERY_FIRST_ADVANCED_PRIZE_COURSE) {
        qualifies = bonusBp >= MIST_SHOOTING_GALLERY_BASIC_PRIZE_BP;
    } else if (bonusBp >= MIST_SHOOTING_GALLERY_ADVANCED_PRIZE_BP) {
        qualifies = 1;
    }
    return qualifies;
}

/// Dispatches initialization, course playback and closing for the gallery controller.
///
/// The descriptor starts at state zero; current state must remain in 0..2.
/// Initialization owns and publishes work that targets and result panels borrow.
/// The gallery and target overlays must remain loaded until controller teardown.
static void _mistShootingGalleryCourseTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gMistShootingGalleryControllerStates;
    states.funcs[task->state](task);
}

/// Dispatches the active controller's selected course script for this tick.
///
/// Requires initialized live work with course 0..4. Each course owns its phase,
/// wave and caption progression; its closing transition advances the task state.
static void _mistShootingGalleryRunCourse(Task* controller)
{
    MistShootingGalleryWork* work   = controller->work;
    TaskFuncTable5           rounds = _gMistShootingGalleryCourseHandlers;

    rounds.funcs[work->course](controller);
}

/// Restores player control and tears down the gallery controller.
///
/// Used as the controller's exit callback. Requires a live player task with
/// `GameActor` work. Releases one menu hold and battle reference before freeing
/// controller work; the final battle release clears pending rewards. The menu
/// release also runs for courses that acquired no hold.
static void _mistShootingGalleryExitCourse(Task* task)
{
    GameActor* player;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    _mistShootingGalleryRestorePlayerControl(player);
    displayReleaseMenuHold();
    sceneReleaseBattleRefAndClearRewards(task, 0);
    taskKill(task);
}

/// Decrements the course's 30 Hz clock while combat actors run; returns frames left.
///
/// A zero clock stays zero. Each course tests the result after drawing the clock
/// and enters its closing state on the tick that returns zero.
static u16 _mistShootingGalleryTickCourseClock(MistShootingGalleryWork* work)
{
    u16 framesLeft = work->timeLeft;

    if ((framesLeft != 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING)) {
        work->timeLeft = framesLeft - 1;
    }
    return work->timeLeft;
}

/// Runs a forty-tick red screen flash, drawing and fading on ticks 10..39.
///
/// No work block is allocated. The task owns a frame countdown in `killCountdown`
/// and an 8-bit colour intensity stored in `spawnArg1.value`. Initialization
/// replaces the spawn payload with 255; each visible tick reduces it by 8 when
/// above 8, then draws. The task kills itself before drawing at countdown zero.
static void _mistShootingGalleryRedFlashTask(Task* task)
{
    enum {
        MIST_SHOOTING_GALLERY_RED_FLASH_INIT              = 0,
        MIST_SHOOTING_GALLERY_RED_FLASH_FADE              = 1,
        MIST_SHOOTING_GALLERY_RED_FLASH_LIFETIME          = 40,
        MIST_SHOOTING_GALLERY_RED_FLASH_DRAW_BELOW        = 31,
        MIST_SHOOTING_GALLERY_RED_FLASH_INITIAL_INTENSITY = 255,
        MIST_SHOOTING_GALLERY_RED_FLASH_FADE_STEP         = 8,
    };

    s16 framesLeft;

    switch (task->state) {
        case MIST_SHOOTING_GALLERY_RED_FLASH_INIT:
            task->state           = MIST_SHOOTING_GALLERY_RED_FLASH_FADE;
            task->killCountdown   = MIST_SHOOTING_GALLERY_RED_FLASH_LIFETIME;
            task->spawnArg1.value = MIST_SHOOTING_GALLERY_RED_FLASH_INITIAL_INTENSITY;
        case MIST_SHOOTING_GALLERY_RED_FLASH_FADE:
            framesLeft = --task->killCountdown;
            if (framesLeft <= 0) {
                taskKill(task);
                return;
            }
            // Leave the opening ticks clear, then fade the red overlay.
            if (framesLeft < MIST_SHOOTING_GALLERY_RED_FLASH_DRAW_BELOW) {
                if (task->spawnArg1.value >= MIST_SHOOTING_GALLERY_RED_FLASH_FADE_STEP + 1) {
                    task->spawnArg1.value -= MIST_SHOOTING_GALLERY_RED_FLASH_FADE_STEP;
                }
                _mistShootingGalleryDrawRedFlash((u8)task->spawnArg1.value);
            }
            return;
    }
}

/// Selects a keyed CAP caption and opens its modal button-dismissed display.
///
/// commandIndex and captionKey follow `actor215100CapCaptionSelectRecord`'s
/// loaded-file bounds. The last text baseline is screen Y 208. dismissButtons
/// is a processed port-0 PAD_BUTTON_* mask, sign-extended from s16 to the task's
/// spawn word. Caption resources and
/// the actor overlay must stay loaded until the modal task closes.
static void _mistShootingGalleryShowCaption(s16 commandIndex, s16 captionKey, s16 dismissButtons)
{
    enum { MIST_SHOOTING_GALLERY_CAPTION_BOTTOM_BASELINE_Y = 208 };

    actor215100CapCaptionSelectRecord(commandIndex, captionKey, MIST_SHOOTING_GALLERY_CAPTION_BOTTOM_BASELINE_Y);
    displayQueueModeTask(&_gMistShootingGalleryCaptionModeTaskDesc, dismissButtons, 0, STAGE_ENTRY_RELOAD);
}

/// Displays the selected caption until a button press closes its display mode.
///
/// Starts in state 0, drawing through a sixteen-tick input delay before testing
/// port 0's newly pressed buttons against `spawnArg1.value`. An accepted press
/// skips drawing that tick; the next tick kills the task and requests mode exit.
/// Owns no work block. The selected caption, actor overlay and its resources
/// must remain live; this task does not advance the caption sequence.
static void _mistShootingGalleryCaptionModeTask(Task* task)
{
    enum { MIST_SHOOTING_GALLERY_CAPTION_INITIALIZE      = 0,
           MIST_SHOOTING_GALLERY_CAPTION_WAIT_FOR_BUTTON = 1,
           MIST_SHOOTING_GALLERY_CAPTION_CLOSE           = 2,
           MIST_SHOOTING_GALLERY_CAPTION_INPUT_DELAY     = 16 };

    switch (task->state) {
        case MIST_SHOOTING_GALLERY_CAPTION_INITIALIZE:
            task->state         = MIST_SHOOTING_GALLERY_CAPTION_WAIT_FOR_BUTTON;
            task->killCountdown = MIST_SHOOTING_GALLERY_CAPTION_INPUT_DELAY;
            // Draw on the initialization tick while consuming the first delay tick.
        case MIST_SHOOTING_GALLERY_CAPTION_WAIT_FOR_BUTTON:
            if (task->killCountdown != 0) {
                task->killCountdown--;
            } else {
                if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, task->spawnArg1.value) != 0) {
                    task->state = task->state + 1;
                } else {
                    actor215100CapCaptionDrawCurrent();
                }
                break;
            }
            actor215100CapCaptionDrawCurrent();
            break;
        case MIST_SHOOTING_GALLERY_CAPTION_CLOSE:
            taskKill(task);
            stageRequestModeTaskExit();
            break;
    }
}

/// Spawns, places and counts one scripted target mount for the active course.
///
/// Borrows a non-marker spawn record and initialized controller work. The signed
/// argument halves retain the original OR/shift packing; XYZ are world-frame game
/// coordinates. Returns borrowed enemy work on success, NULL on allocation failure.
/// A successful mount returns the live-target count when it is rejected or exits.
static Enemy* _mistShootingGallerySpawnTarget(Task* controller, const _MistShootingGallerySpawn* spawn)
{
    enum {
        MIST_SHOOTING_GALLERY_SPAWN_HIGH_HALF_SHIFT = 16,
        MIST_SHOOTING_GALLERY_TARGET_CLUT_ROW       = 2,
    };
    MistShootingGalleryWork* work;
    Enemy*                   enemy;
    TmdObject*               model;
    GfxCoord*                rootCoord;

    work  = controller->work;
    enemy = enemySpawnFromTable(gActor107600TaskDescriptors, ACTOR_107600_TASK_MOUNT, spawn->spawnArgLo | (spawn->spawnArgHi << MIST_SHOOTING_GALLERY_SPAWN_HIGH_HALF_SHIFT), NULL);
    if (enemy != NULL) {
        // Retain the parent overwrite before transfer; source-ring ownership is unproven.
        enemy->task->parent = controller;
        taskReparent(controller, enemy->task);
        model                    = enemy->task->extra.tmd;
        model->texturePageOffset = 0;
        model->clutRowOffset     = MIST_SHOOTING_GALLERY_TARGET_CLUT_ROW;
        // Both primitive-buffer halves must use the gallery palette.
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
        rootCoord             = enemy->task->extra.tmd->coords;
        rootCoord->coord.t[0] = spawn->x;
        rootCoord->coord.t[1] = spawn->y;
        rootCoord->coord.t[2] = spawn->z;
        enemy->workType       = ENEMY_WORK_ACTOR;
        work->liveTargets++;
    }
    return enemy;
}
