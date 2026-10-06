#include "rooms/mine_forked_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/actor_messages.h"

/// The area object's position / rotation path, one `SVECTOR` per step: `pos` and
/// `rot` are the halves `_mineForkedTunnelInitAreaObject` and
/// `_mineForkedTunnelApplyAreaObjectCommand` compose into the `ActorTransform` they
/// use for model placement (entry 0 of each) and that
/// `_mineForkedTunnelTickAreaObject` walks one entry per step of
/// `Task::killCountdown`, which holds at 110. Both are 240 entries - the
/// position table starts where the rotation table ends, and the pitch table
/// below starts where the position table ends.
extern SVECTOR D_mine_forked_tunnel_80181244[240];
extern SVECTOR D_mine_forked_tunnel_80180AC4[240];

/// The placement `_mineForkedTunnelInitAreaObject` uses instead when the
/// `0x75` game flag is set: a complete `ActorTransform` sitting in the room's
/// `.data`, offset (0x8CD, 0x3C4, 0x46B) with a half-turn about Y.
extern ActorTransform D_mine_forked_tunnel_80181BBC;

/// The `ActorTransform` the tunnel's pitch-animated object adopts: state 0
/// (`_mineForkedTunnelAttachAreaObjectChild`) copies it onto the task's coordinate
/// whole, and state 1 (`_mineForkedTunnelTiltAreaObjectChild`) then keeps its `pos`
/// while taking the `rot` from the pitch table below. Position
/// (0xB4, -0xEB, -0x30C), rotation zero.
extern ActorTransform D_mine_forked_tunnel_80181BA4;

/// The pitch curve `_mineForkedTunnelTiltAreaObjectChild` walks that object
/// through, one `SVECTOR` per step of the counter it runs while
/// `Task::spawnArg1` is 1: entries 0-15 are zero, then `vx` falls to -8 and
/// climbs to 175 before settling at 173 (4096 is a full turn), so the object
/// rises over the sequence and holds. The table is 54 entries, the counter's
/// limit, so the last step lands on the settling value.
extern SVECTOR D_mine_forked_tunnel_801819C4[54];

/// Two-entry `TaskDesc` table `_mineForkedTunnelInitAreaObject` spawns the
/// child part from; `taskSpawnFromTable` picks entry 1.
extern TaskDesc D_mine_forked_tunnel_80181B74[];

extern TaskMessageEntry D_mine_forked_tunnel_80181B8C[3];

/// Work block of the room's area object, the model the switch event sends
/// rolling down the tunnel slope; allocated zeroed and kept at `Task::work`.
///
/// The two matrices are the storage the object's `TmdObject::lightMtx` and
/// `colorMtx` point at, which its child part borrows as well.
typedef struct {
    MATRIX light;         // the model's light matrix
    MATRIX color;         // the model's colour matrix
    Task*  child;         // the part parented onto the object's coordinate and tilted separately; NULL when its spawn failed
    s32    freeCountdown; // frames until the hidden model's primitive buffers are freed, releasing them when it reaches 0 (-1 idle)
} _MineForkedTunnelAreaObjectWork;
STATIC_ASSERT_SIZEOF(_MineForkedTunnelAreaObjectWork, 0x48);

extern WorldCollisionGrid D_mine_forked_tunnel_80181C5C;
extern WorldCollisionGrid D_mine_forked_tunnel_80183D70;

/// The tunnel's per-view effect anchors, projected by
/// `mineForkedTunnelDrawViewFlaresTask` with `_glowDrawFlare`
/// (radius scale 0x300). Views 2 and 3 share the first anchor, view 4 draws the
/// second and third together and view 5 the fourth.
extern SVECTOR D_mine_forked_tunnel_80183614[];
extern SVECTOR D_mine_forked_tunnel_8018361C[];
extern SVECTOR D_mine_forked_tunnel_8018362C[];

extern TaskDesc         D_mine_forked_tunnel_80183104[];
extern TaskMessageEntry D_mine_forked_tunnel_80181C80[];
extern EvsCommand       D_mine_forked_tunnel_801831AC[];
extern EvsCommand       D_mine_forked_tunnel_801834F4[];

static void _mineForkedTunnelInitAreaObject(Task* task);
static void _mineForkedTunnelTickAreaObject(Task* task);
static s32  _mineForkedTunnelApplyAreaObjectCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused);
static void _mineForkedTunnelTiltAreaObjectChild(Task* task);
static void _mineForkedTunnelAreaObjectChildTask(Task* task);
static void _mineForkedTunnelExitAreaObject(Task* task);
static void _mineForkedTunnelBindAreaObjectLighting(Task* task);
static s32  _mineForkedTunnelSetAreaObjectDrawMode(Task* task, s32 messageId, s32 drawMode, s32 unused);
static void _mineForkedTunnelAttachAreaObjectChild(Task* task);
static void _mineForkedTunnelUpdateAreaObjectCollision(s32 switchUsed);
static void func_mine_forked_tunnel_8017E1E8(Task* arg0);
static void _mineForkedTunnelIdleRoomTask(Task* unusedTask);
static void _mineForkedTunnelSetSpriteBatchesHidden(u8 hidden);
static s32  _mineForkedTunnelHandleSwitchAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unused);
static void _mineForkedTunnelRestoreAreaObjectSoundMix(void);

/// Script commands accepted by the switch-controlled area object.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_RESET      = 0,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_TILT_CHILD = 1,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_START_PATH = 2,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_PARK       = 3,
};

/// Motion gates stored in the area object and child's first spawn argument.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE    = 0,
    MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING = 1,
};

/// Reserved collision-box prefix, smaller than the complete room grid pools.
enum {
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT   = 3,
    MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT = 8,
};

/// Restores the box's faces and XYZ components in the room's reserved prefix.
///
/// Grid pointers must be side-effect-free expressions with live, disjoint pools;
/// they are evaluated repeatedly. The room pools are writable and contain three
/// normals/faces and eight vertices. Vector fourth words and later entries are
/// retained. The index argument is a writable s32 cursor, left at eight.
#define MINE_FORKED_TUNNEL_COPY_AREA_OBJECT_COLLISION_BOX(roomGrid, templateGrid, entryIndex)                          \
    {                                                                                                                  \
        for ((entryIndex) = 0; (entryIndex) < MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_FACE_COUNT; (entryIndex)++) {   \
            (roomGrid)->normals[entryIndex].vx = (templateGrid)->normals[entryIndex].vx;                               \
            (roomGrid)->normals[entryIndex].vy = (templateGrid)->normals[entryIndex].vy;                               \
            (roomGrid)->normals[entryIndex].vz = (templateGrid)->normals[entryIndex].vz;                               \
            (roomGrid)->faces[entryIndex]      = (templateGrid)->faces[entryIndex];                                    \
        }                                                                                                              \
        for ((entryIndex) = 0; (entryIndex) < MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT; (entryIndex)++) { \
            (roomGrid)->vertices[entryIndex].vx = (templateGrid)->vertices[entryIndex].vx;                             \
            (roomGrid)->vertices[entryIndex].vy = (templateGrid)->vertices[entryIndex].vy;                             \
            (roomGrid)->vertices[entryIndex].vz = (templateGrid)->vertices[entryIndex].vz;                             \
        }                                                                                                              \
    }

/// Draw modes carried by the area object's ACTOR_MESSAGE_SET_MODEL_DRAW payload.
enum {
    MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_AUTO    = 0,
    MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_AUTO    = 1,
    MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_RELEASE = 2,
    MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_MANUAL  = 3,
};

/// Inventory request to use a collected key item in this room.
enum { MINE_FORKED_TUNNEL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// State table of the tunnel's area-object task, indexed by `Task::state`: set-up,
/// the per-frame path walk, and the exit that releases the area object.
static const TaskFuncTable3 D_mine_forked_tunnel_8017D5C4 = {
    { _mineForkedTunnelInitAreaObject, _mineForkedTunnelTickAreaObject, _mineForkedTunnelExitAreaObject },
};

/// State table of the area object's pitch-animated child, indexed by `Task::state`:
/// attach to the area object, walk the pitch curve, and `taskKill`.
static const TaskFuncTable3 D_mine_forked_tunnel_8017D5D0 = {
    { _mineForkedTunnelAttachAreaObjectChild, _mineForkedTunnelTiltAreaObjectChild, taskKill },
};

/// State table of the room's message-driven task, indexed by `Task::state`:
/// set-up, an idle state, and `taskKill`.
static const TaskFuncTable3 D_mine_forked_tunnel_8017D5DC = {
    { func_mine_forked_tunnel_8017E1E8, _mineForkedTunnelIdleRoomTask, taskKill },
};

static u32     _gMineForkedTunnelModel03340PartVerts[1];
static SVECTOR _gMineForkedTunnelModel03340Verts[20];
static SVECTOR _gMineForkedTunnelModel03340Normals[12];
static TmdBone _gMineForkedTunnelModel03340Skeleton[1];
static u32     _gMineForkedTunnelModel03340Stream[104];

static s32 _mineForkedTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unused);
s32        func_mine_forked_tunnel_8017E0F0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_mine_forked_tunnel_8017E134(Task*, s32, s32, s32);

