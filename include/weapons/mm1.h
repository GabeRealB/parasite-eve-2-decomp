#ifndef WEAPONS_MM1_H
#define WEAPONS_MM1_H

#include "common.h"

/// The overlay's view of the owning `Task`: only the state index the weapon's
/// per-frame dispatcher reads is known so far.
typedef struct Mm1Task {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  state;
} Mm1Task;

typedef void (*Mm1StateFn)(Mm1Task* task);

void func_mm1_8011D3A8(Mm1Task* arg0);
void func_mm1_8011D704(Mm1Task* arg0);
void func_mm1_8011DBD8(Mm1Task* arg0);

#endif
