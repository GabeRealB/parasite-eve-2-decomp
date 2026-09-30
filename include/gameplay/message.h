#ifndef GAMEPLAY_MESSAGE_H
#define GAMEPLAY_MESSAGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"

/// Requests and resolves a room transition within the active stage.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` passes a borrowed request and a writable reply;
/// they may be the same object. Handlers copy the complete record before
/// resolving the destination, and may retain a copy for a deferred transition.
/// Queries test whether a transition can proceed without committing its effects;
/// handlers may leave the destination selectors unchanged in query mode.
///
/// Area, room and arrival IDs are 1-based and must exist in the active stage's
/// tables. Warp senders select arrival IDs from a four-bit value. Only fields
/// read by the selected handler need initialization. `flagId` is zero or a
/// valid game-flag nibble index. The record is eight bytes with two-byte alignment.
typedef struct {
    u16 areaId;    // Area within the active stage; committed to the saved area's byte
    u8  warp;      // Arrival record within the area's warp table
    s8  room;      // Room within the area (1 default); handlers resolve it from game progress
    u8  field_4;   // Warp senders initialize this to 1; its request role is unproven
    u8  queryOnly; // Execution choice (0 execute effects, nonzero query)
    u16 flagId;    // Optional game-flag nibble to update when handling the event (0 none)
} RoomEventMsg;
STATIC_ASSERT_SIZEOF(RoomEventMsg, 0x8);

/// Room-transition message and execution choices; the stored choice remains a byte.
enum {
    ROOM_EVENT_MESSAGE_RESOLVE = 0x13EE,
    ROOM_EVENT_EXECUTE         = 0,
    ROOM_EVENT_QUERY_ONLY      = 1,
};

/// Angular scale and wrapping used by actor placement and facing records.
enum {
    ACTOR_TRANSFORM_ANGLE_TURN      = 4096,
    ACTOR_TRANSFORM_ANGLE_HALF_TURN = 2048,
    ACTOR_TRANSFORM_ANGLE_MASK      = ACTOR_TRANSFORM_ANGLE_TURN - 1,
};

/// An actor position and orientation used by placement tables and task messages.
///
/// Coordinates use whole world-coordinate units in the frame selected by the
/// receiver or table; part placements may be relative to another model. Euler
/// angles use 4096 units per turn and need not be normalized. The SDK vectors'
/// fourth components have no placement meaning.
///
/// Placement handlers use both vectors; turning and approach handlers may read
/// only yaw or position. Initialize every component the selected handler reads.
/// Message handlers consume the values during dispatch, copying any destination
/// needed by later frames. Keep a borrowed message live through dispatch and a
/// borrowed spawn placement live until the task has initialized from it.
/// The record occupies 24 bytes with four-byte alignment.
typedef struct {
    VECTOR  pos; // Signed X/Y/Z position in the receiver's coordinate frame
    SVECTOR rot; // Signed X/Y/Z Euler angles; 4096 units per turn
} ActorTransform;
STATIC_ASSERT_SIZEOF(ActorTransform, 0x18);

struct AnimationPlayRequest;
struct ActorCommand;
struct AnimationSet;

struct DirectionActionRequest;

/// One payload word in the PS1 message ABI. A message id determines whether
/// the recipient interprets the word as an integer or as an object address.
typedef union GpMessageArg {
    s32                            value;
    const void*                    pointer;
    void*                          storage;
    u8*                            bytes;
    VECTOR*                        vector;
    ActorTransform*                transform;
    struct AnimationPlayRequest*   animation;
    struct ActorCommand*           command;
    RoomEventMsg*                  location;
    struct DirectionActionRequest* direction;
    RoomEventMsg*                  roomEvent;
} GpMessageArg __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(GpMessageArg, 4);

/// 8-byte id/handler record. `Task::msgTable` points at a table of these
/// (`Gp_Slot4MsgTable`, `D_8010FB90`, …). `Gp_DispatchMsg` walks it and calls the
/// matching handler with the same four arguments. Terminator id is
/// `0x7FFFFFFF`.
typedef s32 (*GpMsgHandler)(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3);