void func_mine_forked_tunnel_8017E2E0(Task*);
void func_mine_forked_tunnel_8017E38C(Task*);

extern AnimationBankCopyRequest   D_mine_forked_tunnel_8018312C;
extern WorldCollisionGrid         D_mine_forked_tunnel_80183D70;
extern WorldCollisionTrigger      D_mine_forked_tunnel_80184F50[6];
extern WorldCollisionTrigger      D_mine_forked_tunnel_80185118[6];
extern WorldCoordRoomAmbientEntry D_mine_forked_tunnel_80185564[8];
extern WorldCoordRoomLights       D_mine_forked_tunnel_80184F38[1];

static TmdBone _gMineForkedTunnelModel01B48Skeleton[1] = {
#include "assets/mine_forked_tunnel_model_01B48_skeleton.inc"
};

static u32 _gMineForkedTunnelModel01B48PartVerts[1] = {
#include "assets/mine_forked_tunnel_model_01B48_partVerts.inc"
};

static SVECTOR _gMineForkedTunnelModel01B48Verts[223] = {
#include "assets/mine_forked_tunnel_model_01B48_verts.inc"
};

static SVECTOR _gMineForkedTunnelModel01B48Normals[56] = {
#include "assets/mine_forked_tunnel_model_01B48_normals.inc"
};

static u32 _gMineForkedTunnelModel01B48Stream[1451] = {
#include "assets/mine_forked_tunnel_model_01B48_stream.inc"
};

TmdSource gMineForkedTunnelModel01B48 = {
    0,
    10460,
    0,
    1,
    _gMineForkedTunnelModel01B48PartVerts,
    _gMineForkedTunnelModel01B48Verts,
    _gMineForkedTunnelModel01B48Normals,
    _gMineForkedTunnelModel01B48Skeleton,
    _gMineForkedTunnelModel01B48Stream,
};

static TmdBone _gMineForkedTunnelModel03340Skeleton[1] = {
#include "assets/mine_forked_tunnel_model_03340_skeleton.inc"
};

static u32 _gMineForkedTunnelModel03340PartVerts[1] = {
#include "assets/mine_forked_tunnel_model_03340_partVerts.inc"
};

static SVECTOR _gMineForkedTunnelModel03340Verts[20] = {
#include "assets/mine_forked_tunnel_model_03340_verts.inc"
};

static SVECTOR _gMineForkedTunnelModel03340Normals[12] = {
#include "assets/mine_forked_tunnel_model_03340_normals.inc"
};

static u32 _gMineForkedTunnelModel03340Stream[104] = {
#include "assets/mine_forked_tunnel_model_03340_stream.inc"
};

static TmdSource _gMineForkedTunnelModel03340 = {
    0,
    728,
    0,
    1,
    _gMineForkedTunnelModel03340PartVerts,
    _gMineForkedTunnelModel03340Verts,
    _gMineForkedTunnelModel03340Normals,
    _gMineForkedTunnelModel03340Skeleton,
    _gMineForkedTunnelModel03340Stream,
};

SVECTOR D_mine_forked_tunnel_80180AC4[240] = {
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -104, 2047, 0, 0 },
    { -103, 2047, 0, 0 },
    { -103, 2047, 0, 0 },
    { -102, 2047, 0, 0 },
    { -102, 2047, 0, 0 },
    { -102, 2047, 0, 0 },
    { -101, 2047, 0, 0 },
    { -101, 2047, 0, 0 },
    { -100, 2047, 0, 0 },
    { -100, 2047, 0, 0 },
    { -96, 2047, 0, 0 },
    { -92, 2047, 0, 0 },
    { -88, 2047, 0, 0 },
    { -84, 2047, 0, 0 },
    { -81, 2047, 0, 0 },
    { -77, 2047, 0, 0 },
    { -73, 2047, 0, 0 },
    { -69, 2047, 0, 0 },
    { -65, 2047, 0, 0 },
    { -61, 2047, 0, 0 },
    { -58, 2047, 0, 0 },
    { -43, 2047, 0, 0 },
    { -29, 2047, 0, 0 },
    { -14, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
    { 0, 2047, 0, 0 },
};

