#include "common.h"

#include <psyq/libapi.h>

#include "main/unknown_syms.h"
#include "main/cdaudio.h"
#include "main/fs.h"
#include "main/task.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "main/devkit.h"
#include "weapons/weapon.h"
#include "actors/actor.h"
#include "kyle/kyle.h"
#include "aya/aya.h"
#include "rooms/room.h"

static void           AudioTick_Process(void);
static AudioTickNode* AudioTick_Remove(AudioTickNode* arg0);
static void           AudioTick_Reset(void);
static void           SndHeap_Reset(void);
static void           Snd_ClearBanks(void);
static long           Spu_TimerCallback(void);
static s32            Spu_TimerReentryWork(void);

TaskDesc D_80067828[] = {
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, func_807257A0 },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0x70, Gp_EffAttachTask37 },
    { 0x0, 0x80, func_800E70AC },
    { 0x0, 0xC0, func_acropolis_bridge_8017F788 },
    { 0x0, 0xC0, func_acropolis_security_room_8017ED68 },
    { 0x0, 0xC0, func_acropolis_security_room_80180294 },
    { 0x0, 0xC0, Gp_PadHoldTask },
    { 0x0, 0xC0, Gp_PadLerpTask },
    { 0x0, 0xC0, Gp_Script18Task },
    { 0x0, 0xC0, func_acropolis_fountain_8017DCD4 },
    { 0x0, 0xC0, func_acropolis_helicopter_landing_pad_8017EF8C },
    { 0x1, 0x60, Gp_EffAttachTask37 },
};

