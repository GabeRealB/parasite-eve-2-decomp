#ifndef KYLE_KYLE_H
#define KYLE_KYLE_H

/* The kyle family's interface to the resident code: what main's task
 * descriptor tables name inside a kyle package.
 */

#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Task entries the resident task descriptor tables name.
void func_kyle_800102_801682B4(Task* arg0);

/// Models those descriptors attach, named after the package that holds them.
extern TmdSource D_kyle_800101_8016C594;
extern TmdSource D_kyle_800101_8016C9E8;
extern TmdSource D_kyle_800101_8016CED8;
extern TmdSource D_kyle_800101_8016D32C;
extern TmdSource D_kyle_800101_8016D81C;
extern TmdSource D_kyle_800103_8016C594;
extern TmdSource D_kyle_800103_8016C9E8;
extern TmdSource D_kyle_800103_8016CED8;
extern TmdSource D_kyle_800103_8016D32C;
extern TmdSource D_kyle_800103_8016D81C;
extern TmdSource D_kyle_800104_8016C594;
extern TmdSource D_kyle_800104_8016C9E8;
extern TmdSource D_kyle_800104_8016CED8;
extern TmdSource D_kyle_800104_8016D32C;
extern TmdSource D_kyle_800104_8016D81C;
extern TmdSource D_kyle_800102_8016CE38;
extern TmdSource D_kyle_800102_8016D28C;
extern TmdSource D_kyle_800102_8016D77C;
extern TmdSource D_kyle_800102_8016DBD0;
extern TmdSource D_kyle_800101_8016DC60;
extern TmdSource D_kyle_800103_8016DE3C;
extern TmdSource D_kyle_800102_8016E0C0;
extern TmdSource D_kyle_800104_8016E568;
extern TmdSource D_kyle_800102_8016E93C;

/// A model argument that points into the zeroed tail of kyle_800102, where the
/// disc image holds no model record.
extern TmdSource D_kyle_800102_801775A8;

#endif /* KYLE_KYLE_H */
