#ifndef WEAPONS_M4A1_GRENADE_H
#define WEAPONS_M4A1_GRENADE_H

#include "common.h"

/// The overlay's view of the owning `Task`: only the state index the weapon's
/// per-frame dispatcher reads is known so far.
typedef struct M4a1GrenadeTask {
    /* 0x00 */ byte pad_0[0x30];
    /* 0x30 */ s32  state;
} M4a1GrenadeTask;

typedef void (*M4a1GrenadeStateFn)(M4a1GrenadeTask* task);

void func_m4a1_grenade_8011D654(M4a1GrenadeTask* arg0);
void func_m4a1_grenade_8011D994(M4a1GrenadeTask* arg0);
void func_m4a1_grenade_8011DE68(M4a1GrenadeTask* arg0);

#endif
