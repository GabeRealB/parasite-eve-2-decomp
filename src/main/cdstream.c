#include "common.h"

#include "main/mem.h"

#include <psyq/libapi.h>

#include "main/unknown_syms.h"
#include "main/cdaudio.h"
#include "main/cdstream.h"
#include "main/devkit.h"

static s32 D_800827E8;
/// Unreferenced.
static u8     D_800827F0[8];
static CdlLOC D_800827F8;
/// Unreferenced.
static u8                     D_80082800[8];
static volatile u16           D_80082808;
static volatile s32           D_8008280C;
static volatile u16           D_80082810;
static volatile s32           D_80082814;
static volatile CdStreamState CdStream_State;
static CdStreamChannels       CdStream_Channels;
static volatile CdReadyQueue  CdReady_Queue;

/* The second byte of D_80068B5C and of D_80068B64 is written as the first
 * symbol's address plus one. Declaring either pair as a struct or an array
 * compiles the byte as an offset from a base register, where the original
 * folds the whole address into the load and store. */

static void CdReady_ClearCallback(void);
static void CdReady_InstallCallback(CdlCB arg0);
static s32  CdStream_Flush(void);
static s32  CdStream_InitDisc(u32* arg0);
static void CdStream_MarkEnding(void);
static void CdStream_ReadyMts(s32 interrupt, u8* result);
static void CdStream_SpuIrqHandler(void);

static void CdStream_AbortPhase(u32* arg0);
static void CdStream_CleanupIrq(void);
static void CdStream_ClearReadySlot(void);
static void CdStream_Continue(void);
static void CdStream_FinishQueueEntry(u32* arg0);
static s32  func_80059EE0(CdReadyEntry* arg0);

static volatile s32 D_80068B54 = 0;
static volatile s32 D_80068B58 = 0;
static volatile u8  D_80068B5C = 0;
/// Unreferenced.
static u8          D_80068B5D = 0;
static u8          D_80068B5E = 0;
static volatile u8 D_80068B5F = 0;
static volatile u8 D_80068B60 = 0;
static volatile u8 D_80068B61 = 0;
static volatile u8 D_80068B62 = 0;
static volatile u8 D_80068B63 = 0;
static volatile u8 D_80068B64 = 0;
/// Unreferenced.
static u8          D_80068B65 = 0;
static u8          D_80068B66 = 0;
static volatile u8 D_80068B67 = 0;
/// Unreferenced.
static s16          D_80068B68   = 0;
static volatile s16 D_80068B6A   = 0;
static void*        D_80068B6C   = NULL;
static s32          D_80068B70   = 0;
static s32          D_80068B74   = 0;
static u16          D_80068B78   = 0;
TaskDesc            D_80068B7C[] = {
    { 0, 0xC0, taskKill },
    { 0, 0xC0, taskKill },
    { 0, 0xC0, taskKill },
    { 0, 0xC0, taskKill },
    { 0, 0xC0, func_80725BB8 },
};
u16 D_80068BB8[] = {
    0x0010,
    0x0010,
    0x0011,
    0x0013,
    0x0014,
    0x0015,
    0x0016,
    0x0017,
    0x0019,
    0x001A,
    0x001C,
    0x001E,
    0x0020,
    0x0021,
    0x0023,
    0x0026,
    0x0028,
    0x002A,
    0x002D,
    0x002F,
    0x0032,
    0x0035,
    0x0039,
    0x003C,
    0x0040,
    0x0043,
    0x0047,
    0x004C,
    0x0050,
    0x0055,
    0x005A,
    0x005F,
    0x0065,
    0x006B,
    0x0072,
    0x0078,
    0x0080,
    0x0087,
    0x008F,
    0x0098,
    0x00A1,
    0x00AA,
    0x00B5,
    0x00BF,
    0x00CB,
    0x00D7,
    0x00E4,
    0x00F1,
    0x0100,
    0x010F,
    0x011F,
    0x0130,
    0x0142,
    0x0155,
    0x016A,
    0x017F,
    0x0196,
    0x01AE,
    0x01C8,
    0x01E3,
    0x0200,
    0x021E,
    0x023E,
    0x0260,
    0x0285,
    0x02AB,
    0x02D4,
    0x02FF,
    0x032C,
    0x035D,
    0x0390,
    0x03C6,
    0x0400,
    0x043C,
    0x047D,
    0x04C1,
    0x050A,
    0x0556,
    0x05A8,
    0x05FE,
    0x0659,
    0x06BA,
    0x0720,
    0x078D,
    0x0800,
    0x0879,
    0x08FA,
    0x0983,
    0x0A14,
    0x0AAD,
    0x0B50,
    0x0BFC,
    0x0CB2,
    0x0D74,
    0x0E41,
    0x0F1A,
};
u16 D_80068C78[] = {
    0x0400,
    0x0400,
    0x0400,
    0x0401,
    0x0401,
    0x0402,
    0x0402,
    0x0403,
    0x0403,
    0x0404,
    0x0404,
    0x0405,
    0x0405,
    0x0406,
    0x0406,
    0x0406,
    0x0407,
    0x0407,
    0x0408,
    0x0408,
    0x0409,
    0x0409,
    0x040A,
    0x040A,
    0x040B,
    0x040B,
    0x040C,
    0x040C,
    0x040D,
    0x040D,
    0x040D,
    0x040E,
    0x040E,
    0x040F,
    0x040F,
    0x0410,
    0x0410,
    0x0411,
    0x0411,
    0x0412,
    0x0412,
    0x0413,
    0x0413,
    0x0414,
    0x0414,
    0x0415,
    0x0415,
    0x0415,
    0x0416,
    0x0416,
    0x0417,
    0x0417,
    0x0418,
    0x0418,
    0x0419,
    0x0419,
    0x041A,
    0x041A,
    0x041B,
    0x041B,
    0x041C,
    0x041C,
    0x041D,
    0x041D,
    0x041E,
    0x041E,
    0x041E,
    0x041F,
    0x041F,
    0x0420,
    0x0420,
    0x0421,
    0x0421,
    0x0422,
    0x0422,
    0x0423,
    0x0423,
    0x0424,
    0x0424,
    0x0425,
    0x0425,
    0x0426,
    0x0426,
    0x0427,
    0x0427,
    0x0428,
    0x0428,
    0x0429,
    0x0429,
    0x0429,
    0x042A,
    0x042A,
    0x042B,
    0x042B,
    0x042C,
    0x042C,
    0x042D,
    0x042D,
    0x042E,
    0x042E,
    0x042F,
    0x042F,
    0x0430,
    0x0430,
    0x0431,
    0x0431,
    0x0432,
    0x0432,
    0x0433,
    0x0433,
    0x0434,
    0x0434,
    0x0435,
    0x0435,
    0x0436,
    0x0436,
    0x0437,
    0x0437,
    0x0438,
    0x0438,
    0x0438,
    0x0439,
    0x0439,
    0x043A,
    0x043A,
    0x043B,
    0x043B,
    0x043C,
};
u16 D_80068D78[] = {
    0x1000,
    0x0FFF,
    0x0FFE,
    0x0FFD,
    0x0FFA,
    0x0FF8,
    0x0FF4,
    0x0FF0,
    0x0FEB,
    0x0FE6,
    0x0FE0,
    0x0FD9,
    0x0FD2,
    0x0FCA,
    0x0FC1,
    0x0FB8,
    0x0FAE,
    0x0FA4,
    0x0F99,
    0x0F8D,
    0x0F81,
    0x0F74,
    0x0F66,
    0x0F58,
    0x0F4A,
    0x0F3A,
    0x0F2A,
    0x0F1A,
    0x0F08,
    0x0EF7,
    0x0EE4,
    0x0ED1,
    0x0EBE,
    0x0EAA,
    0x0E95,
    0x0E80,
    0x0E6A,
    0x0E53,
    0x0E3C,
    0x0E25,
    0x0E0D,
    0x0DF4,
    0x0DDB,
    0x0DC1,
    0x0DA7,
    0x0D8C,
    0x0D70,
    0x0D54,
    0x0D38,
    0x0D1B,
    0x0CFD,
    0x0CDF,
    0x0CC1,
    0x0CA1,
    0x0C82,
    0x0C62,
    0x0C41,
    0x0C20,
    0x0BFF,
    0x0BDD,
    0x0BBA,
    0x0B97,
    0x0B74,
    0x0B50,
    0x0B2B,
    0x0B07,
    0x0AE1,
    0x0ABC,
    0x0A96,
    0x0A6F,
    0x0A48,
    0x0A21,
    0x09F9,
    0x09D1,
    0x09A9,
    0x0980,
    0x0957,
    0x092D,
    0x0903,
    0x08D8,
    0x08AE,
    0x0883,
    0x0857,
    0x082C,
    0x0800,
    0x07D3,
    0x07A6,
    0x0779,
    0x074C,
    0x071F,
    0x06F1,
    0x06C3,
    0x0694,
    0x0665,
    0x0637,
    0x0607,
    0x05D8,
    0x05A8,
    0x0578,
    0x0548,
    0x0518,
    0x04E8,
    0x04B7,
    0x0486,
    0x0455,
    0x0424,
    0x03F2,
    0x03C1,
    0x038F,
    0x035D,
    0x032B,
    0x02F9,
    0x02C7,
    0x0294,
    0x0262,
    0x022F,
    0x01FD,
    0x01CA,
    0x0197,
    0x0164,
    0x0132,
    0x00FF,
    0x00CC,
    0x0099,
    0x0066,
    0x0033,
    0x0000,
    0x0000,
};
u16 D_80068E78[] = {
    0x0000,
    0x0001,
    0x0004,
    0x0009,
    0x0010,
    0x0019,
    0x0024,
    0x0031,
    0x0041,
    0x0052,
    0x0065,
    0x007A,
    0x0092,
    0x00AB,
    0x00C7,
    0x00E4,
    0x0104,
    0x0125,
    0x0149,
    0x016E,
    0x0196,
    0x01BF,
    0x01EB,
    0x0219,
    0x0249,
    0x027A,
    0x02AE,
    0x02E4,
    0x031C,
    0x0356,
    0x0392,
    0x03D0,
    0x0410,
    0x0452,
    0x0496,
    0x04DC,
    0x0524,
    0x056E,
    0x05BA,
    0x0608,
    0x0659,
    0x06AB,
    0x06FF,
    0x0756,
    0x07AE,
    0x0808,
    0x0865,
    0x08C3,
    0x0924,
    0x0986,
    0x09EB,
    0x0A51,
    0x0ABA,
    0x0B25,
    0x0B91,
    0x0C00,
    0x0C71,
    0x0CE4,
    0x0D58,
    0x0DCF,
    0x0E48,
    0x0EC3,
    0x0F40,
    0x0FBF,
    0x1040,
    0x10C3,
    0x1148,
    0x11CF,
    0x1258,
    0x12E3,
    0x1371,
    0x1400,
    0x1491,
    0x1524,
    0x15BA,
    0x1651,
    0x16EA,
    0x1786,
    0x1823,
    0x18C3,
    0x1964,
    0x1A08,
    0x1AAD,
    0x1B55,
    0x1BFF,
    0x1CAA,
    0x1D58,
    0x1E08,
    0x1EB9,
    0x1F6D,
    0x2023,
    0x20DB,
    0x2195,
    0x2251,
    0x230F,
    0x23CF,
    0x2491,
    0x2555,
    0x261B,
    0x26E3,
    0x27AD,
    0x2879,
    0x2947,
    0x2A18,
    0x2AEA,
    0x2BBE,
    0x2C94,
    0x2D6D,
    0x2E47,
    0x2F24,
    0x3002,
    0x30E3,
    0x31C5,
    0x32AA,
    0x3390,
    0x3479,
    0x3563,
    0x3650,
    0x373F,
    0x3830,
    0x3922,
    0x3A17,
    0x3B0E,
    0x3C07,
    0x3D02,
    0x3DFF,
    0x3EFE,
    0x3FFF,
};

