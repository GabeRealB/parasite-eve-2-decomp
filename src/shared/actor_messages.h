/* Standard handlers actors install in their message tables. ACTOR_MESSAGE_PLACE
 * places the model from a transform, in several variants that differ in how
 * the rotation is built, whether the yaw is recorded and what they return.
 * ACTOR_MESSAGE_SET_MODEL_DRAW shows, hides or buffer-flags the model. Each package includes
 * the handlers its table names; a file with a second copy of one includes the
 * fragment again under that copy's name. actorMsgSetPairVisibility reads the
 * package's published tasks, gActorSelfTask and gActorHelperTask.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_ACTOR_MESSAGES_H
#define SRC_SHARED_ACTOR_MESSAGES_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

/// State indices the handlers test and store in `ActorMsgStateWork::state`.
///
/// A state index selects an entry of the package's own state table. Every
/// package that includes `actorMsgSetVisibility` gives `HIDDEN` and `PATROL`
/// these numbers, and every one that includes `actorMsgReleaseHold` the other
/// three; the rest of each table is the package's.
enum {
    ACTOR_MESSAGE_STATE_HIDDEN    = 0x00, // model not drawn; the actor waits to be shown
    ACTOR_MESSAGE_STATE_GRAB_HOLD = 0x0D, // holds the grabbed player
    ACTOR_MESSAGE_STATE_GRAB_DONE = 0x0E, // follows the hold once the player is let go
    ACTOR_MESSAGE_STATE_DORMANT   = 0x16, // idles in place until the player comes near
    ACTOR_MESSAGE_STATE_PATROL    = 0x18  // walks between its two patrol points, watching for the player
};

/// The start of an actor's work block as `actorMsgSetVisibility` and
/// `actorMsgReleaseHold` see it.
///
/// Each package's work block is a type of its own; these handlers are shared
/// between packages and know only that it opens with the state index. The
/// rest of the block is the package's.
typedef struct {
    s16 state; // index of the state handler the actor's tick runs; the handlers store `ACTOR_MESSAGE_STATE_*` values
} ActorMsgStateWork;

/// The start of an actor's work block as `actorMsgPlaceRecordYaw` and
/// `actorMsgPlaceYawFirst` see it.
///
/// Each package's work block is a type of its own; these handlers are shared
/// between packages and know only where it keeps the heading of the last
/// placement. The rest of the block is the package's.
typedef struct {
    byte packageFields[0x16]; // the package's own members ahead of the heading; the handlers touch none of them
    s16  placedYaw;           // heading the model root was left facing by the last placement message; 4096 units per turn
} ActorMsgYawWork;
STATIC_ASSERT_SIZEOF(ActorMsgYawWork, 0x18);

/// Places the model root with Rx * Ry * Rz, leaving its stored Euler angles unchanged.
///
/// Requires a live TMD task and a readable, word-aligned placement through the
/// call. Position uses the existing root parent's frame; angles use 4096 units
/// per turn. Only vector X/Y/Z components are read; the payload is not retained.
/// Invalidates the composed transform. The message ID and second payload are
/// ignored. Returns 1.
s32 actorMsgPlace(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

/// Function identifier of the XYZ-placement fragment that records the root yaw.
///
/// Bind before this header and keep it bound through the fragment include;
/// declare a private instance static in the carrier before including this header.
/// The selected function has the `actorMsgPlaceRecordYaw` signature below.
#ifndef ACTOR_MESSAGE_PLACE_RECORD_YAW
#define ACTOR_MESSAGE_PLACE_RECORD_YAW actorMsgPlaceRecordYaw
#endif

/// Work type used by the XYZ-placement fragment's `placedYaw` store.
///
/// Bind through the fragment include to a type with a writable signed-halfword
/// placedYaw member. The default handlers use `ActorMsgYawWork`; a carrier whose
/// heading lives elsewhere selects its actual allocated work type.
#ifndef ACTOR_MESSAGE_YAW_WORK_TYPE
#define ACTOR_MESSAGE_YAW_WORK_TYPE ActorMsgYawWork
#endif

/// Places the model root with Rx * Ry * Rz and records its resulting heading.
///
/// Has `actorMsgPlace`'s placement contract and requires writable work of
/// `ACTOR_MESSAGE_YAW_WORK_TYPE`. Stores ratan2(-m[2][0], m[2][2]) in `placedYaw`, in
/// 4096 units per turn; this is derived from the matrix rather than copied from
/// the requested Y angle. The message ID and second payload are ignored. Returns 1.
s32 ACTOR_MESSAGE_PLACE_RECORD_YAW(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

/// Places the model root with Ry * Rx * Rz and records its resulting heading.
///
/// Has `actorMsgPlaceRecordYaw`'s work, payload and return contract, using the
/// requested Y angle to replace the rotation before composing X and Z.
s32 actorMsgPlaceYawFirst(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

/// Visibility modes for an actor with the `ActorMsgStateWork` prefix.
enum {
    ACTOR_MESSAGE_VISIBILITY_HIDE                         = 0,
    ACTOR_MESSAGE_VISIBILITY_SHOW                         = 1,
    ACTOR_MESSAGE_VISIBILITY_KEEP_FLAGS_SKIP_AUTO_BUFFER  = 2,
    ACTOR_MESSAGE_VISIBILITY_CLEAR_FLAGS_SKIP_AUTO_BUFFER = 3,
};

/// Sets the model's flags, primitive buffer and actor state for a visibility mode.
///
/// Requires a live TMD task and writable `ActorMsgStateWork` prefix. HIDE
/// replaces all flags with active-draw exclusion; SHOW clears all flags and
/// selects `ACTOR_MESSAGE_STATE_PATROL`. Both allocate a primitive buffer only
/// when it is missing, retaining an existing buffer. The SKIP_AUTO_BUFFER modes
/// set that bit without allocating or freeing a buffer;
/// one retains the other flags and one clears them first. All modes except SHOW
/// select `ACTOR_MESSAGE_STATE_HIDDEN`. Other modes change nothing. The message ID and second payload
/// are ignored. Returns 0.
s32 actorMsgSetVisibility(Task* task, s32 msgId, s32 mode, s32 unusedArg);

/// Places the model root and stores its Euler angles for later updates.
///
/// Has `actorMsgPlace`'s payload and frame contract. Copies X/Y/Z angles into
/// `GfxCoord::param.rot`, builds the local rotation with SDK `RotMatrix` (Rx *
/// Ry * Rz), and invalidates the composed transform. Returns 0.
s32 actorMsgPlaceEuler(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

/// Places the model root using SDK `RotMatrix`, retaining its stored Euler angles.
///
/// Has `actorMsgPlace`'s payload and frame contract, builds Rx * Ry * Rz before
/// writing translation, and returns 0. The placement is only read, including
/// the angle vector passed to the SDK's unqualified input signature.
s32 actorMsgPlaceRotMatrix(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

/// Replaces the model root's local transform with yaw, pitch and roll, in that order.
///
/// Requires a live TMD task with a writable root coordinate and a readable,
/// word-aligned placement through the call. Position uses the root parent's
/// frame; signed angles use 4096 units per turn and need not be normalized.
/// Builds Ry * Rx * Rz, reads only vector X/Y/Z and retains no payload pointer.
/// Leaves the parent and stored Euler angles unchanged, and invalidates
/// composition. Requires an initialized graphics scratch stack. Ignores the
/// message ID and second payload word; callers must ignore the dispatch result.
void actorMsgPlaceYawPitchRoll(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);

/// Places a model in world space by parenting its root to `gGfxViewCoord`.
///
/// Requires a live TMD task and readable, word-aligned placement through the
/// call, with XYZ position in world-coordinate units and angles in 4096 units
/// per turn. Builds Ry * Rx * Rz, leaves stored Euler angles unchanged and
/// invalidates composition. Reads only vector X/Y/Z; retains no payload pointer.
/// The view coordinate must remain live while used as the parent. The message
/// ID and second payload are ignored. No return value is defined.
void actorMsgPlaceInView(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

/// Flags for replacing the published actor/helper models' visibility together.
enum {
    ACTOR_MESSAGE_PAIR_SHOW             = 1 << 0,
    ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER = 1 << 1,
};

/// Replaces the flags of the carrier's published actor and helper models.
///
/// Requires live TMD tasks in the carrier's `gActorSelfTask` and
/// `gActorHelperTask` globals. SHOW clears every model flag; without it both
/// receive only active-draw exclusion. SKIP_AUTO_BUFFER adds that bit to both.
/// Other request bits are ignored. Neither allocates nor releases buffers.
/// The receiver, message ID and second payload are ignored. Returns 0.
s32 actorMsgSetPairVisibility(Task* task, s32 msgId, s32 flags, s32 unusedArg);

/// Draw modes that change only active-draw and automatic-buffer flags.
enum {
    ACTOR_MESSAGE_DRAW_HIDE                  = 0,
    ACTOR_MESSAGE_DRAW_SHOW                  = 1,
    ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER = 2,
};

/// Sets a live TMD task's draw mode while retaining all unrelated model flags.
///
/// HIDE excludes active drawing and enables automatic missing-buffer recovery;
/// SHOW permits both; HIDE_SKIP_AUTO_BUFFER disables both. Other modes change
/// nothing. No buffers are allocated or freed, and no actor state is changed.
/// The message ID and second payload are ignored. No return value is defined.
void actorMsgSetDrawMode(Task* task, s32 msgId, s32 mode, s32 unusedArg);

/// Reports whether a living enemy or its remaining opaque model is present.
///
/// Requires a live TMD task whose second spawn argument borrows a live `Enemy`.
/// Positive HP returns 1 without reading the model flags. Otherwise active-draw
/// exclusion or semi-transparency returns 0, and other flags return 1.
/// The message ID and both payload words are ignored.
s32 actorMsgIsPresent(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);

/// Requests the actor's transition out of a player hold.
///
/// Requires writable work with the `ActorMsgStateWork` prefix.
/// `ACTOR_MESSAGE_STATE_GRAB_HOLD` moves to `ACTOR_MESSAGE_STATE_GRAB_DONE`
/// while the player has positive HP, or `ACTOR_MESSAGE_STATE_DORMANT` otherwise; other
/// states remain unchanged. The state's handler performs any later release.
/// The message ID and both payload words are ignored. Returns 1.
s32 actorMsgReleaseHold(Task* task, s32 msgId, s32 unusedFirstArg, s32 unusedArg);

/// Places the model root and stores Euler angles, using SDK `RotMatrixZYX`.
///
/// Has `actorMsgPlaceEuler`'s payload, frame, storage and return contract, with
/// the rotation built as Rz * Ry * Rx.
s32 actorMsgPlaceEulerZyx(Task* task, s32 msgId, const ActorTransform* placement, s32 unusedArg);

#endif /* SRC_SHARED_ACTOR_MESSAGES_H */
