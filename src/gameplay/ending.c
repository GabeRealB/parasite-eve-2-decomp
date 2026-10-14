#include "ending.h"

#include "types.h"

#include "area_transitions.h"
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

/// Signed BP award or escape penalty, retained while the single result panel is live.
static s16 _gItemMenuBattleResultBpDelta;

/// EXP award narrowed to a signed halfword before application and result drawing.
static s16 _gItemMenuBattleResultExpGain;

/// Base MP award, excluding equipment recovery, retained by the active result panel.
static s16 _gItemMenuBattleResultMpGain;

/// Equipment HP recovery before the player's maximum-HP cap, or zero on escape.
static s16 _gItemMenuBattleResultHpRecovery;

/// Equipment MP recovery added to the base award and displayed as a separate bonus.
///
/// Stores the unsigned quarter-round-up narrowed to s16, then capped above at 99;
/// negative narrowed values are retained. Zero on escape or without recovery.
static s16 _gItemMenuBattleResultMpRecovery;

PadScriptCmd gPadScriptTransitionCommands[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 4) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};
PadScriptVibrationSegment gPadScriptTransitionSegments[3] = {
    { 200, 255, 7, 1 },
    { 170, 70, 7, 1 },
    { 180, 60, 1, 0 }
};

const TaskFuncTable6 gPlayClockStates = { {
    playClockInitializeTask,
    playClockUpdateTask,
    playClockStartDeathFade,
    playClockWaitDeathFade,
    playClockRestartSessionTask,
    taskKill,
} };

/// Encoded title of the battle-result gains and totals panel.
static const char _gItemMenuBattleResultTitleText[] = "Battle Result";

/// Encoded heading of the battle-result column showing the player's updated totals.
static const u8 _gItemMenuBattleResultTotalLabelText[] = "Total";

const u8 gHudHpLabelText[] = "HP";

const u8 gHudMpLabelText[] = "MP";

/// Encoded BP label for the signed award/penalty and updated total.
static const u8 _gItemMenuBattleResultBpLabelText[] = "BP";

/// Encoded EXP label for the battle award and updated total.
static const u8 _gItemMenuBattleResultExpLabelText[] = "EXP";

const char gItemPickupTitleText[] = "Item";

void sceneBattleStartTransitionTask(Task* task)
{
    enum {
        SCENE_BATTLE_START_INITIALIZE        = 0,
        SCENE_BATTLE_START_LOAD_MUSIC        = 1,
        SCENE_BATTLE_START_DELAY_FRAMES      = 30,
        SCENE_BATTLE_START_LONG_DELAY_FRAMES = 90,
        STAGE_MUSIC_LOAD_IDLE                = 0xFF
    };
    GameSession* session;
    HudState*    hud;

    // Combat enters its fighting state before the transition delay and music load.
    if (task->state == SCENE_BATTLE_START_INITIALIZE) {
        hud                 = task->spawnArg2.pointer;
        hud->battleStep     = HUD_BATTLE_STEP_FIGHT;
        task->killCountdown = SCENE_BATTLE_START_DELAY_FRAMES;
        if ((GAME_LOCATION_WORD(gGameSession->location) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R48, 0, 0)) {
            task->killCountdown = SCENE_BATTLE_START_LONG_DELAY_FRAMES;
        }
        sndEvtRequestScriptStart(SOUND_AREA_EXIT, 0, 0);
        padScriptSpawn(gPadScriptTransitionCommands, gPadScriptTransitionSegments);
        areaSetCurrentMapMark();
        task->state++;
    } else if (task->state == SCENE_BATTLE_START_LOAD_MUSIC) {
        session = gGameSession;
        if (!(session->flowFlags & GAME_SESSION_FLOW_SKIP_ENDING_MUSIC)) {
            gStageMusicParams.fadeOutTicks = 0;
            gStageMusicParams.field_2      = 0;
            if ((session->flowFlags & GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY) == 0) {
                taskSpawnFromTable(&gStageMusicTaskDesc, 0, STAGE_MUSIC_REQUEST_COUNTDOWN, 0);
            } else {
                taskSpawnFromTable(&gStageMusicTaskDesc, 0, STAGE_MUSIC_REQUEST_LOAD_ONLY, 0);
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
            _gItemMenuBattleResultHpRecovery = 0;
            _gItemMenuBattleResultMpRecovery = 0;
            _gItemMenuBattleResultBpDelta    = gSceneCombatState.bpReward;
            _gItemMenuBattleResultExpGain    = gSceneCombatState.expReward;
            _gItemMenuBattleResultMpGain     = gSceneCombatState.mpReward;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_MP_RECOVERY) != 0) {
                _gItemMenuBattleResultMpRecovery = ((u32)(gSceneCombatState.mpReward - 1) >> 2) + 1;
                if (_gItemMenuBattleResultMpRecovery >= ITEM_MENU_BATTLE_MP_BONUS_MAX + 1) {
                    _gItemMenuBattleResultMpRecovery = ITEM_MENU_BATTLE_MP_BONUS_MAX;
                }
            }
            if (equipmentHasEffect(EQUIPMENT_EFFECT_HP_RECOVERY) != 0) {
                hpRecovery                       = (u16)gSceneCombatState.mpReward;
                _gItemMenuBattleResultHpRecovery = hpRecovery;
                player->hp                      += hpRecovery;
                if (player->hp >= player->hpMax) {
                    player->hp = player->hpMax;
                }
            }
        } else {
            _gItemMenuBattleResultExpGain    = 0;
            _gItemMenuBattleResultBpDelta    = ITEM_MENU_BATTLE_ESCAPE_BP_PENALTY;
            _gItemMenuBattleResultMpGain     = ITEM_MENU_BATTLE_ESCAPE_MP_GAIN;
            _gItemMenuBattleResultHpRecovery = 0;
            _gItemMenuBattleResultMpRecovery = 0;
        }
        player->bp += _gItemMenuBattleResultBpDelta;
        if (player->bp > ITEM_MENU_BATTLE_POINT_TOTAL_MAX) {
            player->bp = ITEM_MENU_BATTLE_POINT_TOTAL_MAX;
        }
        if (player->bp < 0) {
            player->bp = 0;
        }
        player->exp += _gItemMenuBattleResultExpGain;
        if (player->exp > ITEM_MENU_BATTLE_POINT_TOTAL_MAX) {
            player->exp = ITEM_MENU_BATTLE_POINT_TOTAL_MAX;
        }
        player->mp += _gItemMenuBattleResultMpGain + _gItemMenuBattleResultMpRecovery;
        if (player->mp > player->mpMax) {
            player->mp = player->mpMax;
        }
        task->killCountdown = 0;
        task->state++;
    }

    // Draw gains immediately, then reveal the updated totals from EXP to HP.
    uiDrawTitle(&object->panel, _gItemMenuBattleResultTitleText);
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
    textDrawString(&totalLabelRequest, _gItemMenuBattleResultTotalLabelText);

    uiDrawHorizontalSeparator(&object->panel, object->panel.contentLeft.signedValue, object->panel.contentRight.signedValue, contentTop + 9);
    uiDrawVerticalSeparator(&object->panel, contentTop + 0xC, object->panel.contentBottom.signedValue, 0x1C);

    contentBottom = object->panel.contentBottom.signedValue;
    rowY          = contentBottom - 2;
    if (_gItemMenuBattleResultHpRecovery > 0) {
        rowY             = contentBottom - 1;
        hpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
        hpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(hpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
        textDrawString(&hpLabelRequest, gHudHpLabelText);
        rowHeight = 0xA;

        hpGainRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        hpGainRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(hpGainRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&hpGainRequest, textItoaUnsigned(numberText, _gItemMenuBattleResultHpRecovery));
        rowY -= 0xA;
    }

    mpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
    mpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpLabelRequest, gHudMpLabelText);

    mpGainRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
    mpGainRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpGainRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&mpGainRequest, textItoaUnsigned(numberText, _gItemMenuBattleResultMpGain));

    if (_gItemMenuBattleResultMpRecovery > 0) {
        numberText[0] = '+';
        textItoaUnsigned(&numberText[1], _gItemMenuBattleResultMpRecovery);
        mpBonusOrBpLabelRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        mpBonusOrBpLabelRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpBonusOrBpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&mpBonusOrBpLabelRequest, numberText);
    }

    rowY                     -= rowHeight;
    mpBonusOrBpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
    mpBonusOrBpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(mpBonusOrBpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&mpBonusOrBpLabelRequest, _gItemMenuBattleResultBpLabelText);

    if (_gItemMenuBattleResultBpDelta < 0) {
        bpGainOrExpLabelRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        bpGainOrExpLabelRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpGainOrExpLabelRequest, &object->panel, ITEM_MENU_BATTLE_PENALTY_COLOR_RGB, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&bpGainOrExpLabelRequest, textItoaSigned(numberText, _gItemMenuBattleResultBpDelta));
    } else {
        bpGainOrExpLabelRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
        bpGainOrExpLabelRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
        ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpGainOrExpLabelRequest, &object->panel, gainColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
        textDrawString(&bpGainOrExpLabelRequest, textItoaUnsigned(numberText, _gItemMenuBattleResultBpDelta));
    }

    rowY                     -= rowHeight;
    totalColorRgb             = ITEM_MENU_BATTLE_TEXT_COLOR_RGB;
    bpGainOrExpLabelRequest.x = object->panel.contentLeft.signedValue + (object->panel.contentOriginX.unsignedValue + 6);
    bpGainOrExpLabelRequest.y = (s16)(object->panel.contentOriginY.unsignedValue - 2) + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(bpGainOrExpLabelRequest, &object->panel, totalColorRgb, TEXT_GLYPH_TABLE_SMALL, TEXT_ALIGNMENT_LEFT, TEXT_DRAW_OUTLINED);
    textDrawString(&bpGainOrExpLabelRequest, _gItemMenuBattleResultExpLabelText);

    expGainRequest.x = object->panel.contentOriginX.unsignedValue + columnX;
    expGainRequest.y = object->panel.contentOriginY.unsignedValue + rowY;
    ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE(expGainRequest, &object->panel, totalColorRgb, TEXT_GLYPH_TABLE_MEDIUM, TEXT_ALIGNMENT_RIGHT, TEXT_DRAW_TRANSLUCENT_OUTLINED);
    textDrawString(&expGainRequest, textItoaUnsigned(numberText, _gItemMenuBattleResultExpGain));

    rowY    = object->panel.contentBottom.signedValue - 2;
    columnX = object->panel.contentRight.signedValue - 2;
    if (_gItemMenuBattleResultHpRecovery > 0) {
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
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, gPadConfirmButtonMask | gPadCancelButtonMask) != 0) {
            object->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

#undef ITEM_MENU_SET_BATTLE_RESULT_TEXT_STYLE