static s32 CdReady_Enqueue(CdReadyEntry* arg0)
{
    u8                     saved;
    s32                    field2;
    s32                    next;
    u32                    flags;
    s32                    temp;
    volatile CdReadyEntry* entry;
    volatile CdReadyQueue* p;

    p                    = &CdReady_Queue;
    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    field2 = (s8)p->readIdx;
    next   = (s8)p->writeIdx;
    next   = next + 1;
    if (next >= 4) {
        next = 0;
    }

    if (next == field2) {
        CdReady_Queue.locked = saved;
        return 0;
    }

    entry                = &CdReady_Queue.entries[(s8)p->writeIdx];
    entry->pollFn        = arg0->pollFn;
    entry->sectorPos     = arg0->sectorPos;
    temp                 = arg0->doneFn;
    flags                = entry->flags;
    flags                = flags | 1;
    entry->doneFn        = temp;
    temp                 = ~4;
    flags                = flags & temp;
    flags                = flags & ~8;
    flags                = flags & ~0x1FE0;
    temp                 = arg0->errorFn;
    flags                = flags | 2;
    entry->flags         = flags;
    entry->errorFn       = temp;
    field2               = (s8)p->writeIdx;
    p->writeIdx          = next;
    CdReady_Queue.locked = saved;
    return field2 + 1;
}

static void CdReady_Poll(void)
{
    volatile CdReadyQueue* p;
    CdReadyEntry*          entry;
    u32                    flags;

    p = &CdReady_Queue;
    if (p->locked == 0 && p->writeIdx != p->readIdx) {
        entry = (CdReadyEntry*)&CdReady_Queue.entries[(s8)p->readIdx];
        flags = entry->flags;
        if (flags & 1) {
            if (((s32 (*)(CdReadyEntry*))entry->pollFn)(entry) != 0) {
                if (entry->doneFn != 0) {
                    ((void (*)(void))entry->doneFn)();
                }
                entry->flags &= ~1;
                entry->flags &= ~4;
                p->readIdx    = p->readIdx + 1;
                if ((s8)p->readIdx >= 4) {
                    p->readIdx = 0;
                }
            }
        } else if (((flags >> 2) & 1) && !((flags >> 1) & 1)) {
            if (entry->errorFn != 0) {
                ((void (*)(CdReadyEntry*))entry->errorFn)(entry);
            }
            if (!((entry->flags >> 3) & 1)) {
                goto advance;
            }
        } else {
        advance:
            entry->flags         &= ~4;
            CdReady_Queue.readIdx = CdReady_Queue.readIdx + 1;
            if ((s8)CdReady_Queue.readIdx >= 4) {
                CdReady_Queue.readIdx = 0;
            }
        }
    }
}

void CdStream_Start(CdStreamParams* arg0)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    s32                     flag;
    volatile CdStreamState* ap;
    volatile CdStreamState* a3;
    CdStreamChannel*        t0;
    CdStreamChannel*        ch1;
    s32                     sectors;
    s16                     pitch;
    u8                      saved;
    s16                     f6;
    s16                     idx;
    u32                     flags;
    CdReadyEntry*           e;
    s32                     one;
    s32                     cflags;
    s16                     vff;
    s16                     v1fc3;
    s16                     v1000;
    s32                     temp;
    u8                      mode;
    s32                     base;
    s32                     rem_tmp;
    s32                     temp_v1;

    p         = &CdStream_State;
    p->voiceL = arg0->voiceL;
    p->voiceR = arg0->voiceR;
    flag      = ((u8)CdStream_State.flags0 >> 4) & 1;
    if (flag == 1) {
        Spu_KeyOff((s8)p->voiceL);
        Spu_KeyOff((s8)p->voiceR);
        if (arg0->voiceFreeCb != 0) {
            ((void (*)(s32))arg0->voiceFreeCb)(
                (flag << (s8)p->voiceL) | (flag << (s8)p->voiceR));
        }
    }
    SpuSetIRQ(0);
    SpuSetIRQCallback(NULL);
    SpuSetTransferCallback(NULL);
    CdSyncCallback(NULL);

    ap                     = &CdStream_State;
    *(s32*)&CdStream_State = 0;
    if (ap->readySlot != 0) {
        a3    = (volatile CdStreamState*)&CdReady_Queue;
        f6    = ap->readySlot;
        saved = CdReady_Queue.locked;
        if (f6 != 0) {
            idx   = f6 - 1;
            e     = (CdReadyEntry*)&CdReady_Queue.entries[idx];
            flags = e->flags;
            if (flags & 1) {
                e->flags = (flags & ~1) | 4;
            }
            CdReady_Queue.locked = saved;
        }
        CdStream_State.readySlot = 0;
    }

    a3              = &CdStream_State;
    a3->startCb     = (void (*)(s32))arg0->startCb;
    a3->voiceFreeCb = (void (*)(s32))arg0->voiceFreeCb;
    a3->field_4     = 0;
    a3->doneCb      = (void (*)(s32))arg0->doneCb;
    a3->field_18    = 0;
    a3->startSector = arg0->startSector;
    a3->field_2C    = arg0->startSector;
    one             = 1;
    a3->field_30    = arg0->startSector;
    a3->field_34    = 0;
    a3->field_38    = one;
    base            = arg0->spuBase;
    sectors         = 0x18;
    {
        s32 ds      = gDisplayState.region;
        a3->spuBase = base;
        if (ds == one) {
            sectors = 0x14;
        }
    }
    a3->sectorsPerChunk = sectors;
    a3->ringHalf        = 0x2770;
    t0                  = (CdStreamChannel*)(a3 + 1);
    a3->sector          = (MtsSector*)arg0->sectorBuf;
    a3->voiceL          = arg0->voiceL;
    vff                 = 0xFF;
    a3->voiceR          = arg0->voiceR;
    mode                = arg0->mode;
    v1fc3               = 0x1FC3;
    v1000               = 0x1000;
    cflags              = 0x6009F;
    t0->attr            = cflags;
    t0[1].attr          = cflags;
    t0->field_C         = 0;
    t0->field_E         = 0;
    t0->field_14        = v1000;
    t0->field_3A        = vff;
    t0->field_3C        = v1fc3;
    t0[1].field_C       = 0;
    t0[1].field_E       = 0;
    t0[1].field_14      = v1000;
    a3->mode            = mode;
    a3->field_1C        = 0;
    a3->field_20        = 0;
    a3->pending         = 0;
    t0->voiceMask       = one << a3->voiceL;
    t0->spuAddr         = a3->spuBase;
    {
        s32          addr;
        register s32 mask asm("v1");
        addr            = a3->spuBase;
        mask            = a3->voiceR;
        addr            = addr + 0x10;
        mask            = one << mask;
        t0->spuAddr2    = addr;
        t0[1].voiceMask = mask;
    }
    temp          = a3->spuBase;
    temp          = temp + 0x40;
    temp          = temp + ((s32)((u16)a3->ringHalf << 16) >> 15);
    t0[1].spuAddr = temp;
    temp          = a3->spuBase;
    {
        s32 shift      = (s32)((u16)a3->ringHalf << 16) >> 15;
        t0[1].field_3A = vff;
        t0[1].field_3C = v1fc3;
        temp           = temp + shift;
    }
    {
        u8 f53         = a3->flags;
        temp           = temp + 0x50;
        t0[1].spuAddr2 = temp;
        if (f53 & 2) {
            ch1           = &t0[1];
            pitch         = (arg0->pitch * 0xB5) >> 8;
            ch1->pitchAlt = pitch;
            ch1->pitch    = pitch;
            t0->pitchAlt  = pitch;
            t0->pitch     = pitch;
        } else {
            u16 pitch_u;
            pitch_u        = (u16)arg0->pitch;
            t0->pitchAlt   = 0;
            t0[1].pitch    = 0;
            t0->pitch      = pitch_u;
            t0[1].pitchAlt = (u16)arg0->pitch;
        }
    }

    rem_tmp                  = (s32)&entry;
    entry.pollFn             = (s32)func_80059EE0;
    temp_v1                  = arg0->startSector;
    entry.doneFn             = (s32)CdStream_Continue;
    entry.errorFn            = (s32)CdStream_FinishQueueEntry;
    entry.sectorPos          = temp_v1;
    CdStream_State.readySlot = CdReady_Enqueue((CdReadyEntry*)rem_tmp);
    CdStream_State.phase     = 2;
    D_80068B74               = -1;
}

