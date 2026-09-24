#include "common.h"

#include "main/task.h"

void Actor01100_Fn0638C(Task* task);
void Actor01100_Fn073DC(Task* task);

/// Runs the task's current state handler from a two-entry table built on the
/// stack: set-up (`Actor01100_Fn0638C`), then the per-frame countdown
/// (`Actor01100_Fn073DC`).
void Actor01100_Fn0663C(Task* task)
{
    TaskFunc funcs[2] = {
        Actor01100_Fn0638C,
        Actor01100_Fn073DC,
    };

    funcs[task->state](task);
}