TaskDesc D_800678F4[] = {
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, Gp_PlayerWorkTask, { &D_aya_10400_8011B078 } },
    { 0x1, 0x50, Gp_PlayerWorkTask, { &D_aya_10300_8011ACE8 } },
    { 0x1, 0x50, Gp_PlayerWorkTask, { &D_aya_10200_8011C210 } },
    { 0x1, 0x50, Gp_PlayerWorkTask, { &D_aya_10500_8011C2E4 } },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_aya_10400_8011B4CC } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10400_8011B9BC } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10400_8011B4CC } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10400_8011BE10 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10300_8011B128 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10300_8011BA58 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10300_8011B568 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_10300_8011BE98 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_80115B90 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_801164C0 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_80115FD0 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_80116900 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_80115B90 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_801164C0 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_80115FD0 } },
    { 0x1, 0x50, func_8010B610, { &D_aya_80116900 } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_p08_8011D924 } },
    { 0x1, 0x50, func_8010B610, { &D_m93r_8011DA74 } },
    { 0x1, 0x50, func_8010B610, { &D_m950_8011DA9C } },
    { 0x1, 0x50, func_8010B610, { &D_p08_8011D924 } },
    { 0x1, 0x50, func_8010B610, { &D_p229_8011E5E4 } },
    { 0x1, 0x50, func_8010B610, { &D_unused_85_8011D53C } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_mongoose_8011D934 } },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_grenade_pistol_8011E28C } },
    { 0x1, 0x50, func_8010B610, { &D_mm1_8011E494 } },
    { 0x1, 0x50, func_8010B610, { &D_pa3_8011DA04 } },
    { 0x1, 0x50, func_8010B610, { &D_sp12_8011DB44 } },
    { 0x1, 0x50, func_8010B610, { &D_as12_8011DCF0 } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_8011DEC4 } },
    { 0x1, 0x50, func_8010B610, { &D_m249_8011DE74 } },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_tonfa_baton_8011DB98, { &D_tonfa_baton_8011E460 } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_8011DEC4 } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_8011DEC4 } },
    { 0x1, 0x50, func_hypervelocity_8011F6C0, { &D_hypervelocity_801202F8 } },
    { 0x1, 0x50, func_8010B610, { &D_gunblade_8011EEB0 } },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_hammer_8011F778 } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_bayonet_8011E9FC } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_grenade_8011EA2C } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_pyke_8011F56C } },
    { 0x1, 0x50, func_8010B610, { &D_m4a1_javelin_8012071C } },
    { 0x1, 0x50, func_8010B610, { &D_mp5a5_8011EAFC } },
    { 0x1, 0x50, func_8010B610, { &D_mp5a5_8011EAFC } },
    { 0x1, 0x50, func_8010B610, { &D_mp5a5_8011EAFC } },
    { 0x1, 0x52, func_m4a1_grenade_8011DE68, { &D_m4a1_grenade_8012E1FC } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x52, func_grenade_pistol_8011DBD0, { &D_grenade_pistol_8012B5A4 } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x52, func_mm1_8011DBD8, { &D_mm1_8012D444 } },
    { 0x1, 0x52, func_kyle_800102_801682B4, { &D_kyle_800102_801775A8 } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_tonfa_baton_8011DB98, { &D_tonfa_baton_8011E5EC } },
    { 0x1, 0x50, func_hypervelocity_8011F6C0, { &D_hypervelocity_801205E4 } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_hypervelocity_8011F6C0, { &D_hypervelocity_80120860 } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_actor_800100_80163CF0, { &D_kyle_8016C594 } },
    { 0x1, 0x50, func_actor_800100_80163CF0, { &D_kyle_800102_8016CE38 } },
    { 0x1, 0x50, func_actor_800100_80163CF0, { &D_kyle_8016C594 } },
    { 0x1, 0x50, func_actor_800100_80163CF0, { &D_kyle_8016C594 } },
    { 0x1, 0x50, func_actor_800200_801626EC, { &D_actor_800200_80169ECC } },
    { 0x1, 0x50, func_actor_800300_801625F4, { &D_actor_800300_8016885C } },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016C9E8 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016D32C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016CED8 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016D81C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800102_8016D28C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800102_8016DBD0 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800102_8016D77C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800102_8016E0C0 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016C9E8 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016D32C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016CED8 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016D81C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016C9E8 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016D32C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016CED8 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_8016D81C } },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800101_8016DC60 } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800102_8016E93C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800103_8016DE3C } },
    { 0x1, 0x50, func_8010B610, { &D_kyle_800104_8016E568 } },
    { 0x0, 0xC0, taskKill },
    { 0x1, 0x50, func_acropolis_cafeteria_80181E70, { &D_acropolis_cafeteria_801858C4 } },
    { 0x1, 0x50, func_acropolis_cafeteria_80181E70, { &D_acropolis_cafeteria_8018625C } },
    { 0x1, 0x50, func_acropolis_cafeteria_80181E70, { &D_acropolis_cafeteria_80186CAC } },
    { 0x1, 0x50, func_acropolis_cafeteria_80181E70, { &D_acropolis_cafeteria_80187518 } },
};

static u8 D_800680A4             = 0;
static u8 D58028_SpuTimerEnabled = 0;
/// Unreferenced.
static s32          D_800680A8   = 0;
s8                  D_800680AC[] = { 0, 1, 2, 3, 4, 7, 0xC, 0xD, -1, -1, -1, -1, -1, -1, 8, 0xA };
static u32          D_800680BC   = 0;
static volatile u32 D_800680C0   = 0;