static void CdStream_Continue(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;

    CdStream_State.readySlot = 0;

    if ((CdStream_State.flags0 >> 2) & 1) {
        if (CdStream_State.field_1C == 0) {
            CdStream_State.phase  = 1;
            CdStream_State.flags1 = CdStream_State.flags1 | 1;
            CdStream_State.flags2 = CdStream_State.flags2 & 0xFD;
            CdStream_State.flags2 = CdStream_State.flags2 & 0xFB;
            if (CdStream_State.doneCb != NULL) {
                CdStream_State.doneCb(1);
            }
            return;
        }
    }

    p               = &CdStream_State;
    entry.pollFn    = (s32)func_80059EE0;
    entry.doneFn    = (s32)CdStream_Continue;
    p->flags2       = p->flags2 & 0xFD;
    entry.errorFn   = (s32)CdStream_FinishQueueEntry;
    entry.sectorPos = p->startSector;
    D_80068B63      = D_80068B63 + 1;
    p->readySlot    = CdReady_Enqueue(&entry);
}

void CdStream_Stop(void)
{
    u8                      saved;
    u8                      temp;
    s16                     arg0;
    s16                     idx;
    u32                     flags;
    CdReadyEntry*           entry;
    volatile CdStreamState* p;

    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    if (CdStream_State.flags0 & 1) {
        CdStream_State.flags0 = CdStream_State.flags0 | 0x40;
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
    } else {
        if (CdStream_State.readySlot != 0) {
            arg0 = CdStream_State.readySlot;
            temp = CdReady_Queue.locked;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
                flags = entry->flags;
                if (flags & 1) {
                    entry->flags = (flags & ~1) | 4;
                }
                CdReady_Queue.locked = temp;
            }
            CdStream_State.readySlot = 0;
        }

        p = &CdStream_State;
        if ((p->flags2 >> 3) & 1) {
            func_800B0118(0, 0);
            p->flags2 = p->flags2 & 0xF7;
        }
    }

    CdReady_Queue.locked = saved;
}

static void func_80058748(void);

static void CdStream_TeardownVoices(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    s32                     flag;
    s16                     arg0;
    s16                     idx;
    u32                     flags;
    CdReadyEntry*           e;
    u8                      saved;
    u8                      t;
    s32                     dead;
    s32                     rem_tmp;
    s32                     temp;

    p = &CdStream_State;
    if (p->flags2 & 1) {
        dead                  = p->field_20;
        t                     = p->flags2;
        t                     = t & 0xFE;
        p->flags2             = t;
        t                     = CdStream_State.flags0;
        t                     = t & 0xFE;
        CdStream_State.flags0 = t;
        p->flags2             = p->flags2 & 0xFD;
        CdStream_State.flags0 = CdStream_State.flags0 & 0xDF;
        flag                  = (CdStream_State.flags0 >> 4) & 1;
        if (flag == 1) {
            Spu_KeyOff((s8)p->voiceL);
            Spu_KeyOff((s8)p->voiceR);
            CdStream_State.flags0 = CdStream_State.flags0 & 0xEF;
            if (p->voiceFreeCb != NULL) {
                p->voiceFreeCb((flag << (s8)p->voiceL) | (flag << (s8)p->voiceR));
            }
        }
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        if (CdStream_State.readySlot != 0) {
            arg0  = CdStream_State.readySlot;
            saved = CdReady_Queue.locked;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                e     = (CdReadyEntry*)&CdReady_Queue.entries[idx];
                flags = e->flags;
                if (flags & 1) {
                    e->flags = (flags & ~1) | 4;
                }
                CdReady_Queue.locked = saved;
            }
            CdStream_State.readySlot = 0;
        }
        rem_tmp = (s32)&entry;
        SOFT_TOUCH_REG(rem_tmp);
        do {
            temp = (s32)func_80059EE0;
        } while (0);
        entry.pollFn          = temp;
        temp                  = (s32)func_80058748;
        p                     = &CdStream_State;
        entry.doneFn          = temp;
        entry.sectorPos       = p->field_30;
        CdStream_State.flags0 = CdStream_State.flags0 & 0xFB;
        entry.errorFn         = (s32)CdStream_FinishQueueEntry;
        p->readySlot          = CdReady_Enqueue((CdReadyEntry*)rem_tmp);
        p->phase              = 2;
    }
}

static void func_80058748(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    register s32            rem_tmp asm("a0");
    s32                     field18;
    register s32            temp asm("v0");
    s32                     rem;
    s32                     quot;

    p            = &CdStream_State;
    field18      = p->field_18;
    temp         = field18 / p->sectorsPerChunk;
    p->readySlot = 0;
    quot         = temp + 1;

    if ((CdStream_State.flags0 >> 2) & 1) {
        p->phase  = 1;
        p->flags1 = p->flags1 | 1;
        p->flags2 = p->flags2 & 0xFD;
        if (p->field_1C & 1) {
            p->field_4 = 0;
        } else {
            rem_tmp               = (u16)p->sectorsPerChunk;
            rem                   = field18 % p->sectorsPerChunk;
            rem_tmp               = rem_tmp - rem;
            rem_tmp               = rem_tmp + 1;
            p->field_4            = rem_tmp;
            CdStream_State.flags0 = CdStream_State.flags0 | 0x20;
        }
        CdStream_CleanupIrq();
    } else {
        do {
            temp = (s32)func_80059EE0;
        } while (0);
        rem             = p->flags2;
        entry.pollFn    = temp;
        temp            = (s32)func_80058748;
        rem             = rem & 0xFD;
        p->flags2       = rem;
        entry.doneFn    = temp;
        D_80068B63      = D_80068B63 + 1;
        temp            = (s32)CdStream_FinishQueueEntry;
        rem             = p->field_30;
        entry.errorFn   = temp;
        p->field_20     = quot;
        rem_tmp         = (s32)&entry;
        entry.sectorPos = rem;
        p->readySlot    = CdReady_Enqueue((CdReadyEntry*)rem_tmp);
    }
}

static void CdStream_CleanupIrq(void)
{
    volatile CdStreamState* p;
    u8                      temp;

    if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
    CdStream_State.flags0 = CdStream_State.flags0 & 0xF7;
    CdStream_State.flags0 = CdStream_State.flags0 & 0xBF;
    func_800B0118(0, 0);
    p                     = &CdStream_State;
    temp                  = p->flags2;
    p->flags2             = temp & 0xF7;
    D_80082808            = 0;
    CdStream_State.flags0 = CdStream_State.flags0 | 1;
}

