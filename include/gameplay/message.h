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

struct ActorCommand;
struct AnimationSet;

/// One integer or object address passed to a task's message handler.
///
/// The message id and receiver determine the interpretation of each of the two
/// argument words, including whether zero denotes an absent optional payload.
/// Pointer arguments borrow storage rather than copying the pointed-to object;
/// keep it live through synchronous dispatch. Any storage retained by a handler
/// must remain live for that handler's use. Writable reply storage must satisfy
/// the selected handler's payload type and complete extent.
///
/// `pointer` transports arbitrary object addresses without changing their bits;
/// its const qualification does not describe the receiver's write permission.
/// The typed views identify command records in event-script operands and room
/// transition requests/replies. This union is one four-byte PS1 argument word,
/// not the payload record itself. Keep `value` first: the transparent union
/// accepts integer and pointer arguments using the integer calling convention.
typedef union {
    s32                  value;     // Integer argument or the complete transported address bits
    const void*          pointer;   // Generic borrowed object address for transport
    struct ActorCommand* command;   // ACTOR_COMMAND_MESSAGE_APPLY: borrowed actor command
    RoomEventMsg*        roomEvent; // ROOM_EVENT_MESSAGE_RESOLVE: borrowed request or writable reply
} TaskMessageArg __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(TaskMessageArg, 4);

/// A synchronous task-message callback receiving an ID and two argument words.
///
/// `task` is the live receiver. `messageId` selects the meaning of `firstArg`
/// and `secondArg`, including each pointer's payload type and write permission.
/// Object arguments borrow storage with the lifetime required by `TaskMessageArg`;
/// the callback must copy any transient data it needs after dispatch returns.
/// The signed result is message-specific and is forwarded unchanged to the
/// sender; zero is not a universal success or failure code.
///
/// Callbacks retain all four PS1 argument-register positions and an `s32` return.
/// The transparent argument union also permits declarations using its member
/// types in either payload position under GCC's function-type compatibility rules.
typedef s32 (*TaskMessageHandler)(Task* task, s32 messageId, TaskMessageArg firstArg, TaskMessageArg secondArg);

typedef struct _GpMsgEntry {
    /* 0x0 */ s32                id;
    /* 0x4 */ TaskMessageHandler handler;
} GpMsgEntry;
STATIC_ASSERT_SIZEOF(GpMsgEntry, 8);

/// 0x10-byte spawn argument for `Gp_SpawnAlly` / `Gp_SpawnPlayer`. `field_0`
/// is copied to `GameActor.rotation.vy`; `field_4` / `field_8` / `field_C` are
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
    /// Requests the selected clip's start without blending from the previous pose.
    ///
    /// Zero in `AnimationPlayRequest.blend`; receivers that honor this choice
    /// ignore `blendFrames` and restart their playback slots. Some receivers
    /// defer the restart or keep an already selected clip. This choice does not
    /// disable interpolation between the clip's own keyframes.
    ANIMATION_BLEND_RESET = 0,
    /// Blends from the pose at the request into the selected clip.
    ///
    /// The value senders store in `AnimationPlayRequest.blend`. Receivers that
    /// honor this choice treat every nonzero value the same way and pass
    /// `blendFrames` through as the blend duration. Some substitute a fixed
    /// duration, defer the blend, or restart when no pose is already playing.
    /// A zero duration still selects this path. The choice does not change
    /// interpolation between the clip's own keyframes.
    ANIMATION_BLEND_INTERPOLATE = 1,
};

/// Grid participation carried by one animation playback request.
///
/// Stored in `AnimationPlayRequest.enableWorldCollision`. The player and
/// companion playback handlers honor it; other animation receivers leave the
/// word unread. Zero drops grid participation for both bodies that receiver's
/// collision update maintains. The first body's participation bit gates
/// push-back from occupied grid contacts, and the player and companion
/// updates also apply a fixed height adjustment while that bit is set. Any
/// other value restores both bodies and that response. Senders that want
/// participation store `ANIMATION_WORLD_COLLISION_ENABLE`.
enum {
    /// Drop grid participation and the contact response gated with it.
    ANIMATION_WORLD_COLLISION_DISABLE = 0,
    /// Restore grid participation and the contact response gated with the first body.
    ///
    /// Handlers treat every nonzero value this way. Senders store this constant.
    ANIMATION_WORLD_COLLISION_ENABLE = 1,
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
    s32 enableWorldCollision;        // Player/companion grid participation (0 drop, any other value restore)
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

/// Scene-child lookup messages with a borrowed, writable `Task*` reply.
///
/// The first payload is a packed placement key (index in bits 12..15, stage in
/// bits 8..11, area in bits 0..7) for a type-9 actor, or a byte ID for a child
/// outside type 9. The second payload addresses one complete `Task*` that is
/// set synchronously to the first matching child, or NULL when none matches.
/// The scene manager must be live; the returned child is borrowed.
enum {
    SCENE_MESSAGE_FIND_PLACED_ACTOR = 0x7D0,
    SCENE_MESSAGE_FIND_OTHER_CHILD  = 0x7D8,
};

/// Actor-command delivery and the scene manager's general actor-message broadcast.
enum {
    /// Applies a borrowed `ActorCommand` in the receiver's command namespace.
    ///
    /// The first argument addresses the command; initialize the context and
    /// command components that the selected handler reads and keep them live
    /// through synchronous dispatch. The second argument is receiver-specific:
    /// usually zero, but placement commands can require an `ActorTransform*`.
    /// Actions, valid command values and the signed result are receiver-specific;
    /// zero does not distinguish an ignored command from one that was applied.
    ///
    /// To broadcast, send `SCENE_MESSAGE_BROADCAST_TO_ACTORS` to the scene manager
    /// with the command as its first argument and this ID as its second. It
    /// forwards the command to type-9 children with a zero second argument and
    /// returns zero, discarding their results.
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
    TaskMessageArg payload;
    payload.pointer = data;
    return Gp_DispatchMsg(task, id, payload.value, arg3);
}

/// Send an object address in arg3, commonly an output/reply destination.
static __inline__ s32 Gp_DispatchMsgReply(Task* task, s32 id, s32 arg2, const void* reply)
{
    TaskMessageArg payload;
    payload.pointer = reply;
    return Gp_DispatchMsg(task, id, arg2, payload.value);
}

/// Send two object addresses through the same word-based message interface.
static __inline__ s32 Gp_DispatchMsgPtrs(Task* task, s32 id, const void* data, const void* reply)
{
    TaskMessageArg payload;
    TaskMessageArg response;
    payload.pointer  = data;
    response.pointer = reply;
    return Gp_DispatchMsg(task, id, payload.value, response.value);
}

#endif // GAMEPLAY_MESSAGE_H
