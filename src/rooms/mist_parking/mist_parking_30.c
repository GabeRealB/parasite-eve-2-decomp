#include "common.h"
#include <psyq/libgte.h>
#include "main/mem.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "rooms/room_common.h"
#include "rooms/mist_parking.h"

/// The two text lines of the block `func_mist_parking_80183304` shows, and
/// the alternative pair it uses when the task's `spawnArg1` is 1.
extern u8* D_mist_parking_8018DF24[4];

/// Allocates a two-line text block, parks it at `Task::work`, spawns it and
/// steps the task on; `func_mist_parking_80183434` is set as the exit
/// callback.
void func_mist_parking_80183304(Task* task)
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
    line               = D_mist_parking_8018DF24;
    table              = (s32)D_mist_parking_8018DF24;
    off                = 8;
    task->work         = (TaskIdMap*)block;
    task->exitCallback = func_mist_parking_80183434;

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

/// Waits for the text block parked at `Task::work` to report a non-zero
/// `TextBlockDesc::field_2`, stores it through `Task::spawnArg2` and steps
/// the task on.
void func_mist_parking_801833F8(Task* task)
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
void func_mist_parking_80183434(Task* arg0)
{
    taskKill(arg0);
    Stage_SetEndingFlag();
}