static void func_8005896C(void)
{
    CdReadyEntry     entry;
    s32              stateOrPosition;
    s16              slot;
    u8               saved;
    CdReadyEntry*    queued;
    CdStreamChannel* channels;
    s32              nextChunk;
    u32              flags;

    stateOrPosition = CdStream_State.field_18;
    if (!(((u8)CdStream_State.flags0 >> 4) & 1) && (((u8)CdStream_State.flags0 >> 5) & 1)) {
        CdAudio_AllocVoices((s8*)&CdStream_State.voiceL, (s8*)&CdStream_State.voiceR);
        channels              = (CdStreamChannel*)(&CdStream_State + 1);
        channels->voiceMask   = (s32)(1 << (s8)CdStream_State.voiceL);
        channels[1].voiceMask = (s32)(1 << CdStream_State.voiceR);
        Spu_ArmKeyOn((s8)CdStream_State.voiceL);
        Spu_ArmKeyOn((s8)CdStream_State.voiceR);
        channels->attr       = 0x6009F;
        channels[1].attr     = 0x6009F;
        channels->field_3A   = 0xFF;
        channels->field_3C   = 0x1FC3;
        channels[1].field_3A = 0xFF;
        channels[1].field_3C = 0x1FC3;
        CdAudio_CopyVoiceData((s8)CdStream_State.voiceL, (s32*)channels);
        /* The channels follow CdStream_State; addressing them from its symbol
         * shares its high half, where &CdStream_Channels would load another. */
        CdAudio_CopyVoiceData((s8)CdStream_State.voiceR, (s32*)((CdStreamChannel*)(&CdStream_State + 1) + 1));
        CdStream_State.flags1 &= 0xFE;
        if (CdStream_State.startCb != NULL) {
            CdStream_State.startCb((1 << CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
        }
        CdStream_State.flags0 |= 0x10;
        CdStream_State.flags0 &= 0xDF;
    }
    stateOrPosition = (s32)&CdStream_State;
    if ((((volatile CdStreamState*)stateOrPosition)->field_20 + 1) < ((volatile CdStreamState*)stateOrPosition)->field_38) {
        if (((u8)((volatile CdStreamState*)stateOrPosition)->flags0 >> 3) & 1) {
            ((volatile CdStreamState*)stateOrPosition)->field_18 = ((volatile CdStreamState*)stateOrPosition)->field_1C * (s16)(u16)((volatile CdStreamState*)stateOrPosition)->sectorsPerChunk;
            stateOrPosition                                      = ((volatile CdStreamState*)stateOrPosition)->field_18;
        } else {
            stateOrPosition = ((volatile CdStreamState*)stateOrPosition)->field_18;
        }
        if (!(((u8)CdStream_State.flags0 >> 2) & 1)) {
            D_80068B5E = (u8)(D_80068B5E + 1);
        stopVoices:
            if (((u8)CdStream_State.flags0 >> 4) & 1) {
                Spu_KeyOff((u32)(s8)CdStream_State.voiceL);
                Spu_KeyOff((u32)(s8)CdStream_State.voiceR);
                if (CdStream_State.voiceFreeCb != NULL) {
                    CdStream_State.voiceFreeCb((1 << (s8)CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
                }
                CdStream_State.flags0 &= 0xEF;
            }
            if (CdStream_State.flags0 & 1) {
                if (D_80082808 == 0) {
                    D_80082808 = 6;
                }
                D_80082810             = D_80082808;
                CdStream_State.flags0 &= 0xFE;
                func_800B0118((s32)(s16)D_80082808, 0);
                D_80082808             = 0;
                CdStream_State.flags2 |= 8;
                CdStream_State.flags2 |= 1;
                CdStream_State.flags2 |= 4;
                CdStream_TeardownVoices();
            }
            CdStream_State.flags0 &= 0xDF;
            CdStream_State.flags0 &= 0xF7;
            return;
        }
        nextChunk                = stateOrPosition / CdStream_State.sectorsPerChunk;
        CdStream_State.field_34 += 1;
        nextChunk               += 1;
        if (nextChunk != CdStream_State.field_34) {
            D_80068B67             += 1;
            nextChunk              -= 1;
            CdStream_State.field_34 = nextChunk;
            CdStream_State.field_20 = nextChunk;
            CdStream_State.field_30 = CdStream_State.startSector + ((s8)(u8)CdStream_State.mtsPeriod * (s8)(u8)CdStream_State.mode * nextChunk);
            if (D_80082808 == 0) {
                D_80082808 = 0xD;
            }
            goto stopVoices;
        }
        if (nextChunk < CdStream_State.field_38) {
            entry.pollFn  = (s32)func_80059EE0;
            entry.doneFn  = (s32)CdStream_ClearReadySlot;
            entry.errorFn = (s32)CdStream_AbortPhase;
            if ((u16)CdStream_State.readySlot != 0) {
                slot  = CdStream_State.readySlot;
                saved = CdReady_Queue.locked;
                if (slot != 0) {
                    queued = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(slot - 1)];
                    flags  = queued->flags;
                    if (flags & 1) {
                        queued->flags = (flags & ~1) | 4;
                    }
                    CdReady_Queue.locked = saved;
                }
                CdStream_State.readySlot = 0;
            }
            CdStream_State.field_30 += (s8)(u8)CdStream_State.mtsPeriod * (s8)(u8)CdStream_State.mode;
            if (((u8)CdStream_State.flags1 >> 2) & 1) {
                CdStream_State.field_30 += (s8)CdStream_State.mtsParam;
            }
            entry.sectorPos          = CdStream_State.field_30;
            CdStream_State.field_20  = nextChunk;
            CdStream_State.flags0   &= 0xFB;
            CdStream_State.readySlot = CdReady_Enqueue(&entry);
            CdStream_State.phase     = 2;
        }
        CdStream_State.flags0 &= 0xF7;
    }
}

static void func_80058ED4(void)
{
    CdReadyEntry  entry;
    s32           position;
    u16           slot;
    u8            saved;
    CdReadyEntry* queued;
    u32           flags;

    position = CdStream_State.field_18;
    if ((CdStream_State.field_20 + 1) < CdStream_State.field_38) {
        if (((u8)CdStream_State.flags0 >> 3) & 1) {
            CdStream_State.field_4  = (u16)CdStream_State.sectorsPerChunk + 1;
            CdStream_State.field_18 = CdStream_State.field_1C * (s16)(u16)CdStream_State.sectorsPerChunk;
            position                = CdStream_State.field_18;
        } else {
            CdStream_State.field_4 = (u16)CdStream_State.sectorsPerChunk - 2;
            position               = CdStream_State.field_18;
        }
        if (!(((u8)CdStream_State.flags0 >> 2) & 1)) {
            D_80068B5E += 1;
        stopVoices:
            if (((u8)CdStream_State.flags0 >> 4) & 1) {
                Spu_KeyOff((u32)(s8)CdStream_State.voiceL);
                Spu_KeyOff((u32)(s8)CdStream_State.voiceR);
                if (CdStream_State.voiceFreeCb != NULL) {
                    CdStream_State.voiceFreeCb((1 << (s8)CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
                }
                CdStream_State.flags0 &= 0xEF;
            }
            CdStream_State.field_4 = 0;
            CdStream_State.flags0 &= 0xDF;
            if (CdStream_State.flags0 & 1) {
                if (D_80082808 == 0) {
                    D_80082808 = 6;
                }
                D_80082810             = D_80082808;
                CdStream_State.flags0 &= 0xFE;
                func_800B0118((s32)(s16)D_80082808, 0);
                D_80082808             = 0;
                CdStream_State.flags2 |= 8;
                CdStream_State.flags2 |= 1;
                CdStream_State.flags2 |= 4;
                if (((u8)CdStream_State.flags0 >> 3) & 1) {
                    CdStream_State.field_18 = CdStream_State.field_1C * (s16)(u16)CdStream_State.sectorsPerChunk;
                }
                CdStream_TeardownVoices();
            }
        } else {
            position                 = position / (s16)(u16)CdStream_State.sectorsPerChunk;
            CdStream_State.field_34 += 1;
            position                += 1;
            if (position != CdStream_State.field_34) {
                D_80068B67             += 1;
                position               -= 1;
                CdStream_State.field_34 = position;
                CdStream_State.field_20 = position;
                CdStream_State.field_30 = CdStream_State.startSector + ((s8)(u8)CdStream_State.mtsPeriod * (s8)(u8)CdStream_State.mode * position);
                if (D_80082808 == 0) {
                    D_80082808 = 0xD;
                }
                goto stopVoices;
            }
            if (position < CdStream_State.field_38) {
                entry.pollFn  = (s32)func_80059EE0;
                entry.doneFn  = (s32)CdStream_ClearReadySlot;
                entry.errorFn = (s32)CdStream_AbortPhase;
                if ((u16)CdStream_State.readySlot != 0) {
                    slot  = (u16)CdStream_State.readySlot;
                    saved = CdReady_Queue.locked;
                    if (slot != 0) {
                        queued = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(slot - 1)];
                        flags  = queued->flags;
                        if (flags & 1) {
                            queued->flags = (flags & ~1) | 4;
                        }
                        CdReady_Queue.locked = saved;
                    }
                    CdStream_State.readySlot = 0;
                }
                CdStream_State.field_30 += (s8)(u8)CdStream_State.mtsPeriod * (s8)(u8)CdStream_State.mode;
                if (((u8)CdStream_State.flags1 >> 2) & 1) {
                    CdStream_State.field_30 += (s8)CdStream_State.mtsParam;
                }
                entry.sectorPos          = CdStream_State.field_30;
                CdStream_State.field_20  = position;
                CdStream_State.flags0   &= 0xFB;
                CdStream_State.readySlot = CdReady_Enqueue(&entry);
                CdStream_State.phase     = 2;
            }
        }
        CdStream_State.flags0 &= 0xF7;
    }
}

void CdStream_Drive(void)
{
    CdReadyEntry*    restartEntry;
    CdReadyEntry*    stopEntry;
    CdReadyEntry*    endEntry;
    s32              position;
    u16              restartSlot, stopSlot, endSlot;
    u8               restartLock, stopLock, endLock;
    s32              phase;
    CdStreamChannel* channels;
    u32              restartFlags;
    u32              stopFlags;
    u32              endFlags;

    if (D_80068B58 == 0) {
        D_80068B58 = 1;
        if (CdStream_State.flags0 & 1) {
            if (((u8)CdStream_State.flags2 >> 1) & 1) {
                CdStream_State.flags0 &= 0xFE;
                func_800B0118((s32)(s16)D_80082808, 0);
                CdStream_State.flags2 |= 8;
                D_80082808             = 0;
                CdStream_State.flags2 |= 1;
                CdStream_TeardownVoices();
            } else {
                if (((u8)CdStream_State.flags1 >> 3) & 1) {
                    if (((u8)CdStream_State.flags0 >> 4) & 1) {
                        Spu_KeyOff((u32)(s8)CdStream_State.voiceL);
                        Spu_KeyOff((u32)(s8)CdStream_State.voiceR);
                        if (CdStream_State.voiceFreeCb != NULL) {
                            CdStream_State.voiceFreeCb((1 << CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
                        }
                        CdStream_State.flags0 &= 0xEF;
                    }
                    if ((u16)CdStream_State.readySlot != 0) {
                        restartSlot = (u16)CdStream_State.readySlot;
                        restartLock = CdReady_Queue.locked;
                        if (restartSlot != 0) {
                            restartEntry = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(restartSlot - 1)];
                            restartFlags = restartEntry->flags;
                            if (restartFlags & 1) {
                                restartEntry->flags = (restartFlags & ~1) | 4;
                            }
                            CdReady_Queue.locked = restartLock;
                        }
                        CdStream_State.readySlot = 0;
                    }
                    CdStream_State.flags0  &= 0xFB;
                    CdStream_State.flags0  &= 0xDF;
                    CdStream_State.field_4  = 0;
                    CdStream_State.field_18 = CdStream_State.field_14;
                    CdStream_State.flags1  &= 0xF7;
                    CdStream_State.flags0  |= 2;
                }
                if (((u8)CdStream_State.flags0 >> 6) & 1) {
                    if (((u8)CdStream_State.flags0 >> 4) & 1) {
                        Spu_KeyOff((u32)(s8)CdStream_State.voiceL);
                        Spu_KeyOff((u32)(s8)CdStream_State.voiceR);
                        if (CdStream_State.voiceFreeCb != NULL) {
                            CdStream_State.voiceFreeCb((1 << (s8)CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
                        }
                        CdStream_State.flags0 &= 0xEF;
                    }
                    CdStream_State.flags0 &= 0xFB;
                    CdStream_State.flags0 &= 0xDF;
                    CdStream_State.flags0 &= 0xFE;
                    if ((u16)CdStream_State.readySlot != 0) {
                        stopSlot = (u16)CdStream_State.readySlot;
                        stopLock = CdReady_Queue.locked;
                        if (stopSlot != 0) {
                            stopEntry = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(stopSlot - 1)];
                            stopFlags = stopEntry->flags;
                            if (stopFlags & 1) {
                                stopEntry->flags = (stopFlags & ~1) | 4;
                            }
                            CdReady_Queue.locked = stopLock;
                        }
                        CdStream_State.readySlot = 0;
                    }
                    if (((u8)CdStream_State.flags2 >> 3) & 1) {
                        func_800B0118(0, 0);
                        CdStream_State.flags2 &= 0xF7;
                    }
                    CdStream_State.flags0 &= 0xBF;
                    CdStream_State.flags2 |= 1;
                } else if (!(((u8)CdStream_State.flags0 >> 1) & 1)) {
                    phase = (s8)CdStream_State.phase;
                    if (phase == 1) {
                        CdStream_State.flags0 |= 2;
                        CdStream_State.field_4 = phase;
                        D_80068B60             = phase;
                    }
                } else {
                    position = CdStream_State.field_18;
                    if (position == 0) {
                        CdAudio_AllocVoices((s8*)&CdStream_State.voiceL, (s8*)&CdStream_State.voiceR);
                        channels            = (CdStreamChannel*)(&CdStream_State + 1);
                        channels->voiceMask = (s32)(1 << (s8)CdStream_State.voiceL);
                        // Preserve the channel base for the second voice mask.
                        SOFT_USE_REG(channels);
                        channels[1].voiceMask = (s32)(1 << CdStream_State.voiceR);
                        if (!(((u8)CdStream_State.flags0 >> 4) & 1)) {
                            Spu_ArmKeyOn((u32)(s8)CdStream_State.voiceL);
                            Spu_ArmKeyOn((u32)(s8)CdStream_State.voiceR);
                        }
                        if (CdStream_State.startCb != NULL) {
                            CdStream_State.startCb((1 << CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
                        }
                        CdStream_State.flags0 |= 0x10;
                        CdStream_State.flags1 |= 0x80;
                    }
                    if (CdStream_State.flags1 & 1) {
                        CdAudio_CopyVoiceData((s8)CdStream_State.voiceL, (s32*)(&CdStream_State + 1));
                        /* The channels follow CdStream_State; addressing them from its symbol
                         * shares its high half, where &CdStream_Channels would load another. */
                        CdAudio_CopyVoiceData((s8)CdStream_State.voiceR, (s32*)((CdStreamChannel*)(&CdStream_State + 1) + 1));
                        CdStream_State.flags1 &= 0xFE;
                    }
                    if ((position / (s16)(u16)CdStream_State.sectorsPerChunk >= CdStream_State.field_38 - 1) &&
                        ((((u8)CdStream_State.flags1 >> 4) & 1) ||
                         ((((u8)CdStream_State.flags1 >> 6) & 1) ? (position / (s16)(u16)CdStream_State.sectorsPerChunk >= CdStream_State.field_38) : (position % (s16)(u16)CdStream_State.sectorsPerChunk >= (CdStream_State.sectorsPerChunk >> 1))) ||
                         (position / (s16)(u16)CdStream_State.sectorsPerChunk >= CdStream_State.field_38))) {
                        if (((u8)CdStream_State.flags0 >> 4) & 1) {
                            Spu_KeyOff((u32)(s8)CdStream_State.voiceL);
                            Spu_KeyOff((u32)(s8)CdStream_State.voiceR);
                            if (CdStream_State.voiceFreeCb != NULL) {
                                CdStream_State.voiceFreeCb((1 << (s8)CdStream_State.voiceL) | (1 << CdStream_State.voiceR));
                            }
                            CdStream_State.flags0 &= 0xEF;
                        }
                        if ((u16)CdStream_State.readySlot != 0) {
                            endSlot = (u16)CdStream_State.readySlot;
                            endLock = CdReady_Queue.locked;
                            if (endSlot != 0) {
                                endEntry = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(endSlot - 1)];
                                endFlags = endEntry->flags;
                                if (endFlags & 1) {
                                    endEntry->flags = (endFlags & ~1) | 4;
                                }
                                CdReady_Queue.locked = endLock;
                            }
                            CdStream_State.readySlot = 0;
                        }
                        if (((u8)CdStream_State.flags2 >> 3) & 1) {
                            func_800B0118(0, 0);
                            CdStream_State.flags2 &= 0xF7;
                        }
                        if (D_80068B5C != 0) {
                            SpuSetIRQ(0);
                            SpuSetIRQCallback(NULL);
                            D_80068B5C = 0;
                        }
                        CdStream_State.flags0 &= 0xFB;
                        CdStream_State.flags0 &= 0xDF;
                        CdStream_State.flags0 &= 0xFE;
                    } else {
                        if (((u8)CdStream_State.flags2 >> 2) & 1) {
                            if (CdStream_State.field_1C & 1) {
                                if (D_80068B5C != 0) {
                                    SpuSetIRQ(0);
                                    SpuSetIRQCallback(NULL);
                                    D_80068B5C = 0;
                                }
                                func_80058ED4();
                                CdStream_State.field_4 += 2;
                            } else {
                                if (D_80068B5C != 0) {
                                    SpuSetIRQ(0);
                                    SpuSetIRQCallback(NULL);
                                    D_80068B5C = 0;
                                }
                                CdStream_State.field_4 = 0;
                                func_8005896C();
                            }
                            CdStream_State.flags2 &= 0xFB;
                        } else if (CdStream_State.field_4 != 0) {
                            CdStream_State.field_4 -= 1;
                            if ((s8)CdStream_State.field_4 < (CdStream_State.sectorsPerChunk >> 2)) {
                                if ((((u8)CdStream_State.flags0 >> 3) & 1) || (CdStream_State.field_4 == 0)) {
                                    if ((u8)D_80068B60 != 0) {
                                        D_80068B60 = 0;
                                    } else if (!(((u8)CdStream_State.flags0 >> 3) & 1)) {
                                        D_80068B61 += 1;
                                    }
                                    if (D_80068B5C != 0) {
                                        SpuSetIRQ(0);
                                        SpuSetIRQCallback(NULL);
                                        D_80068B5C = 0;
                                    }
                                    CdStream_State.field_4 = 0;
                                    func_8005896C();
                                }
                            }
                        } else if (((CdStream_State.field_18 % (CdStream_State.sectorsPerChunk * 2)) >= ((s16)(u16)CdStream_State.sectorsPerChunk - 4)) && ((((u8)CdStream_State.flags0 >> 3) & 1) || ((CdStream_State.field_18 % (CdStream_State.sectorsPerChunk * 2)) == ((s16)(u16)CdStream_State.sectorsPerChunk + 3)))) {
                            if (!(((u8)CdStream_State.flags0 >> 3) & 1)) {
                                D_80068B61 += 1;
                            }
                            if (D_80068B5C != 0) {
                                SpuSetIRQ(0);
                                SpuSetIRQCallback(NULL);
                                D_80068B5C = 0;
                            }
                            func_80058ED4();
                        }
                        if (CdStream_State.flags0 & 1) {
                            CdStream_State.field_18 += 1;
                        }
                    }
                }
            }
        }
        CdReady_Poll();
        D_80068B58 = 0;
    }
}

static s32 func_80059EE0(CdReadyEntry* arg0)
{
    struct {
        CdlLOC       loc;
        u8           pad[4];
        s8           mode[8];
        u8           result[8];
        AsyncCbEntry entry;
    } sp;
    s32 phase;
    s32 modeSync;
    s32 locationSync;
    s32 readSync;
    s32 pauseSync;
    u32 flags;
    u32 modeFlags;
    u32 irqAddr1;
    u32 irqAddr2;
    u32 irqOffset;
    u32 initFlags;
    u32 errorCode;

    flags = arg0->flags;
    if ((flags >> 1) & 1) {
        arg0->flags = flags & ~2;
        D_800827E8  = 0;
        if (!(CdStream_State.flags & 1) && CdStream_State.pending == 0) {
            if ((CdStream_State.flags1 >> 1) & 1) {
                initFlags   = arg0->flags & ~0x10;
                initFlags  &= ~0x1FE0;
                arg0->flags = initFlags | 0x80;
            } else {
                if (!(CdMode() & 0x80)) {
                    arg0->flags |= 0x10;
                } else {
                    arg0->flags &= ~0x10;
                }
                arg0->flags = (*(volatile u32*)&arg0->flags & ~0x1FE0) | 0x20;
            }
        } else {
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x80;
            if (CdStream_State.pending != 0) {
                AsyncCb_Cancel((s16)CdStream_State.pending);
            }
            sp.entry.field_8       = (s32 (*)(AsyncCbEntry*))CdStream_InitDisc;
            sp.entry.field_C       = (void (*)(AsyncCbEntry*))CdStream_MarkEnding;
            sp.entry.field_10      = (s32 (*)(AsyncCbEntry*))CdStream_Flush;
            CdStream_State.pending = func_8004DE18(&sp.entry);
        }
    }
    D_8008280C = (arg0->flags >> 5) & 0xFF;
    if (CdStream_State.pending != 0) {
        return 0;
    }
    phase = (arg0->flags >> 5) & 0xFF;
    switch (phase) {
        case 1:
            sp.mode[0] = -0x60;
        retry_mode:
            CdControlF(CdlSetmode, (u8*)&sp.mode[0]);
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x40;
            goto phase_advanced;
        case 2:
            modeSync = CdSync(1, &sp.result[0]);
            if (modeSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)modeSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    /* Keep each disk-error path separate before the shared counter update. */
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)modeSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_800827E8++ < 0x258) {
                    goto retry_mode;
                }
                goto stream_error;
            }
            CdStream_State.flags1 |= 2;
            modeFlags              = arg0->flags;
            if (!((modeFlags >> 4) & 1)) {
                arg0->flags = ((u32)~0x1FE0 & modeFlags) | 0x80;
            } else {
                arg0->flags = (modeFlags & ~0x1FE0) | 0x60;
                D_800827E8  = 3;
                case 3:
                    if (--D_800827E8 != 0) {
                        goto phase_advanced;
                    }
                    arg0->flags = (arg0->flags & ~0x1FE0) | 0x80;
            }
            D_800827E8 = 0;
        case 4:
            D_80068B54              = 0;
            CdStream_State.field_4C = 5;
            CdStream_State.field_2C = arg0->sectorPos;
        set_location:
            CdIntToPos(arg0->sectorPos, &sp.loc);
            CdControlF(CdlSetloc, &sp.loc.minute);
            D_800827E8  = 0;
            arg0->flags = (arg0->flags & ~0x1FE0) | 0xA0;
        case 5:
            locationSync = CdSync(1, &sp.result[0]);
            if (locationSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)locationSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)locationSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_800827E8++ < 0x258) {
                    goto set_location;
                }
                goto stream_error;
            }
            if (locationSync != CdlComplete) {
                goto wait_for_progress;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0xC0;
        case 6:
            D_800827E8 = 0;
        start_read:
            CdReady_InstallCallback((CdlCB)CdStream_ReadyMts);
            CdStream_State.remaining = 0;
            CdStream_State.field_4C  = 1;
            CdControlF(CdlReadN, NULL);
            arg0->flags = (arg0->flags & ~0x1FE0) | 0xE0;
            goto phase_advanced;
        case 7:
            D_80068B54 = 0;
            readSync   = CdSync(1, &sp.result[0]);
            if (readSync == CdlDiskError) {
                CdReady_ClearCallback();
                arg0->flags = (arg0->flags & ~0x1FE0) | 0xC0;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)readSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)readSync;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_800827E8++ < 0x258) {
                    goto start_read;
                }
                goto stream_error;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x100;
        case 8:
            if (CdStream_State.field_4C == 1) {
                goto wait_for_progress;
            }
            if (CdStream_State.field_4C == 0) {
                goto wait_for_progress;
            }
            if (CdStream_State.field_4C == 4) {
                CdStream_State.flags0 |= 4;
                CdReady_ClearCallback();
                if (CdStream_State.field_1C & 1) {
                    irqOffset = ((CdStream_State.ringHalf + 0x3F) & ~0x3F);
                    irqAddr1  = CdStream_State.spuBase + irqOffset;
                    if (D_80068B5C != 0) {
                        SpuSetIRQ(0);
                        SpuSetIRQCallback(NULL);
                    }
                    D_80068B5C = 1;
                    SpuSetIRQCallback(CdStream_SpuIrqHandler);
                    SpuSetIRQAddr(irqAddr1);
                    SpuSetIRQ(1);
                } else if (!((CdStream_State.flags0 >> 4) & 1)) {
                    CdStream_State.flags0 |= 0x20;
                } else if (CdStream_State.field_1C != 0) {
                    irqAddr2 = CdStream_State.spuBase + 0x40;
                    if (D_80068B5C != 0) {
                        SpuSetIRQ(0);
                        SpuSetIRQCallback(NULL);
                    }
                    D_80068B5C = 1;
                    SpuSetIRQCallback(CdStream_SpuIrqHandler);
                    SpuSetIRQAddr(irqAddr2);
                    SpuSetIRQ(1);
                }
            } else if (CdStream_State.field_4C != 2) {
                if (CdStatus() & CdlStatShellOpen) {
                    if (D_80082808 == 0) {
                        D_80082808 = 5;
                    }
                    D_80082810            = D_80082808;
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    goto stream_error;
                }
                if (D_80082808 == 0) {
                    D_80082808 = 9;
                }
                CdStream_State.flags2 |= 2;
                CdStream_State.flags0 &= 0xFB;
            } else {
                goto wait_for_progress;
            }
            D_800827E8              = 0;
            CdStream_State.field_4C = 0;
            CdReady_ClearCallback();
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x120;
        case 9:
            D_800827E8 = 0;
        pause_read:
            CdControlF(CdlPause, NULL);
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x160;
            goto phase_advanced;
        case 11:
            D_80068B54 = 0;
            pauseSync  = CdSync(1, &sp.result[0]);
            if (pauseSync == CdlDiskError) {
                D_80068B64 += 1;
                if ((sp.result[0] & CdlStatShellOpen) || (CdStatus() & CdlStatShellOpen)) {
                    CdStream_State.flags |= 1;
                    (*((volatile u8*)&D_80068B5C + 1))++;
                    if (D_80082808 == 0) {
                        D_80082808 = (u16)pauseSync;
                    }
                    D_80082810 = D_80082808;
                    goto stream_error;
                }
                arg0->flags = (arg0->flags & ~0x1FE0) | 0x120;
                if (D_800827E8 >= 0x258) {
                    goto stream_error;
                }
                D_800827E8 += 1;
                goto pause_read;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x180;
        case 12:
            if (SpuIsTransferCompleted(0) == 0) {
                goto wait_for_progress;
            }
            arg0->flags = (arg0->flags & ~0x1FE0) | 0x140;
        case 10:
            if (CdSync(1, &sp.result[0]) == 0) {
                goto wait_for_progress;
            }
            D_8008280C = 0;
            return 1;
    }
phase_advanced:
    D_80082814 = (arg0->flags >> 5) & 0xFF;
    return 0;
wait_for_progress:
    D_80082814 = (arg0->flags >> 5) & 0xFF;
    if (++D_800827E8 < 0x259) {
        return 0;
    }
stream_error:
    CdStream_State.flags2 |= 2;
    if (D_80082808 == 0) {
        errorCode = (arg0->flags >> 5) & 0xFF;
        /* Keep the phase extraction separate from the error-code shift. */
        SOFT_USE_REG(errorCode);
        errorCode *= 0x10;
        D_80082808 = errorCode | 0xA;
    }
    CdStream_State.field_4C = 0;
    CdReady_ClearCallback();
    CdFlush();
    return 1;
}

static void CdStream_ReadyMts(s32 interrupt, u8* result)
{
    volatile CdStreamState* state;
    s16                     chunkSectors;
    s32                     intr;
    s32                     skipIndex;
    s32                     skipStamp;
    s32                     timer;
    s32                     sectorPos;
    s32                     attributes;
    s32                     writeSize;
    u32                     timerPhase;
    u32                     previousPhase;
    volatile CdStreamState* channelState;
    volatile CdStreamState* regionState;
    CdStreamChannels*       channels;
    s32                     channelBase;
    s32                     channelCount;

    if ((u16)D_80068B6A != 0) {
        D_80082808             = 0xC;
        D_80082810             = D_80082808;
        CdStream_State.flags2 |= 2;
        return;
    }
    D_80068B6A = 1;
    intr       = interrupt & 0xFF;
    if (intr == 1) {
        if ((s16)(u16)CdStream_State.field_4C != 2) {
            if ((s16)(u16)CdStream_State.field_4C == intr) {
                if (SpuIsTransferCompleted(0) == 0) {
                    D_80068B62 += 1;
                    ResetRCnt(0xF2000002U);
                    previousPhase = 0;
                    while (1) {
                        timer = GetRCnt(0xF2000002U);
                        if ((u32)(timer & 0xFFFF) < 0x52B0U) {
                            timerPhase = timer & 0x7F;
                            if (timerPhase < previousPhase) {
                                previousPhase = timerPhase;
                                if (SpuIsTransferCompleted(0) != 0) {
                                    previousPhase = timerPhase;
                                    goto read_header;
                                }
                            }
                            previousPhase = timerPhase;
                        } else {
                            break;
                        }
                    }
                    D_80082808 = 4;
                    D_80082810 = D_80082808;
                    goto check_status;
                }
                goto read_header;
            }
        } else {
        read_header:
            if (CdGetSector(&D_800827F8, 3) == 0) {
                *((volatile u8*)&D_80068B64 + 1) = (u8)(*((volatile u8*)&D_80068B64 + 1) + 1);
                if (D_80082808 == 0) {
                    D_80082808 = 2;
                }
                D_80082810 = D_80082808;
                goto check_status;
            }
            sectorPos = CdPosToInt(&D_800827F8);
            if (sectorPos != CdStream_State.field_2C) {
                if (sectorPos < CdStream_State.field_2C) {
                    if (CdStream_State.field_2C >= (sectorPos + 4)) {
                        goto sector_mismatch;
                    }
                } else {
                sector_mismatch:
                    D_80068B5F += 1;
                    if (D_80082808 == 0) {
                        D_80082808 = 3;
                    }
                    D_80082810 = D_80082808;
                    goto check_status;
                }
            } else {
                CdStream_State.field_2C += 1;
                if ((s16)(u16)CdStream_State.field_4C == 2) {
                    skipIndex = D_80068B78++ & 0xFF;
                    skipStamp = skipIndex | ((CdStream_State.field_1C << 8) & 0xFFFF00);
                    if (D_80068B74 < skipStamp) {
                        if ((((s32 (*)(s32, s32))func_800AF590)(0, 0) << 0x10) == 0) {
                            *(volatile s32*)&D_80068B74 = skipStamp;
                            goto advance_countdown;
                        }
                        goto check_status;
                    }
                    CdGetSector(CdStream_State.sector, 0x200);
                advance_countdown:
                    state                    = &CdStream_State;
                    CdStream_State.countdown = (u16)CdStream_State.countdown - 1;
                    if ((u16)CdStream_State.countdown == 0) {
                        CdStream_State.field_4C = 4;
                    }
                } else {
                    if (CdGetSector(CdStream_State.sector, 0x200) == 0) {
                        *((volatile u8*)&D_80068B64 + 1) = (u8)(*((volatile u8*)&D_80068B64 + 1) + 1);
                        if (D_80082808 == 0) {
                            D_80082808 = 2;
                        }
                        D_80082810 = D_80082808;
                        goto check_status;
                    }
                    if ((u16)CdStream_State.remaining == 0) {
                        if ((CdStream_State.sector->magic & ~0xFF) != 0x4D545300) {
                            D_80082808 = 8;
                            D_80082810 = D_80082808;
                            goto check_status;
                        }
                        if (CdStream_State.sector->field_0 == 0) {
                            CdStream_State.mtsPeriod = (s8)CdStream_State.sector->field_D;
                            CdStream_State.remaining = (s16)(s8)(u8)CdStream_State.mtsPeriod;
                            CdStream_State.mtsParam  = CdStream_State.sector->field_E;
                            if (CdStream_State.sector->field_F & 0x80) {
                                CdStream_State.flags1 |= 4;
                            } else {
                                CdStream_State.flags1 &= 0xFB;
                            }
                            regionState  = &CdStream_State;
                            chunkSectors = 0x30;
                            if ((s8)(u8)regionState->mtsPeriod == 5) {
                                chunkSectors          = 0x18;
                                regionState->ringHalf = 0x2770;
                                if (gDisplayState.region == 1) {
                                    chunkSectors = 0x14;
                                }
                            } else {
                                regionState->ringHalf = 0x4ED0;
                                if (gDisplayState.region == 1) {
                                    chunkSectors = 0x28;
                                }
                            }
                            regionState->sectorsPerChunk = chunkSectors;
                            channels                     = &CdStream_Channels;
                            /* CdStream_State sits directly before the channels; reaching it back
                             * from `channels` keeps one base register for both objects. */
                            channelState                             = (volatile CdStreamState*)channels - 1;
                            *(volatile s32*)&channels->ch[0].spuAddr = channelState->spuBase;
                            /* Keep the channel address stores in initialization order. */
                            channelBase             = channelState->spuBase + 0x40;
                            channels->ch[1].spuAddr = channelBase + ((s32)((u16)channelState->ringHalf << 0x10) >> 0xF);
                            channelState->flags1    = (u8)(channelState->flags1 | 1);
                            SOFT_BARRIER();
                            attributes             = channels->ch[0].attr | 0x80;
                            channels->ch[0].attr   = attributes;
                            channels->ch[1].attr   = attributes;
                            channelState->field_1C = (s32)channelState->sector->field_0;
                            channelState->field_38 = (s32)channelState->sector->field_4;
                            if (!((u8)channelState->sector->field_F & 0x60)) {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x40);
                            } else if ((u8)channelState->sector->field_F & 0x40) {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x20);
                            } else {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x10);
                            }
                            goto start_chunk;
                        }
                        if (CdStream_State.sector->field_F & 0x80) {
                            CdStream_State.mtsParam = CdStream_State.sector->field_E;
                            if (CdStream_State.mtsParam != 0) {
                                CdStream_State.flags1 |= 4;
                            } else {
                                CdStream_State.flags1 &= 0xFB;
                            }
                        } else {
                            CdStream_State.flags1  &= 0xFB;
                            CdStream_State.mtsParam = 0;
                        }
                        CdStream_State.remaining = (s16)(s8)(u8)CdStream_State.mtsPeriod;
                    }
                    if (((s16)(u16)CdStream_State.remaining % (s8)(u8)CdStream_State.mtsPeriod) == 0) {
                        CdStream_State.field_1C = CdStream_State.sector->field_0;
                        if (CdStream_State.field_1C == 0) {
                            CdStream_State.field_38 = CdStream_State.sector->field_4;
                        }
                    start_chunk:
                        channelCount = (u8)CdStream_State.sector->magic;
                        /* Preserve the signed comparison of the channel count. */
                        if ((channelCount >= 2) && (CdStream_State.sector->field_C == 0)) {
                            CdStream_State.mode      = (s8)(u8)CdStream_State.sector->magic;
                            CdStream_State.remaining = (s8)(u8)CdStream_State.mtsPeriod * (s8)(u8)CdStream_State.mode;
                        }
                        if (CdStream_State.field_1C & 1) {
                            CdStream_State.spuAddr = CdStream_State.spuBase + (s16)(u16)CdStream_State.ringHalf;
                        } else {
                            CdStream_State.spuAddr = CdStream_State.spuBase;
                        }
                        CdStream_State.spuAddr += CdStream_State.sector->field_C * (((s32)((u16)CdStream_State.ringHalf << 0x10) >> 0xF) + 0x40);
                        SpuSetTransferStartAddr((u32)CdStream_State.spuAddr);
                        *(volatile s32*)&D_80068B70 = CdStream_State.spuAddr;
                        /* The audio in a header sector starts after the MtsSector header. */
                        SpuWrite((u8*)(CdStream_State.sector + 1), 0x800U);
                        *(void* volatile*)&D_80068B6C = CdStream_State.sector + 1;
                        CdStream_State.spuAddr       += 0x7F0;
                    } else {
                        SpuSetTransferStartAddr((u32)CdStream_State.spuAddr);
                        *(volatile s32*)&D_80068B70 = CdStream_State.spuAddr;
                        if (((s16)(u16)CdStream_State.remaining % (s8)(u8)CdStream_State.mtsPeriod) == 1) {
                            writeSize = ((s16)(u16)CdStream_State.ringHalf - 0x7F0) % 0x800;
                            if (writeSize == 0) {
                                writeSize = 0x800;
                            }
                            if (!(CdStream_State.field_1C & 1)) {
                                /* The two ring halves retain separate transfer paths. */
                                SpuWrite((u8*)CdStream_State.sector, (writeSize + 0x3F) & ~0x3F);
                                *(void* volatile*)&D_80068B6C = CdStream_State.sector;
                                CdStream_State.spuAddr       += writeSize;
                            } else {
                                SpuWrite((u8*)CdStream_State.sector, (writeSize + 0x3F) & ~0x3F);
                                *(void* volatile*)&D_80068B6C = CdStream_State.sector;
                                CdStream_State.spuAddr       += writeSize;
                            }
                        } else {
                            SpuWrite((u8*)CdStream_State.sector, 0x800U);
                            *(void* volatile*)&D_80068B6C = CdStream_State.sector;
                            CdStream_State.spuAddr       += 0x800;
                        }
                    }
                    state                    = &CdStream_State;
                    CdStream_State.remaining = (u16)CdStream_State.remaining - 1;
                    if ((u16)CdStream_State.remaining == 0) {
                        if (((u8)CdStream_State.flags1 >> 2) & 1) {
                            CdStream_State.countdown = (s16)(s8)CdStream_State.mtsParam;
                            if ((u16)CdStream_State.countdown != 0) {
                                CdStream_State.field_4C = 2;
                                D_80068B78              = 0;
                                goto unlock;
                            } else {
                                goto mark_complete;
                            }
                        } else {
                        mark_complete:
                            state->field_4C = 4;
                        }
                    }
                }
            }
        }
    } else {
        D_80082808 = 7;
        D_80082810 = D_80082808;
    check_status:
        if (*result & 0x10) {
            if (D_80082808 == 0) {
                D_80082808 = 5;
            }
            D_80082810                       = D_80082808;
            CdStream_State.flags            |= 1;
            CdStream_State.field_4C          = 3;
            CdStream_State.flags2           |= 2;
            *((volatile u8*)&D_80068B5C + 1) = (u8)(*((volatile u8*)&D_80068B5C + 1) + 1);
        } else if ((u16)CdStream_State.field_4C != 0) {
            if ((s16)(u16)CdStream_State.field_4C != 4) {
                CdStream_State.field_4C = 3;
                if (D_80082808 == 0) {
                    D_80082808 = 0xB;
                }
                D_80082810             = D_80082808;
                CdStream_State.flags2 |= 2;
            }
        }
    }