typedef struct _GpMsgEntry {
    /* 0x0 */ s32          id;
    /* 0x4 */ GpMsgHandler handler;
} GpMsgEntry;
STATIC_ASSERT_SIZEOF(GpMsgEntry, 8);

/// 0x10-byte spawn argument for `Gp_SpawnAlly` / `Gp_SpawnPlayer`. `field_0`
/// is copied to `GameActor.field_52`; `field_4` / `field_8` / `field_C` are
/// copied to the extra coordinate translation.
typedef struct _GpActorArg {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ byte pad_2[2];
    /* 0x4 */ s32  field_4;
    /* 0x8 */ s32  field_8;
    /* 0xC */ s32  field_C;
} GpActorArg;
STATIC_ASSERT_SIZEOF(GpActorArg, 0x10);

/// Playback choices stored as signed words in an animation request.
enum {
    ANIMATION_BLEND_RESET             = 0,
    ANIMATION_BLEND_INTERPOLATE       = 1,
    ANIMATION_WORLD_COLLISION_DISABLE = 0,
    ANIMATION_WORLD_COLLISION_ENABLE  = 1,
};

/// Animation message ids for playback, borrowed tables and writable bank extensions.
enum {
    ANIMATION_MESSAGE_PLAY                = 0x3E8,
    ANIMATION_MESSAGE_INSTALL_AND_PLAY    = 0x3F4,
    ANIMATION_MESSAGE_COPY_BANK_EXTENSION = 0x3F7,
    ANIMATION_MESSAGE_REPLACE_AND_PLAY    = 0x3FF,
};

/// Requests animation playback on a player, companion or scripted actor.
///
/// The message id selects the interpretation of `source`: indexed playback
/// uses a receiver-specific bank selector; the player's install and replace
/// messages use an animation-set table. Bank and animation ids must be valid
/// for that receiver, which may translate them to local clip ids. Some actors
/// use a fixed blend duration or reset a newly selected bank despite `blend`.
///
/// Dispatch consumes the request synchronously. The table and clip data are
/// borrowed and must remain live while the receiver's playback references them.
/// Event scripts resolve indexed player/companion requests against the equipped
/// weapon or companion before dispatch.
typedef struct AnimationPlayRequest {
    union {
        s32                   index; // Receiver-specific animation bank selector
        struct AnimationSet** sets;  // Borrowed animation-set table for install/replace messages
    } source;
    s32 animationId;                 // Receiver-specific animation id within the selected bank
    s32 blend;                       // Transition choice (0 reset, nonzero interpolate when supported)
    s32 blendFrames;                 // Requested transition duration in frames; ignored on reset
    s32 enableWorldCollision;        // Player/companion world collision and ground following (0 disable, nonzero enable)
} AnimationPlayRequest;
STATIC_ASSERT_SIZEOF(AnimationPlayRequest, 0x14);

/// The payload of the message that copies animation parameters onto a
/// task's current animation block: `count` words, at most 0x20. The source
/// is usually an array of animation-set pointers; the receiver copies words.
typedef struct GpCopyArg {
    union {
        s32*                  words;
        struct AnimationSet** sets;
    } source;
    s32 count;
} GpCopyArg;
STATIC_ASSERT_SIZEOF(GpCopyArg, 8);

/// The payload of the message that holds the player or the companion in a
/// timed state. The receivers read only `field_14`, a frame count they store as
/// the state's countdown; senders also fill `field_4`.
typedef struct GpDelayArg {
    byte pad_0[4];
    s32  field_4;
    byte pad_8[0xC];
    s32  field_14;
} GpDelayArg;
STATIC_ASSERT_SIZEOF(GpDelayArg, 0x18);

/// Actor-command delivery and the scene manager's general actor-message broadcast.
enum {
    ACTOR_COMMAND_MESSAGE_APPLY       = 0x7DB,
    SCENE_MESSAGE_BROADCAST_TO_ACTORS = 0x7DA,
};

