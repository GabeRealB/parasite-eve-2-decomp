#include "ending.h"

#include "types.h"

#include "area_transitions.h"
#include "gameplay/ending.h"
#include "hud.h"
#include "hud_sprites.h"
#include "items.h"
#include "model_lighting.h"
#include "gameplay/pad_script.h"
#include "gameplay/scene_combat.h"

#include "main/areas.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

s16 D_80114BDC;

s16 D_80114BDE;

s16 D_80114BE0;

s16 D_80114BE2;

s16 D_80114BE4;

extern const char Gp_StrBattleResult[];

extern const char Gp_StrTotal[];

extern const char Gp_StrBP[];

extern const char Gp_StrEXP[];

PadScriptCmd D_80114A24[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 4) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};
PadScriptVibrationSegment D_80114A34[3] = {
    { 200, 255, 7, 1 },
    { 170, 70, 7, 1 },
    { 180, 60, 1, 0 }
};

const TaskFuncTable6 Gp_PlayClockStates = { {
    playClockInitializeTask,
    Gp_TickPlayClock,
    playClockStartDeathFade,
    playClockWaitDeathFade,
    Gp_RestartSessionTask,
    taskKill,
} };

const char Gp_StrBattleResult[] = "Battle Result";

const char Gp_StrTotal[] = "Total";

const char Gp_StrHP[] = "HP";

const char Gp_StrMP[] = "MP";

const char Gp_StrBP[] = "BP";

const char Gp_StrEXP[] = "EXP";

const char Gp_StrItem[] = "Item";

void sceneBattleStartTransitionTask(Task* task)
{
    enum {
        SCENE_BATTLE_START_INITIALIZE        = 0,
        SCENE_BATTLE_START_LOAD_MUSIC        = 1,
        SCENE_BATTLE_START_DELAY_FRAMES      = 30,
        SCENE_BATTLE_START_LONG_DELAY_FRAMES = 90,
        STAGE_MUSIC_REQUEST_COUNTDOWN        = 2,
        STAGE_MUSIC_REQUEST_LOAD_ONLY        = 3,
        STAGE_MUSIC_LOAD_IDLE                = 0xFF
    };
    GameSession* session;
    HudState*    hud;

    // Combat enters its fighting state before the transition delay and music load.
    if (task->state == SCENE_BATTLE_START_INITIALIZE) {
        hud                 = task->spawnArg2.pointer;
        hud->battleStep     = HUD_BATTLE_STEP_FIGHT;
        task->killCountdown = SCENE_BATTLE_START_DELAY_FRAMES;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, 0, 0)) {
            task->killCountdown = SCENE_BATTLE_START_LONG_DELAY_FRAMES;
        }
        sndEvtRequestScriptStart(SOUND_AREA_EXIT, 0, 0);
        padScriptSpawn(D_80114A24, D_80114A34);
        areaSetCurrentMapMark();
        task->state++;
    } else if (task->state == SCENE_BATTLE_START_LOAD_MUSIC) {
        session = gGameSession;
        if (!(session->flowFlags & GAME_SESSION_FLOW_SKIP_ENDING_MUSIC)) {
            gStageMusicParams.fadeOutTicks = 0;
            gStageMusicParams.field_2      = 0;
            if ((session->flowFlags & GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY) == 0) {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, STAGE_MUSIC_REQUEST_COUNTDOWN, 0);
            } else {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, STAGE_MUSIC_REQUEST_LOAD_ONLY, 0);
            }
        } else {
            gStageMusicLoadState = STAGE_MUSIC_LOAD_IDLE;
        }
        task->state++;
    }
    // The delay includes both setup updates; exit also waits for the music loader.
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        if (gStageMusicLoadState == STAGE_MUSIC_LOAD_IDLE) {
            taskKill(task);
            stageRequestModeTaskExit();
        }
    }
}

/// Sets a battle-result line's style, leaving its pixel position intact.
///
/// request is a side-effect-free writable TextDrawReq lvalue, evaluated five
/// times; panel is a readable UiPanel pointer evaluated once. Other arguments
/// are evaluated once. Captures no locals. Use as a standalone block statement
/// without a trailing else. Undefined after the result task.
#define ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(request, panel, rgb, glyphs, align, mode) \
    {                                                                                    \
        (request).otIndex    = (panel)->otIndex.signedValue + 1;                         \
        (request).colorRgb   = (rgb);                                                    \
        (request).glyphTable = (glyphs);                                                 \
        (request).alignment  = (align);                                                  \
        (request).drawMode   = (mode);                                                   \
    }