unlock:
    D_80068B6A = 0;
}

static s32 CdStream_InitDisc(u32* arg0)
{
    struct {
        u8     result[8];
        s8     mode;
        u8     pad[7];
        CdlLOC loc;
    } sp;
    s32          sync;
    u32          flags;
    u32          temp;
    register u32 a asm("v0");
    register u32 b asm("v1");

    flags = *arg0;
    if ((flags >> 1) & 1) {
        temp  = flags & ~2;
        temp  = temp & ~0xFF0;
        *arg0 = temp | 0x10;
    }

    a          = *(volatile u32*)arg0;
    b          = *(volatile u32*)arg0;
    a          = (a >> 4) & 0xFF;
    b          = (b >> 4) & 0xFF;
    D_80068B66 = a;
    switch (b) {
        case 1:
            if (CdControlB(CdlNop, NULL, sp.result) == 0) {
                return 0;
            }
            if (sp.result[0] & CdlStatShellOpen) {
                return 0;
            }
            if (sp.result[0] & CdlStatStandby) {
                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                case 2:
                    if (CdControl(CdlGetTN, NULL, sp.result) != 0) {
                        *arg0 = (*arg0 & ~0xFF0) | 0x40;
                        case 3:
                            sync = CdSync(1, sp.result);
                            if (sync == CdlDiskError) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x20;
                            } else if (sync == CdlComplete) {
                                *arg0 = (*arg0 & ~0xFF0) | 0x40;
                                case 4:
                                    CdIntToPos(0, &sp.loc);
                                    if (CdControl(CdlSeekL, (u8*)&sp.loc, sp.result) != 0) {
                                        *arg0 = (*arg0 & ~0xFF0) | 0x50;
                                        case 5:
                                            sync = CdSync(1, sp.result);
                                            if ((sync == CdlDiskError) && (sp.result[0] & CdlStatError) &&
                                                (sp.result[1] & 0x40)) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x10;
                                            } else if (sync == CdlComplete) {
                                                *arg0 = (*arg0 & ~0xFF0) | 0x60;
                                                case 6:
                                                    sp.mode = -0x60;
                                                    if (CdControl(CdlSetmode, (u8*)&sp.mode, NULL) != 0) {
                                                        *arg0                        = (*arg0 & ~0xFF0) | 0x70;
                                                        CdStream_State.settleCounter = 0;
                                                    }
                                            }
                                    }
                            }
                    }
            }
            break;
        case 7:
            CdStream_State.settleCounter = CdStream_State.settleCounter + 1;
            if (CdStream_State.settleCounter >= 4) {
                return 1;
            }
            break;
    }
    return 0;
}