SVECTOR D_mine_forked_tunnel_80181244[240] = {
    { 2253, 45, 8778, 0 },
    { 2253, 46, 8772, 0 },
    { 2253, 47, 8766, 0 },
    { 2253, 48, 8760, 0 },
    { 2253, 49, 8754, 0 },
    { 2253, 50, 8748, 0 },
    { 2253, 51, 8743, 0 },
    { 2253, 52, 8737, 0 },
    { 2253, 53, 8731, 0 },
    { 2253, 54, 8725, 0 },
    { 2253, 55, 8719, 0 },
    { 2253, 56, 8713, 0 },
    { 2253, 57, 8707, 0 },
    { 2253, 58, 8701, 0 },
    { 2253, 59, 8695, 0 },
    { 2253, 59, 8689, 0 },
    { 2253, 60, 8683, 0 },
    { 2253, 61, 8677, 0 },
    { 2253, 62, 8671, 0 },
    { 2253, 63, 8665, 0 },
    { 2253, 64, 8659, 0 },
    { 2253, 65, 8653, 0 },
    { 2253, 66, 8647, 0 },
    { 2253, 67, 8641, 0 },
    { 2253, 68, 8635, 0 },
    { 2253, 69, 8629, 0 },
    { 2253, 70, 8623, 0 },
    { 2253, 71, 8617, 0 },
    { 2253, 72, 8612, 0 },
    { 2253, 73, 8606, 0 },
    { 2253, 73, 8600, 0 },
    { 2253, 74, 8594, 0 },
    { 2253, 75, 8588, 0 },
    { 2253, 76, 8582, 0 },
    { 2253, 77, 8576, 0 },
    { 2253, 78, 8570, 0 },
    { 2253, 79, 8564, 0 },
    { 2253, 80, 8558, 0 },
    { 2253, 81, 8552, 0 },
    { 2253, 82, 8546, 0 },
    { 2253, 83, 8540, 0 },
    { 2253, 84, 8534, 0 },
    { 2253, 85, 8528, 0 },
    { 2253, 86, 8522, 0 },
    { 2253, 87, 8516, 0 },
    { 2253, 87, 8510, 0 },
    { 2253, 88, 8504, 0 },
    { 2253, 89, 8498, 0 },
    { 2253, 90, 8492, 0 },
    { 2253, 91, 8487, 0 },
    { 2253, 98, 8447, 0 },
    { 2253, 105, 8408, 0 },
    { 2253, 112, 8369, 0 },
    { 2253, 119, 8330, 0 },
    { 2253, 126, 8290, 0 },
    { 2253, 133, 8251, 0 },
    { 2253, 140, 8212, 0 },
    { 2253, 147, 8173, 0 },
    { 2253, 153, 8133, 0 },
    { 2253, 160, 8094, 0 },
    { 2253, 167, 8055, 0 },
    { 2253, 174, 8016, 0 },
    { 2253, 181, 7976, 0 },
    { 2253, 188, 7937, 0 },
    { 2253, 195, 7898, 0 },
    { 2253, 202, 7859, 0 },
    { 2253, 209, 7819, 0 },
    { 2253, 216, 7780, 0 },
    { 2253, 223, 7741, 0 },
    { 2253, 230, 7702, 0 },
    { 2253, 237, 7663, 0 },
    { 2253, 243, 7623, 0 },
    { 2253, 250, 7584, 0 },
    { 2253, 257, 7545, 0 },
    { 2253, 264, 7506, 0 },
    { 2253, 271, 7466, 0 },
    { 2253, 278, 7427, 0 },
    { 2253, 285, 7388, 0 },
    { 2253, 292, 7349, 0 },
    { 2253, 299, 7309, 0 },
    { 2253, 306, 7270, 0 },
    { 2253, 313, 7231, 0 },
    { 2253, 320, 7192, 0 },
    { 2253, 327, 7152, 0 },
    { 2253, 333, 7113, 0 },
    { 2253, 340, 7074, 0 },
    { 2253, 347, 7035, 0 },
    { 2253, 354, 6995, 0 },
    { 2253, 361, 6956, 0 },
    { 2253, 368, 6917, 0 },
    { 2253, 375, 6878, 0 },
    { 2253, 382, 6839, 0 },
    { 2253, 389, 6799, 0 },
    { 2253, 396, 6760, 0 },
    { 2253, 403, 6721, 0 },
    { 2253, 410, 6682, 0 },
    { 2253, 417, 6642, 0 },
    { 2253, 423, 6603, 0 },
    { 2253, 430, 6564, 0 },
    { 2253, 437, 6525, 0 },
    { 2253, 444, 6485, 0 },
    { 2253, 451, 6446, 0 },
    { 2253, 458, 6407, 0 },
    { 2253, 465, 6368, 0 },
    { 2253, 472, 6328, 0 },
    { 2253, 479, 6289, 0 },
    { 2253, 486, 6250, 0 },
    { 2253, 493, 6211, 0 },
    { 2253, 500, 6172, 0 },
    { 2253, 507, 6132, 0 },
    { 2253, 513, 6093, 0 },
    { 2253, 520, 6054, 0 },
    { 2253, 527, 6015, 0 },
    { 2253, 534, 5975, 0 },
    { 2253, 541, 5936, 0 },
    { 2253, 548, 5897, 0 },
    { 2253, 555, 5858, 0 },
    { 2253, 562, 5818, 0 },
    { 2253, 569, 5779, 0 },
    { 2253, 576, 5740, 0 },
    { 2253, 583, 5701, 0 },
    { 2253, 590, 5661, 0 },
    { 2253, 596, 5622, 0 },
    { 2253, 603, 5583, 0 },
    { 2253, 610, 5544, 0 },
    { 2253, 617, 5504, 0 },
    { 2253, 624, 5465, 0 },
    { 2253, 631, 5426, 0 },
    { 2253, 638, 5387, 0 },
    { 2253, 645, 5348, 0 },
    { 2253, 652, 5308, 0 },
    { 2253, 659, 5269, 0 },
    { 2253, 666, 5230, 0 },
    { 2253, 673, 5191, 0 },
    { 2253, 680, 5151, 0 },
    { 2253, 686, 5112, 0 },
    { 2253, 693, 5073, 0 },
    { 2253, 700, 5034, 0 },
    { 2253, 707, 4994, 0 },
    { 2253, 714, 4955, 0 },
    { 2253, 721, 4916, 0 },
    { 2253, 728, 4877, 0 },
    { 2253, 735, 4837, 0 },
    { 2253, 742, 4798, 0 },
    { 2253, 749, 4759, 0 },
    { 2253, 756, 4720, 0 },
    { 2253, 763, 4680, 0 },
    { 2253, 770, 4641, 0 },
    { 2253, 776, 4602, 0 },
    { 2253, 783, 4563, 0 },
    { 2253, 790, 4524, 0 },
    { 2253, 797, 4484, 0 },
    { 2253, 804, 4445, 0 },
    { 2253, 811, 4406, 0 },
    { 2253, 818, 4367, 0 },
    { 2253, 825, 4327, 0 },
    { 2253, 832, 4288, 0 },
    { 2253, 839, 4249, 0 },
    { 2253, 846, 4210, 0 },
    { 2253, 853, 4170, 0 },
    { 2253, 858, 4131, 0 },
    { 2253, 864, 4092, 0 },
    { 2253, 870, 4053, 0 },
    { 2253, 876, 4013, 0 },
    { 2253, 882, 3974, 0 },
    { 2253, 888, 3935, 0 },
    { 2253, 893, 3896, 0 },
    { 2253, 899, 3856, 0 },
    { 2253, 905, 3817, 0 },
    { 2253, 907, 3778, 0 },
    { 2253, 909, 3739, 0 },
    { 2253, 911, 3700, 0 },
    { 2253, 914, 3660, 0 },
    { 2253, 916, 3621, 0 },
    { 2253, 918, 3582, 0 },
    { 2253, 920, 3543, 0 },
    { 2253, 922, 3503, 0 },
    { 2253, 924, 3464, 0 },
    { 2253, 926, 3425, 0 },
    { 2253, 928, 3386, 0 },
    { 2253, 930, 3346, 0 },
    { 2253, 933, 3307, 0 },
    { 2253, 935, 3268, 0 },
    { 2253, 937, 3229, 0 },
    { 2253, 939, 3190, 0 },
    { 2253, 941, 3150, 0 },
    { 2253, 943, 3111, 0 },
    { 2253, 945, 3072, 0 },
    { 2253, 947, 3033, 0 },
    { 2253, 949, 2994, 0 },
    { 2253, 951, 2954, 0 },
    { 2253, 954, 2915, 0 },
    { 2253, 956, 2876, 0 },
    { 2253, 958, 2837, 0 },
    { 2253, 958, 2798, 0 },
    { 2253, 958, 2758, 0 },
    { 2253, 958, 2719, 0 },
    { 2253, 958, 2680, 0 },
    { 2253, 958, 2641, 0 },
    { 2253, 958, 2601, 0 },
    { 2253, 957, 2562, 0 },
    { 2253, 957, 2523, 0 },
    { 2253, 957, 2484, 0 },
    { 2253, 957, 2444, 0 },
    { 2253, 957, 2405, 0 },
    { 2253, 957, 2366, 0 },
    { 2253, 957, 2327, 0 },
    { 2253, 957, 2287, 0 },
    { 2253, 957, 2248, 0 },
    { 2253, 957, 2209, 0 },
    { 2253, 957, 2170, 0 },
    { 2253, 957, 2130, 0 },
    { 2253, 957, 2091, 0 },
    { 2253, 957, 2052, 0 },
    { 2253, 957, 2013, 0 },
    { 2253, 957, 1973, 0 },
    { 2253, 957, 1934, 0 },
    { 2253, 957, 1895, 0 },
    { 2253, 957, 1856, 0 },
    { 2253, 956, 1816, 0 },
    { 2253, 956, 1777, 0 },
    { 2253, 956, 1738, 0 },
    { 2253, 956, 1699, 0 },
    { 2253, 956, 1659, 0 },
    { 2253, 956, 1620, 0 },
    { 2253, 956, 1581, 0 },
    { 2253, 956, 1542, 0 },
    { 2253, 956, 1502, 0 },
    { 2253, 956, 1463, 0 },
    { 2253, 956, 1424, 0 },
    { 2253, 956, 1385, 0 },
    { 2253, 956, 1345, 0 },
    { 2253, 956, 1306, 0 },
    { 2253, 956, 1267, 0 },
    { 2253, 956, 1228, 0 },
    { 2253, 956, 1188, 0 },
    { 2253, 956, 1149, 0 },
    { 2253, 956, 1110, 0 },
    { 2253, 955, 1071, 0 },
    { 2253, 955, 1031, 0 },
};

SVECTOR D_mine_forked_tunnel_801819C4[54] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { -1, 0, 0, 0 },
    { -2, 0, 0, 0 },
    { -4, 0, 0, 0 },
    { -6, 0, 0, 0 },
    { -7, 0, 0, 0 },
    { -8, 0, 0, 0 },
    { -8, 0, 0, 0 },
    { -8, 0, 0, 0 },
    { -6, 0, 0, 0 },
    { -3, 0, 0, 0 },
    { 1, 0, 0, 0 },
    { 7, 0, 0, 0 },
    { 14, 0, 0, 0 },
    { 21, 0, 0, 0 },
    { 30, 0, 0, 0 },
    { 39, 0, 0, 0 },
    { 48, 0, 0, 0 },
    { 58, 0, 0, 0 },
    { 68, 0, 0, 0 },
    { 78, 0, 0, 0 },
    { 89, 0, 0, 0 },
    { 99, 0, 0, 0 },
    { 109, 0, 0, 0 },
    { 120, 0, 0, 0 },
    { 130, 0, 0, 0 },
    { 139, 0, 0, 0 },
    { 149, 0, 0, 0 },
    { 157, 0, 0, 0 },
    { 165, 0, 0, 0 },
    { 173, 0, 0, 0 },
    { 175, 0, 0, 0 },
    { 172, 0, 0, 0 },
    { 166, 0, 0, 0 },
    { 163, 0, 0, 0 },
    { 165, 0, 0, 0 },
    { 168, 0, 0, 0 },
    { 171, 0, 0, 0 },
    { 173, 0, 0, 0 },
};

TaskDesc D_mine_forked_tunnel_80181B74[2] = {
    { { { TASK_BODY_TMD, 192 } }, mineForkedTunnelAreaObjectTask, { .model = &gMineForkedTunnelModel01B48 } },
    { { { TASK_BODY_TMD, 192 } }, _mineForkedTunnelAreaObjectChildTask, { .model = &_gMineForkedTunnelModel03340 } },
};