static void Spu_InitSystem(s32 arg0)
{
    s32* temp_v0;

    D_800680C0 = 0;
    if (arg0 == 1) {
        goto wait_spu_transfer;
    }
    if (arg0 < 2) {
        if (arg0 == 0) {
            goto init_spu;
        }
        goto end;
    }
    if (arg0 == 2) {
        goto setup_events;
    }
    goto end;

init_spu:
    SpuInit();
    D58028_SpuTimerEnabled = false;
    goto unknown;

wait_spu_transfer:
    SpuIsTransferCompleted(1);

unknown:
    D_800680BC = 0;
    Spu_ResetCommonAttr();

setup_events:
    SndHeap_Reset();
    SndEvt_Reset();
    AsyncCb_Reset();
    Spu_ConfigReverb(3);
    Spu_InitVoices();
    Snd_ClearBanks();
    AudioTick_Reset();
    Snd_RegisterTickCallbacks();
    Snd_InitBanks(0);
    Midi_InitSystem(0);

    temp_v0  = SndHeap_Malloc(4);
    *temp_v0 = 0;

    AudioTick_Insert(&Snd_ReverbWarmupCb, 0, 0x8801, temp_v0);
    if (D58028_SpuTimerEnabled) {
        DisableEvent(D648E0_SpuTimerED);
        CloseEvent(D648E0_SpuTimerED);
        StopRCnt(RCntCNT0);
        D58028_SpuTimerEnabled = false;
    }

    if (gDisplayState.region == 1) {
        D_800680A4 = 0;
        D_8007E0CC = 0;
        SetRCnt(RCntCNT0, 0xffff, RCntMdINTR | RCntMdSC);
        ResetRCnt(RCntCNT0);
        StartRCnt(RCntCNT0);
        EnterCriticalSection();
        D648E0_SpuTimerED = OpenEvent(RCntCNT0, EvSpINT, EvMdINTR, Spu_TimerCallback);

        // HACK: What is this? The control flow of this function already
        // looks bad. To add insult to injury, This is the output that we
        // want:
        //
        // jal      OpenEvent
        // addiu    a3, a3, %lo(Spu_TimerCallback)
        // sw       v0, %lo(DE648E0_SpuTimerED)(s0)
        // jal      ExitCriticalSection
        // nop
        //
        // And this is the assembly that we get without this line:
        //
        // jal      OpenEvent
        // addiu    a3, a3, %lo(Spu_TimerCallback)
        // jal      ExitCriticalSection
        // sw       v0, %lo(DE648E0_SpuTimerED)(s0)
        //
        // Somehow the developers managed to insert the additional nop
        // instruction, and the only way I could think of is to insert
        // an empty assembler instruction. Maybe it has something to do with
        // the compiler/maspsx version, or rewriting the function with more
        // sensible control flow could fix it, but this matches.
        SOFT_BARRIER();

        ExitCriticalSection();
        EnableEvent(D648E0_SpuTimerED);
        D58028_SpuTimerEnabled = true;
    }
    D_800680A4 = 0;
    D_8007E0CC = 0;

end:
    D_800680C0 = 1;
}

SndBank* Snd_AllocBank(SndBankPayload* payload)
{
    u16      type;
    s8       temp;
    s32      slot;
    SndBank* bank;
    s32      size;
    s32      temp_v0;
    s32      temp_a0;
    s32      temp_a0_2;
    s32      shared;

    type = payload->field_4 & 0xF000;
    temp = D_800680AC[type >> 12];
    {
        register s32 p asm("a0");
        p = temp;
        if (temp == -1) {
            return NULL;
        }
        slot = p;
    }

    if (type == 0x4000) {
        slot = D_80082122 + 4;
    }

    if (type == 0xF000) {
        shared = D_8007E0D4;
        if (shared != 0) {
            bank            = &Snd_Banks[(s8)slot];
            bank->heapBlock = (void*)shared;
            goto setup_ptrs;
        }
    }

    bank = &Snd_Banks[(s8)slot];
    Snd_FreeBank(bank);

    size = ((payload->field_8 * 5) + payload->field_7) * 4 + (payload->field_7 * 2);

    switch (payload->field_4 & 0xF000) {
        case 0x2000:
            if (size < 0xCF) {
                size = 0xCE;
            }
            break;
        case 0xE000:
            if (size < 0x79) {
                size = 0x78;
            }
            break;
        case 0xF000:
            if (size < 0x583) {
                size = 0x582;
            }
            break;
    }

    temp_v0         = (s32)SndHeap_Malloc(size);
    bank->heapBlock = (void*)temp_v0;
    if (temp_v0 == 0) {
        return NULL;
    }

setup_ptrs:
    temp_a0          = (s32)bank->heapBlock;
    bank->groups     = (SndBankGroup*)temp_a0;
    temp_a0_2        = temp_a0 + (payload->field_7 * 4);
    bank->notes      = (SndNote*)temp_a0_2;
    bank->groupIndex = (u16*)(temp_a0_2 + (payload->field_8 * 0x14));
    return bank;
}

