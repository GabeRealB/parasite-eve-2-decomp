#ifndef MAIN_TASK_TYPES_H
#define MAIN_TASK_TYPES_H

#include "common.h"

#include "main/tmd_types.h"

struct ModelObjectCoordBody;
struct Task;

/// Recognized body kinds in `Task::bodyKind`.
enum {
    /// Body kind for a live task without an attached model or coordinate body.
    ///
    /// Zero in the low byte of `TaskDesc::header.fields.flags` requests no body
    /// allocation: spawning sets `Task::extra.allocation` to NULL and ignores
    /// `TaskDesc::data`. The task can still own callback work and children.
    /// A successful later body attachment replaces this byte-sized kind.
    /// Teardown still releases work and collects the task; the separate 0xFF
    /// body-kind marker denotes completed teardown, even for a bodyless task.
    TASK_BODY_NONE = 0,
    /// Body kind for an owned runtime TMD model in `Task::extra.tmd`.
    ///
    /// Value 1 occupies the low byte of `TaskDesc::header.fields.flags` at spawn and is
    /// stored in the byte-sized `Task::bodyKind` after successful attachment.
    /// The model owns its part coordinates and any primitive buffer, while
    /// borrowing its `TmdSource`. A missing buffer does not change this kind.
    /// Teardown unlinks the model and releases its owned storage.
    TASK_BODY_TMD = 1,
    /// Body kind for an owned single-coordinate body in `Task::extra.coordBody`.
    ///
    /// Value 2 occupies the low byte of `TaskDesc::header.fields.flags` at spawn and is
    /// stored in the byte-sized `Task::bodyKind` after successful attachment.
    /// The body embeds one `GfxCoord`, refreshed by the model draw passes;
    /// it supplies a transform without a TMD source or primitive buffer.
    /// Descriptor attachment failure aborts spawning and releases the task.
    /// Unlink before freeing the body. Normal teardown releases it immediately
    /// and marks the task for later collection, leaving `Task::extra` unchanged;
    /// immediate teardown also frees the task.
    TASK_BODY_COORD = 2
};

/// One owned body allocation, interpreted according to `Task::bodyKind`.
///
/// `TASK_BODY_TMD` selects `tmd`, whose coordinates have `partCount` elements;
/// `TASK_BODY_COORD` selects `coordBody`, which owns exactly one coordinate.
/// Their coordinate extents and release paths differ.
/// `allocation` is the kind-independent pointer view used to test attachment
/// success; NULL denotes no attached body at spawn time.
///
/// Attachment links the body into its kind's refresh/draw list. Unlink before
/// releasing it. Copies of this pointer union borrow the allocation and do not
/// transfer ownership. Teardown leaves the pointer unchanged; a released body
/// (`bodyKind` 0xFF) must not be dereferenced even when non-NULL.
typedef union {
    TmdObject*                   tmd;        // TASK_BODY_TMD: model with owned part coordinates and optional primitive buffer
    struct ModelObjectCoordBody* coordBody;  // TASK_BODY_COORD: single transform for tasks that emit their own primitives
    void*                        allocation; // Kind-independent allocation pointer; NULL for no body at spawn time
} TaskBody;
STATIC_ASSERT_SIZEOF(TaskBody, 4);

/// A task handler taking its live task as the only argument and returning nothing.
///
/// Used for frame updates, teardown and task-state dispatch, including content
/// handlers invoked by task-owned UI panels. The task supplies the handler's
/// work, body and spawn arguments; there is no separate callback context or
/// return status. The pointer must be live at entry; a teardown handler may
/// release it before returning.
typedef void (*TaskFunc)(struct Task* task);

/// Three task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles. The selector may be a task state,
/// an actor mode, or a work substate. Dispatch requires an index in 0..2
/// and a non-NULL entry, which receives the live task as its only argument.
/// There is no terminator or bounds check in the table.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[3]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable3;
STATIC_ASSERT_SIZEOF(TaskFuncTable3, 0xC);

/// Four task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// an actor state or a work substate. Dispatch requires an index in 0..3
/// and a non-NULL entry, which receives the live task as its only argument.
/// There is no terminator or bounds check in the table.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[4]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable4;
STATIC_ASSERT_SIZEOF(TaskFuncTable4, 0x10);

/// Five task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// a work substate, a script lane opcode or a mini-game difficulty. Dispatch
/// requires an index in 0..4 and a non-NULL entry, which receives the live
/// task as its only argument. There is no terminator or bounds check in the
/// table.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[5]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable5;
STATIC_ASSERT_SIZEOF(TaskFuncTable5, 0x14);