TaskMessageEntry D_mine_forked_tunnel_80181B8C[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _mineForkedTunnelSetAreaObjectDrawMode },
    { ACTOR_COMMAND_MESSAGE_APPLY, _mineForkedTunnelApplyAreaObjectCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorTransform D_mine_forked_tunnel_80181BA4 = { { 180, -235, -780, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_mine_forked_tunnel_80181BBC = { { 2253, 964, 1131, 0 }, { 0, 2047, 0, 0 } };

static SVECTOR _gMineForkedTunnelCollision0469CNormals[3] = {
#include "assets/mine_forked_tunnel_collision_0469C_normals.inc"
};

static SVECTOR _gMineForkedTunnelCollision0469CVerts[8] = {
#include "assets/mine_forked_tunnel_collision_0469C_verts.inc"
};

static WorldCollisionGridFace _gMineForkedTunnelCollision0469CFaces[3] = {
#include "assets/mine_forked_tunnel_collision_0469C_faces.inc"
};

static s16 _gMineForkedTunnelCollision0469CCells[4] = {
#include "assets/mine_forked_tunnel_collision_0469C_cells.inc"
};

#define GRID_CELL(i) (&_gMineForkedTunnelCollision0469CCells[i])
static s16* _gMineForkedTunnelCollision0469CTable[1] = {
#include "assets/mine_forked_tunnel_collision_0469C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_forked_tunnel_80181C5C = { NULL, _gMineForkedTunnelCollision0469CNormals, _gMineForkedTunnelCollision0469CVerts, _gMineForkedTunnelCollision0469CFaces, _gMineForkedTunnelCollision0469CTable, -1747, -7643, 1, 1, 4000, 3 };

TaskMessageEntry D_mine_forked_tunnel_80181C80[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_mine_forked_tunnel_8017E0F0 },
    { MINE_FORKED_TUNNEL_MESSAGE_USE_KEY_ITEM, _mineForkedTunnelRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mineForkedTunnelHandleSwitchAction },
    { ROOM_MESSAGE_COMMAND, func_mine_forked_tunnel_8017E134 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static AnimationPackedPose _gMineForkedTunnelAnimation049C4Bank1[6] = {
#include "assets/mine_forked_tunnel_animation_049C4_bank1.inc"
};

static AnimationPackedRotation _gMineForkedTunnelAnimation049C4Bank4[46] = {
#include "assets/mine_forked_tunnel_animation_049C4_bank4.inc"
};

static AnimationRecord _gMineForkedTunnelAnimation049C4Records[109] = {
#include "assets/mine_forked_tunnel_animation_049C4_records.inc"
};

static u16 _gMineForkedTunnelAnimation049C4Indices[20] = {
#include "assets/mine_forked_tunnel_animation_049C4_indices.inc"
};

static AnimationSet _gMineForkedTunnelAnimation049C4 = {
    _gMineForkedTunnelAnimation049C4Records,
    _gMineForkedTunnelAnimation049C4Indices,
    { NULL, _gMineForkedTunnelAnimation049C4Bank1, NULL, NULL, _gMineForkedTunnelAnimation049C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineForkedTunnelAnimation04C50Bank1[2] = {
#include "assets/mine_forked_tunnel_animation_04C50_bank1.inc"
};

static AnimationPackedRotation _gMineForkedTunnelAnimation04C50Bank4[47] = {
#include "assets/mine_forked_tunnel_animation_04C50_bank4.inc"
};

static AnimationRecord _gMineForkedTunnelAnimation04C50Records[90] = {
#include "assets/mine_forked_tunnel_animation_04C50_records.inc"
};

static u16 _gMineForkedTunnelAnimation04C50Indices[20] = {
#include "assets/mine_forked_tunnel_animation_04C50_indices.inc"
};

static AnimationSet _gMineForkedTunnelAnimation04C50 = {
    _gMineForkedTunnelAnimation04C50Records,
    _gMineForkedTunnelAnimation04C50Indices,
    { NULL, _gMineForkedTunnelAnimation04C50Bank1, NULL, NULL, _gMineForkedTunnelAnimation04C50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gMineForkedTunnelAnimation05B1CBank1[33] = {
#include "assets/mine_forked_tunnel_animation_05B1C_bank1.inc"
};

static AnimationPackedRotation _gMineForkedTunnelAnimation05B1CBank4[381] = {
#include "assets/mine_forked_tunnel_animation_05B1C_bank4.inc"
};

static AnimationRecord _gMineForkedTunnelAnimation05B1CRecords[447] = {
#include "assets/mine_forked_tunnel_animation_05B1C_records.inc"
};

static u16 _gMineForkedTunnelAnimation05B1CIndices[20] = {
#include "assets/mine_forked_tunnel_animation_05B1C_indices.inc"
};

static AnimationSet _gMineForkedTunnelAnimation05B1C = {
    _gMineForkedTunnelAnimation05B1CRecords,
    _gMineForkedTunnelAnimation05B1CIndices,
    { NULL, _gMineForkedTunnelAnimation05B1CBank1, NULL, NULL, _gMineForkedTunnelAnimation05B1CBank4, NULL, NULL, NULL },
};

TaskDesc D_mine_forked_tunnel_80183104[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_mine_forked_tunnel_8017E2E0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mine_forked_tunnel_8017E38C, { .value = 0 } },
};

AnimationSet* D_mine_forked_tunnel_8018311C[4] = {
    NULL,
    &_gMineForkedTunnelAnimation049C4,
    &_gMineForkedTunnelAnimation04C50,
    &_gMineForkedTunnelAnimation05B1C,
};

AnimationBankCopyRequest D_mine_forked_tunnel_8018312C = { { .sets = D_mine_forked_tunnel_8018311C }, ARRAY_SIZE(D_mine_forked_tunnel_8018311C) };

AnimationPlayRequest D_mine_forked_tunnel_80183134 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mine_forked_tunnel_80183148 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_forked_tunnel_8018315C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mine_forked_tunnel_80183170 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_mine_forked_tunnel_80183184 = { { 2251, 0, 9707, 0 }, { 0, 2047, 0, 0 } };

ActorCommand D_mine_forked_tunnel_8018319C = { { .loc = { 4, 7 } }, MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_RESET };

ActorCommand D_mine_forked_tunnel_801831A0 = { { .loc = { 4, 7 } }, MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_TILT_CHILD };

ActorCommand D_mine_forked_tunnel_801831A4 = { { .loc = { 4, 7 } }, MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_START_PATH };

ActorCommand D_mine_forked_tunnel_801831A8 = { { .loc = { 4, 7 } }, MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_PARK };

EvsCommand D_mine_forked_tunnel_801831AC[35] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mine_forked_tunnel_8018312C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_forked_tunnel_8018315C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mine_forked_tunnel_80183184 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 55 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mine_forked_tunnel_8018319C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 55 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mine_forked_tunnel_801831A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54070003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_forked_tunnel_80183170 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54070005 }, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 55 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mine_forked_tunnel_801831A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mine_forked_tunnel_80183148 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mineForkedTunnelRestoreAreaObjectSoundMix }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 59 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54070005 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 55 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mine_forked_tunnel_801831A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54070006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mine_forked_tunnel_801834F4[12] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x54070005 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 55 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mine_forked_tunnel_801831A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54070006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_mine_forked_tunnel_80183614[1] = {
    { 2250, -1870, 9120, 0 },
};

SVECTOR D_mine_forked_tunnel_8018361C[2] = {
    { 2250, -1260, 6010, 0 },
    { 2250, -1060, 3200, 0 },
};

SVECTOR D_mine_forked_tunnel_8018362C[1] = {
    { 430, -950, 2530, 0 },
};

WorldCoordRoomLighting D_mine_forked_tunnel_80183634[1] = {
    { D_mine_forked_tunnel_80184F38, D_mine_forked_tunnel_80185564 },
};

WorldCollisionRoomResources D_mine_forked_tunnel_8018363C[1] = {
    { &D_mine_forked_tunnel_80183D70, D_mine_forked_tunnel_80184F50, D_mine_forked_tunnel_80185118, NULL },
};

u8* D_mine_forked_tunnel_8018364C[1] = {
    gViewIdentityMap,
};

ViewCount D_mine_forked_tunnel_80183650[1] = { 7 };

DirectionWarpEntry D_mine_forked_tunnel_80183654[1] = {
    { { { .word = 2048 }, 2350, 0, 0x2904 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 2350, 0, 0x2904 }, { 0, 0, 0, 0 }, 0x54070002, 0x54070001, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gMineForkedTunnelCollision067B0Normals[33] = {
#include "assets/mine_forked_tunnel_collision_067B0_normals.inc"
};

static SVECTOR _gMineForkedTunnelCollision067B0Verts[88] = {
#include "assets/mine_forked_tunnel_collision_067B0_verts.inc"
};

static WorldCollisionGridFace _gMineForkedTunnelCollision067B0Faces[37] = {
#include "assets/mine_forked_tunnel_collision_067B0_faces.inc"
};

static s16 _gMineForkedTunnelCollision067B0Cells[160] = {
#include "assets/mine_forked_tunnel_collision_067B0_cells.inc"
};

#define GRID_CELL(i) (&_gMineForkedTunnelCollision067B0Cells[i])
static s16* _gMineForkedTunnelCollision067B0Table[8] = {
#include "assets/mine_forked_tunnel_collision_067B0_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_mine_forked_tunnel_80183D70 = { NULL, _gMineForkedTunnelCollision067B0Normals, _gMineForkedTunnelCollision067B0Verts, _gMineForkedTunnelCollision067B0Faces, _gMineForkedTunnelCollision067B0Table, 650, 520, 2, 4, 4000, 37 };

ViewCamera D_mine_forked_tunnel_80183D94[7] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { -1500, 0x40CA, -5500 } }, 289 },
    { { { { 4001, 0, 872 }, { -228, 3952, 1049 }, { -842, -1074, 3861 } }, { -2676, -186, -4405 } }, 289 },
    { { { { 2839, 0, 2952 }, { 2773, 1402, -2667 }, { -1010, 3848, 972 } }, { -2889, 3366, -9548 } }, 269 },
    { { { { -3961, 0, 1040 }, { 284, 3939, 1084 }, { -1001, 1121, -3810 } }, { -2883, 1117, -8571 } }, 289 },
    { { { { -2159, 0, 3480 }, { 2753, 2505, 1708 }, { -2128, 3240, -1320 } }, { -2927, 2785, -2683 } }, 257 },
    { { { { -605, 0, 4050 }, { 477, 4067, 71 }, { -4022, 483, -601 } }, { -3100, 876, -9740 } }, 289 },
    { { { { -3711, 0, -1732 }, { -1330, 2622, 2850 }, { 1109, 3145, -2376 } }, { -1662, 2731, -0x2A11 } }, 269 },
};

SpriteBatch D_mine_forked_tunnel_80183E90[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_forked_tunnel_80183EA0[35] = {
    { 142, 0x3FC0, { .fields = { 40, 64 } }, -136, -120, 517, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -144, -56, 483, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -160, 24, 444, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 429, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -160, -56, 434, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, -120, 469, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 88, -120, 433, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 96, -96, 389, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, -16, 364, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 112, 64, 364, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 128, 64, 314, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, 128, -16, 331, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, -96, 356, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, -120, 374, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -56, 1246, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -80, 1575, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -24, -112, 1338, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 16, -112, 1320, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 48, -112, 1149, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, -72, 1110, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 64, -56, 1096, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -24, -72, 1269, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -24, 0, 1186, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 64, 16, 1034, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, 48, 571, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -120, 88, 492, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 104, 478, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 16, 629, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 594, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, 80, 569, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, -8, 683, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -96, 0, 634, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 32, 655, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, 80, 613, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, 0, 702, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_forked_tunnel_8018415C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 10, 0, 0, { 2, 0 } },
    { 24, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_forked_tunnel_80184184[36] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 40, 350, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 48, 96, 213, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 72, 224, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 72, 72, 219, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 88, 48, 224, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 112, 16, 235, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -8, 250, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 16, 528, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 16, 850, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 24, 591, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 32, 671, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 32, 777, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 40, 819, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 24, 844, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -120, 40, 737, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 32, 641, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, 24, 568, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 16, 518, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -48, 436, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -32, 486, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -128, -16, 549, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -8, 647, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 8, 705, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 24, 703, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 16, 616, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 8, 548, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 0, 493, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 56, 117, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 101, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 112, 119, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 112, 150, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 104, 195, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 104, 246, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 96, 362, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 96, 588, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 104, 747, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_forked_tunnel_80184454[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 29, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_forked_tunnel_80184474[82] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 80, 857, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -24, -120, 604, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 32 } }, -144, -120, 542, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -120, 40, 630, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 72, 656, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, -128, 0, 607, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -136, -40, 584, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -136, -88, 561, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 64, -80, 736, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 64, -120, 706, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 56, -32, 756, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, -120, 683, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -80, 710, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 88, -32, 738, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 0, 753, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 56, 16, 793, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 56, 822, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 80, 64, 798, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, 16, 791, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -120, 576, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -96, 1286, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -88, 1982, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -120, 1926, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -88, -120, 1182, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -88, -96, 1208, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -88, -64, 1246, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -88, -32, 1282, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 0, 1335, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -32, -120, 1276, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -96, 1309, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, -8, 1427, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -16, -56, 1363, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 64, 432, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 72, 326, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -160, 48, 351, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 24, 374, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 8, 406, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -144, 16, 425, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -120, 48, 480, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -112, 64, 452, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 88, 436, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 104, 425, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 104, 400, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 104, 377, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 88, 401, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 72, 409, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -136, 48, 408, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 24, 441, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -160, -120, 484, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -120, 1083, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 900, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 16, 1375, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -8, 1337, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 0, -120, 1260, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 8, -112, 1211, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -104, 1298, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 24, -96, 1437, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 1321, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, -120, 1044, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -120, 1014, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 0, 1175, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 24, -32, 1190, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -120, 934, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -120, 805, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -104, 997, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -104, 866, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -88, 1120, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -88, 1021, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, -72, 1176, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -72, 1068, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -32, 1114, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, -32, 980, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, -16, 1110, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 0, 1112, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 16, 1092, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 32, 969, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -16, 918, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, 8, 964, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, 32, 892, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 64, -120, 877, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 8, -88, 1984, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 0, -40, 2019, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_forked_tunnel_80184ADC[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { 19, 13, 0, 0, { 3, 0 } },
    { 32, 17, 0, 0, { 2, 0 } },
    { 49, 31, 0, 0, { 4, 0 } },
    { 80, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_mine_forked_tunnel_80184B14[26] = {
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, -72, 696, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, -88, 672, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 685, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, -120, 642, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 96, -120, 611, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -88, 358, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -72, 381, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -56, 404, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 16, 272, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 24, 285, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 32, 300, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 40, 317, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, -16, 437, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 564, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -64, 513, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 112, -40, 473, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 8, 410, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 32, 383, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 56, 357, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 330, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -8, -120, 919, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -32, -120, 961, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -8, -72, 1039, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -32, -72, 1087, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, -8, -24, 1213, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -32, -24, 1271, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_mine_forked_tunnel_80184D1C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 1, 0 } },
    { 2, 18, 0, 0, { 2, 0 } },
    { 20, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_forked_tunnel_80184D44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_mine_forked_tunnel_80184D54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_mine_forked_tunnel_80184D64[7] = {
    { { .empty = D_mine_forked_tunnel_80183E90 }, D_mine_forked_tunnel_80183E90, NULL },
    { { .elements = D_mine_forked_tunnel_80183EA0 }, D_mine_forked_tunnel_8018415C, NULL },
    { { .elements = D_mine_forked_tunnel_80184184 }, D_mine_forked_tunnel_80184454, NULL },
    { { .elements = D_mine_forked_tunnel_80184474 }, D_mine_forked_tunnel_80184ADC, NULL },
    { { .elements = D_mine_forked_tunnel_80184B14 }, D_mine_forked_tunnel_80184D1C, NULL },
    { { .empty = D_mine_forked_tunnel_80184D44 }, D_mine_forked_tunnel_80184D44, NULL },
    { { .empty = D_mine_forked_tunnel_80184D54 }, D_mine_forked_tunnel_80184D54, NULL },
};

WorldCoordPointLight D_mine_forked_tunnel_80184DB8[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2240, -1639, 9120 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3316, 2385 }, { 0, 0 } }, 259, 2501 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2310, -657, 6485 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3211, 2810, 1964 }, { 0, 0 } }, 201, 3004 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2240, -900, 3700 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3397, 2467 }, { 0, 0 } }, 259, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1421, -769, 2480 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3316, 2385 }, { 0, 0 } }, 280, 2961 },
};

WorldCoordRoomLights D_mine_forked_tunnel_80184F38[1] = {
    { 0, NULL, ARRAY_SIZE(D_mine_forked_tunnel_80184DB8), D_mine_forked_tunnel_80184DB8, 0, NULL },
};

WorldCollisionTrigger D_mine_forked_tunnel_80184F50[6] = {
    { NULL, NULL, NULL, { 1572, -960, 2665, 0 }, { { -829, -3520, 1976, 0 }, { 830, -3520, -1975, 0 }, { -829, 3520, 1976, 0 }, { 830, 3520, -1975, 0 } }, { -3786, 0, -1590, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1440, -976, 2644, 0 }, { { 796, -3552, -1936, 0 }, { -795, -3552, 1937, 0 }, { 796, 3552, -1936, 0 }, { -795, 3552, 1937, 0 } }, { 3789, 0, 1556, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2272, -992, 6400, 0 }, { { 1536, -2864, 0, 0 }, { -1536, -2864, 0, 0 }, { 1536, 2864, 0, 0 }, { -1536, 2864, 0, 0 } }, { 0, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2304, -1041, 6561, 0 }, { { -1536, -3008, 0, 0 }, { 1536, -3008, 0, 0 }, { -1536, 3008, 0, 0 }, { 1536, 3008, 0, 0 } }, { 0, 0, -4114, 0 }, { 0, 0, 4096, 0 }, 3376, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2304, -1040, 9488, 0 }, { { -1536, -3040, 0, 0 }, { 1536, -3040, 0, 0 }, { -1536, 3040, 0, 0 }, { 1536, 3040, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 3405, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2336, -944, 9408, 0 }, { { 1536, -2976, 0, 0 }, { -1536, -2976, 0, 0 }, { 1536, 2976, 0, 0 }, { -1536, 2976, 0, 0 } }, { 0, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_mine_forked_tunnel_80185118[6] = {
    { NULL, NULL, NULL, { 2240, -48, 0x2A10, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2208, -64, 9600, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, 896, 976, 0 }, { { -656, 0, -1088, 0 }, { 656, 0, -1088, 0 }, { -656, 0, 1088, 0 }, { 656, 0, 1088, 0 } }, { 0, 4105, 0, 0 }, { 4052, 0, 601, 0 }, 1267, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1632, 896, 576, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, 4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2304, 912, 2080, 0 }, { { -1024, 0, -368, 0 }, { 1024, 0, -368, 0 }, { -1024, 0, 368, 0 }, { 1024, 0, 368, 0 } }, { 0, 4099, 0, 0 }, { 201, 0, 4091, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1584, 896, 1088, 0 }, { { -304, 0, -1136, 0 }, { 304, 0, -1136, 0 }, { -304, 0, 1136, 0 }, { 304, 0, 1136, 0 } }, { 0, 4112, 0, 0 }, { -4091, 0, 201, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_mine_forked_tunnel_801852E0[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_forked_tunnel_801852EC[2] = {
    { 16, 16, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor01600_D127BC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_forked_tunnel_80185304[3] = {
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor02500_D05B88 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_forked_tunnel_80185328[3] = {
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor02500_D05B88 },
    { 15, 15, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_mine_forked_tunnel_8018534C[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_mine_forked_tunnel_80185364[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_forked_tunnel_80185374[5] = {
    { 16, 0, 0, 1900, 225, 7900, 1900, 0, 2, 4, 0 },
    { 16, 0, 0, 2640, 500, 6140, 2250, 0, 2, 4, 0 },
    { 16, 0, 0, 1380, 1000, 630, 1580, 0, 2, 4, 0 },
    { 16, 0, 0, 860, 1000, 800, 2650, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_forked_tunnel_801853C4[11] = {
    { 25, 0, 0, 2000, 550, 5650, 0, 0, 2, 4, 2 },
    { 25, 0, 0, 2200, 1000, 2300, 0, 0, 2, 4, 2 },
    { 8, 0, 0, 2300, -1300, 9300, 2048, 0, 4, 6, 3 },
    { 8, 0, 0, 2500, -1400, 9100, 3072, 0, 4, 6, 3 },
    { 8, 0, 0, 2000, -1300, 9400, 1024, 0, 4, 6, 3 },
    { 8, 0, 0, 2000, -1000, 3950, 2750, 0, 4, 6, 3 },
    { 8, 0, 0, 2100, -1000, 3700, 3072, 0, 4, 6, 3 },
    { 8, 0, 0, 2000, -1200, 3600, 3500, 0, 4, 6, 3 },
    { 8, 0, 0, 700, -800, 2250, 3500, 0, 4, 6, 3 },
    { 8, 0, 0, 600, -900, 2400, 3800, 0, 4, 6, 3 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_forked_tunnel_80185474[6] = {
    { 25, 0, 0, 2200, 900, 3500, 0, 0, 2, 4, 0 },
    { 25, 0, 0, 2200, 1000, 2300, 0, 0, 2, 4, 0 },
    { 15, 0, 2, 2500, -500, 3000, 1024, 0, 4, 6, 0 },
    { 15, 0, 2, 1500, -500, 3000, 3072, 0, 4, 6, 0 },
    { 15, 0, 2, 1500, -500, 6000, 3072, 0, 4, 6, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_mine_forked_tunnel_801854D4[3] = {
    { 37, 0, 2, 330, -500, 1700, 3072, 0, 2, 4, 0 },
    { 37, 0, 2, 1100, -500, 380, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_mine_forked_tunnel_80185504[12] = {
    { NULL, NULL },
    { D_mine_forked_tunnel_80185364, D_mine_forked_tunnel_801852E0 },
    { D_mine_forked_tunnel_80185374, D_mine_forked_tunnel_801852EC },
    { D_mine_forked_tunnel_801853C4, D_mine_forked_tunnel_80185304 },
    { D_mine_forked_tunnel_80185474, D_mine_forked_tunnel_80185328 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_mine_forked_tunnel_801854D4, D_mine_forked_tunnel_8018534C },
};

WorldCoordRoomAmbientEntry D_mine_forked_tunnel_80185564[8] = {
    { .viewCount = ARRAY_SIZE(D_mine_forked_tunnel_80185564) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 480, 480, 360, 465 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 430, 440, 350, 425 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_mine_forked_tunnel_801855A4 = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_mine_forked_tunnel_801855B0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_mine_forked_tunnel_801855B8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_mine_forked_tunnel_801855A4 },
};

WorldCollisionSurfaceProperties* D_mine_forked_tunnel_801855C0[8] = {
    D_mine_forked_tunnel_801855B0,
    D_mine_forked_tunnel_801855B8,
    D_mine_forked_tunnel_801855B0,
    D_mine_forked_tunnel_801855B0,
    D_mine_forked_tunnel_801855B0,
    D_mine_forked_tunnel_801855B0,
    D_mine_forked_tunnel_801855B0,
    D_mine_forked_tunnel_801855B0,
};

static void _glowDrawFlare(const SVECTOR* worldPoint, s32 textureIndex, s32 radiusScale);

/// Initializes the switch-controlled area object and its optional tilting child.
///
/// Requires a live TMD body and the area object's enemy allocation in
/// `spawnArg2`. Owns one zeroed work block; teardown releases it and the child.
/// Uses the path start while the switch flag is clear, otherwise the parked
/// placement. Allocation failure starts teardown; child-spawn failure is tolerated.
static void _mineForkedTunnelInitAreaObject(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_AREA_OBJECT_BUFFER_RELEASE_IDLE = -1,
        MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_DESCRIPTOR    = 1,
    };
    _MineForkedTunnelAreaObjectWork* work;
    ActorTransform                   placement;

    work = memCalloc(sizeof(_MineForkedTunnelAreaObjectWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }

    task->work          = work;
    work->freeCountdown = MINE_FORKED_TUNNEL_AREA_OBJECT_BUFFER_RELEASE_IDLE;

    if (gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED) == 0) {
        placement.pos.vx = D_mine_forked_tunnel_80181244[0].vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244[0].vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244[0].vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4[0].vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4[0].vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4[0].vz;
        actorMsgPlaceEulerZyx(task, ACTOR_MESSAGE_PLACE, &placement, 0);
    } else {
        actorMsgPlaceEulerZyx(task, ACTOR_MESSAGE_PLACE, &D_mine_forked_tunnel_80181BBC, 0);
    }

    _mineForkedTunnelSetAreaObjectDrawMode(task, ACTOR_MESSAGE_SET_MODEL_DRAW, MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_AUTO, 0);
    _mineForkedTunnelBindAreaObjectLighting(task);
    // The child borrows the parent's coordinate and work-owned lighting matrices.
    work->child    = taskSpawnFromTable(D_mine_forked_tunnel_80181B74, MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_DESCRIPTOR, 0, task);
    task->msgTable = D_mine_forked_tunnel_80181B8C;
    _mineForkedTunnelUpdateAreaObjectCollision(gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED));
    task->exitCallback = _mineForkedTunnelExitAreaObject;
    task->state++;
}

/// Advances the area object's path, shadow, lighting and deferred buffer release.
///
/// Requires initialized work and a live TMD body. `spawnArg1.value` gates motion;
/// `killCountdown` is a nonnegative path index, advancing through entries 0..109
/// and holding at 110. The full authored position and rotation tables are longer.
/// Hidden drawing still permits motion and buffer release. An armed release
/// frees on zero and decrements to -1, leaving subsequent updates idle.
static void _mineForkedTunnelTickAreaObject(Task* task)
{
    enum {
        MINE_FORKED_TUNNEL_AREA_OBJECT_PATH_STEPS       = 110,
        MINE_FORKED_TUNNEL_AREA_OBJECT_SHADOW_HALF_SIZE = 512,
        MINE_FORKED_TUNNEL_AREA_OBJECT_LIGHT_COUNT      = 3,
    };
    TmdObject*     model;
    ActorTransform placement;
    VECTOR3        groundPoint;

    model = task->extra.tmd;

    // The signed task counter is a path cursor until task teardown takes it over.
    if (task->spawnArg1.value == MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING && task->killCountdown < MINE_FORKED_TUNNEL_AREA_OBJECT_PATH_STEPS) {
        placement.pos.vx = D_mine_forked_tunnel_80181244[task->killCountdown].vx;
        placement.pos.vy = D_mine_forked_tunnel_80181244[task->killCountdown].vy;
        placement.pos.vz = D_mine_forked_tunnel_80181244[task->killCountdown].vz;
        placement.rot.vx = D_mine_forked_tunnel_80180AC4[task->killCountdown].vx;
        placement.rot.vy = D_mine_forked_tunnel_80180AC4[task->killCountdown].vy;
        placement.rot.vz = D_mine_forked_tunnel_80180AC4[task->killCountdown].vz;

        actorMsgPlaceEulerZyx(task, ACTOR_MESSAGE_PLACE, &placement, 0);
        task->killCountdown++;
    }

    // The shadow samples the cached world transform before lighting composes it.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords->workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, MINE_FORKED_TUNNEL_AREA_OBJECT_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        actorRenderComposeCoord(task->extra.tmd->coords);
        worldCoordSetModelLighting(model, task->extra.tmd->coords->workm.t, 0, MINE_FORKED_TUNNEL_AREA_OBJECT_LIGHT_COUNT);
    }

    if (((_MineForkedTunnelAreaObjectWork*)task->work)->freeCountdown >= 0) {
        if (((_MineForkedTunnelAreaObjectWork*)task->work)->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        ((_MineForkedTunnelAreaObjectWork*)task->work)->freeCountdown--;
    }
}

/// Copies a placement, rebuilds its ZYX rotation and invalidates composition.
///
/// Both pointers must refer to live, disjoint storage for the call. Coordinates
/// use the destination's parent frame and angles use 4096 units per turn.
/// The placement is borrowed and unchanged.
static inline void _mineForkedTunnelApplyAreaObjectPlacement(GfxCoord* coord, const ActorTransform* placement)
{
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->param.rot.vx = placement->rot.vx;
    coord->param.rot.vy = placement->rot.vy;
    coord->param.rot.vz = placement->rot.vz;
    RotMatrixZYX(&coord->param.rot, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies a script command to the switch-controlled area object.
///
/// Borrows `command` during dispatch and requires an initialized area-object task.
/// Command 0 stops and rewinds both motion cursors and places the parent at the
/// path start; 1 starts the child's tilt if present; 2 starts the parent's path;
/// 3 parks and stops the parent and refreshes collision from the current switch
/// flag, leaving both cursors and the child unchanged. The command's location,
/// message ID and final payload are ignored. All commands, including unknown
/// values with no effect, return zero.
static s32 _mineForkedTunnelApplyAreaObjectCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unused)
{
    ActorTransform                   placement;
    const ActorTransform*            parkedPlacement;
    GfxCoord*                        coord;
    _MineForkedTunnelAreaObjectWork* work;

    switch (command->command) {
        case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_RESET:
            work                  = task->work;
            task->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE;
            task->killCountdown   = 0;
            if (work->child != NULL) {
                work->child->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE;
                work->child->killCountdown   = 0;
            }
            placement.pos.vx = D_mine_forked_tunnel_80181244[0].vx;
            placement.pos.vy = D_mine_forked_tunnel_80181244[0].vy;
            placement.pos.vz = D_mine_forked_tunnel_80181244[0].vz;
            placement.rot.vx = D_mine_forked_tunnel_80180AC4[0].vx;
            placement.rot.vy = D_mine_forked_tunnel_80180AC4[0].vy;
            placement.rot.vz = D_mine_forked_tunnel_80180AC4[0].vz;

            coord = task->extra.tmd->coords;
            _mineForkedTunnelApplyAreaObjectPlacement(coord, &placement);
            break;
        case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_TILT_CHILD:
            work = task->work;
            if (work->child != NULL) {
                work->child->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING;
            }
            break;
        case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_START_PATH:
            task->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING;
            break;
            // Retained block shape preserves the target's placement loads.
            do {
                case MINE_FORKED_TUNNEL_AREA_OBJECT_COMMAND_PARK:
                    parkedPlacement = &D_mine_forked_tunnel_80181BBC;
                    coord           = task->extra.tmd->coords;
                    _mineForkedTunnelApplyAreaObjectPlacement(coord, parkedPlacement);

                    _mineForkedTunnelUpdateAreaObjectCollision(gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED));
                    task->spawnArg1.value = MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_IDLE;
                    break;
            } while (0);
    }
    return 0;
}

/// Advances the child's tilt while keeping its translation fixed in the parent frame.
///
/// Requires an attached TMD child and a nonnegative `killCountdown` cursor.
/// Motion runs only for `spawnArg1.value` 1, applies the 54 authored rotations
/// in order (4096 angle units per turn), and holds once the cursor reaches 54.
static void _mineForkedTunnelTiltAreaObjectChild(Task* task)
{
    ActorTransform placement;
    GfxCoord*      coord;

    if (task->spawnArg1.value == MINE_FORKED_TUNNEL_AREA_OBJECT_MOTION_RUNNING && task->killCountdown < ARRAY_SIZE(D_mine_forked_tunnel_801819C4)) {
        placement.pos.vx = D_mine_forked_tunnel_80181BA4.pos.vx;
        placement.pos.vy = D_mine_forked_tunnel_80181BA4.pos.vy;
        placement.pos.vz = D_mine_forked_tunnel_80181BA4.pos.vz;
        placement.rot.vx = D_mine_forked_tunnel_801819C4[task->killCountdown].vx;
        placement.rot.vy = D_mine_forked_tunnel_801819C4[task->killCountdown].vy;
        placement.rot.vz = D_mine_forked_tunnel_801819C4[task->killCountdown].vz;

        coord = task->extra.tmd->coords;
        _mineForkedTunnelApplyAreaObjectPlacement(coord, &placement);

        task->killCountdown++;
    }
}

void mineForkedTunnelAreaObjectTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = D_mine_forked_tunnel_8017D5C4;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Releases the area object's enemy record and tears down its task and child model.
///
/// Requires a live enemy allocation in `task->spawnArg2`; task teardown owns
/// the work matrices and model body. The task may be freed before return.
static void _mineForkedTunnelExitAreaObject(Task* task)
{
    enemyTaskExit(task);
}

/// Binds the area object's model to its work-owned light and colour matrices.
///
/// Requires a TMD body and initialized area-object work. The model and its
/// child borrow the matrices until task teardown; this does not fill them.
static void _mineForkedTunnelBindAreaObjectLighting(Task* task)
{
    TmdObject*                       model;
    _MineForkedTunnelAreaObjectWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
}

#include "../../shared/actor_messages_place_euler_zyx.inc.c"

/// Sets the area object's active drawing and primitive-buffer policy.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` with a signed mode word: 0 hides
/// with automatic buffer recovery, 1 shows and allocates missing buffers,
/// 2 hides and schedules buffer release, and 3 shows with automatic recovery
/// disabled. Mode 2 requires area-object work and releases on the third
/// subsequent object update, after two countdown decrements. Other modes do
/// not cancel an armed release. Returns 0 for modes 0..3, or 1 without changes
/// for any other mode. The message ID and final payload are ignored.
static s32 _mineForkedTunnelSetAreaObjectDrawMode(Task* task, s32 messageId, s32 drawMode, s32 unused)
{
    TmdObject* model;
    s32        result;

    model  = task->extra.tmd;
    result = 0;
    switch (drawMode) {
        case MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_AUTO:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_AUTO:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case MINE_FORKED_TUNNEL_MODEL_DRAW_HIDE_RELEASE: {
            _MineForkedTunnelAreaObjectWork* work;

            // Keep hidden buffers alive for two decrements before releasing them.
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work                = task->work;
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        }
        case MINE_FORKED_TUNNEL_MODEL_DRAW_SHOW_MANUAL:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Runs the area-object child's attach, tilt or teardown state while actors run.
///
/// Requires `state` in 0..2 and a live TMD body before teardown. State 0 borrows
/// the parent task passed in `spawnArg2.pointer`; later updates use its attached
/// coordinate and lighting. The owning room overlay must remain loaded.
static void _mineForkedTunnelAreaObjectChildTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = D_mine_forked_tunnel_8017D5D0;

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Attaches the tilting child to its area object and sets its local placement.
///
/// `spawnArg2.pointer` must supply a live parent task; both tasks need TMD bodies.
/// The child borrows the parent's coordinate and work-owned lighting matrices
/// until parent teardown. It becomes visible with an ordering-table offset of
/// -1, adopts the fixed parent-relative placement, and advances to the tilt state.
static void _mineForkedTunnelAttachAreaObjectChild(Task* task)
{
    enum { MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_OT_OFFSET = -1 };

    Task*      parent;
    TmdObject* model;
    TmdObject* parentModel;
    GfxCoord*  parentCoord;
    GfxCoord*  coord;
    GfxCoord*  placementCoord;

    parent      = task->spawnArg2.pointer;
    model       = task->extra.tmd;
    parentModel = parent->extra.tmd;
    coord       = model->coords;
    parentCoord = parentModel->coords;

    // Share the model frame and lighting before joining the parent's teardown tree.
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = parentCoord;
    model->lightMtx     = parentModel->lightMtx;
    model->colorMtx     = parentModel->colorMtx;
    model->otOffset     = MINE_FORKED_TUNNEL_AREA_OBJECT_CHILD_OT_OFFSET;
    taskReparent(parent, task);
    model->flags = model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;

    placementCoord = task->extra.tmd->coords;
    _mineForkedTunnelApplyAreaObjectPlacement(placementCoord, &D_mine_forked_tunnel_80181BA4);
    task->state = task->state + 1;
}

/// Rebuilds the area object's collision box in the reserved prefix of the room grid.
///
/// Copies three normals and faces and eight vertices from the box template;
/// the rest of the room grid and the vectors' fourth words remain intact.
/// Offsets vertices by (0, 0, -200) world units while `switchUsed` is zero,
/// or (0, -3000, -200) for any nonzero value, moving the box above the passage.
/// Both grid pools must be live, writable on the room side, and disjoint.
static void _mineForkedTunnelUpdateAreaObjectCollision(s32 switchUsed)
{
    enum {
        MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_Z_OFFSET = -200,
        MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CLEAR_Y  = -3000,
    };
    WorldCollisionGrid*       roomGrid;
    const WorldCollisionGrid* templateGrid;
    SVECTOR                   offset;
    s32                       entryIndex;

    roomGrid     = &D_mine_forked_tunnel_80183D70;
    templateGrid = &D_mine_forked_tunnel_80181C5C;

    // The room's cell lists already reserve these entries for the movable box.
    MINE_FORKED_TUNNEL_COPY_AREA_OBJECT_COLLISION_BOX(roomGrid, templateGrid, entryIndex);

    if (switchUsed == 0) {
        offset.vx = 0;
        offset.vy = 0;
        offset.vz = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_Z_OFFSET;
    } else {
        offset.vy = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_CLEAR_Y;
        offset.vx = 0;
        offset.vz = MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_Z_OFFSET;
    }

    for (entryIndex = 0; entryIndex < MINE_FORKED_TUNNEL_AREA_OBJECT_COLLISION_VERTEX_COUNT; entryIndex++) {
        roomGrid->vertices[entryIndex].vx += offset.vx;
        roomGrid->vertices[entryIndex].vy += offset.vy;
        roomGrid->vertices[entryIndex].vz += offset.vz;
    }
}

/// Rejects every key-item-use request with zero, leaving the item and room unchanged.
///
/// `itemId` is the inventory's collected item ID; all four arguments are ignored.
static s32 _mineForkedTunnelRejectKeyItemMessage(Task* unusedTask, s32 messageId, s32 itemId, s32 unused)
{
    enum { MINE_FORKED_TUNNEL_KEY_ITEM_REFUSED = 0 };

    return MINE_FORKED_TUNNEL_KEY_ITEM_REFUSED;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_map_shelter_80179A04`, returning 1.
s32 func_mine_forked_tunnel_8017E0F0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_shelter_80179A04(in, out);
    return 1;
}

s32 func_mine_forked_tunnel_8017E134(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if ((arg2 == 2) && (Gp_GetCurBit2Flag(1) == 1)) {
        if (gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_152) == 0) {
            Gp_RunCapCmd1(5);
        } else {
            taskSpawnFromTable(D_mine_forked_tunnel_80183104, 1, 0, 0);
        }
    }
    return 0;
}

/// Starts the switch interaction when requested and the switch has not been used.
///
/// Borrows a live `DirectionActionRequest` during synchronous room dispatch.
/// Action ID 1 spawns the switch-interaction task while the switch flag is zero;
/// other IDs and an already-used switch have no effect. Ignores control,
/// argument, receiving task, message ID and final payload. Returns zero even
/// when spawning fails; the room task does not retain the request.
static s32 _mineForkedTunnelHandleSwitchAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unused)
{
    enum {
        MINE_FORKED_TUNNEL_ACTION_USE_SWITCH             = 1,
        MINE_FORKED_TUNNEL_SWITCH_INTERACTION_DESCRIPTOR = 0,
    };

    if ((actionRequest->actionId == MINE_FORKED_TUNNEL_ACTION_USE_SWITCH) && (gameFlagGetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED) == 0)) {
        taskSpawnFromTable(D_mine_forked_tunnel_80183104, MINE_FORKED_TUNNEL_SWITCH_INTERACTION_DESCRIPTOR, 0, 0);
    }
    return 0;
}

/// State 0 of the room's message-driven task family: park the room's
/// `TaskMessageEntry` table in `Task::msgTable`, publish the task in pointer slot 7,
/// arm the message flag, then hand off to `_mineForkedTunnelSetSpriteBatchesHidden`.
static void func_mine_forked_tunnel_8017E1E8(Task* arg0)
{
    arg0->msgTable = D_mine_forked_tunnel_80181C80;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    gStageSceneMusicEntry = 1;
    _mineForkedTunnelSetSpriteBatchesHidden(Gp_GetCurBit2Flag(1) == 2);
    arg0->state = (s32)(arg0->state + 1);
}

/// Keeps the initialized room task available to receive messages without per-frame work.
static void _mineForkedTunnelIdleRoomTask(Task* unusedTask)
{
    // Preserve the idle callback's otherwise unused 16-byte stack frame.
    char unusedStackFrame[0x10];
}

void mineForkedTunnelRoomTask(Task* task)
{
    const TaskFuncTable3 stateHandlers = D_mine_forked_tunnel_8017D5DC;

    stateHandlers.funcs[task->state](task);
}

/// Requests centred, unattenuated playback for the area object's movement sound.
///
/// The event script has already started the sound with attenuation 32. This
/// queued mix request changes an existing matching slot; it starts no sound.
static void _mineForkedTunnelRestoreAreaObjectSoundMix(void)
{
    sndEvtRequestScriptMix(SOUND_MINE_FORKED_TUNNEL_OBJECT_MOVE, 0, 0);
}

void func_mine_forked_tunnel_8017E2E0(Task* arg0)
{
    s32 state;

    state = arg0->state;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(1);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (capIsBusy() == 0) {
                if (capGetVariantKey() == state) {
                    func_800E8634(D_mine_forked_tunnel_801831AC, 0, D_mine_forked_tunnel_801834F4);
                    gameFlagSetNibble(GAME_FLAG_MINE_FORKED_TUNNEL_SWITCH_USED, 1);
                }
                taskKill(arg0);
            }
            break;
    }
}

void func_mine_forked_tunnel_8017E38C(Task* arg0)
{
    s32 state;
    s16 temp;

    state = arg0->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StartCapSlot(2, 0, 0);
            arg0->state = arg0->state + 1;
            break;
        case 1:
            if (capIsBusy() == 0) {
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            temp                = arg0->killCountdown + 1;
            arg0->killCountdown = temp;
            if (temp >= 0xB) {
                if (capGetVariantKey() == state) {
                    Gp_StartCapSlot(2, 0, 1);
                    _mineForkedTunnelSetSpriteBatchesHidden(true);
                }
                Gp_MsgPlayerWeapon(1);
                taskKill(arg0);
            }
            break;
    }
}

/// Hides or shows the selected sprite batches in mapped views 4 and 5.
///
/// Requires this room's stage/area sprite directory to be loaded. A zero byte
/// shows both ranges; any other byte hides them, normalized to 1. View 4's
/// batch 5 covers source sprites 80..81; view 5's batch 3 covers 20..25.
static void _mineForkedTunnelSetSpriteBatchesHidden(u8 hidden)
{
    const GameLocationKey* location;
    const SpriteView*      areaViews;
    SpriteBatch*           view4Batches;
    SpriteBatch*           view5Batches;

    location  = &gGameSession->location.loc;
    areaViews = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1];

    if (!hidden) {
        view4Batches           = areaViews[3].batches;
        view4Batches[5].hidden = false;
        view5Batches           = areaViews[4].batches;
        view5Batches[3].hidden = false;
        return;
    }
    view4Batches           = areaViews[3].batches;
    view4Batches[5].hidden = true;
    view5Batches           = areaViews[4].batches;
    view5Batches[3].hidden = true;
}

#include "../../shared/glow_draw_flare.inc.c"

void mineForkedTunnelDrawViewFlaresTask(Task* unusedTask)
{
    enum {
        MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX = 1,
        MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE  = 0x300,
    };

    s32 mappedViewIndex;

    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    mappedViewIndex                  = viewGetMappedIndex() & 0xFF;

    switch (mappedViewIndex) {
        case 2:
        case 3:
            _glowDrawFlare(D_mine_forked_tunnel_80183614, MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            break;
        case 4:
            _glowDrawFlare(&D_mine_forked_tunnel_8018361C[0], MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            _glowDrawFlare(&D_mine_forked_tunnel_8018361C[1], MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            break;
        case 5:
            _glowDrawFlare(D_mine_forked_tunnel_8018362C, MINE_FORKED_TUNNEL_FLARE_TEXTURE_INDEX, MINE_FORKED_TUNNEL_FLARE_RADIUS_SCALE);
            break;
        default:
            return;
    }
}
