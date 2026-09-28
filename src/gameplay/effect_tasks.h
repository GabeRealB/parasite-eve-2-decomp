#ifndef GAMEPLAY_PRIVATE_EFFECT_TASKS_H
#define GAMEPLAY_PRIVATE_EFFECT_TASKS_H

#include "types.h"

#include "main/task.h"
#include "main/tmd.h"

/// Six CLUT X coordinates (0x20, 0x30, 0xC0, 0xD0, 0xE0, 0xF0) selected by
/// the top nibble of `Gp_DrawFxQuad`'s angle argument and paired with CLUT
/// Y 0x10B.
extern u16 Gp_QuadClutX[];

extern u16 D_80111EB4[];

/// 8 packed RGB-nibble colors. Index is `cln(spawnArg1 << 12) / 2839 & 7`.
/// High nibble is the `Gp_DrawFadeQuad` blend; low three nibbles are R, G, B.
extern u16 Gp_FadeQuadColors[];

extern TmdSource D_80111FC8;

extern TmdSource D_801120E4;

extern TmdSource D_80112200;

extern TmdSource D_8011231C;

extern TmdSource D_801124B8;

void Gp_EffPolyTask9C(Task* arg0);

void Gp_EffCtlTask2B(Task* arg0);

void Gp_EffCtlTask6A(Task* arg0);

void Gp_EffCtlTask6B(Task* arg0);

void func_800ED42C(Task* arg0);

void Gp_EffCtlTask6C(Task* arg0);

void Gp_EffSprTask34(Task* arg0);

void Gp_EffSprTask72(Task* arg0);

void Gp_EffLineTaskA3(Task* arg0);

void Gp_EffSprTask35(Task* arg0);

void Gp_EffSprTask6F(Task* arg0);

void Gp_EffModelTask(Task* arg0);

void Gp_EffCtlTask6E(Task* arg0);

void Gp_EffCtlTask6D(Task* arg0);

void Gp_EffTileTaskA4(Task* arg0);

void Gp_EffCtlTask3B(Task* arg0);

void Gp_EffSprTask5C(Task* arg0);

void func_800F289C(Task* arg0);

void Gp_EffSprTask76(Task* arg0);

void Gp_EffSprTask7C(Task* arg0);

void func_800F4308(Task* arg0);

void Gp_EffLineTask92(Task* arg0);

void Gp_EffSprTask9E(Task* arg0);

void Gp_EffSprTask54(Task* arg0);

void Gp_EffSprTask53(Task* arg0);

#endif // GAMEPLAY_PRIVATE_EFFECT_TASKS_H