static void CdReady_InstallCallback(CdlCB arg0)
{
    volatile CdReadyQueue* p;

    p = &CdReady_Queue;
    if (p->callbackInstalled == 0) {
        p->prevCallback = CdReadyCallback(arg0);
    } else {
        CdReadyCallback(arg0);
    }
    CdReady_Queue.callbackInstalled = 1;
}

static void CdReady_ClearCallback(void)
{
    volatile CdReadyQueue* p;

    p = &CdReady_Queue;
    if (p->callbackInstalled != 0) {
        CdReadyCallback(0);
        p->prevCallback      = 0;
        p->callbackInstalled = 0;
    }
}

void CdStream_Reset(void)
{
    MEM_CLEAR_WORDS(&CdReady_Queue, sizeof(CdReady_Queue) / 4);
    /* The clear runs past CdStream_State into the channel table after it. */
    MEM_CLEAR_WORDS(&CdStream_State, 0x36);

    SetRCnt(RCntCNT2, 0xFFFF, RCntMdNOINTR);
    StartRCnt(RCntCNT2);
    D_80068B58 = 0;
    D_80082808 = 0;
    D_80082810 = D_80082808;
    D_80068B6A = 0;
    D_80068B5C = 0;
}

void CdStream_ArmSpuIrq(void)
{
    volatile CdStreamState* p;

    CdStream_State.flags0 = CdStream_State.flags0 & 0xFD;
    CdStream_State.flags0 = CdStream_State.flags0 & 0xF7;
    p                     = &CdStream_State;
    p->field_4            = 1;
    p->field_18           = 0;
    SpuSetIRQ(0);
    SpuSetIRQCallback(CdStream_SpuIrqHandler);
    SpuSetIRQAddr((p->spuBase + p->ringHalf + 0x4F) & ~0x3F);
    CdStream_State.flags0 = CdStream_State.flags0 & 0xBF;
    CdStream_State.flags0 = CdStream_State.flags0 | 1;
}