void Spu_Init(void)
{
    Spu_InitSystem(0);
}

void Spu_WaitDma(void)
{
    Spu_InitSystem(1);
}

void Audio_IrqFrameWork(void)
{
    if (D_800680C0 != 0) {
        D_800680C0 = 0;
        func_8004E200();
        SndEvt_Process();
        AudioTick_Process();
        Spu_FlushVoiceUpdates();
        D_800680BC += 1;
        if (gDisplayState.region == 1) {
            D_8007E0CC = 6;
            ResetRCnt(RCntCNT0);
            D_800680A4 = 1;
        }
        D_800680C0 = 1;
    }
}

static void Snd_ClearBanks(void)
{
    s32      i;
    s32*     p;
    SndBank* ptr;
    u16      flag;

    p = (s32*)Snd_Banks;
    i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while ((u32)i < 0x80);

    flag = 0xFFFF;
    i    = 0xF;
    ptr  = Snd_Banks;
    ptr += 0xF;
    do {
        ptr->bankId = flag;
        i--;
        ptr--;
    } while (i >= 0);

    D_8007E0D4 = 0;
}

void Snd_FreeBank(SndBank* bank)
{
    if ((bank != NULL) && ((bank->bankId & 0xF000) != 0xF000)) {
        SndHeap_Free(bank->heapBlock);
        bank->heapBlock  = NULL;
        bank->groups     = NULL;
        bank->notes      = NULL;
        bank->groupIndex = NULL;
        bank->bankId     = 0xFFFF;
        bank->imageSize  = 0;
    }
}

SndBank* Snd_FindBank(u16 bankId)
{
    s32      i;
    SndBank* ptr;
    s32      id;

    if (bankId == 0xFFFF) {
        bankId = 0;
    }
    id = bankId;

    for (i = 0, ptr = Snd_Banks; i < 0x10; i++, ptr++) {
        if (ptr->bankId == id) {
            return ptr;
        }
    }
    return NULL;
}

void Snd_BuildGroupIndex(SndBank* bank)
{
    u16*          table;
    SndBankGroup* data;
    s32           i;
    u8            count;

    table = bank->groupIndex;
    if (table != NULL) {
        data   = bank->groups;
        *table = 0;
        count  = bank->groupCount;
        table++;
        i = count - 1;
        if (i > 0) {
            i = count - 2;
            if (i != -1) {
                do {
                    *table = table[-1] + data->noteCount;
                    data++;
                    i--;
                    table++;
                } while (i != -1);
            }
        }
    }
}

void LinInterp_Setup(LinInterp* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 temp;
    s32 limit;

    arg1 &= 0xFF;
    arg2 &= 0xFF;

    if (arg1 != arg2) {
        if (arg3 != 0) {
            goto setup;
        }
    }

    arg0->field_8 = 0;
    arg0->field_4 = 0;
    arg0->field_0 = 0;
    arg0->field_E = 0;
    return;

setup:
    limit         = 0xFFFF;
    arg0->field_8 = limit / arg3;
    temp          = arg2 - arg1;
    if (temp < 0) {
        arg0->field_C = -1;
        arg0->field_0 = limit;
        arg0->field_4 = 0;
    } else {
        arg0->field_C = 1;
        arg0->field_0 = 0;
        arg0->field_4 = limit;
    }
    arg0->field_E = 1;
}

s32 LinInterp_Apply(LinInterp* arg0, s32 arg1)
{
    s32 var_a1;

    var_a1 = arg1;
    if (arg0->field_E == 1) {
        if (arg0->field_0 == arg0->field_4) {
            arg0->field_8 = 0;
        }
        var_a1 = (s32)((u32)(var_a1 * arg0->field_0) / 65535);
    }
    return var_a1;
}

void LinInterp_Step(LinInterp* arg0)
{
    s32 step = arg0->field_8;

    if (step) {
        if (arg0->field_C < 0) {
            if ((u32)(arg0->field_4 + step) >= (u32)arg0->field_0) {
                arg0->field_0 = arg0->field_4;
            } else {
                arg0->field_0 = arg0->field_0 - step;
            }
        } else {
            arg0->field_0 = arg0->field_0 + step;
            if ((u32)arg0->field_0 >= (u32)arg0->field_4) {
                arg0->field_0 = arg0->field_4;
            }
        }
    }
}

void Spu_ApplyPanVolume(s16* arg0, s16 arg1, s32 arg2)
{
    s16 index;
    u32 left;
    u32 right;

    if (arg1 > 0) {
        index = arg1 - 1;
        if (arg1 >= 0x80) {
            index = 0x7E;
        }
    } else {
        index = 0;
    }

    left  = (u32)(arg2 * D_80068D78[index]) >> 0xC;
    right = (u32)(arg2 * D_80068D78[0x7E - index]) >> 0xC;

    if (!(CdVol_GetMixMode() & 0xFF)) {
        right = (u32)((left + right) * D_80068D78[0x3F]) >> 0xC;
        left  = right;
    }

    if (left < 0x4000U) {
        arg0[0] = (s16)left;
    } else {
        arg0[0] = 0x3FFF;
    }

    if (right < 0x4000U) {
        arg0[1] = (s16)right;
    } else {
        arg0[1] = 0x3FFF;
    }
}

s32 AudioTick_Insert(void* poll, u32 onRemove, u16 id, s32* arg)
{
    AudioTickNode* node;
    AudioTickNode* head;
    AudioTickNode* p;
    AudioTickNode* next;
    u16            id16;
    u16            key;

    id16 = id;
    key  = id16;
    head = &AudioTick_List;
    if (head == NULL) {
        return -1;
    }
    AudioTick_Enabled = 0;
    node              = SndHeap_Malloc(sizeof(AudioTickNode));
    if (node == NULL) {
        AudioTick_Enabled = 1;
        return -1;
    }
    node->poll     = (s32)poll;
    node->onRemove = onRemove;
    node->id       = id16;
    node->arg      = (s32)arg;
    node->prev     = 0;
    node->next     = 0;

    p = head;
    for (;;) {
        next = (AudioTickNode*)p->next;
        if (next == NULL) {
            p->next           = (s32)node;
            node->prev        = (s32)p;
            node->next        = 0;
            AudioTick_Enabled = 1;
            return 0;
        }
        if ((u16)next->id == key) {
            SndHeap_Free(node);
            AudioTick_Enabled = 1;
            return -2;
        }
        if (key < (u16)next->id) {
            node->next                      = (s32)next;
            AudioTick_Enabled               = 1;
            ((AudioTickNode*)p->next)->prev = (s32)node;
            p->next                         = (s32)node;
            node->prev                      = (s32)p;
            return 0;
        }
        p = next;
    }
}

static void SndHeap_Reset(void)
{
    SndHeap_Start              = (HeapBlockHeader*)SndHeap_Buffer;
    SndHeap_Start->size        = SNDHEAP_SIZE;
    SndHeap_Start->magic       = SNDHEAP_START_MAGIC;
    SndHeap_Start->isAllocated = false;
    SndHeap_Start->prev        = NULL;
    SndHeap_Start->next        = NULL;
}

