#include "common.h"

#include <psyq/libgte.h>

#include "main/mem.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_80183304.h"

void RoomsShared80183304(Task* task)
{
    RoomTextBlock* block;
    TextLineNode*  node;
    u8**           line;
    s32            table;
    s32            off;
    s32            mode;
    s32            i;

    block = Mem_Calloc(sizeof(RoomTextBlock), 0);
    node  = block->lines;
    if (block == NULL) {
        Task_Kill(task);
        return;
    }

    i                  = 0;
    mode               = 1;
    line               = RoomsShared80183304Lines;
    table              = (s32)RoomsShared80183304Lines;
    off                = 8;
    task->idMap        = (TaskIdMap*)block;
    task->exitCallback = Room_Script21;

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
