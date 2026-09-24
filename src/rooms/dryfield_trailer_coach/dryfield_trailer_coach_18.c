#include "common.h"

#include <psyq/libgte.h>

#include "main/mem.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_trailer_coach.h"

/// The message pointers the two-line text block reads: entries 0-1 by default,
/// entries 2-3 when the task's `spawnArg1` is 1.
extern u8* D_dryfield_trailer_coach_801853E4[4];

void func_dryfield_trailer_coach_801827D0(Task* arg0);

/// Opens a two-line text block: allocates the `RoomTextBlock` (killing the task
/// if that fails), links its two line nodes to the lines of
/// `D_dryfield_trailer_coach_801853E4` chosen by `spawnArg1`, hands the list to
/// `Ui_SpawnTextBlock` and advances the task.
void func_dryfield_trailer_coach_801826A0(Task* task)
{
    RoomTextBlock* block;
    TextLineNode*  node;
    u8**           line;
    s32            table;
    s32            off;
    s32            mode;
    s32            i;

    block = memCalloc(sizeof(RoomTextBlock), 0);
    node  = block->lines;
    if (block == NULL) {
        taskKill(task);
        return;
    }

    i                  = 0;
    mode               = 1;
    line               = D_dryfield_trailer_coach_801853E4;
    table              = (s32)D_dryfield_trailer_coach_801853E4;
    off                = 8;
    task->work         = (TaskIdMap*)block;
    task->exitCallback = func_dryfield_trailer_coach_801827D0;

    for (; i < 2; i++) {
        if (task->spawnArg1 == mode) {
            node->text = *(u8**)(off + table);
        } else {
            node->text = *line;
        }
        node->next = node + 1;
        node++;
        line++;
        off += 4;
    }
    node[-1].next = NULL;

    block->desc.count   = 2;
    block->desc.lines   = block->lines;
    block->desc.field_8 = 0;
    block->field_C      = 0;
    Ui_SpawnTextBlock(&block->desc, 0, 0, 0);
    task->state++;
}

/// Waits for the text block parked at `Task::work` to report a result in
/// `TextBlockDesc::field_2`, stores it through `Task::spawnArg2` and advances
/// the task.
void func_dryfield_trailer_coach_80182794(Task* task)
{
    s16 result;

    result = ((RoomTextBlock*)task->work)->desc.field_2;
    if (result != 0) {
        *(s32*)task->spawnArg2 = result;
        task->state            = task->state + 1;
    }
}

/// Exit callback of the text-block task: kills it and calls
/// `Stage_SetEndingFlag`.
void func_dryfield_trailer_coach_801827D0(Task* arg0)
{
    taskKill(arg0);
    Stage_SetEndingFlag();
}