/// A borrowed command interpreted in an actor's stage/area command namespace.
///
/// `ACTOR_COMMAND_MESSAGE_APPLY` delivers the record directly. The scene
/// manager's `SCENE_MESSAGE_BROADCAST_TO_ACTORS` forwards it synchronously to
/// its type-9 children when the second payload is `ACTOR_COMMAND_MESSAGE_APPLY`.
/// Keep the complete record live through dispatch; event scripts borrow their
/// command records until the corresponding instruction executes.
///
/// Context tags usually come from the active location, but synthetic namespaces
/// also occur, including stage 0/area 44 and stage 9/area 1. Handlers may ignore
/// the context. Commands select receiver-specific actions or states, and some
/// receivers split the word into an opcode and parameters or a table index.
/// Initialize every component the selected handler reads and use values valid
/// for that handler. The record occupies four bytes with two-byte alignment.
typedef struct ActorCommand {
    union {
        struct {
            u8 stage; // Stage tag of the command namespace; may be synthetic
            u8 area;  // Area tag within that command namespace
        } loc;
        u16 key;      // Packed context: stage in bits 0-7, area in bits 8-15
    } context;        // Command namespace, usually the sender's stage and area
    u16 command;      // Receiver-specific action, state or packed parameters
} ActorCommand;
STATIC_ASSERT_SIZEOF(ActorCommand, 4);

/// The payload of message 0x3FE, which moves the receiver by a displacement:
/// `x`, `y` and `z` are added onto its coordinate. With `field_10` 7 the move
/// also decides whether the receiver faces along it or away from it, from
/// `x` / `z`; the receiver keeps `field_10` in its own state either way.
/// `field_12` zero first resets the receiver's movement state, so a sender
/// moving it over several frames sets it after the first.
typedef struct GpMoveArg {
    s32  x;
    s32  y;
    s32  z;
    byte pad_C[4];
    s16  field_10;
    u8   field_12;
    byte pad_13;
} GpMoveArg;
STATIC_ASSERT_SIZEOF(GpMoveArg, 0x14);

/// The payload of message 0x3EF, which stops the receiver where it is and
/// plays one of two animations: `field_0` non-zero picks the second. The
/// receiver keeps both words in its own state; senders fill them as two words.
typedef struct GpFacingArg {
    s32 field_0;
    s32 field_4;
} GpFacingArg;
STATIC_ASSERT_SIZEOF(GpFacingArg, 8);

/// The optional second payload of message 0x3F2, which sends the receiver to a
/// `ActorTransform` destination: two values the receiver keeps in its own state
/// while it gets there. Without one it clears both.
typedef struct GpOverrideArg {
    s32 field_0;
    s32 field_4;
} GpOverrideArg;
STATIC_ASSERT_SIZEOF(GpOverrideArg, 8);

/// Optional animation for message 0x7DD's placement-and-approach handlers.
/// The first word selects the animation; the byte selects the next animation
/// id kept by the actor. A null payload uses the receiver's own defaults.
typedef struct GpSpawnAnimArg {
    s32 field_0;
    u8  field_4;
} GpSpawnAnimArg;
STATIC_ASSERT_SIZEOF(GpSpawnAnimArg, 8);

s32 Gp_DispatchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Send an object address in arg2; the recipient's message id defines its type.
static __inline__ s32 Gp_DispatchMsgPtr(Task* task, s32 id, const void* data, s32 arg3)
{
    GpMessageArg payload;
    payload.pointer = data;
    return Gp_DispatchMsg(task, id, payload.value, arg3);
}

/// Send an object address in arg3, commonly an output/reply destination.
static __inline__ s32 Gp_DispatchMsgReply(Task* task, s32 id, s32 arg2, const void* reply)
{
    GpMessageArg payload;
    payload.pointer = reply;
    return Gp_DispatchMsg(task, id, arg2, payload.value);
}

/// Send two object addresses through the same word-based message interface.
static __inline__ s32 Gp_DispatchMsgPtrs(Task* task, s32 id, const void* data, const void* reply)
{
    GpMessageArg payload;
    GpMessageArg response;
    payload.pointer  = data;
    response.pointer = reply;
    return Gp_DispatchMsg(task, id, payload.value, response.value);
}

#endif // GAMEPLAY_MESSAGE_H
