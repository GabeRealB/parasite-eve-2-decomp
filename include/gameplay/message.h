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

/// Messages a room's task receives from the CAP interpreter and from actors.
///
/// The room task registered in `GAME_TASK_SLOT_ROOM` owns the table; each room
/// gives the arguments and result their meaning, and a room may forward the
/// message to a task of its own (answering -1 while that task is absent).
/// Some controlling actors carry these ids in their own tables.
enum {
    /// A room command routed from a CAP command; the first argument selects it.
    ROOM_MESSAGE_COMMAND = 0x13F0,
    /// A room sound command routed from a CAP command or a scene object.
    ROOM_MESSAGE_SOUND = 0x13F2,
    /// An actor reports an event to the room, such as an enemy being pulled in
    /// or despawning. The first argument selects the event.
    ROOM_MESSAGE_ACTOR_EVENT = 0x13F4,
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

/// Reserved message ID marking the end of a task-message table.
///
/// Use it in the final record's signed ID word, paired with a null callback. A lookup
/// for any other ID returns zero when it reaches this record. The end test
/// uses this ID alone: neither -1 nor a null callback terminates the search.
/// Never dispatch this reserved ID; equality is tested before the end marker,
/// so it would select the terminal null callback. A table without this marker
/// may receive only IDs with non-null callbacks in its entries.
enum {
    TASK_MESSAGE_TABLE_END = 0x7FFFFFFF,
};

/// Maps a receiver-specific message ID to a synchronous task-message callback.
///
/// Tables installed in `Task::msgTable` are borrowed and read only during
/// dispatch. Keep the table and its callbacks live while the task can receive
/// messages. Entries are searched in order, and the first matching ID wins.
/// End the table with `{ TASK_MESSAGE_TABLE_END, NULL }` so unsupported IDs
/// return zero. Never send the reserved end ID: equality is tested before the
/// end marker, so it would select the null callback.
///
/// Every other entry requires a non-null `TaskMessageHandler`. The message ID
/// and receiver select the argument interpretations and signed result; the
/// entry itself owns no payload storage. Each record is eight bytes, aligned
/// to four bytes, with one signed ID word followed by one callback address.
typedef struct {
    s32                messageId; // Receiver-specific ID, or TASK_MESSAGE_TABLE_END
    TaskMessageHandler handler;   // Callback for this ID (NULL only at the end marker)
} TaskMessageEntry;
STATIC_ASSERT_SIZEOF(TaskMessageEntry, 8);

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
    ANIMATION_MESSAGE_PLAY = 0x3E8,
    /// Returns 1 while any animation slot after the root has not settled on its
    /// boundary pose, otherwise 0. Takes no payload.
    ANIMATION_MESSAGE_IS_PLAYING          = 0x3ED,
    ANIMATION_MESSAGE_INSTALL_AND_PLAY    = 0x3F4,
    ANIMATION_MESSAGE_COPY_BANK_EXTENSION = 0x3F7,
    /// Sets the playback rate of every animation slot from the first argument,
    /// clamped to 1..0x7F. Returns 0.
    ANIMATION_MESSAGE_SET_RATE         = 0x3FD,
    ANIMATION_MESSAGE_REPLACE_AND_PLAY = 0x3FF,
};

