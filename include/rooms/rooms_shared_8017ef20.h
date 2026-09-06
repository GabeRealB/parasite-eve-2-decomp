#ifndef ROOMS_SHARED_8017EF20_H
#define ROOMS_SHARED_8017EF20_H

#include "common.h"

#include "main/task.h"
#include "main/ui.h"

/// The panel caption, "Play Data", and the list state behind it. Each carrying
/// room holds its own copy of both.
extern char   RoomsShared8017ef20Title[];
extern UiList RoomsShared8017ef20List;

/// The save point's Play Data panel: lays the list out under the shared row
/// descriptor on the first tick, then runs it and reports a cancel back to the
/// caller. Nineteen rooms carry this body.
void RoomsShared8017ef20(Task* task);

#endif // ROOMS_SHARED_8017EF20_H
