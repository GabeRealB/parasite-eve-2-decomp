#ifndef WEAPONS_SHARED_8011DDF8_H
#define WEAPONS_SHARED_8011DDF8_H

#include "common.h"

/// The weapon task's work block, as this state function sees it: a single
/// frame countdown near the end of the block.
typedef struct WeaponShared8011ddf8Work {
    /* 0x00 */ byte pad_0[0x88];
    /* 0x88 */ s32  timer;
} WeaponShared8011ddf8Work;

/// The overlay's view of the owning `Task`: the work pointer it counts down
/// and the state field it advances when the countdown runs out.
typedef struct WeaponShared8011ddf8 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ WeaponShared8011ddf8Work* field_1C;
    /* 0x20 */ byte                      pad_20[0x10];
    /* 0x30 */ s32                       state;
} WeaponShared8011ddf8;

void WeaponsShared8011ddf8(WeaponShared8011ddf8* arg0);

#endif
