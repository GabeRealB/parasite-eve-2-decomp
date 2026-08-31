#include "common.h"

#include "actors/actor_103800.h"

void Actor03800_Fn00974(Actor103800* arg0);
void Actor03800_Fn00A98(Actor103800* arg0);
void Actor03800_Fn026F8(Actor103800* arg0);
void Actor03800_Fn02848(Actor103800* arg0);
void Actor03800_Fn03594(Actor103800* arg0);
void Actor03800_Fn03628(Actor103800* arg0);
void Actor03800_Fn036EC(Actor103800* arg0);
void Actor03800_Fn03744(Actor103800* arg0);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn000B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0010C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L001CC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L002C0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L002DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L002F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00300);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00310);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0031C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00320);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00330);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00398);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn003B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00434);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00450);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004A0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L004F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00718);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00938);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00944);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0095C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn00974);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L009D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L009F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A58);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00A80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn00A98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B2C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B6C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B70);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00B80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00BA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00BE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00BFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C34);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00C6C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D88);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00D8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00DCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E44);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E54);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00E74);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EA0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EC0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EE0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00EF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F18);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00F7C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L00FF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01030);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01034);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0109C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01100);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01150);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01178);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L011D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L011F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01260);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01274);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01278);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L012A4);

void Actor03800_L012AC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn012B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L012DC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0131C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01334);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01398);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01410);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01424);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0148C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01498);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L014F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L014FC);

void Actor03800_L01518(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01520);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01560);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01570);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L015D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L015D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01630);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn0166C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L016D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L016E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01754);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01800);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01808);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0185C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L018B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L018C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L018E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01910);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01914);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01918);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01948);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01980);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L019B4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L019C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L019D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01A40);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01A8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01A98);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01AB0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01ABC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01AD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01B24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01B60);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01BB4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01BF8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C10);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C30);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C3C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01C50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01C9C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01CBC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01CDC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01CF4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01D08);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01EA8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01ED4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn01EEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01F14);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01F94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01FEC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L01FF0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02014);

void Actor03800_L02060(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02068);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02090);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L020F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02110);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02168);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0216C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02190);

void Actor03800_L021DC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn021E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02258);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02280);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02310);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02324);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L024EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02504);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02524);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0254C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02550);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02584);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L025B8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L025D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0261C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02654);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02658);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02664);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02684);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026C4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L026F0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn026F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02764);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0278C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0279C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L027A4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L027C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L027D8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L027E4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L027F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L027FC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02848);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0288C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L028E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L028E8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02998);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L029F8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02A1C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02A24);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02A4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02A68);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02A8C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02A94);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02B38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02B78);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02BA4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02BB8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02BD0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02C5C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02C88);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02CAC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02CCC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02D0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02D38);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02D4C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02D64);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02D90);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02DAC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02DC0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02E00);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02E04);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02E2C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn02E50);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02EFC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02F0C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02F68);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02F80);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L02FE8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03008);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03044);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03054);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03064);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03074);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03084);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03090);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03094);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03148);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn0315C);

void Actor03800_Fn031B8(Actor103800Ctx* arg0, Actor103800* arg1)
{
    Actor103800Work* work;
    s32              state;
    s32              one;

    state = D_801153F4;
    one   = 1;
    work  = arg1->field_1C;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg1->field_2C->field_C = 0;
    arg0->field_14          = 0;
    goto default_body;
case2:
    arg1->field_2C->field_C = 0x80;
    arg0->field_14          = one;
    return;
default_body:
    if (arg0->field_4C != 0) {
        Actor03800_Fn00974(arg1);
    }
    Actor03800_Fn00A98(arg1);
    Actor03800_Fn032D8(arg1);
    if (work->field_360 != 0) {
        Actor03800_Fn026F8(arg1);
    }
    Actor03800_Fn02848(arg1);
    if (work->field_36A != 0) {
        Actor03800_Fn03594(arg1);
    }
    Actor03800_Fn03628(arg1);
    work->field_344->flg = 0;
    Gp_UpdateCoord(work->field_344);
case1:
    Actor03800_Fn036EC(arg1);
    Actor03800_Fn03744(arg1);
}

void Actor03800_Fn01150(Actor103800* arg0);
void Actor03800_Fn012B4(Actor103800* arg0);
void Actor03800_Fn01520(Actor103800* arg0);
void Actor03800_Fn0166C(Actor103800* arg0);
void Actor03800_Fn03420(Actor103800* arg0);
void Actor03800_Fn01948(Actor103800* arg0);
void Actor03800_Fn01AD0(Actor103800* arg0);
void Actor03800_Fn01C50(Actor103800* arg0);
void Actor03800_Fn01EEC(Actor103800* arg0);
void Actor03800_Fn02068(Actor103800* arg0);
void Actor03800_Fn021E4(Actor103800* arg0);
void Actor03800_Fn034B0(Actor103800* arg0);
void Actor03800_Fn02584(Actor103800* arg0);

void Actor03800_Fn032D8(Actor103800* arg0)
{
    Actor103800Work* work;
    s16              state;
    s16              mag;

    work  = arg0->field_1C;
    state = work->field_352;
    switch (state) {
        case 0:
            Actor03800_Fn01150(arg0);
            break;
        case 1:
            Actor03800_Fn012B4(arg0);
            break;
        case 2:
            Actor03800_Fn01520(arg0);
            break;
        case 3:
            Actor03800_Fn0166C(arg0);
            break;
        case 4:
            Actor03800_Fn03420(arg0);
            break;
        case 5:
            Actor03800_Fn01948(arg0);
            break;
        case 6:
            Actor03800_Fn01AD0(arg0);
            break;
        case 7:
            Actor03800_Fn01C50(arg0);
            break;
        case 8:
            Actor03800_Fn01EEC(arg0);
            break;
        case 9:
            Actor03800_Fn02068(arg0);
            break;
        case 10:
            Actor03800_Fn021E4(arg0);
            break;
        case 11:
            Actor03800_Fn034B0(arg0);
            break;
        case 12:
            Actor03800_Fn02584(arg0);
            break;
    }
    if (work->field_36E == 0) {
        work->field_21E = -0xFA;
        mag             = 0xFA;
    } else {
        work->field_21E = -0x15E;
        mag             = 0x15E;
    }
    work->field_228 = mag;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03420);

void Actor03800_L03440(void)
{
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03448);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0345C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03470);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03498);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn034B0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L034F4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03508);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03514);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03560);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03584);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03594);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03614);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03628);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0367C);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L036A8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L036BC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L036D4);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn036EC);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn03744);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L03794);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L037C8);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L037D0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_Fn037E0);

INCLUDE_ASM("actors/nonmatchings/lib/actor_103800_text", Actor03800_L0382C);