void* SndHeap_Malloc(size_t size)
{
    // Simple first-fit allocator, using linked lists.
    // The allocator splits up the available space into variable-length blocks.
    // Each block starts with a header, which contains the total length of the
    // block in bytes (including the header), whether it is allocated, a magic
    // value, and pointers to the previous/next blocks. The header is followed
    // by a chunk of data, which is returned to the caller. The returned
    // pointer (and the block header) are aligned to 4 bytes.
    size_t           maxBlockSize;
    size_t           newBlockSize;
    size_t           allocSize;
    HeapBlockHeader* block;
    HeapBlockHeader* newBlock;

    // Start the search at the first header.
    maxBlockSize = 0;
    block        = SndHeap_Start;

    // Reserve additional space for the header and align to 4 bytes.
    allocSize = (size + sizeof(HeapBlockHeader) + 3) & ~3;

    // Find the first suitable block.
    if (block != NULL) {
        // If we use a normal while(...) loop GCC decides to allocate the
        // registers in the order `t0`, `t1`, `t2`. Instead the registers
        // must be allocated in the order `t1`, `t2`, `t0`. To achieve this
        // we manually place the constants in the correct registers.
        size_t          heapStart         = (size_t)&SndHeap_Buffer;
        register size_t heapEnd asm("t1") = heapStart + SNDHEAP_SIZE;
        register size_t magic asm("t2")   = SNDHEAP_MAGIC;

        // Actual loop body.
        do {
            // Check that the block is still in bounds of our heap.
            if ((size_t)block < heapStart || heapEnd < (size_t)block) {
                return NULL;
            }

            // Skip allocated blocks.
            if (block->isAllocated) {
                goto next;
            }

            // Does not do anything, but is in the assembly for some reason.
            if (maxBlockSize < block->size) {
                maxBlockSize = block->size;
            }

            // If we found a block that is big enough, we can allocate from it.
            if (block->size >= allocSize) {
                // We allocate by splitting the block in two such that:
                //
                // [ block    | byte 0 | ... | byte blockSize ]
                //
                // Turns into the following if there is enough space
                // for a new block:
                //
                // [ block    | byte 0 | ... | byte allocSize ]
                // [ newBlock | byte 0 | ... | byte restSize  ]
                //
                // Or otherwise into:
                //
                // [ block    | byte 0 | ... | byte allocSize ]
                // [            byte 0 | ... | byte restSize  ]
                newBlockSize = block->size - allocSize;
                newBlock     = (HeapBlockHeader*)((u8*)block + allocSize);

                // If there is enough space for a new block, we must link it
                // to the current block.
                if (sizeof(HeapBlockHeader) < newBlockSize) {
                    newBlock->size        = newBlockSize;
                    newBlock->magic       = magic;
                    newBlock->isAllocated = false;

                    if (block->next == NULL) {
                        newBlock->next = NULL;
                    } else {
                        block->next->prev = newBlock;
                        newBlock->next    = block->next;
                    }
                    block->next    = newBlock;
                    newBlock->prev = block;
                    block->size    = allocSize;
                }

                // The allocated data is located just after the header.
                block->isAllocated = true;
                return (u8*)(block + 1);
            }

        next:
            block = block->next;
        } while (block != NULL);
    }

    return NULL;
}

