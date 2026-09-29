#ifndef MAIN_TASK_TYPES_H
#define MAIN_TASK_TYPES_H

#include "common.h"

#include "main/tmd_types.h"

struct GpDisp2d;
struct Task;

/// The body a task owns, whose kind its `spawnType` names: a model for 1, a 2D
/// display for 2, nothing for 0.
typedef union TaskBody {
    TmdObject*       tmd;    // spawnType 1
    struct GpDisp2d* disp2d; // spawnType 2
} TaskBody;

/// A function the task system calls with the task that owns the slot.
///
/// Both the per-frame step and the teardown are one of these (`Task::callback`
/// and `Task::exitCallback`), as are the per-state handlers a dispatcher picks
/// out by `Task::state`. The task is the only argument: whatever a body needs
/// it reaches through that, in `Task::work`, `Task::extra` or `Task::spawnArg2`.
typedef void (*TaskFunc)(struct Task* task);

/// Fixed-size table of `TaskFunc` callbacks. Copied onto the stack by state
/// dispatchers (e.g. `GameFlow_DispatchTable`) so the call uses a local jump table.
typedef struct {
    TaskFunc funcs[3];
} TaskFuncTable3;

typedef struct {
    TaskFunc funcs[4];
} TaskFuncTable4;

typedef struct {
    TaskFunc funcs[5];
} TaskFuncTable5;

typedef struct {
    TaskFunc funcs[6];
} TaskFuncTable6;

typedef struct {
    TaskFunc funcs[7];
} TaskFuncTable7;

typedef struct {
    TaskFunc funcs[8];
} TaskFuncTable8;

typedef struct {
    TaskFunc funcs[9];
} TaskFuncTable9;

typedef struct {
    TaskFunc funcs[10];
} TaskFuncTable10;

typedef struct {
    TaskFunc funcs[11];
} TaskFuncTable11;

typedef struct {
    TaskFunc funcs[12];
} TaskFuncTable12;

typedef struct {
    TaskFunc funcs[14];
} TaskFuncTable14;

typedef struct {
    TaskFunc funcs[16];
} TaskFuncTable16;

typedef struct {
    TaskFunc funcs[18];
} TaskFuncTable18;

/// Intrusive list link for a `Task`, and the type a task list is headed by.
///
/// The link is the task's first member, so a pointer to one is also the task
/// that carries it. A head is a bare node belonging to no task: its `next` is
/// the first task on the list, and its `prev` the last, which is the head
/// itself while the list is empty. The head is the only node that is not a task,
/// and a walk reaches it through `prev` alone, so `next` names a task while
/// `prev` names a node.
typedef struct TaskNode {
    struct Task*     next; // Following task, or NULL past the last
    struct TaskNode* prev; // Preceding node, or the head at the front
} TaskNode;
STATIC_ASSERT_SIZEOF(TaskNode, 0x8);

/// 2-byte table entry (id + type). Indexed via TaskIdMap.
typedef struct _TaskIdPair {
    /* 0x0 */ u8 id;
    /* 0x1 */ u8 type;
} TaskIdPair;
STATIC_ASSERT_SIZEOF(TaskIdPair, 0x2);

/// Index + pointer into a TaskIdPair table. Allocated (memCalloc(8)) and stored
/// at Task::work by Task_AllocIdMap; read by Stage_ApplyTableEntryWhenIdle / Stage_LoadOrCountdownTask.
typedef struct _TaskIdMap {
    /* 0x0 */ u16         index;
    /* 0x2 */ byte        pad_2[2];
    /* 0x4 */ TaskIdPair* table;
} TaskIdMap;
STATIC_ASSERT_SIZEOF(TaskIdMap, 0x8);

/// One task argument word: a value or an object pointer, selected by the task
/// type. Transparent-union calls preserve the original one-register ABI.
typedef union TaskSpawnArg {
    s32           value;
    u32           unsignedValue;
    long          signedWord;
    unsigned long unsignedWord;
    void*         pointer;
    const void*   constPointer;
} TaskSpawnArg __attribute__((transparent_union));
STATIC_ASSERT_SIZEOF(TaskSpawnArg, 4);