/// Six task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// or a state, substate or step index kept in the task's work. Dispatch
/// requires an index in 0..5 and a non-NULL entry, which receives the live
/// task as its only argument. There is no terminator or bounds check in the
/// table, so a dispatcher whose selector also takes out-of-range sentinels,
/// such as a negative task state marking a finished wait, must test for them
/// before indexing.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[6]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable6;
STATIC_ASSERT_SIZEOF(TaskFuncTable6, 0x18);

/// Seven task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// an actor state or a work substate. Dispatch requires an index in 0..6
/// and a non-NULL entry, which receives the live task as its only argument.
/// There is no terminator or bounds check in the table, so a selector that
/// also indexes a longer table in another mode relies on its writers staying
/// within the seven slots while this one is in use.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[7]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable7;
STATIC_ASSERT_SIZEOF(TaskFuncTable7, 0x1C);

/// Eight task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// an actor's mode-specific state, or a state or substate kept in the task's
/// work. Dispatch requires an index in 0..7 and a non-NULL entry, which
/// receives the live task as its only argument. There is no terminator or
/// bounds check in the table, so a selector that also indexes a longer table
/// in another mode relies on its writers staying within the eight slots while
/// this one is in use, and the last slot of a stepping script must jump back
/// or release the task rather than step the selector on.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task unless
/// its dispatcher goes on using the task after the call.
typedef struct {
    TaskFunc funcs[8]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable8;
STATIC_ASSERT_SIZEOF(TaskFuncTable8, 0x20);

/// Nine task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// an actor's mode-specific state, or a state or step index kept in the
/// task's work. Dispatch requires an index in 0..8 and a non-NULL entry,
/// which receives the live task as its only argument. There is no terminator
/// or bounds check in the table, so a selector that also indexes a longer
/// table in another mode relies on its writers staying within the nine slots
/// while this one is in use, and the last slot must jump to another slot,
/// leave the mode or release the task rather than step the selector on.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task unless
/// its dispatcher goes on using the task after the call.
typedef struct {
    TaskFunc funcs[9]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable9;
STATIC_ASSERT_SIZEOF(TaskFuncTable9, 0x24);

/// Ten task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state or
/// a state index kept in the task's work, whether stepped through a sequence
/// or set by name. Dispatch requires an index in 0..9 and a non-NULL entry,
/// which receives the live task as its only argument. There is no terminator
/// or bounds check in the table, so a selector that also indexes a longer
/// table in another task state relies on its writers staying within the ten
/// slots while this one is in use, and the last slot must jump to another
/// slot, leave the task state or release the task rather than step the
/// selector on.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task unless
/// its dispatcher goes on using the task after the call.
typedef struct {
    TaskFunc funcs[10]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable10;
STATIC_ASSERT_SIZEOF(TaskFuncTable10, 0x28);

/// Eleven task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be a task state,
/// a work state, mode or substate, or a variant passed in the task's spawn
/// argument. Dispatch requires an index in 0..10 and a non-NULL entry, which
/// receives the live task as its only argument. There is no terminator or
/// bounds check in the table, so a selector that is only masked to a wider
/// range relies on its writers staying within the eleven slots.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[11]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable11;
STATIC_ASSERT_SIZEOF(TaskFuncTable11, 0x2C);

/// Twelve task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector may be an actor's
/// mode-specific state or a step index kept in the task's work. Dispatch
/// requires an index in 0..11 and a non-NULL entry, which receives the live
/// task as its only argument. There is no terminator or bounds check in the
/// table, so a selector that also indexes a longer table in another task
/// state relies on its writers staying within the twelve slots while this one
/// is in use.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A dispatcher that goes on using the
/// task after the call relies on its handlers leaving the task live.
typedef struct {
    TaskFunc funcs[12]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable12;
STATIC_ASSERT_SIZEOF(TaskFuncTable12, 0x30);

/// Fourteen task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector is the task state of a
/// room script, whether one that only steps forward to a final kill slot or
/// one whose handlers jump between named states. Dispatch requires an index
/// in 0..13 and a non-NULL entry, which receives the live task as its only
/// argument. There is no terminator or bounds check in the table, so the
/// last slot must not step the selector on while leaving the task live.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[14]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable14;
STATIC_ASSERT_SIZEOF(TaskFuncTable14, 0x38);

/// Sixteen task handlers stored as a value for whole-table copies.
///
/// Each table defines its slots' roles: the selector is the task state of a
/// room script that waits on an on-screen prompt, whose handlers step forward
/// through an outcome's sequence, jump back to the waiting state or release
/// the task. Dispatch requires an index in 0..15 and a non-NULL entry, which
/// receives the live task as its only argument. There is no terminator or
/// bounds check in the table, so the last slot must jump back or release the
/// task rather than step the selector on.
/// Copying it copies callback pointers, not task or work storage; the callback
/// code must remain loaded for the call. A handler may release the task.
typedef struct {
    TaskFunc funcs[16]; // Handlers in selector order; slot meanings belong to each table
} TaskFuncTable16;
STATIC_ASSERT_SIZEOF(TaskFuncTable16, 0x40);

typedef struct {
    TaskFunc funcs[18];
} TaskFuncTable18;

/// Intrusive execution-list links, also used as the bare head of a task list.
///
/// A task embeds these links as its first member. Forward links end at NULL;
/// backward links reach the bare head, which belongs to no task. On the head,
/// `next` is the first task and `prev` is the last task's node, or the head
/// itself when empty. Insertion preserves ascending byte priority and the
/// spawn order of tasks with equal priority; changing a task's priority does
/// not relink it. Only nodes embedded in tasks can be converted to `Task*`.
typedef struct TaskNode {
    struct Task*     next; // Following task, or NULL past the last
    struct TaskNode* prev; // Preceding node, or the head at the front
} TaskNode;
STATIC_ASSERT_SIZEOF(TaskNode, 0x8);

/// Unsigned low and signed high halfwords of a `TaskSpawnArg` word.
///
/// `TaskSpawnArg::halves` reads the same four bytes as the complete argument
/// word on the little-endian PS1. Integer promotion preserves `low` in
/// 0..65535 and sign-extends `high` to -32768..32767. The receiving callback
/// defines each half's units, flags, sentinels and valid values; these can
/// change when the callback reuses the argument word as state.
typedef struct {
    u16 low;  // Bits 0..15, promoted without sign extension
    s16 high; // Bits 16..31, promoted with sign extension
} TaskArgHalves;
STATIC_ASSERT_SIZEOF(TaskArgHalves, 4);

/// An untagged 32-bit task argument, interpreted by the receiving task or helper.
///
/// Spawn helpers copy payload words into `Task::spawnArg1` or `Task::spawnArg2`;
/// a descriptor selector may instead be a table index or a descriptor pointer.
/// Callbacks may subsequently update their payloads as state. A packed argument
/// can be read through `halves` or `signedBytes`, whose byte indices run from
/// least to most significant on the PS1. Units, flags and sentinels belong to
/// the individual callback's contract.
///
/// Pointer payloads refer to storage rather than copying it. The receiving
/// callback determines its lifetime and whether it must be released; the task
/// scheduler only copies the argument word. The transparent union accepts the
/// scalar and pointer forms at calls while retaining the one-register ABI.
/// Keep `value` first for that ABI. Both integer members are needed: callers
/// pass signed values and unsigned ones, and GCC accepts an argument only
/// through a member of its own type.
typedef union {
    s32           value;          // Signed value or packed argument bits
    u32           unsignedValue;  // Unsigned view of the same bits
    void*         pointer;        // Object pointer, interpreted by the receiving callback
    TaskArgHalves halves;         // Unsigned low half and signed high half
    s8            signedBytes[4]; // Signed byte view of the complete argument word
} TaskSpawnArg __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(TaskSpawnArg, 4);

/// A task's message table entry; defined with the dispatcher in `gameplay/message.h`.
struct TaskMessageEntry;

/// A primary-heap game object updated by the cooperative task scheduler.
///
/// Each task belongs to an execution list headed by a `TaskNode` and may also
/// belong to a parent's circular child list. Its descriptor supplies the initial
/// callback, byte priority and body kind. Spawn arguments are copied as words;
/// callbacks define their interpretation and the lifetime of pointer payloads.
/// `state`, `status`, `extraState` and `killCountdown` are callback-owned storage
/// except while the task system uses them for stop and teardown protocols.
///
/// Default teardown treats non-NULL `work` as one owned primary-heap allocation;
/// callbacks release its nested resources separately. A task using borrowed work
/// storage must clear this slot before default teardown. Teardown frees work before
/// releasing the body and does not clear either pointer. Normal model teardown
/// waits two countdown callbacks; coordinate bodies are released immediately.
/// The execution pass collects the task after a callback returns with
/// `bodyKind` 0xFF.
/// Immediate teardown can free the task within its callback. A bare list head
/// is never a task, and released pointers must not be dereferenced.
typedef struct Task {
    TaskNode                       node;          // Intrusive list links; a task is its own list node
    struct Task*                   parent;        // Parent in the teardown tree; NULL for an unattached task
    struct Task*                   firstChild;    // Head of the child ring; NULL when childless
    struct Task*                   nextSibling;   // Next child in that ring; the task itself when it is an only child
    TaskFunc                       callback;      // Per-frame entry point, called by the exec passes
    TaskFunc                       exitCallback;  // Teardown handler; initially taskKill, replaced by callbacks with nested resources
    void*                          work;          // Callback-defined work block; default teardown frees it, so borrowed storage must be cleared first
    TaskSpawnArg                   spawnArg2;     // Second mutable payload word; callback defines values, pointer type and lifetime
    const struct TaskMessageEntry* msgTable;      // Borrowed id/handler table, or NULL; the required ids and each handler's payload types are receiver-specific
    u8                             bodyKind;      // Body kind (0 none, 1 TMD model, 2 coordinate body, 0xFF released and awaiting collection)
    u8                             priority;      // Ascending execution order at insertion; also the exact byte selected by filtered passes
    s16                            killCountdown; // Callback-owned signed counter; remaining callback ticks during deferred body teardown
    TaskBody                       extra;         // The body the task owns, attached and released according to `bodyKind`
    s32                            state;         // Callback-defined state or counter, often an index into a handler table
    TaskSpawnArg                   spawnArg1;     // First mutable payload word; callback defines values, pointer type and lifetime
    u8                             status;        // Callback-defined byte; 0xFF signals a stop request to Task_PollKill
    byte                           unknown_39[3]; // No field access established; role unproven
    union {
        s32   value;
        void* pointer;
    } extraState;       // Callback-defined integer or borrowed pointer; result word returned with a stop request
    byte unknown_40[8]; // No field access established; role unproven
} Task;
STATIC_ASSERT_SIZEOF(Task, 0x48);

/// Shared model-buffer option and table terminator for task descriptors.
enum {
    /// Disables automatic primitive-buffer allocation for a task's TMD body.
    ///
    /// Bit 8 of the u16 `TaskDesc::header.fields.flags`; combine with
    /// `TASK_BODY_TMD`. Spawning still creates the model and its coordinates,
    /// leaves its buffer NULL and sets `TMD_OBJECT_SKIP_AUTO_BUFFER`. Later
    /// missing-buffer recovery skips the model while that runtime bit is set.
    /// Explicit allocation remains allowed, and existing buffers are released
    /// normally. The option is ignored for bodyless and coordinate-body spawns.
    /// This descriptor mask is separate from creation-buffer bit 0 and from
    /// the runtime mask in `TmdObject::flags`.
    TASK_DESC_SKIP_AUTO_MODEL_BUFFER = 1 << 8,
    TASK_DESC_END                    = 0xFFFF // Complete flags halfword ending a walked descriptor table; never spawn it
};

/// A 12-byte spawn recipe supplying a task's body, execution priority and initial callback.
///
/// Bank spawns index a descriptor table; direct spawns accept an entry or a table
/// plus an index. Indices must address live entries, excluding any terminator.
/// Walked location tables end at `TASK_DESC_END` in `header.fields.flags`.
/// The little-endian `header.word` view compares both complete halfwords at once:
/// flags occupy bits 0..15 and priority occupies bits 16..31.
///
/// Spawning reads the descriptor synchronously and retains no pointer to it.
/// The callback's code and any attached model's borrowed geometry must outlive
/// their use by the task. Model attachment failure aborts the spawn; the model
/// body and any buffer it allocates belong to the new task.
///
/// `data` is descriptor metadata, separate from the two payload words copied
/// into `Task::spawnArg1` and `Task::spawnArg2`. The spawner reads `data.model`
/// only for a TMD body and ignores this word for other body kinds. Location
/// tables use `data.value` as a decimal stage/area/room key before spawning.
typedef struct {
    union {
        struct {
            u16 flags;    // Low byte: body kind (0 none, 1 TMD model, 2 coordinate body); bit 8: skip automatic model-buffer allocation and recovery
            u16 priority; // Low byte sets ascending execution order; equal priorities retain spawn order
        } fields;
        s32 word;         // Both complete halfwords, used for descriptor selection; priority in the upper half
    } header;
    TaskFunc callback;    // Initial per-frame handler; receives the spawned task when dispatched
    union {
        TmdSource* model; // TASK_BODY_TMD: borrowed source for the newly attached model
        s32        value; // Descriptor-specific metadata; location key = stage * 10000 + area * 100 + room (room 0 selects the area)
    } data;
} TaskDesc;
STATIC_ASSERT_SIZEOF(TaskDesc, 0xc);

#endif // MAIN_TASK_TYPES_H