void SndHeap_Free(void* ptr)
{
    // This is the inverse of the allocation function. Given a pointer, that we
    // assume points to the start of the data region which was returned by the
    // allocation function, we insert it into the linked list of blocks. To
    // prevent fragmentation, we first try to merge neighboring blocks, if they
    // are not in use.
    size_t           heapStart;
    size_t           heapEnd;
    HeapBlockHeader* header;

    // If `ptr` is `NULL` we are done.
    if (ptr == NULL) {
        return;
    }

    // Safety check: Ensure that the pointer is contained in the heap region.
    // Otherwise we return.
    heapStart = (size_t)SndHeap_Buffer;
    if ((size_t)ptr < heapStart) {
        return;
    }

    heapEnd = heapStart + SNDHEAP_SIZE;
    if (heapEnd < (size_t)ptr) {
        return;
    }

    // As with the allocation, the data pointer is located directly after the
    // block header. For some reason, the original code first sets the
    // `isAllocated` flag to `false`, before checking the magic number.
    //
    // TODO: Maybe there same heap is reused with a block kind that is not
    // merged on free. Investigate!
    header              = ptr - sizeof(HeapBlockHeader);
    header->isAllocated = false;
    if (header->magic != SNDHEAP_MAGIC && header->magic != SNDHEAP_START_MAGIC) {
        return;
    }

    // If the preceding block is also free, we grow it to take up the
    // additional space of the current block and make it point to the
    // succeeding block. `header` will always point to the earliest block.
    if (header->prev != NULL && header->prev->isAllocated == false) {
        if (header->next != NULL) {
            header->next->prev = header->prev;
            header->prev       = header->prev;
        }
        header->prev->next  = header->next;
        header->prev->size += header->size;
        header              = header->prev;
    }

    // We do the same, in case the succeeding neighbor is also not in use.
    if (header->next != NULL && header->next->isAllocated == false) {
        if (header->next->next != NULL) {
            header->next->next->prev = header;
        }
        header->size += header->next->size;
        header->next  = header->next->next;
    }

    // This should not be required, since all headers already had the flag
    // set to `false`. Nevertheless, here it is.
    header->isAllocated = false;
}

static long Spu_TimerCallback(void)
{
    if (D_800680A4 != 0) {
        D_8007E0CC--;
        if (D_8007E0CC == 0) {
            D_800680A4 = 0;
            Spu_TimerReentryWork();
        }
    }
    return 0;
}

static s32 Spu_TimerReentryWork(void)
{
    if (D_800680C0 == 0) {
        return 0;
    }
    D_800680C0 = 0;
    func_8004E200();
    AudioTick_Process();
    Spu_FlushVoiceUpdates();
    D_800680C0  = 1;
    D_800680BC += 1;
    return 0;
}

static void AudioTick_Reset(void)
{
    AudioTick_List.poll     = 0;
    AudioTick_List.onRemove = 0;
    AudioTick_List.id       = 0;
    AudioTick_List.arg      = 0;
    AudioTick_List.next     = NULL;
    AudioTick_List.prev     = 0;
    AudioTick_Enabled       = 1;
}

static void AudioTick_Process(void)
{
    AudioTickNode* head;
    AudioTickNode* node;
    s32            (*callback)(s32);

    head = &AudioTick_List;
    if (AudioTick_Enabled != 0) {
        if (head != NULL) {
            node = (AudioTickNode*)head->next;
            while (1) {
                if (node == NULL) {
                    break;
                }
                callback = (s32 (*)(s32))node->poll;
                if (callback != NULL) {
                    if (callback(node->arg) == -1) {
                        node = AudioTick_Remove(node);
                        continue;
                    }
                }
                node = (AudioTickNode*)node->next;
            }
        }
    }
}

static AudioTickNode* AudioTick_Remove(AudioTickNode* arg0)
{
    void           (*callback)(void);
    AudioTickNode* head;
    AudioTickNode* prev;
    AudioTickNode* curr;

    head              = &AudioTick_List;
    callback          = (void (*)(void))arg0->onRemove;
    AudioTick_Enabled = 0;
    if (callback != NULL) {
        callback();
    }

    prev = head;
    if (prev->next != 0) {
        do {
            curr = (AudioTickNode*)prev->next;
            if ((u16)curr->id == (u16)arg0->id) {
                prev->next = arg0->next;
                if (arg0->next != 0) {
                    ((AudioTickNode*)arg0->next)->prev = (s32)prev;
                }
                AudioTick_Enabled = 1;
                return (AudioTickNode*)prev->next;
            }
            prev = curr;
        } while (prev->next != 0);
    }
    AudioTick_Enabled = 1;
    return NULL;
}
