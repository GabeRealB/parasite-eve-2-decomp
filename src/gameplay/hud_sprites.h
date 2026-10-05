#ifndef GAMEPLAY_PRIVATE_HUD_SPRITES_H
#define GAMEPLAY_PRIVATE_HUD_SPRITES_H

#include "types.h"

#include "hud.h"

#include "main/task_types.h"

void Gp_PlayClockState2(Task* arg0);

void Gp_PlayClockState3(Task* arg0);

void Gp_StartAreaBgm(s16* arg0);

u8* Gp_GetAttachLevels(void);

void Gp_ResetHudFx(HudState* hud);

void Gp_EnqueueAttach7Cd(void);

/// State-F0 gate stub; callers supply an unused action code.
s32 func_800A7CB0(s32 unused);

void func_800A7DB8(s32 arg0);

void func_800A7DE0(void);

void Gp_LoadStageView(void);

s32 Gp_IsStateF0Active(void);

/// Empty hook called when the view gate differs from the live save's view.
void viewChangeStub(void);

void func_800A7E4C(void);

void Gp_DrawHudSprites(HudState* hud);

void Gp_DrawHudNumbers(s32 x, s32 y, s32 cur, s32 max, s32 kind);

void Gp_UpdateLinkXforms(void);

s32 func_800A7550(void);

void func_800A7824(s32 arg0, s32 arg1, s32 arg2);

void Gp_HudTrackSlot0(HudTargetHpReadout* readout);

void Gp_DrawItemObtained(Task* arg0);

void Gp_DrawItemTitle(Task* arg0);

s32 Gp_GetAttachLevel(s32 arg0);

#endif // GAMEPLAY_PRIVATE_HUD_SPRITES_H