void itemMenuBattleResultTask(Task* task)
{
    enum {
        ITEM_MENU_BATTLE_INITIALIZE          = 0,
        ITEM_MENU_BATTLE_WON                 = 0,
        ITEM_MENU_BATTLE_ESCAPE_BP_PENALTY   = -10,
        ITEM_MENU_BATTLE_ESCAPE_MP_GAIN      = 1,
        ITEM_MENU_BATTLE_POINT_TOTAL_MAX     = 999999,
        ITEM_MENU_BATTLE_MP_BONUS_MAX        = 99,
        ITEM_MENU_BATTLE_ELAPSED_UPDATES_MAX = 500,
        ITEM_MENU_BATTLE_EXP_TOTAL_UPDATE    = 51,
        ITEM_MENU_BATTLE_BP_TOTAL_UPDATE     = 81,
        ITEM_MENU_BATTLE_MP_TOTAL_UPDATE     = 111,
        ITEM_MENU_BATTLE_HP_TOTAL_UPDATE     = 141,
        ITEM_MENU_BATTLE_NUMBER_TEXT_BYTES   = 32,
        ITEM_MENU_BATTLE_TEXT_COLOR_RGB      = 0x606060,
        ITEM_MENU_BATTLE_PENALTY_COLOR_RGB   = 0x0D287F
    };
    u8            numberText[ITEM_MENU_BATTLE_NUMBER_TEXT_BYTES];
    TextDrawReq   totalLabelRequest;
    TextDrawReq   hpLabelRequest;
    TextDrawReq   hpGainRequest;
    TextDrawReq   mpLabelRequest;
    TextDrawReq   mpGainRequest;
    TextDrawReq   mpBonusOrBpLabelRequest;
    TextDrawReq   bpGainOrExpLabelRequest;
    TextDrawReq   expGainRequest;
    TextDrawReq   hpOrMpTotalRequest;
    TextDrawReq   bpTotalRequest;
    TextDrawReq   expTotalRequest;
    UiObject*     object;
    PlayerStatus* player;
    s32           columnX;
    s32           rowHeight;
    s32           gainColorRgb;
    s32           totalColorRgb;
    s32           rowY;
    s32           contentTop;
    s32           contentBottom;
    s32           totalLabelX;
    u16           hpRecovery;

    player = &gPlayerStatus;
    object = task->spawnArg2.pointer;
    // Apply rewards once; preserve the halfword recovery and reward narrowing.
    if (task->state == ITEM_MENU_BATTLE_INITIALIZE) {
        if (task->spawnArg1.value == ITEM_MENU_BATTLE_WON) {
            D_80114BE2 = 0;
            D_80114BE4 = 0;
            D_80114BDC = gSceneCombatState.bpReward;
            D_80114BDE = gSceneCombatState.expReward;
            D_80114BE0 = gSceneCombatState.mpReward;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_MP_RECOVERY) != 0) {
                D_80114BE4 = ((u32)(gSceneCombatState.mpReward - 1) >> 2) + 1;
                if (D_80114BE4 >= ITEM_MENU_BATTLE_MP_BONUS_MAX + 1) {
                    D_80114BE4 = ITEM_MENU_BATTLE_MP_BONUS_MAX;
                }
            }
            if (equipmentHasEffect(EQUIPMENT_EFFECT_HP_RECOVERY) != 0) {
                hpRecovery  = (u16)gSceneCombatState.mpReward;
                D_80114BE2  = hpRecovery;
                player->hp += hpRecovery;
                if (player->hp >= player->hpMax) {
                    player->hp = player->hpMax;
                }
            }
        } else {
            D_80114BDE = 0;
            D_80114BDC = ITEM_MENU_BATTLE_ESCAPE_BP_PENALTY;
            D_80114BE0 = ITEM_MENU_BATTLE_ESCAPE_MP_GAIN;
            D_80114BE2 = 0;
            D_80114BE4 = 0;
        }
        player->bp += D_80114BDC;
        if (player->bp > ITEM_MENU_BATTLE_POINT_TOTAL_MAX) {
            player->bp = ITEM_MENU_BATTLE_POINT_TOTAL_MAX;
        }
        if (player->bp < 0) {
            player->bp = 0;
        }
        player->exp += D_80114BDE;
        if (player->exp > ITEM_MENU_BATTLE_POINT_TOTAL_MAX) {
            player->exp = ITEM_MENU_BATTLE_POINT_TOTAL_MAX;
        }
        player->mp += D_80114BE0 + D_80114BE4;
        if (player->mp > player->mpMax) {
            player->mp = player->mpMax;
        }
        task->killCountdown = 0;
        task->state++;
    }

    // Draw gains immediately, then reveal the updated totals from EXP to HP.
    uiDrawTitle(&object->panel, Gp_StrBattleResult);
    if (task->killCountdown < ITEM_MENU_BATTLE_ELAPSED_UPDATES_MAX) {
        task->killCountdown++;
    }

    columnX      = 0;
    rowHeight    = 0xE;
    gainColorRgb = ITEM_MENU_BATTLE_TEXT_COLOR_RGB;

    contentTop          = object->panel.contentTop.signedValue;
    totalLabelX         = object->panel.contentOriginX.unsignedValue - 4;
    totalLabelRequest.x = object->panel.contentRight.signedValue + totalLabelX;
    totalLabelRequest.y = object->panel.contentOriginY.unsignedValue + contentTop + 5;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(totalLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_OUTLINED);
    textDrawString(&totalLabelRequest, Gp_StrTotal);

    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, contentTop + 9);
    uiDrawVerticalSeparator(&object->panel, contentTop + 0xC, object->panel.contentBottom.signedValue, 0x1C);

    contentBottom = object->panel.contentBottom.signedValue;
    rowY          = contentBottom - 2;
    if (D_80114BE2 > 0) {
        rowY             = contentBottom - 1;
        hpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
        hpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(hpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&hpLabelRequest, Gp_StrHP);
        rowHeight = 0xA;

        hpGainRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        hpGainRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(hpGainRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&hpGainRequest, textItoaUnsigned(numberText, D_80114BE2));
        rowY -= 0xA;
    }

    mpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
    mpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpLabelRequest, Gp_StrMP);

    mpGainRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
    mpGainRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpGainRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpGainRequest, textItoaUnsigned(numberText, D_80114BE0));

    if (D_80114BE4 > 0) {
        numberText[0] = '+';
        textItoaUnsigned(&numberText[1], D_80114BE4);
        mpBonusOrBpLabelRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        mpBonusOrBpLabelRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpBonusOrBpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&mpBonusOrBpLabelRequest, numberText);
    }

    rowY                     -= rowHeight;
    mpBonusOrBpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
    mpBonusOrBpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpBonusOrBpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpBonusOrBpLabelRequest, Gp_StrBP);

    if (D_80114BDC < 0) {
        bpGainOrExpLabelRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        bpGainOrExpLabelRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpGainOrExpLabelRequest, &object->panel, ITEM_MENU_BATTLE_PENALTY_COLOR_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&bpGainOrExpLabelRequest, textItoaSigned(numberText, D_80114BDC));
    } else {
        bpGainOrExpLabelRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        bpGainOrExpLabelRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpGainOrExpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&bpGainOrExpLabelRequest, textItoaUnsigned(numberText, D_80114BDC));
    }

    rowY                     -= rowHeight;
    totalColorRgb             = ITEM_MENU_BATTLE_TEXT_COLOR_RGB;
    bpGainOrExpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
    bpGainOrExpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpGainOrExpLabelRequest, &object->panel, totalColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&bpGainOrExpLabelRequest, Gp_StrEXP);

    expGainRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
    expGainRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(expGainRequest, &object->panel, totalColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&expGainRequest, textItoaUnsigned(numberText, D_80114BDE));

    rowY    = object->panel.contentBottom.signedValue - 2;
    columnX = object->panel.contentRight.signedValue - 2;
    if (D_80114BE2 > 0) {
        rowY = object->panel.contentBottom.signedValue - 1;
        if (task->killCountdown >= ITEM_MENU_BATTLE_HP_TOTAL_UPDATE) {
            hpOrMpTotalRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
            hpOrMpTotalRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
            ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(hpOrMpTotalRequest, &object->panel, totalColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
            textDrawString(&hpOrMpTotalRequest, textItoaUnsigned(numberText, player->hp));
        }
        rowY -= rowHeight;
    }
    if (task->killCountdown >= ITEM_MENU_BATTLE_MP_TOTAL_UPDATE) {
        hpOrMpTotalRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        hpOrMpTotalRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(hpOrMpTotalRequest, &object->panel, ITEM_MENU_BATTLE_TEXT_COLOR_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&hpOrMpTotalRequest, textItoaUnsigned(numberText, player->mp));
    }
    rowY -= rowHeight;
    if (task->killCountdown >= ITEM_MENU_BATTLE_BP_TOTAL_UPDATE) {
        bpTotalRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        bpTotalRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpTotalRequest, &object->panel, ITEM_MENU_BATTLE_TEXT_COLOR_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&bpTotalRequest, textItoaUnsigned(numberText, player->bp));
    }
    rowY -= rowHeight;
    if (task->killCountdown >= ITEM_MENU_BATTLE_EXP_TOTAL_UPDATE) {
        expTotalRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        expTotalRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(expTotalRequest, &object->panel, ITEM_MENU_BATTLE_TEXT_COLOR_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&expTotalRequest, textItoaUnsigned(numberText, player->exp));
    }

    if (object->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

#undef ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE
