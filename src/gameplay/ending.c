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
    Gp_InitPlayClock,
    Gp_TickPlayClock,
    Gp_PlayClockState2,
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

// "Item obtained!"
// "Bonus item!!"

/* r1 = long vector in, r2 = long vector out: r2 = RT * r1 + TR at full
 * 32-bit precision, the input split into three 10/11-bit slices. */
#define gte_RotTransLV(r1, r2) __asm__ volatile( \
    "lw	$14, 0( %0 );"                           \
    "lw	$15, 4( %0 );"                           \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "andi	$12, $12, 0xffff;"                     \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $0;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $13, $16;"                        \
    "sll	$13, $13, 16;"                          \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $12, $16;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $2;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "andi	$12, $14, 0xffff;"                     \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $16, $12;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "andi	$13, $15, 0xffff;"                     \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $16, $13;"                        \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $4;"                              \
    "lw	$16, 8( %0 );"                           \
    "addiu	$14, $0, -0x400;"                     \
    "srl	$15, $16, 31;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $1;"                              \
    "sra	$16, $16, 10;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "addu	$12, $12, $15;"                        \
    "mtc2	$12, $3;"                              \
    "sra	$16, $16, 10;"                          \
    "addu	$12, $16, $15;"                        \
    "mtc2	$12, $5;"                              \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A480012;"                          \
    "mfc2	$14, $25;"                             \
    "mfc2	$15, $26;"                             \
    "mfc2	$16, $27;"                             \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A40E012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$16, $12, $16;"                        \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A416012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$16, $12, $16;"                        \
    "sw	$14, 0( %1 );"                           \
    "sw	$15, 4( %1 );"                           \
    "sw	$16, 8( %1 )"                            \
    :                                            \
    : "r"(r1), "r"(r2)                           \
    : "$12", "$13", "$14", "$15", "$16", "memory")

void Gp_EndingTask(Task* arg0)
{
    GameSession* session;
    HudState*    hud;

    if (arg0->state == 0) {
        hud                 = arg0->spawnArg2.pointer;
        hud->battleStep     = HUD_BATTLE_STEP_FIGHT;
        arg0->killCountdown = 0x1E;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 48, 0, 0)) {
            arg0->killCountdown = 0x5A;
        }
        sndEvtRequestScriptStart(SOUND_AREA_EXIT, 0, 0);
        Gp_SpawnScript18(D_80114A24, D_80114A34);
        Gp_SetCurAreaFlag4();
        arg0->state++;
    } else if (arg0->state == 1) {
        session = gGameSession;
        if (!(session->flowFlags & GAME_SESSION_FLOW_SKIP_ENDING_MUSIC)) {
            gStageMusicParams.fadeOutTicks = 0;
            gStageMusicParams.field_2      = 0;
            if ((session->flowFlags & GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY) == 0) {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 2, 0);
            } else {
                taskSpawnFromTable(&Stage_MusicTaskDesc, 0, 3, 0);
            }
        } else {
            gStageMusicLoadState = 0xFF;
        }
        arg0->state++;
    }
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        if (gStageMusicLoadState == 0xFF) {
            taskKill(arg0);
            stageRequestModeTaskExit();
        }
    }
}

void func_800A087C(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    TextDrawReq   req6;
    TextDrawReq   req7;
    TextDrawReq   req8;
    TextDrawReq   req9;
    TextDrawReq   req10;
    TextDrawReq   req11;
    UiObject*     obj;
    PlayerStatus* cfg;
    s32           col;
    s32           step;
    s32           color;
    s32           color2;
    s32           y;
    s32           top;
    s32           h;
    s32           tx;
    u16           add;

    cfg = &gPlayerStatus;
    obj = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value == 0) {
            D_80114BE2 = 0;
            D_80114BE4 = 0;
            D_80114BDC = gSceneCombatState.bpReward;
            D_80114BDE = gSceneCombatState.expReward;
            D_80114BE0 = gSceneCombatState.mpReward;
            if (equipmentHasEffect(EQUIPMENT_EFFECT_MP_RECOVERY) != 0) {
                D_80114BE4 = ((u32)(gSceneCombatState.mpReward - 1) >> 2) + 1;
                if (D_80114BE4 >= 100) {
                    D_80114BE4 = 99;
                }
            }
            if (equipmentHasEffect(EQUIPMENT_EFFECT_HP_RECOVERY) != 0) {
                add        = (u16)gSceneCombatState.mpReward;
                D_80114BE2 = add;
                cfg->hp   += add;
                if (cfg->hp >= cfg->hpMax) {
                    cfg->hp = cfg->hpMax;
                }
            }
        } else {
            D_80114BDE = 0;
            D_80114BDC = -10;
            D_80114BE0 = 1;
            D_80114BE2 = 0;
            D_80114BE4 = 0;
        }
        cfg->bp += D_80114BDC;
        if (cfg->bp > 999999) {
            cfg->bp = 999999;
        }
        if (cfg->bp < 0) {
            cfg->bp = 0;
        }
        cfg->exp += D_80114BDE;
        if (cfg->exp > 999999) {
            cfg->exp = 999999;
        }
        cfg->mp += D_80114BE0 + D_80114BE4;
        if (cfg->mp > cfg->mpMax) {
            cfg->mp = cfg->mpMax;
        }
        arg0->killCountdown = 0;
        arg0->state++;
    }

    uiDrawTitle(&(obj)->panel, Gp_StrBattleResult);
    if (arg0->killCountdown < 500) {
        arg0->killCountdown++;
    }

    col   = 0;
    step  = 0xE;
    color = 0x606060;

    top             = obj->panel.contentTop.signedValue;
    tx              = obj->panel.contentOriginX.unsignedValue - 4;
    req1.x          = obj->panel.contentRight.signedValue + tx;
    req1.y          = obj->panel.contentOriginY.unsignedValue + top + 5;
    req1.otIndex    = obj->panel.otIndex.signedValue + 1;
    req1.colorRgb   = color;
    req1.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req1.alignment  = TEXT_ALIGNMENT_RIGHT;
    req1.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req1, Gp_StrTotal);

    uiDrawHorizontalSeparator(&(obj)->panel, obj->panel.contentLeft.signedValue, obj->panel.contentRight.signedValue, top + 9);
    uiDrawVerticalSeparator(&(obj)->panel, top + 0xC, obj->panel.contentBottom.signedValue, 0x1C);

    h = obj->panel.contentBottom.signedValue;
    y = h - 2;
    if (D_80114BE2 > 0) {
        y               = h - 1;
        req2.x          = obj->panel.contentLeft.signedValue + (obj->panel.contentOriginX.unsignedValue + 6);
        req2.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
        req2.otIndex    = obj->panel.otIndex.signedValue + 1;
        req2.colorRgb   = color;
        req2.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req2.alignment  = TEXT_ALIGNMENT_LEFT;
        req2.drawMode   = TEXT_DRAW_OUTLINED;
        textDrawString(&req2, Gp_StrHP);
        step = 0xA;

        req3.x          = obj->panel.contentOriginX.unsignedValue + col;
        req3.y          = obj->panel.contentOriginY.unsignedValue + y;
        req3.otIndex    = obj->panel.otIndex.signedValue + 1;
        req3.colorRgb   = color;
        req3.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req3.alignment  = TEXT_ALIGNMENT_RIGHT;
        req3.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req3, textItoaUnsigned(buf, D_80114BE2));
        y -= 0xA;
    }

    req4.x          = obj->panel.contentLeft.signedValue + (obj->panel.contentOriginX.unsignedValue + 6);
    req4.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req4.otIndex    = obj->panel.otIndex.signedValue + 1;
    req4.colorRgb   = color;
    req4.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req4.alignment  = TEXT_ALIGNMENT_LEFT;
    req4.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req4, Gp_StrMP);

    req5.x          = obj->panel.contentOriginX.unsignedValue + col;
    req5.y          = obj->panel.contentOriginY.unsignedValue + y;
    req5.otIndex    = obj->panel.otIndex.signedValue + 1;
    req5.colorRgb   = color;
    req5.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req5.alignment  = TEXT_ALIGNMENT_RIGHT;
    req5.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req5, textItoaUnsigned(buf, D_80114BE0));

    if (D_80114BE4 > 0) {
        buf[0] = '+';
        textItoaUnsigned(&buf[1], D_80114BE4);
        req6.x          = obj->panel.contentOriginX.unsignedValue + col;
        req6.y          = obj->panel.contentOriginY.unsignedValue + y;
        req6.otIndex    = obj->panel.otIndex.signedValue + 1;
        req6.colorRgb   = color;
        req6.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req6.alignment  = TEXT_ALIGNMENT_LEFT;
        req6.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req6, buf);
    }

    y              -= step;
    req6.x          = obj->panel.contentLeft.signedValue + (obj->panel.contentOriginX.unsignedValue + 6);
    req6.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req6.otIndex    = obj->panel.otIndex.signedValue + 1;
    req6.colorRgb   = color;
    req6.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req6.alignment  = TEXT_ALIGNMENT_LEFT;
    req6.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req6, Gp_StrBP);

    if (D_80114BDC < 0) {
        req7.x          = obj->panel.contentOriginX.unsignedValue + col;
        req7.y          = obj->panel.contentOriginY.unsignedValue + y;
        req7.otIndex    = obj->panel.otIndex.signedValue + 1;
        req7.colorRgb   = 0xD287F;
        req7.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req7.alignment  = TEXT_ALIGNMENT_RIGHT;
        req7.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req7, textItoaSigned(buf, D_80114BDC));
    } else {
        req7.x          = obj->panel.contentOriginX.unsignedValue + col;
        req7.y          = obj->panel.contentOriginY.unsignedValue + y;
        req7.otIndex    = obj->panel.otIndex.signedValue + 1;
        req7.colorRgb   = color;
        req7.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req7.alignment  = TEXT_ALIGNMENT_RIGHT;
        req7.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req7, textItoaUnsigned(buf, D_80114BDC));
    }

    y              -= step;
    color2          = 0x606060;
    req7.x          = obj->panel.contentLeft.signedValue + (obj->panel.contentOriginX.unsignedValue + 6);
    req7.y          = (s16)(obj->panel.contentOriginY.unsignedValue - 2) + y;
    req7.otIndex    = obj->panel.otIndex.signedValue + 1;
    req7.colorRgb   = color2;
    req7.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req7.alignment  = TEXT_ALIGNMENT_LEFT;
    req7.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req7, Gp_StrEXP);

    req8.x          = obj->panel.contentOriginX.unsignedValue + col;
    req8.y          = obj->panel.contentOriginY.unsignedValue + y;
    req8.otIndex    = obj->panel.otIndex.signedValue + 1;
    req8.colorRgb   = color2;
    req8.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req8.alignment  = TEXT_ALIGNMENT_RIGHT;
    req8.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    textDrawString(&req8, textItoaUnsigned(buf, D_80114BDE));

    y   = obj->panel.contentBottom.signedValue - 2;
    col = obj->panel.contentRight.signedValue - 2;
    if (D_80114BE2 > 0) {
        y = obj->panel.contentBottom.signedValue - 1;
        if (arg0->killCountdown >= 0x8D) {
            req9.x          = obj->panel.contentOriginX.unsignedValue + col;
            req9.y          = obj->panel.contentOriginY.unsignedValue + y;
            req9.otIndex    = obj->panel.otIndex.signedValue + 1;
            req9.colorRgb   = color2;
            req9.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req9.alignment  = TEXT_ALIGNMENT_RIGHT;
            req9.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&req9, textItoaUnsigned(buf, cfg->hp));
        }
        y -= step;
    }
    if (arg0->killCountdown >= 0x6F) {
        req9.x          = obj->panel.contentOriginX.unsignedValue + col;
        req9.y          = obj->panel.contentOriginY.unsignedValue + y;
        req9.otIndex    = obj->panel.otIndex.signedValue + 1;
        req9.colorRgb   = 0x606060;
        req9.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req9.alignment  = TEXT_ALIGNMENT_RIGHT;
        req9.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req9, textItoaUnsigned(buf, cfg->mp));
    }
    y -= step;
    if (arg0->killCountdown >= 0x51) {
        req10.x          = obj->panel.contentOriginX.unsignedValue + col;
        req10.y          = obj->panel.contentOriginY.unsignedValue + y;
        req10.otIndex    = obj->panel.otIndex.signedValue + 1;
        req10.colorRgb   = 0x606060;
        req10.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req10.alignment  = TEXT_ALIGNMENT_RIGHT;
        req10.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req10, textItoaUnsigned(buf, cfg->bp));
    }
    y -= step;
    if (arg0->killCountdown >= 0x33) {
        req11.x          = obj->panel.contentOriginX.unsignedValue + col;
        req11.y          = obj->panel.contentOriginY.unsignedValue + y;
        req11.otIndex    = obj->panel.otIndex.signedValue + 1;
        req11.colorRgb   = 0x606060;
        req11.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        req11.alignment  = TEXT_ALIGNMENT_RIGHT;
        req11.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&req11, textItoaUnsigned(buf, cfg->exp));
    }

    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}