/// A cooperatively scheduled game object. Actors, UI and loading steps are all
/// tasks, so one spawn, tick and kill path serves them all.
///
/// A task is a per-frame `callback` plus an optional body, and it belongs to two
/// structures at once: an intrusive list rooted at a `TaskNode`, which the exec
/// passes walk in `priority` order, and an optional parent/child tree whose
/// children form a ring (`firstChild` / `nextSibling`). The spawn helpers build
/// one from a `TaskDesc`, which supplies its `callback` and `priority`;
/// `spawnArg1`, `spawnArg2` and `extra` carry whatever the spawned type needs.
/// The task system treats `spawnArg1` as one word; a type that packs several
/// values into it takes them apart with shifts and masks of that word.
///
/// Several further slots are the task's own storage that the task system borrows
/// to run its protocol, so they hold whatever the spawned type puts there between
/// those uses: `status` carries a stop request, `extraState` the word that
/// request hands back, and `killCountdown` the delay before the body goes.
///
/// Killing a task that owns a body is spread over two steps, so nothing frees it
/// while its callback is still running: `taskKill` releases the body — a TMD model
/// only once its `killCountdown` has run out, a 2D display straight away — and
/// marks the task with `spawnType` 0xFF, and the exec pass that sees the mark
/// unlinks and frees the task once the callback has returned.
typedef struct Task {
    TaskNode     node;          // Intrusive list links; a task is its own list node
    struct Task* parent;        // Owning task; NULL when the task sits at the top level
    struct Task* firstChild;    // Head of the child ring; NULL when childless
    struct Task* nextSibling;   // Next child in that ring; the task itself when it is an only child
    TaskFunc     callback;      // Per-frame entry point, called by the exec passes
    TaskFunc     exitCallback;  // Runs as the task is torn down
    void*        work;          // Per-task work block, freed on kill; whatever the spawned type needs
    TaskSpawnArg spawnArg2;     // Second spawn argument; its meaning is the spawned type's
    void*        msgTable;      // Table of id/handler records the task answers messages with
    u8           spawnType;     // Body kind (0 none, 1 TMD model, 2 2D display); 0xFF marks a task to collect
    u8           priority;      // List position; lower runs earlier, and selects which pass picks the task up
    s16          killCountdown; // Frames left before the body is released; the task's own timer otherwise
    TaskBody     extra;         // The body the task owns, attached and released according to `spawnType`
    s32          state;         // Index a handler dispatches on to pick its per-state function
    TaskSpawnArg spawnArg1;     // First argument, interpreted by the task type.
    u8           status;        // The task's own byte; the task system records a stop request in it as 0xFF
    byte         unknown_39[3];
    union {
        s32   value;
        void* pointer;
    } extraState; // Stop-request word or task-owned payload, including command replies
    byte         unknown_40[8];
} Task;
STATIC_ASSERT_SIZEOF(Task, 0x48);

/// One entry of a task table: what a spawn helper turns into a running `Task`.
///
/// The shared tables are reached by name — `gTaskDescBanks[bank][type]` for the
/// banks, a package's own table for its rooms and actors — and every spawn path
/// ends in `Task_SpawnFromDesc`, which reads these four fields and nothing else.
/// A table that is walked rather than indexed ends on an entry whose `flags` is
/// all ones.
///
/// The argument is the descriptor's own: a kind-1 descriptor names the model its
/// task attaches, and one that attaches no model keeps whatever it needs there.
typedef struct {
    u16      flags;       // Body kind in the low byte (0 none, 1 TMD model, 2 2D display), plus bit 8 to attach the model without allocating its buffer
    u16      priority;    // List position the spawned task takes; its low byte is what `Task::priority` gets
    TaskFunc callback;    // Per-frame entry point the spawned task runs
    union {
        TmdSource* model; // Kind 1: the model the task attaches
        void*      storage; // Other kinds may pass a pointer to their own data
        s32        value; // The descriptor's own value, where it attaches no model
    } arg;
} TaskDesc;
STATIC_ASSERT_SIZEOF(TaskDesc, 0xc);

#endif // MAIN_TASK_TYPES_H
