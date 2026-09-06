#ifndef ROOMS_SHARED_8017FC38_H
#define ROOMS_SHARED_8017FC38_H

#include "common.h"

#include "main/task.h"

/// The seven state handlers of the room's monitor task. Every carrying room
/// holds its own table at its own address, named there by the family's symbol
/// maps - in the security room it is the head of the larger
/// `AsrMonitorStateTable` its own C defines - so the shared object owns no
/// data.
extern const TaskFuncTable7 RoomsShared8017fc38Table;

/// Runs the room monitor's current state, copying the seven handlers onto the
/// stack first so the call goes through a local table rather than through
/// `.rodata`. Four rooms carry this body.
void RoomsShared8017fc38(Task* task);

#endif // ROOMS_SHARED_8017FC38_H