static void CdStream_SpuIrqHandler(void)
{
    CdStream_State.flags0 = CdStream_State.flags0 | 8;
}

void CdStream_SetPitch(s16 arg0)
{
    CdStreamChannels*       p;
    volatile CdStreamState* q;
    CdStreamChannel*        ch1b;
    CdStreamChannel*        ch1;
    s16                     val;
    s32                     t0;
    s32                     t1;

    p = &CdStream_Channels;
    /* CdStream_State sits directly before the channels; reaching it back from
     * `p` keeps one base register for both objects. */
    q = (volatile CdStreamState*)p - 1;

    if ((q->flags0 >> 1) & 1) {
        if (q->flags1 & 1) {
            t0            = p->ch[0].attr;
            t1            = p->ch[1].attr;
            p->ch[0].attr = t0 | 3;
            p->ch[1].attr = t1 | 3;
        } else {
            p->ch[1].attr = 3;
            p->ch[0].attr = 3;
            q->flags1     = q->flags1 | 1;
        }
    }

    if (CdStream_State.flags & 2) {
        ch1b              = &p->ch[1];
        val               = (s16)((arg0 * 0xB5) >> 8);
        ch1b->pitch       = val;
        p->ch[0].pitchAlt = val;
        ch1b->pitchAlt    = val;
        p->ch[0].pitch    = val;
        return;
    }
    ch1               = &p->ch[1];
    ch1->pitchAlt     = arg0;
    p->ch[0].pitch    = arg0;
    ch1->pitch        = 0;
    p->ch[0].pitchAlt = 0;
}

