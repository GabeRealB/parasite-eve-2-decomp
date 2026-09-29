#ifndef GAMEPLAY_MESSAGE_H
#define GAMEPLAY_MESSAGE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"

/// 8-byte room destination record the message handlers receive alongside the
/// request. Handlers registered in a room's `(msgId, handler)` dispatch table
/// are passed the incoming record and an outgoing copy of it, and answer by
/// editing `field_3` of the copy. `field_5` non-zero suppresses the side
/// effects (the handler only reports what *would* happen); `field_6` is the
/// nibble index passed to `Gp_SetNibbleIf`. Alignment is 2, which is why the
/// whole-record copies compile to `lwl`/`lwr` pairs.
typedef struct _RoomEventMsg {
    union {
        struct {
            u8 field_0, field_1;
        } bytes;
        u16 packed;
    } prefix; // Destination identifier; the warp code also addresses its bytes.
    /* 0x2 */ u8  field_2;
    /* 0x3 */ s8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ u16 field_6;
} RoomEventMsg;
STATIC_ASSERT_SIZEOF(RoomEventMsg, 0x8);

struct GpXformArg;
struct AnimationPlayRequest;
struct GpCmdArg;
struct GpAnimSet;

struct _GpMsg13EF;

/// One payload word in the PS1 message ABI. A message id determines whether
/// the recipient interprets the word as an integer or as an object address.
typedef union GpMessageArg {
    s32                          value;
    const void*                  pointer;
    void*                        storage;
    u8*                          bytes;
    VECTOR*                      vector;
    struct GpXformArg*           transform;
    struct AnimationPlayRequest* animation;
    struct GpCmdArg*             command;
    RoomEventMsg*                location;
    struct _GpMsg13EF*           direction;
    RoomEventMsg*                roomEvent;
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

/// A position and a set of Euler angles, the payload of the messages that put
/// a task somewhere. The player takes it to be placed or warped, and as the
/// point to walk to; the actors take it to be placed, and as the point to walk
/// to or turn towards, where some read only the position.
typedef struct GpXformArg {
    VECTOR  pos;
    SVECTOR rot;
} GpXformArg;
STATIC_ASSERT_SIZEOF(GpXformArg, 0x18);

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
        s32                index; // Receiver-specific animation bank selector
        struct GpAnimSet** sets;  // Borrowed animation-set table for install/replace messages
    } source;
    s32 animationId;              // Receiver-specific animation id within the selected bank
    s32 blend;                    // Transition choice (0 reset, nonzero interpolate when supported)
    s32 blendFrames;              // Requested transition duration in frames; ignored on reset
    s32 enableWorldCollision;     // Player/companion world collision and ground following (0 disable, nonzero enable)
} AnimationPlayRequest;
STATIC_ASSERT_SIZEOF(AnimationPlayRequest, 0x14);

/// The payload of the message that copies animation parameters onto a
/// task's current animation block: `count` words, at most 0x20. The source
/// is usually an array of animation-set pointers; the receiver copies words.
typedef struct GpCopyArg {
    union {
        s32*               words;
        struct GpAnimSet** sets;
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

/// A command to an actor, the payload of message 0x7DB. A sender hands it to
/// one actor directly, or as message 0x7DA to the actor manager in pointer
/// slot 4, which passes it on to its actors as 0x7DB. `from` says who the
/// command is from, usually the stage and area of the room sending it; a
/// receiver tests the two bytes together as one halfword before it acts on
/// `command`.
typedef struct GpCmdArg {
    union {
        struct {
            u8 stage;
            u8 area;
        } loc;
        u16 key; // `loc` read as one halfword, area in the high byte
    } from;
    u16 command; // What the receiver is to do: a state to enter or a request number
} GpCmdArg;
STATIC_ASSERT_SIZEOF(GpCmdArg, 4);

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
/// `GpXformArg` destination: two values the receiver keeps in its own state
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