/// Scripted-control messages of the player and companion tasks.
///
/// The player's `Gp_PlayerMsgTable` and the companion actors' tables share this
/// protocol. A companion that does not support an id maps it to its generic
/// animation handler instead, so check the receiver's table before relying on
/// a result. Messages that take scripted control leave the receiver in
/// `GAME_ACTOR_MODE_SCRIPTED` until `GAME_ACTOR_MESSAGE_END_SCRIPTED`.
enum {
    /// Places the receiver at a borrowed `ActorTransform`: its position and all
    /// three angles. Returns 0.
    GAME_ACTOR_MESSAGE_PLACE = 0x3E9,
    /// Takes scripted control and turns the receiver to the yaw of a borrowed
    /// `ActorTransform`, playing the turn animation for that direction. Returns 0.
    GAME_ACTOR_MESSAGE_TURN_TO_YAW = 0x3EE,
    /// Returns nonzero while a scripted turn, walk or timed state is still pending.
    GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING = 0x3F0,
    /// Ends scripted control and restores the equipped weapon's animation bank
    /// and collision. The first argument selects how play resumes. Returns 1,
    /// changing nothing, when the receiver is not under scripted control.
    GAME_ACTOR_MESSAGE_END_SCRIPTED = 0x3F1,
    /// Takes scripted control and walks the receiver to a borrowed
    /// `ActorTransform` position, with an optional `GameActorMoveAnim`. Returns 0.
    GAME_ACTOR_MESSAGE_MOVE_TO = 0x3F2,
    /// Sets the model's draw and buffer state from the mode in the first
    /// argument (0-4), allocating or freeing its buffers where the mode needs.
    GAME_ACTOR_MESSAGE_SET_MODEL_DRAW = 0x3F3,
    /// Reparents the receiver's model to a borrowed `GfxCoord`. Returns 0.
    GAME_ACTOR_MESSAGE_ATTACH_TO_COORD = 0x3F5,
    /// Applies damage to the receiver (`Gp_ApplyPlayerDamage`, `Gp_HurtAlly`).
    GAME_ACTOR_MESSAGE_APPLY_DAMAGE = 0x3F9,
    /// Moves the receiver by a borrowed `GpMoveArg` displacement.
    GAME_ACTOR_MESSAGE_MOVE_BY = 0x3FE,
    /// Restarts the model's texture animation sequences: 0 resets both, 1-3
    /// select the first sequence and higher values the second. Returns 0.
    GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE = 0x401,
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

/// Copies borrowed words into the selected player or companion animation-bank extension.
///
/// `ANIMATION_MESSAGE_COPY_BANK_EXTENSION` overwrites the bank from set index
/// `ANIMATION_BANK_BASE_SET_COUNT`, leaving later entries unchanged. The player
/// selects its character/equipped-weapon bank; a companion selects its saved
/// type/variant bank. The receiver does not start playback or select a new bank.
/// The selected resource bank must be loaded and writable.
/// Counts above `ANIMATION_BANK_EXTENSION_CAPACITY` return 1 without copying;
/// nonpositive counts copy nothing and return 0. Other accepted counts return 0.
///
/// Positive counts require that many readable, word-aligned source words.
/// The source can be a set-pointer table, including null entries, or a word
/// span that also covers adjacent request/script data. The count is not always
/// the number of playable clips. Only valid set pointers may be used for playback.
/// The request and source span are borrowed through synchronous dispatch;
/// copied clip pointers and their data must remain live while playback uses them.
/// The record occupies eight bytes with four-byte alignment.
typedef struct {
    union {
        const s32*                  words; // Read-only word span, possibly including data after the set pointers
        struct AnimationSet* const* sets;  // Read-only set-pointer table; the descriptors remain borrowed
    } source;                              // Borrowed source span in either view
    s32 wordCount;                         // Number of 32-bit words to overwrite (1..32, nonpositive no copy)
} AnimationBankCopyRequest;
STATIC_ASSERT_SIZEOF(AnimationBankCopyRequest, 8);

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

/// Makes the scene manager run the exit routine of every placed (type-9)
/// actor among its children. Takes no payload and returns 0.
enum {
    SCENE_MESSAGE_EXIT_PLACED_ACTORS = 0x7D9,
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

/// Messages of placed actors, the tasks the scene manager spawns from a
/// room's placement records. Each actor's table selects the handler; most use
/// the shared handlers of the actor message and motion libraries.
enum {
    /// Plays an animation from a borrowed `AnimationPlayRequest`, in the
    /// receiver's own banks.
    ACTOR_MESSAGE_PLAY_ANIMATION = 0x7D3,
    /// Places the receiver's model from a borrowed `ActorTransform`. Receivers
    /// differ in which angles they apply and in their order.
    ACTOR_MESSAGE_PLACE = 0x7D4,
    /// Sets the model's draw or visibility mode from the first argument. 0 hides
    /// the model and 1 shows it; the receiver defines any further modes.
    ACTOR_MESSAGE_SET_MODEL_DRAW = 0x7D5,
    /// Returns nonzero while the actor is still present in the scene.
    ACTOR_MESSAGE_IS_PRESENT = 0x7D6,
    /// Walks the receiver to a borrowed target position.
    ACTOR_MESSAGE_WALK_TO = 0x7DD,
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

/// The payload of `GAME_ACTOR_MESSAGE_MOVE_BY`, which moves the receiver by a displacement:
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

/// Optional clips for a scripted move of the player or a companion.
///
/// `GAME_ACTOR_MESSAGE_MOVE_TO` walks the receiver to a borrowed
/// `ActorTransform` and reads this record when the sender supplies one.
/// Message 0x3FB reads it the same way on the player and on a companion
/// whose handler for that id takes this record. One companion answers
/// 0x3FB as an animation request and does not read it.
/// A null pointer selects both default clips.
///
/// `approachAnimId` is the clip played while moving, in the receiver's
/// current animation bank. Zero selects that move's default: the usual
/// walk, the unarmed walk when no weapon model is attached, or the
/// alternate approach's clip. `arrivalAnimId` is the clip played on
/// arrival. Zero selects clip 1. Handlers keep the low 16 bits of each word.
///
/// The record is borrowed through synchronous dispatch and is not copied.
/// It occupies eight bytes with four-byte alignment.
typedef struct {
    s32 approachAnimId; // Approach clip; 0 selects the move's default
    s32 arrivalAnimId;  // Arrival clip; 0 selects clip 1
} GameActorMoveAnim;
STATIC_ASSERT_SIZEOF(GameActorMoveAnim, 8);

/// Optional clips for `ACTOR_MESSAGE_WALK_TO` on a scripted walker.
///
/// The message's first argument is the borrowed destination. This record,
/// when supplied, names the clip the walk starts in and the clip kept for a
/// later step of that walk. Both ids are in the receiver's animation bank 0.
/// A null pointer leaves the choice to the receiver. Dispatch borrows the
/// record for the call and does not copy it. The record occupies eight bytes.
typedef struct {
    s32 animationId; // Clip played as the walk starts
    u8  nextAnimId;  // Clip kept for a later step of the walk
} ActorMotionWalkAnim;
STATIC_ASSERT_SIZEOF(ActorMotionWalkAnim, 8);

/// Sends two argument words synchronously to a task's first matching message handler.
///
/// `receiver` must be a live task. Its borrowed, read-only message table selects
/// the meaning of `messageId`, both arguments and any result. Never send
/// `TASK_MESSAGE_TABLE_END`; equality precedes the end test and would select
/// its null callback. A table without that marker may receive only IDs present
/// in its entries with non-null handlers.
///
/// `firstArg` and `secondArg` are complete 32-bit PS1 argument words, carrying
/// integers or encoded object addresses. Pointer payloads use the message's
/// required type, extent, alignment and write permission, with the borrowed
/// lifetime described by `TaskMessageArg`. The pointer adapters encode those
/// addresses; the dispatcher neither copies payloads nor retains them.
///
/// Returns zero for an absent table or when the search reaches the end marker;
/// otherwise forwards the handler's return word. Inspect that word only for
/// messages whose handlers define a result; its meaning is receiver-specific,
/// and zero is not a universal success or failure code.
s32 taskMessageDispatch(Task* receiver, s32 messageId, s32 firstArg, s32 secondArg);

/// Dispatches a synchronous task message with an object address as its first payload.
///
/// `receiver` must be a live task. Its message table and `messageId` select the
/// payload's type, complete extent, alignment and write permission; a null
/// payload is valid only when that message permits it. Storage is borrowed as
/// described by `TaskMessageArg`. `secondArg` remains a signed integer word,
/// whose meaning is also selected by the message. The handler's signed result
/// is returned unchanged, or zero if the task has no matching handler.
///
/// Each argument is evaluated once, with ordinary function-argument ordering.
/// The cast encodes the complete object address in the PS1's 32-bit integer
/// message ABI. Keep it in the call expression: an inline parameter or union
/// temporary can make GCC retain a stack address across successive dispatches.
#define TASK_MESSAGE_DISPATCH_POINTER(receiver, messageId, payload, secondArg) \
    taskMessageDispatch((receiver), (messageId), (s32)(payload), (secondArg))

/// Dispatches a synchronous task message with an object address as its second payload.
///
/// `receiver` must be a live task. Its message table and `messageId` select
/// the meaning of `firstArg` and of the object addressed by `payload`,
/// including that object's type, complete extent, alignment, and whether the
/// handler reads the object, writes it, or both. A null address is valid only
/// when that message permits it. Storage is borrowed as described by
/// `TaskMessageArg`. The handler's signed result is returned unchanged, or
/// zero if the task has no matching handler.
///
/// Each argument is evaluated once, with ordinary function-argument ordering.
/// The cast encodes the complete object address in the PS1's 32-bit integer
/// message ABI. Keep it in the call expression: an inline parameter or union
/// temporary can make GCC retain a stack address across successive dispatches.
#define TASK_MESSAGE_DISPATCH_SECOND_POINTER(receiver, messageId, firstArg, payload) \
    taskMessageDispatch((receiver), (messageId), (firstArg), (s32)(payload))

/// Send two object addresses through the same word-based message interface.
static __inline__ s32 Gp_DispatchMsgPtrs(Task* task, s32 id, const void* data, const void* reply)
{
    TaskMessageArg payload;
    TaskMessageArg response;
    payload.pointer  = data;
    response.pointer = reply;
    return taskMessageDispatch(task, id, payload.value, response.value);
}

#endif // GAMEPLAY_MESSAGE_H
