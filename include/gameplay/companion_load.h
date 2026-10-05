#ifndef GAMEPLAY_COMPANION_LOAD_H
#define GAMEPLAY_COMPANION_LOAD_H

#include "main/task_types.h"

/// Empty entry point called first in a pair before Acropolis scene transitions.
///
/// Its intended role is unproven.
void func_800ABFF8(void);

/// Empty entry point called second in a pair before Acropolis scene transitions.
///
/// Its intended role is unproven.
void func_800AC000(void);

void Gp_LoadStateTask(Task* task);

void func_800AC0F0(Task* task);

#endif // GAMEPLAY_COMPANION_LOAD_H
