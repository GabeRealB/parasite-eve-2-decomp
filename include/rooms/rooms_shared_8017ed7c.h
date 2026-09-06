#ifndef ROOMS_SHARED_8017ED7C_H
#define ROOMS_SHARED_8017ED7C_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The vending machine's outer list task: clears the item preview, spawns the
/// room's row panel and the shared holder panel, lays the list out with a
/// 5-pixel inset and scrolling on, then services its children every frame -
/// code 6 tears a child down and re-arms the list, -1 drops the holder and
/// propagates upwards. Seven rooms carry this body.
void RoomsShared8017ed7c(Task* task);

/// Per-room data of the outer list: its caption, the list itself and the row
/// panel it spawns.
extern u8           RoomsShared8017ed7cTitle[];
extern UiList       RoomsShared8017ed7cList;
extern UiObjectDesc RoomsShared8017ed7cRowsDesc;

#endif // ROOMS_SHARED_8017ED7C_H