static void CdStream_SetFlag14(s32 arg0)
{
    volatile CdStreamState* p;
    u8                      temp;

    p    = &CdStream_State;
    temp = p->flags1;
    if (temp >> 7) {
        p->field_14 = arg0;
        p->flags1   = p->flags1 | 8;
        p->flags0   = p->flags0 | 1;
    }
}

static void CdStream_AbortPhase(u32* arg0)
{
    u32 temp_v1;

    temp_v1 = *arg0;
    if ((temp_v1 >> 1) & 1) {
        *arg0 = temp_v1 & ~8;
        return;
    }
    *arg0                   = temp_v1 & ~8;
    CdStream_State.field_4C = 0;
    switch ((*arg0 >> 5) & 0xFF) {
        case 6:
        case 7:
            CdReady_ClearCallback();
            goto shared_flush;
        case 8:
            CdReady_ClearCallback();
            CdFlush();
            CdControlF(CdlPause, NULL);
            *arg0 = ((*arg0 | 8) & ~0x1FE0) | 0x1C0;
            break;
        case 9:
        case 11:
        case 12:
        case 14:
            CdFlush();
            *arg0 = (*arg0 & ~0x1FE0) | 0x1A0;
            /* fallthrough */
        case 13:
            if (SpuIsTransferCompleted(0) == 0) {
                *arg0 |= 8;
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 10:
        shared_flush:
            CdFlush();
            break;
    }
}

static void CdStream_FinishQueueEntry(u32* arg0)
{
    CdStream_AbortPhase(arg0);
    if (!((*arg0 >> 3) & 1) && (CdStream_State.doneCb != NULL)) {
        CdStream_State.doneCb(0);
    }
}

static void CdReady_Cancel(s16 arg0)
{
    u8            temp;
    s16           idx;
    u32           flags;
    CdReadyEntry* entry;

    temp = CdReady_Queue.locked;
    if (arg0 != 0) {
        idx   = arg0 - 1;
        entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
        flags = entry->flags;
        if (flags & 1) {
            entry->flags = (flags & ~1) | 4;
        }
        CdReady_Queue.locked = temp;
    }
}

s32 CdStream_IsBusy(void)
{
    if (CdStream_State.flags0 & 1) {
        return 1;
    }
    return (CdReady_Queue.writeIdx != CdReady_Queue.readIdx) ? 1 : (CdStream_State.pending != 0);
}

static void CdStream_ClearReadySlot(void)
{
    CdStream_State.readySlot = 0;
}

void CdStream_SetLinkedPitch(s32 arg0)
{
    if ((s8)arg0) {
        CdStream_State.flags = CdStream_State.flags | 2;
    } else {
        CdStream_State.flags = CdStream_State.flags & 0xFD;
    }
}

static void CdStream_MarkEnding(void)
{
    CdStream_State.flags   = CdStream_State.flags & 0xFE;
    CdStream_State.flags1  = CdStream_State.flags1 | 2;
    CdStream_State.pending = 0;
}

static s32 CdStream_Flush(void)
{
    CdFlush();
    return 0;
}

static void CdStream_ConfigureSpuIrq(s32 arg0, u32 arg1)
{
    if (arg0 == 1) {
        if (D_80068B5C != 0) {
            SpuSetIRQ(0);
            SpuSetIRQCallback(0);
        }
        D_80068B5C = arg0;
        SpuSetIRQCallback(CdStream_SpuIrqHandler);
        SpuSetIRQAddr(arg1);
        SpuSetIRQ(1);
    } else if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
}

static void func_8005BCF8(void)
{
}
