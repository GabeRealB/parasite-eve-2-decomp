#include "common.h"

#define MC_C

#include <psyq/libmcrd.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "main/unknown_syms.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "psyq/kernel.h"
#include "psyq/libmcrd.h"
#include "psyq/strings.h"

static const char D_800139A8[];

static void Mc_BuildFileName(u8* arg0, s32 arg1);
static void Mc_InitDualBankBuffers(void);
static void Mc_KillIfCountdown(Task* arg0, McWork* arg1);
static void Mc_ResetWork(Task* arg0, McWork* arg1);
static void Mc_StateBackupBuffers(Task* arg0, McWork* arg1);
static void Mc_StateCompareBuffers(Task* arg0, McWork* arg1);
static void Mc_StateCreateFile(Task* arg0, McWork* arg1);
static void Mc_StateFormat(Task* arg0, McWork* arg1);
static void Mc_StateFreeBuffer(Task* arg0, McWork* arg1);
static void Mc_StateNameEntry(Task* arg0, McWork* arg1);
static void Mc_StateOpenNext(Task* arg0, McWork* arg1);
static void Mc_StateOpenRead(Task* arg0, McWork* arg1);
static void Mc_StateOpenSelected(Task* arg0, McWork* arg1);
static void Mc_StatePadFileName(Task* arg0, McWork* arg1);
static s32  Mc_VerifySaveHdrChecksum(McSaveData* arg0);
static void Mc_StateSaveSlotUi(UiList* arg0, UiObject* arg1);

/// Work area of the memory-card dialogs, passed to every state handler.
static McWork D_80071730;
/// The save data, twice: the buffer slot fills, copies and compares both
/// banks as one run from the first.
McSaveData Mc_SaveData[2];
u8         D_800733F0[2][0x6C];
u8         D_800734C8[2][0xB0];
u8         D_80073628[2][0x24];
u8         D_80073670[2][0xE4];
u8         D_80073838[2][0xA4];
u8         D_80073980[0x200];
/// Unreferenced.
static u8    D_80073B80[8];
PlayerStatus Player_Status;
/// Last `rand()` result drawn by `Mc_DispatchStateTable`; nothing reads it.
static s32 D_80073C08;

static const char Mc_StrMemoryCard[] = "Memory Card";

static u8 D_80060A48[] = "New Block";
u8        D_80060A54[] = "Yes";
u8        D_80060A58[] = "No";
u8        D_80060A5C[] = "Cancel";
u8        D_80060A64[] = "OK";
static u8 D_80060A68[] = "";
static u8 D_80060A6C[] = "Failed to access MEMORY CARD.";
static u8 D_80060A8C[] = "Checking MEMORY CARD in slot 1.";
static u8 D_80060AAC[] = "MEMORY CARD has been changed.";
static u8 D_80060ACC[] = "Insert MEMORY CARD in slot 1.";
static u8 D_80060AEC[] = "No MEMORY CARD inserted.";
static u8 D_80060B08[] = "Saving...";
static u8 D_80060B14[] = "Loading...";
static u8 D_80060B20[] = "Formatting...";
static u8 D_80060B30[] = "Save game data?";
static u8 D_80060B40[] = "Load game data?";
static u8 D_80060B50[] = "Format MEMORY CARD?";
static u8 D_80060B64[] = "No game data available. Insert";
static u8 D_80060B84[] = "Create save data?";
static u8 D_80060B98[] = "Creating save data...";
static u8 D_80060BB0[] = "Overwrite data?";
static u8 D_80060BC0[] = "Please try again.";
static u8 D_80060BD4[] = "MEMORY CARD full. Insert";
static u8 D_80060BF0[] = "Do not remove MEMORY CARD.";
static u8 D_80060C0C[] = "another MEMORY CARD in slot 1.";
static u8 D_80060C2C[] = "Need not to save now.";
static u8 D_80060C44[] = "Select data.";
static u8 D_80060C54[] = "Save Failed!";
static u8 D_80060C64[] = "Load Failed!";
static u8 D_80060C74[] = "Format Failed!";
static u8 D_80060C84[] = "MEMORY CARD not formatted.";
static u8 D_80060CA0[] = "Save data corrupted.";
static u8 D_80060CB8[] = "Cannot load data.";
static u8 D_80060CCC[] = "Save data corrupted.\nLoad aborted";
/// Unreferenced.
static u8 D_80060CF0[] = "Save data corrupted.";

static McPromptPair Mc_PromptTable[] = {
    { D_80060A6C, D_80060BC0 },
    { D_80060A8C, D_80060BF0 },
    { D_80060AAC, D_80060A68 },
    { D_80060AEC, D_80060ACC },
    { D_80060B08, D_80060BF0 },
    { D_80060B14, D_80060BF0 },
    { D_80060B20, D_80060BF0 },
    { D_80060B30, D_80060A68 },
    { D_80060B40, D_80060A68 },
    { D_80060C84, D_80060B50 },
    { D_80060B64, D_80060C0C },
    { D_80060B84, D_80060A68 },
    { D_80060B98, D_80060BF0 },
    { D_80060C74, D_80060BC0 },
    { D_80060C54, D_80060BC0 },
    { D_80060C64, D_80060BC0 },
    { D_80060A6C, D_80060BC0 },
    { D_80060BB0, D_80060A68 },
    { D_80060BC0, D_80060A68 },
    { D_80060BD4, D_80060C0C },
    { D_80060C2C, D_80060A68 },
    { D_80060A68, D_80060A68 },
    { D_80060C44, D_80060A68 },
    { D_80060CA0, D_80060CB8 },
};

static u8  D_80060DC8[]         = "BASLUS-01042*";
static u8  Mc_FileName[0x18]    = "BASLUS-01042________";
static u8  Mc_FileNameBuf[0x18] = "BASLUS-01042________";
static u8  D_80060E08[64]       = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789;:";
static u16 Mc_GlyphsUpper[]     = {
    0x6082,
    0x6182,
    0x6282,
    0x6382,
    0x6482,
    0x6582,
    0x6682,
    0x6782,
    0x6882,
    0x6982,
    0x6A82,
    0x6B82,
    0x6C82,
    0x6D82,
    0x6E82,
    0x6F82,
    0x7082,
    0x7182,
    0x7282,
    0x7382,
    0x7482,
    0x7582,
    0x7682,
    0x7782,
    0x7882,
    0x7982,
    0x0000,
    0x0000,
};
static u16 Mc_GlyphsLower[] = {
    0x8182,
    0x8282,
    0x8382,
    0x8482,
    0x8582,
    0x8682,
    0x8782,
    0x8882,
    0x8982,
    0x8A82,
    0x8B82,
    0x8C82,
    0x8D82,
    0x8E82,
    0x8F82,
    0x9082,
    0x9182,
    0x9282,
    0x9382,
    0x9482,
    0x9582,
    0x9682,
    0x9782,
    0x9882,
    0x9982,
    0x9A82,
    0x0000,
    0x0000,
};
static u16 Mc_GlyphsSymbol[] = {
    0x4081,
    0x4981,
    0x6881,
    0x9481,
    0x9081,
    0x9381,
    0x9581,
    0x6681,
    0x6981,
    0x6A81,
    0x9681,
    0x7B81,
    0x4381,
    0x7C81,
    0x4481,
    0x5E81,
    0x4F82,
    0x5082,
    0x5182,
    0x5282,
    0x5382,
    0x5482,
    0x5582,
    0x5682,
    0x5782,
    0x5882,
    0x4681,
    0x4781,
    0x8381,
    0x8181,
    0x8481,
    0x4881,
    0x9781,
    0x0000,
};

static u8 Mc_DefaultChecksumSrc[] = {
#include "assets/mc_save_header.inc"
};

McBufferSlot Mc_BufferSlots[9] = {
    { (McChecksumBlock*)Mc_DefaultChecksumSrc, 0x100, 4 },
    { (McChecksumBlock*)&Mc_SaveData[0], 0x944, 0x26 },
    { (McChecksumBlock*)&Player_Status, 0x40, 1 },
    { (McChecksumBlock*)D_800733F0, 0x6C, 2 },
    { (McChecksumBlock*)D_800734C8, 0xB0, 3 },
    { (McChecksumBlock*)D_80073628, 0x24, 1 },
    { (McChecksumBlock*)D_80073670, 0xE4, 4 },
    { (McChecksumBlock*)D_80073838, 0xA4, 3 },
    { (McChecksumBlock*)D_80073980, 0x100, 4 },
};

static UiListItemFunc D_80061168[] = { Mc_StateSaveSlotUi };
UiList                D_8006116C   = { D_80061168, 0x0F, 0x0F, 0, 0x2E };
static UiListItemFunc D_80061190[] = { McMenu_ConfirmWithRender };
UiList                D_80061194   = { D_80061190, 0x0F, 0x0F, 0, 0x2E };

static u8* D_800611B8[] = { (u8*)D_80013B9C, (u8*)D_80013B94, (u8*)D_80013B88, (u8*)D_80013B7C };

UiObjectDesc D_800611C8[] = {
    { 2, 0xFF70, 0xFFE7, 0x120, 0x32, 0x14, 0, 0, 0xC0, Mc_DispatchStateTable26, 0 },
    { 2, 0xFF70, 0xFFE7, 0x120, 0x32, 0x14, 0, 0, 0xC0, Mc_DispatchStateTable, 0 },
};
static UiObjectDesc D_80061200[] = {
    { 0x80002, 0xFF78, 0x0A, 0x120, 0x3C, 0x0C, 0, 0, 0xC0, McMenu_SelectList, 0 },
};
static UiObjectDesc D_8006121C[] = {
    { 0x80002, 0xFF78, 0x0A, 0x120, 0x3C, 0x0C, 0, 0, 0xC0, McMenu_SelectListAlt, 0 },
    { 2, 0xFFC4, 0x1E, 0xC8, 0x3C, 0x1C, 0, 0, 0xC0, McMenu_FileInformation, 0 },
};

static void Mc_BuildFileName(u8* arg0, s32 arg1)
{
    s32 i;

    i = 0;
    do {
        *arg0 = D_80060DC8[i];
        i++;
        arg0++;
    } while (i < 0xC);

    *arg0   = D_80060E08[arg1];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    *++arg0 = D_80060E08[rand() & 0x3F];
    arg0[1] = 0;
}

static void Mc_InitDualBankBuffers(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];
    McSaveData* p;
    s32         one;
    s32         two;
    s32         idx;

    Mem_Set(&Player_Status, 0, 0x40);
    Mem_Set(Player_Status.field_40, 0xFF, 0x40);
    Mem_Set(D_80073980, 0, 0x100);
    Mem_Set(&D_80073980[0x100], 0xFF, 0x100);

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    do {
        b = D_800734C8;
        Mem_Set(b, 0, 0xB0);
        c = D_80073628;
        Mem_Set(c, 0, 0x24);
        d = D_80073670;
        Mem_Set(d, 0, 0xE4);
        e = D_80073838;
        Mem_Set(e, 0, 0xA4);
        Mem_Set(a + 1, 0xFF, 0x6C);
        Mem_Set(b + 1, 0xFF, 0xB0);
        Mem_Set(c + 1, 0xFF, 0x24);
        Mem_Set(d + 1, 0xFF, 0xE4);
        Mem_Set(e + 1, 0xFF, 0xA4);
        p = &Mc_SaveData[0];
    } while (0);

    one              = 1;
    p->at4.loc.area  = 0x14;
    two              = 2;
    p->at4.loc.stage = one;
    p->at4.loc.view  = one;
    p->at4.loc.room  = one;
    p->at4.loc.warp  = 7;
    p->at4.loc.place = one;
    p->sceneEvent    = two;
    p->characterId   = one;
    Player_InitNewGameStats();
    idx                          = p->characterId - 1;
    (&Player_Status)[idx].weapon = two;
}

/// Store the checksum of a buffer's payload (the `size - 4` bytes after its
/// header) in the header: the signed byte sum and its complement.
static inline void _mcWriteBlockChecksum(u8* data, s32 size)
{
    McChecksumBlock* block;
    s16              sum;
    u32              i;

    block = (McChecksumBlock*)data;
    sum   = 0;
    data  = block->field_4;
    size -= 4;
    i     = 0;
    if (size != 0) {
        do {
            i    += 1;
            sum  += (s8)*data;
            data += 1;
        } while (i < size);
    }
    block->field_0 = sum;
    block->field_2 = ~sum;
}

void Mc_InitBufferSlots(void)
{
    McBufferSlot* base;
    McBufferSlot* slot;
    u8*           ptr;
    u32           size;
    u32           i;
    s32           fill;

    fill = -1;
    base = Mc_BufferSlots;
    slot = base + 1;
    do {
        size = slot->field_4;
        ptr  = (u8*)slot->field_0;
        for (i = 0; i < size; i++) {
            *ptr++ = 0;
        }
        for (i = 0; i < size; i++) {
            *ptr++ = fill;
        }
        _mcWriteBlockChecksum((u8*)slot->field_0, size);
        slot++;
    } while (slot < base + 9);

    gDisplayState.roomVariant = 1;
    Mc_InitDualBankBuffers();

    Mc_SaveData[0].vibration    = 0;
    Mc_SaveData[0].buttonLayout = 0;
    Mc_SaveData[0].musicVolume  = 0;
    Mc_SaveData[0].cursorMode   = 0;
    Mc_SaveData[0].soundMode    = 0;
    Mc_SaveData[0].moveMode     = 0;
    CdVol_SetMixMode(1);
    Snd_ApplyVolumeTable(0);
}

/// Prompt + optional choice dialog (Mc_PromptTable[mode]).
static s32 Mc_PromptDialog(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = arg0->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(D_800612D0, one, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.x = (obj->panel.field_20.u + obj->panel.field_1E.u + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.field_22.u + obj->panel.field_1A.u + 8;
            obj->field_2C                             = 0;
            obj->panel.field_0.w                      = 0;
        }
        return 0;
    }
    child.object = child.task->spawnArg2;
    if (child.object->field_2E == 6) {
        obj->field_2C = child.object->field_2C;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.field_0.w = one;
    }
    return obj->field_2C;
}

static s32 Mc_PromptDialogChoice(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = arg0->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(D_800612D0, 0, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.x = (obj->panel.field_20.u + obj->panel.field_1E.u + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.field_22.u + obj->panel.field_1A.u + 0x10;
            obj->field_2C                             = 0;
            obj->panel.field_0.w                      = 0;
        }
        return 0;
    }
    child.object = child.task->spawnArg2;
    if (child.object->field_2E == 6) {
        obj->field_2C = child.object->field_2C;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.field_0.w = one;
    }
    return obj->field_2C;
}

static s32 Mc_PromptDialogSpawn(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = arg0->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(D_800612D0, 3, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.x = (obj->panel.field_20.u + obj->panel.field_1E.u + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.field_22.u + obj->panel.field_1A.u + 0x10;
            obj->field_2C                             = 0;
            obj->panel.field_0.w                      = 0;
        }
        return 0;
    }
    child.object = child.task->spawnArg2;
    if (child.object->field_2E == 6) {
        obj->field_2C = child.object->field_2C;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.field_0.w = one;
    }
    return obj->field_2C;
}

static s32 Mc_PromptDialogFile(Task* arg0, s32 arg1, s32 arg2)
{
    s32           ret;
    s32           one;
    UiObject*     obj;
    McPromptChild child;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[arg1];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, one, 0);

    child.task = arg0->firstChild;
    if (child.task == NULL) {
        child.object = Ui_SpawnFromDesc(D_800612D0, 2, one, 2, obj);
        if (child.object != NULL) {
            child.object->panel.bounds.unsignedRect.h = 0x12;
            child.object->panel.bounds.unsignedRect.x = (obj->panel.field_20.u + obj->panel.field_1E.u + 5) - child.object->panel.bounds.unsignedRect.w;
            child.object->panel.bounds.unsignedRect.y = obj->panel.field_22.u + obj->panel.field_1A.u + 8;
            obj->field_2C                             = 0;
            obj->panel.field_0.w                      = 0;
        }
        return 0;
    }
    child.object = child.task->spawnArg2;
    if (child.object->field_2E == 6) {
        obj->field_2C = child.object->field_2C;
        Ui_TeardownTree(child.object, child.object->owner);
        obj->panel.field_0.w = one;
    }
    return obj->field_2C;
}

static inline u16* Mc_EncodeTitleText(s8* arg0, u16* arg1)
{
    u16* lower;
    u16* upper;
    u16* symbol;
    s32  ch;
    s32  idx;
    u8   ch_u;

    ch_u = *arg0;
    if (*arg0 != 0) {
        lower  = Mc_GlyphsLower;
        upper  = Mc_GlyphsUpper;
        symbol = Mc_GlyphsSymbol;
        do {
            ch = (s8)ch_u;
            if (ch >= 0x61) {
                idx   = (ch - 0x61) * 2;
                idx  += (s32)lower;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x41) {
                idx   = (ch - 0x41) * 2;
                idx  += (s32)upper;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x20) {
                idx   = (ch - 0x20) * 2;
                idx  += (s32)symbol;
                *arg1 = *(u16*)idx;
            }
            arg0++;
            ch_u = *arg0;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

static inline u16* Mc_EncodeTitleLiteral(s8* arg0, u16* arg1)
{
    u16* lower;
    u16* upper;
    u16* symbol;
    s32  ch;
    s32  idx;

    if (*arg0 != 0) {
        lower  = Mc_GlyphsLower;
        upper  = Mc_GlyphsUpper;
        symbol = Mc_GlyphsSymbol;
        do {
            ch = *arg0;
            if (ch >= 0x61) {
                idx   = (ch - 0x61) * 2;
                idx  += (s32)lower;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x41) {
                idx   = (ch - 0x41) * 2;
                idx  += (s32)upper;
                *arg1 = *(u16*)idx;
            } else if (ch >= 0x20) {
                idx   = (ch - 0x20) * 2;
                idx  += (s32)symbol;
                *arg1 = *(u16*)idx;
            }
            arg0++;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

static inline void Mc_UpdateTitleHeaderChecksum(void)
{
    u16 sum;
    u8* ptr;
    s32 limit;
    s32 i;
    s16 tmp;

    sum                           = 0;
    ptr                           = (u8*)&Mc_SaveData[0];
    ptr                          += 4;
    limit                         = 0x38;
    i                             = 0;
    Mc_SaveData[0].hdrChecksum    = 0;
    Mc_SaveData[0].hdrChecksumInv = 0xFFFF;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    Mc_SaveData[0].hdrChecksum    = sum;
    Mc_SaveData[0].hdrChecksumInv = 0xFFFF - (u32)sum;
    Mc_VerifySaveHdrChecksum(&Mc_SaveData[0]);
}

static inline u8* Mc_CopyTitleBytes(u8* src, u8* dst)
{
    while (*src != 0) {
        *dst++ = *src++;
    }
    *dst = 0;
    return dst;
}

static inline void Mc_UpdateTitleDataChecksum(void)
{
    u16              sum;
    s32              count;
    u8*              src;
    McChecksumBlock* dst;
    s32              i;

    sum          = 0;
    count        = 0x200;
    src          = Mc_DefaultChecksumSrc;
    dst          = (McChecksumBlock*)&Mc_SaveData[0].dataChecksum;
    i            = 0;
    dst->field_0 = sum;
    dst->field_2 = 0xFFFF - (u32)sum;
    do {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    } while (i < count);
    dst->field_0 = sum;
    dst->field_2 = 0xFFFF - (u32)sum;
}

static void func_80030AB0(McWork* work)
{
    u8          buffer[0x20];
    u16*        title;
    s32         number;
    s32         i;
    s32         candidate;
    s32         available;
    McSaveData* slot;

    title  = (u16*)(Mc_DefaultChecksumSrc + 4);
    number = 1;
    if (work->field_288 > 0) {
        for (i = 0; i < work->field_288; i++) {
            slot = (McSaveData*)((s32)work + 0x294 + i * 0x80);
            if ((s8)slot->savePoint == (s8)Mc_SaveData[0].savePoint) {
                if (slot->saveNumber >= number) {
                    number = slot->saveNumber + 1;
                }
            }
        }
        if (number >= 100) {
            for (candidate = 1; candidate < 100; candidate++) {
                available = 1;
                for (i = 0; i < work->field_288; i++) {
                    slot = (McSaveData*)((s32)work + 0x294 + i * 0x80);
                    if ((s8)slot->savePoint == (s8)Mc_SaveData[0].savePoint && slot->saveNumber == candidate) {
                        available = 0;
                        break;
                    }
                }
                if (available == 1) {
                    number = candidate;
                    break;
                }
            }
        }
    }
    Mc_SaveData[0].saveNumber   = number;
    title                       = Mc_EncodeTitleLiteral("PE2 ", title);
    title                       = Mc_EncodeTitleText((s8*)Text_FormatTime(buffer, Mc_SaveData[0].playTime), title);
    title                       = Mc_EncodeTitleLiteral(" ", title);
    Mc_DefaultChecksumSrc[0x43] = 0;
    Mc_DefaultChecksumSrc[0x42] = 0;
    title                       = (u16*)Mc_CopyTitleBytes(D_800675F0[(s8)Mc_SaveData[0].savePoint], (u8*)title);
    title                       = Mc_EncodeTitleLiteral("(", title);
    title                       = Mc_EncodeTitleText((s8*)Text_ItoaSigned(buffer, Mc_SaveData[0].saveNumber), title);
    title                       = Mc_EncodeTitleLiteral((s8*)D_800139A8, title);
    *title                      = 0;
    if (Mc_SaveData[0].saveCount == 0xFF) {
        Mc_SaveData[0].saveCount = 0;
    } else if (Mc_SaveData[0].saveCount < 99) {
        Mc_SaveData[0].saveCount++;
    }
    Mc_UpdateTitleHeaderChecksum();
    Mc_UpdateTitleDataChecksum();
    Mc_SaveData[0].bufferChecksum    = 0;
    Mc_SaveData[0].bufferChecksumInv = 0xFFFF;
}

static const char D_800139A8[] = ")";

static void Mc_StateScanDirFlags(Task* arg0, McWork* arg1);
static void Mc_StateListDirectory(Task* arg0, McWork* arg1);
static void Mc_StateFileSelect(Task* arg0, McWork* arg1);

static void Mc_WriteSlotChecksumsEx(Task* arg0, McWork* arg1);
static void Mc_StateAcceptMode1(Task* arg0, McWork* arg1);
static void Mc_StateSyncAdvance(Task* arg0, McWork* arg1);
static void Mc_StateDrawPromptAdvance(Task* arg0, McWork* arg1);
static void Mc_StatePromptChoiceB(Task* arg0, McWork* arg1);
static void Mc_StateDrawPrompt4(Task* arg0, McWork* arg1);
static void Mc_StateEnterDialog4(Task* arg0, McWork* arg1);
static void Mc_StateWriteFile(Task* arg0, McWork* arg1);
static void Mc_StatePromptChoiceGeneric(Task* arg0, McWork* arg1);
static void Mc_StateWriteData(Task* arg0, McWork* arg1);
static void Mc_StateClosePrompt(Task* arg0, McWork* arg1);
static void Mc_StateSyncPromptFile3(Task* arg0, McWork* arg1);
static void Mc_StatePromptChoice9(Task* arg0, McWork* arg1);
static void Mc_StateColdBoot(Task* arg0, McWork* arg1);
static void Mc_StateEnterPrompt0(Task* arg0, McWork* arg1);
static void Mc_StateSyncPrompt13(Task* arg0, McWork* arg1);
static void Mc_StatePromptCountdown(Task* arg0, McWork* arg1);
static void Mc_StateDrawPromptTo1F(Task* arg0, McWork* arg1);
static void Mc_StateCountdownPrompt4(Task* arg0, McWork* arg1);
static void Mc_StateDrawPrompt1Advance(Task* arg0, McWork* arg1);
static void Mc_StateReadHeader(Task* arg0, McWork* arg1);
static void Mc_StateUiCountdown2(Task* arg0, McWork* arg1);
static void Mc_StateUiCountdownF(Task* arg0, McWork* arg1);
static void Mc_StateUiCountdownE(Task* arg0, McWork* arg1);
static void Mc_StateEnterPromptE(Task* arg0, McWork* arg1);
static void Mc_StateEnterPromptD(Task* arg0, McWork* arg1);

static const McStateFuncTable44 Mc_PromptStates = { {
    Mc_ResetWork,
    Mc_WriteSlotChecksumsEx,
    Mc_StateAcceptMode1,
    Mc_StateSyncAdvance,
    Mc_StateCompareBuffers,
    Mc_StateDrawPromptAdvance,
    Mc_StateOpenRead,
    Mc_StatePromptChoiceB,
    Mc_StateDrawPrompt4,
    Mc_StateCreateFile,
    Mc_StateEnterDialog4,
    Mc_StateWriteFile,
    Mc_StateSyncAdvance,
    Mc_StatePadFileName,
    Mc_StatePromptChoiceGeneric,
    Mc_StateBackupBuffers,
    Mc_StateWriteData,
    Mc_StateSyncAdvance,
    Mc_StateFreeBuffer,
    Mc_StateClosePrompt,
    Mc_StateSyncPromptFile3,
    Mc_StatePromptChoice9,
    Mc_StateColdBoot,
    Mc_StateFormat,
    Mc_StateEnterPrompt0,
    Mc_StateSyncPrompt13,
    Mc_StateNameEntry,
    Mc_StatePromptCountdown,
    Mc_StateDrawPromptTo1F,
    Mc_StateCountdownPrompt4,
    Mc_KillIfCountdown,
    Mc_StateDrawPrompt1Advance,
    Mc_StateScanDirFlags,
    Mc_StateListDirectory,
    Mc_StateOpenSelected,
    Mc_StateReadHeader,
    Mc_StateSyncAdvance,
    Mc_StateOpenNext,
    Mc_StateFileSelect,
    Mc_StateUiCountdown2,
    Mc_StateUiCountdownF,
    Mc_StateUiCountdownE,
    Mc_StateEnterPromptE,
    Mc_StateEnterPromptD,
} };

static void Mc_StateScanDirFlags(Task* arg0, McWork* arg1)
{
    s32       ret;
    UiObject* obj;
    s32       i;
    s32       j;
    s32       size;
    s32       blocks;
    s32       head;
    s32       idx;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        arg1->field_288 = 0;
        /* Mark every block free, then claim the 8KB blocks each file spans. */
        for (i = 0; i < 15; i++) {
            arg1->field_A24[i] = -1;
        }
        MemCardGetDirentry(
            arg1->field_C, "*", arg1->field_30, &arg1->field_288, 0,
            0xF);

        arg1->field_28C = 0;
        if (arg1->field_288 != 0) {
            for (i = 0; i < arg1->field_288; i++) {
                size   = arg1->field_30[i].size;
                head   = arg1->field_30[i].head;
                blocks = size / 0x2000 + ((size % 0x2000) != 0);
                head  /= 64;
                head  -= 1;
                for (j = 0; j < blocks; j++) {
                    arg1->field_A24[head + j] = -2;
                }
                arg1->field_28C += blocks;
            }
        }
        arg0->state += 1;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, Mc_PromptTable[idx].field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, Mc_PromptTable[idx].field_4, ret, 1, 0);
}

static void Mc_StateListDirectory(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           one;
    s32           var_s0;
    s32           temp_v0;
    s32           temp_v0_2;
    s32           var_v0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_288 = 0;
    MemCardGetDirentry(
        arg1->field_C, (char*)D_80060DC8, arg1->field_30, &arg1->field_288, 0,
        0xF);
    temp_v0         = arg1->field_28C - arg1->field_288;
    arg1->field_28C = temp_v0;
    if (temp_v0 == 0xF) {
        var_v0 = 0x19;
    } else {
        if (arg1->field_28 == -1) {
            arg1->field_290 = 0;
        } else {
            temp_v0_2       = arg1->field_288;
            arg1->field_290 = 0;
            if (temp_v0_2 != 0) {
                var_s0 = 0;
                if (temp_v0_2 > 0) {
                    do {
                        if (strncmp(arg1->field_30[var_s0].name, (char*)Mc_FileName, 0x14) == 0) {
                            arg1->field_290 = var_s0;
                            break;
                        }
                        temp_v0_2 = arg1->field_288;
                        var_s0   += 1;
                    } while (var_s0 < temp_v0_2);
                }
            }
        }
        arg1->field_A14 = 0;
        if (arg1->field_288 > 0) {
            var_v0 = arg0->state + 1;
        } else {
            var_v0 = 0x26;
        }
    }
    arg0->state = var_v0;

    /* Map each file's first block back to its directory entry. */
    if (arg1->field_288 > 0) {
        s32 i;

        for (i = 0; i < arg1->field_288; i++) {
            s32 head              = arg1->field_30[i].head;
            head                 /= 64;
            head                 -= 1;
            arg1->field_A24[head] = i;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, one, 0);
}

/// Inline form of Mc_DrawPrompt.
static inline void _mcDrawPrompt(Task* task, s32 mode)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    obj           = task->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[mode];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

/// Tear down the task's child UI and report status on the task's own object.
static inline void _mcCloseChild(Task* task, s32 status)
{
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    child = task->firstChild;
    if (child != NULL) {
        obj                  = child->spawnArg2;
        flag                 = task->spawnArg2;
        obj->panel.field_0.w = 0;
        Ui_TeardownTree(obj, obj->owner);
        flag->panel.field_0.w = status;
    }
}

/// Inline form of Mc_CopyFileName: 0 saves Mc_FileName to Mc_FileNameBuf, otherwise restores it.
static inline void _mcCopyFileName(s32 arg0)
{
    u8* src;
    u8* dst;
    s32 i;

    if (arg0 == 0) {
        src = Mc_FileName;
        dst = Mc_FileNameBuf;
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
    }

    for (i = 0; i < 0x15; i++) {
        *dst++ = *src++;
    }
}

static void Mc_StateFileSelect(Task* arg0, McWork* arg1)
{
    UiObject* obj;
    UiObject* childObj;
    Task*     child;
    s32       syncResult;
    s32       i;

    obj           = arg0->spawnArg2;
    arg1->field_8 = 0x16;
    _mcDrawPrompt(arg0, 0x16);

    child = arg0->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(D_8006121C, (s32)arg1, 1, 2, obj) != 0) {
            D_80061194.field_4 = arg1->field_288;
            if (arg1->field_288 < 0xF - arg1->field_28C) {
                D_80061194.field_4++;
            }
            D_80061194.field_10  = arg1->field_290;
            obj->field_2C        = 0;
            obj->panel.field_0.w = 0;
        }
    } else {
        childObj = child->spawnArg2;
        if (childObj->field_2E == 6) {
            obj->field_2C             = childObj->field_2C;
            childObj->panel.field_0.w = 0;
            Ui_TeardownTree(childObj, childObj->owner);
            obj->panel.field_0.w = 1;
            if (obj->field_2C >= 0) {
                if (obj->field_2C < arg1->field_288) {
                    u8* src;
                    u8* name;
                    s32 matchCount;

                    src        = (u8*)arg1->field_30[obj->field_2C].name;
                    name       = Mc_FileName;
                    matchCount = 0x14;
                    _mcCopyFileName(0);
                    for (i = 0; i < 0x14; i++) {
                        if (*name == *src) {
                            matchCount--;
                        }
                        *name = *src;
                        name++;
                        src++;
                    }
                    *name = 0;
                    if (matchCount != 0) {
                        arg1->field_28 = -1;
                    }
                    arg1->field_8 = 1;
                    arg0->state   = 5;
                } else {
                    _mcCopyFileName(0);
                    Mc_BuildFileName(Mc_FileName, obj->field_2C);
                    arg1->field_8 = 1;
                    arg0->state   = 5;
                }
            } else {
                arg0->state = 0x29;
            }
            return;
        }
    }

    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1 && arg1->field_14 != 0) {
            arg0->state = 2;
            _mcCloseChild(arg0, syncResult);
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

/// Mask of the buffer slots to write, bit n for slot n: set where the slot's
/// two halves differ, and always for slots 0, 1 and 8.
static inline s32 _mcCompareBufferHalves(void)
{
    McBufferSlot* base;
    u8*           src;
    u8*           dest;
    u32           count;
    u32           i;
    u32           j;
    s32           flags;

    flags = 0;
    i     = 0;
    base  = Mc_BufferSlots;
    do {
        src   = (u8*)base[8 - i].field_0;
        count = base[8 - i].field_4;
        j     = 0;
        dest  = src + count;
        while (j < count) {
            if (*src != *dest) {
                flags |= 1;
            }
            j    += 1;
            src  += 1;
            dest += 1;
        }
        i      += 1;
        flags <<= 1;
    } while (i < 8U);
    flags |= 0x100;
    flags |= 3;
    return flags;
}

static void Mc_StateCompareBuffers(Task* arg0, McWork* arg1)
{
    s32           flags;
    s32           ret;
    u32           status;
    s32           idx;
    s32           one;
    s32           ch;
    s32           i;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    switch (status) {
        case 0:
            arg1->field_24 = 9;
            flags          = _mcCompareBufferHalves();
            arg1->field_2C = 1;
            arg1->field_28 = flags;
            arg0->state    = 0x1F;
            break;
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0          = 0;
            *ptr1          = 0;
            arg1->field_24 = 9;
            arg1->field_28 = -1;
            arg1->field_2C = 1;
            arg0->state    = 0x1F;
            break;
        case 1:
            arg0->state = 0x14;
            break;
        case 4:
            arg0->state = 0x15;
            break;
        case 2:
        default:
            arg0->state = 0x18;
            break;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    one   = 1;
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, one, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, one, 0);
}

static void Mc_StateOpenRead(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        MemCardClose();
        status         = MemCardOpen(arg1->field_C, Mc_FileName, 2);
        arg1->field_14 = status;
        switch (status) {
            case 0:
                arg0->state = 0x1A;
                break;
            case 1:
                arg0->state = 0x18;
                break;
            case 2:
                arg0->state = 0x18;
                break;
            case 3:
                arg0->state = 0x18;
                break;
            case 4:
                arg0->state = 0x18;
                break;
            case 5:
                arg0->state = 0x7;
                break;
            default:
                arg0->state = 0x18;
                break;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateCreateFile(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        status         = MemCardCreateFile(arg1->field_C, Mc_FileName, 1);
        arg1->field_14 = status;
        switch (status) {
            case 0:
                arg0->state = 0xA;
                break;
            case 1:
                arg0->state = 0x14;
                break;
            case 4:
                arg0->state = 0x15;
                break;
            case 7:
                arg0->state = 0x19;
                break;
            case 2:
            case 3:
            case 5:
            case 6:
            default:
                arg0->state = 0x2A;
                break;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StatePadFileName(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    if (status < 4U) {
        ptr1 = Mc_FileName;
        if (status == 0) {
            arg1->field_24 = 9;
            arg1->field_28 = -1;
            arg1->field_2C = 0;
            arg0->state    = 5;
        } else {
            goto pad;
        }
    } else {
        ptr1 = Mc_FileName;
    pad:
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 0x2A;
    }
    arg1->field_18 = 0;

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateNameEntry(Task* task, McWork* work)
{
    s32 syncResult;
    u8* src;
    u8* dst;
    s32 i;

    if (work->field_2C == 1) {
        work->field_8 = 0x11;
        switch (Mc_PromptDialogSpawn(task, 0x11, work->field_0)) {
            case 0:
                break;
            case 1:
                work->field_4  = 0xE;
                work->field_1C = 0;
                task->state    = 0x28;
                break;
            case -1:
                src = Mc_FileNameBuf;
                dst = Mc_FileName;
                for (i = 0; i < 0x15; i++) {
                    *dst++ = *src++;
                }
                task->killCountdown = 0xC;
                task->state         = 0x27;
                break;
        }
        syncResult = MemCardSync(1, (long*)&work->field_10, (long*)&work->field_14);
        if (syncResult != -1) {
            if (syncResult == 1 && work->field_14 != 0) {
                task->state = 2;
                _mcCloseChild(task, syncResult);
            }
        } else {
            MemCardExist(work->field_C);
        }
    } else {
        work->field_1C = 0;
        work->field_8  = 4;
        task->state    = 0xF;
        _mcDrawPrompt(task, work->field_8);
    }
}

/// Copy the first half of each of Mc_BufferSlots[1..8] over its second half.
static inline void _mcCopyBufferHalves(void)
{
    McBufferSlot* p;
    McBufferSlot* base;
    u8*           src;
    u8*           dest;
    u32           count;
    u32           i;
    u32           j;

    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        src   = (u8*)p->field_0;
        count = p->field_4;
        j     = 0;
        dest  = src + count;
        while (j < count) {
            j      += 1;
            *dest++ = *src++;
        }
        i += 1;
        p += 1;
    } while (i < 9U);
}

/// Inline form of Mc_WriteFirstByteChecksum.
static inline void _mcWriteFirstByteChecksum(void)
{
    McChecksumBlock* temp;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              next;
    s16              sum;
    u32              i;

    sum  = 0;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        temp = p->field_0;
        p   += 1;
        i   += 1;
        next = sum + *(u8*)temp;
        sum  = next;
    } while (i < 9U);
    Mc_SaveData[0].bufferChecksum    = next;
    Mc_SaveData[0].bufferChecksumInv = ~next;
}

static void Mc_StateBackupBuffers(Task* arg0, McWork* arg1)
{
    McChecksumBlock* buf;
    s32              size;
    void*            mem;

    if (arg1->field_24 == 0) {
        _mcCopyBufferHalves();
        arg1->field_A18 = 0x33;
        arg0->state     = 0x13;
    } else if (arg1->field_28 & 1) {
        size           = Mc_BufferSlots[9 - arg1->field_24].field_4;
        buf            = Mc_BufferSlots[9 - arg1->field_24].field_0;
        arg1->field_20 = (((u32)(size * 2 - 1) >> 7) + 1) << 7;
        mem            = memCalloc(arg1->field_20, 0);
        if (mem != 0) {
            arg1->field_18 = (s32)mem;
            if (arg1->field_24 == 9) {
                func_80030AB0(arg1);
                memcpy(mem, buf, size * 2);
            } else {
                _mcWriteBlockChecksum((u8*)buf, size);
                if (arg1->field_24 == 8) {
                    _mcWriteFirstByteChecksum();
                }
                memcpy(mem, buf, size);
                memcpy((u8*)mem + size, buf, size);
            }
            arg1->field_4  = 0;
            arg0->state    = arg0->state + 1;
            arg1->field_24 = arg1->field_24 - 1;
            arg1->field_28 = (u32)arg1->field_28 >> 1;
        } else {
            arg1->field_4 = arg1->field_4 + 1;
        }
    } else {
        arg1->field_1C = arg1->field_1C + Mc_BufferSlots[9 - arg1->field_24].field_8;
        arg1->field_24 = arg1->field_24 - 1;
        arg1->field_28 = (u32)arg1->field_28 >> 1;
    }

    arg1->field_8 = 4;
    _mcDrawPrompt(arg0, 4);
}

static void Mc_StateFreeBuffer(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    switch (status) {
        case 0:
            arg1->field_1C += Mc_BufferSlots[8 - arg1->field_24].field_8;
            arg0->state     = 0xF;
            break;
        case 1:
            MemCardClose();
            arg0->state = 0x14;
            break;
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0 = 0;
            *ptr1 = 0;
            MemCardClose();
            arg0->state = 0x2;
            break;
        case 2:
        case 4:
        case 5:
        default:
            arg0->state = 0x2A;
            break;
    }
    memFree((void*)arg1->field_18);
    arg1->field_18 = 0;

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateFormat(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           status;
    s32           idx;
    s32           next;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        status         = MemCardFormat(arg1->field_C);
        arg1->field_14 = status;
        if (status != 1) {
            if (status != 0) {
                next = 0x2B;
            } else {
                Mc_BuildFileName(Mc_FileName, 0);
                next            = 0x8;
                arg1->field_288 = 0;
            }
        } else {
            next = 0x14;
        }
        arg0->state = next;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateSyncFileSelect(Task* task, McWork* work)
{
    UiObject* obj;
    s32       syncResult;
    s32       i;
    Task*     child;
    UiObject* childObj;
    u8*       src;
    u8*       dst;

    obj           = task->spawnArg2;
    work->field_8 = 0x16;
    _mcDrawPrompt(task, 0x16);

    syncResult = MemCardSync(1, (long*)&work->field_10, (long*)&work->field_14);
    if (syncResult == -1) {
        MemCardExist(work->field_C);
    } else if (syncResult == 1 && work->field_14 != 0) {
        task->state = 7;
        _mcCloseChild(task, syncResult);
        return;
    }
    child = task->firstChild;
    if (child == NULL) {
        if (Ui_SpawnFromDesc(D_80061200, (s32)work, 1, 2, obj) != 0) {
            D_8006116C.field_4   = work->field_288;
            obj->field_2C        = 0;
            obj->panel.field_0.w = 0;
        }
    } else {
        childObj = child->spawnArg2;
        if (childObj->field_2E == 6) {
            obj->field_2C             = childObj->field_2C;
            childObj->panel.field_0.w = 0;
            Ui_TeardownTree(childObj, childObj->owner);
            obj->panel.field_0.w = 1;
            if (obj->field_2C >= 0) {
                src = (u8*)work->field_30[obj->field_2C].name;
                dst = Mc_FileName;
                for (i = 0; i < 0x14; i++) {
                    *dst++ = *src++;
                }
                *dst        = 0;
                task->state = 0xC;
            } else {
                task->state = 3;
            }
        }
    }
}

/// Jump table of 26 McStateFunc handlers used by Mc_DispatchStateTable26.
static const McStateFuncTable26 Mc_FileSelectStates = { {
    (McStateFunc)0x80035A94,
    (McStateFunc)0x80035AD4,
    (McStateFunc)0x80035AF0,
    (McStateFunc)0x80035C2C,
    (McStateFunc)0x80035D14,
    (McStateFunc)0x80035E18,
    (McStateFunc)0x80035E48,
    (McStateFunc)0x80035ED4,
    (McStateFunc)0x8003429C,
    (McStateFunc)0x800327A4,
    (McStateFunc)0x80035FD8,
    (McStateFunc)0x800360C8,
    (McStateFunc)0x800361C0,
    (McStateFunc)0x800328FC,
    (McStateFunc)0x80032AB0,
    (McStateFunc)0x800362A4,
    (McStateFunc)0x8003429C,
    (McStateFunc)0x80032D54,
    (McStateFunc)0x800363AC,
    (McStateFunc)0x80036488,
    (McStateFunc)0x800365B0,
    (McStateFunc)0x800366BC,
    (McStateFunc)0x8003429C,
    (McStateFunc)0x800367CC,
    (McStateFunc)0x80032578,
    (McStateFunc)0x800368DC,
} };

static void Mc_StateBlankFileName(Task* arg0, McWork* arg1)
{
    s32           ret;
    u32           status;
    s32           idx;
    s32           i;
    s32           ch;
    u8*           ptr1;
    u8*           ptr0;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    status = arg1->field_14;
    switch (status) {
        case 0:
        case 3:
            ptr1 = Mc_FileName;
            ptr0 = Mc_FileNameBuf;
            i    = 0;
            ch   = 0x5F;
            do {
                if (i >= 0xC) {
                    *ptr0 = ch;
                    *ptr1 = ch;
                }
                ptr1++;
                i++;
                ptr0++;
            } while (i < 0x14);
            *ptr0       = 0;
            *ptr1       = 0;
            arg0->state = 0x12;
            break;
        case 1:
            arg0->state = 0xA;
            break;
        case 4:
            arg0->state = 0xB;
            break;
        case 2:
        default:
            arg0->state = 0x6;
            break;
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateSyncOpen(Task* arg0, McWork* arg1)
{
    s32   syncResult;
    u32   status;
    char* fileName;

    _mcDrawPrompt(arg0, arg1->field_8);

    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_10 == syncResult) {
                if (arg1->field_14 != 0) {
                    arg0->state = 6;
                }
            }
            arg1->field_4 -= 1;
            if (arg1->field_4 == 0) {
                fileName = Mc_FileName;
                MemCardClose();
                status         = MemCardOpen(arg1->field_C, fileName, 1);
                arg1->field_14 = status;
                switch (status) {
                    case 0:
                        arg1->field_1C = 0;
                        arg0->state    = 0xE;
                        break;
                    case 1:
                    case 2:
                        arg0->state = 6;
                        break;
                    case 5:
                        arg0->state = 0xB;
                        break;
                    case 3:
                    case 4:
                    default:
                        arg0->state = 6;
                        break;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

/// Inline form of Mc_VerifySlotChecksums.
static inline s32 _mcVerifySlotChecksums(void)
{
    McChecksumBlock* block;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    u32              count;
    u32              i;
    u32              j;
    u8*              ptr;
    s32              ok;

    ok   = 1;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum   = 0;
        block = p->field_0;
        count = p->field_4;
        ptr   = block->field_4;
        count = count - 4;
        j     = 0;
        while (j < count) {
            j   += 1;
            sum += (s8)*ptr++;
        }
        if ((u16)block->field_0 != (sum & 0xFFFF)) {
            ok = 0;
        }
        i += 1;
        p += 1;
    } while (i < 9U);
    return ok;
}

/// Inline form of Mc_WriteSlotChecksums: store the signed byte sum of each
/// buffer slot 1..8's payload, and its complement, in the buffer's header.
static inline void _mcWriteSlotChecksums(void)
{
    McChecksumBlock* block;
    McBufferSlot*    p;
    McBufferSlot*    base;
    s16              sum;
    s32              inv;
    u32              count;
    u32              i;
    u32              j;
    u8*              ptr;

    i    = 1;
    inv  = 0xFFFF;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum   = 0;
        j     = 0;
        block = p->field_0;
        count = p->field_4;
        ptr   = block->field_4;
        count = count - 4;
        while (j < count) {
            j   += 1;
            sum += (s8)*ptr++;
        }
        p             += 1;
        i             += 1;
        block->field_2 = inv - sum;
        block->field_0 = sum;
    } while (i < 9U);
}

/// Inline form of Mc_VerifyFirstByteChecksum.
static inline s32 _mcVerifyFirstByteChecksum(void)
{
    s32           sum;
    u32           i;
    McBufferSlot* p;
    McBufferSlot* base;

    sum  = 0;
    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        sum += *(u8*)p->field_0;
        p   += 1;
        i   += 1;
    } while (i < 9);
    return ((u16)Mc_SaveData[0].bufferChecksum ^ (sum & 0xFFFF)) == 0;
}

static void Mc_StateVerifyFinish(Task* arg0, McWork* arg1)
{
    s32   size;
    void* mem;

    if (arg1->field_24 == 0) {
        if (_mcVerifySlotChecksums() && _mcVerifyFirstByteChecksum()) {
            Game_ClearEd68();
            gDisplayState.at100.flags.pendingPlayerPos = 1;
            arg0->state                                = 3;
        } else {
            Mc_InitBufferSlots();
            arg0->state = 0x19;
        }
    } else if (arg1->field_28 & 1) {
        size           = Mc_BufferSlots[9 - arg1->field_24].field_4;
        size         <<= 1;
        size          -= 1;
        size           = (u32)size >> 7;
        size          += 1;
        size         <<= 7;
        arg1->field_20 = size;
        mem            = Mem_Malloc(size, 0);
        arg1->field_18 = (s32)mem;
        if (mem != 0) {
            arg1->field_4  = 0;
            arg0->state    = arg0->state + 1;
            arg1->field_24 = arg1->field_24 - 1;
            arg1->field_28 = (u32)arg1->field_28 >> 1;
        } else {
            arg1->field_4 = arg1->field_4 + 1;
        }
    } else {
        arg1->field_1C = arg1->field_1C + Mc_BufferSlots[9 - arg1->field_24].field_8;
        arg1->field_24 = arg1->field_24 - 1;
        arg1->field_28 = (u32)arg1->field_28 >> 1;
    }

    arg1->field_8 = 5;
    _mcDrawPrompt(arg0, 5);
}

/// Checksum the 0x200-byte work buffer into the work's sum / complement pair,
/// clearing the pair first.
static inline void _mcWriteWorkChecksum(McWork* work)
{
    s16  sum;
    s32  count;
    u8*  src;
    u16* dst;
    s32  i;

    sum    = 0;
    count  = 0x200;
    src    = (u8*)work->field_18;
    dst    = &work->field_A1C;
    i      = 0;
    dst[0] = 0;
    dst[1] = ~0;
    do {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    } while (i < count);
    dst[0] = sum;
    dst[1] = ~sum;
}

static void Mc_StateFinishWrite(Task* arg0, McWork* arg1)
{
    u32 status;
    s32 slotIdx;
    s32 size;
    s32 i;
    s32 ch;
    u8* ptr1;
    u8* ptr0;

    status = arg1->field_14;
    if (status < 4U) {
        if (status == 0) {
            slotIdx = 8 - arg1->field_24;
            if (slotIdx == 0) {
                _mcWriteWorkChecksum(arg1);
            } else {
                size   = Mc_BufferSlots[slotIdx].field_4;
                size <<= 1;
                memcpy(Mc_BufferSlots[slotIdx].field_0, (void*)arg1->field_18, size);
            }
            arg1->field_1C += Mc_BufferSlots[8 - arg1->field_24].field_8;
            arg0->state     = 0xE;
        } else {
            goto pad;
        }
    } else {
    pad:
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 6;
    }

    memFree((void*)arg1->field_18);
    arg1->field_18 = 0;
    _mcDrawPrompt(arg0, arg1->field_8);
}

/// Inline form of Mc_VerifySaveHdrChecksum: whether a save header names a valid
/// save point and carries the checksum of its 0x38 bytes from `at4`.
static inline s32 _mcVerifySaveHdrChecksum(McSaveData* save)
{
    u16 sum;
    u8* ptr;
    s32 limit;
    s32 i;

    sum = 0;
    if ((u32)(save->savePoint - 1) >= 0x10U) {
        return 0;
    }
    ptr   = (u8*)&save->at4;
    limit = 0x38;
    i     = 0;
    do {
        i   += 1;
        sum += (s8)*ptr;
        ptr += 1;
    } while (i < limit);
    return save->hdrChecksum == sum;
}

static void Mc_StateSaveSlotUi(UiList* arg0, UiObject* arg1)
{
    s32 base;
    s32 off;
    s32 enabled;

    enabled = 1;
    off     = (arg0->field_8 << 7) + 0x294;
    base    = arg1->owner->spawnArg1;
    if (!_mcVerifySaveHdrChecksum((McSaveData*)(base + off))) {
        enabled = 0;
        Ui_LookupTable(arg1, 2);
    }
    func_800330D8(arg1, base, arg0->field_8, 0, arg0->field_1A + 7);
    if (arg0->field_C == 1) {
        if (enabled && Pad_CheckButtons(0, 1, Pad_MaskConfirm)) {
            SndEvt_EnqueueType6(0x16, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = arg0->field_8;
        } else if (Pad_CheckButtons(0, 1, Pad_MaskCancel)) {
            SndEvt_EnqueueType6(0x3B, 0, 0);
            arg1->field_2E = 6;
            arg1->field_2C = -1;
        }
    }
}

void func_800330D8(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    union {
        u8          buf[0x20];
        TextDrawReq req;
    } sp20;
    TextDrawReq sp40;
    TextDrawReq sp50;
    union {
        u8          buf[0x10];
        TextDrawReq req;
    } sp60;
    TextDrawReq sp70;
    TextDrawReq sp80;
    TextDrawReq sp90;
    s32         x;
    s32         y;
    s32         off;
    s32         textX;
    s32         color;
    McSaveData* save;

    color = Ui_LookupTable(arg0, 1);
    if (arg2 < ((McWork*)arg1)->field_288) {
        off  = (arg2 << 7) + 0x294;
        save = (McSaveData*)(arg1 + off);
        if (!_mcVerifySaveHdrChecksum(save)) {
            x = arg3 + arg0->panel.field_1C.s + 8;
            y = arg4 + (s16)arg0->panel.field_18.u + 0x11;
            if (((McWork*)arg1)->field_A20 == 0) {
                Text_DrawPrompt(arg0, x, y, D_80060CCC, 0x606060, 1, 0);
                return;
            }
            Text_DrawMultiLine(arg0, x, y, D_80060CCC, 0x37A78, 1, 0);
            return;
        }
        x               = arg3 + arg0->panel.field_1C.s + 2;
        y               = (arg4 + (s16)arg0->panel.field_1A.u) - 0x10;
        sp40.x          = arg0->panel.field_20.u + x;
        sp40.y          = arg0->panel.field_22.u + (y - 2);
        sp40.otIndex    = arg0->panel.field_14.s + 1;
        sp40.field_8    = 0x606060;
        sp40.glyphTable = 5;
        sp40.centerMode = 0;
        sp40.field_E    = 1;
        func_8002E53C(&sp40, D_80013B6C);
        sp50.x          = arg0->panel.field_20.u + 0x28 + x;
        sp50.y          = arg0->panel.field_22.u + y;
        sp50.otIndex    = arg0->panel.field_14.s + 1;
        sp50.field_8    = color;
        sp50.glyphTable = 0;
        sp50.centerMode = 0;
        sp50.field_E    = 3;
        func_8002E53C(&sp50, Text_FormatTime(sp20.buf, (s32)save->playTime));
        if (save->clearCount > 0) {
            x                   = (arg3 + (s16)arg0->panel.field_1E.u) - 4;
            y                   = (arg4 + (s16)arg0->panel.field_1A.u) - 0xB;
            sp60.req.x          = arg0->panel.field_20.u + (x - 0x1E);
            sp60.req.y          = arg0->panel.field_22.u + (y - 2);
            sp60.req.otIndex    = arg0->panel.field_14.s + 1;
            sp60.req.field_8    = 0x606060;
            sp60.req.glyphTable = 5;
            sp60.req.centerMode = 2;
            sp60.req.field_E    = 1;
            func_8002E53C(&sp60.req, D_80013B74);
            sp70.x          = arg0->panel.field_20.u + x;
            sp70.y          = arg0->panel.field_22.u + y;
            sp70.otIndex    = arg0->panel.field_14.s + 1;
            sp70.field_8    = color;
            sp70.glyphTable = 0;
            sp70.centerMode = 2;
            sp70.field_E    = 3;
            func_8002E53C(&sp70, Text_ItoaUnsigned(sp20.buf, (u32)save->clearCount));
            if ((s8)save->savePoint != 0xF) {
                sp80.x          = arg0->panel.field_20.u + x;
                sp80.y          = arg0->panel.field_22.u + 8 + y;
                sp80.otIndex    = arg0->panel.field_14.s + 1;
                sp80.field_8    = 0x606060;
                sp80.glyphTable = 5;
                sp80.centerMode = 2;
                sp80.field_E    = 1;
                func_8002E53C(&sp80, D_800611B8[save->gameMode]);
            }
        }
        x = arg3 + arg0->panel.field_1C.s + 4;
        y = arg4 + (s16)arg0->panel.field_18.u + 0x11;
        Text_DrawPrompt(arg0, x, y, D_80067418[(s8)save->savePoint], color, 1, 0);
        sp60.buf[0] = 0;
        Text_Strcat(sp60.buf, D_80013BA4);
        Text_Strcat(sp60.buf, Text_ItoaSigned(sp20.buf, (s32)save->saveNumber));
        Text_Strcat(sp60.buf, D_800139A8);
        sp70.x          = arg0->panel.field_20.u + (x + Text_MeasureWidth(D_80067418[(s8)save->savePoint]));
        sp70.y          = arg0->panel.field_22.u + (y - 3);
        sp70.otIndex    = arg0->panel.field_14.s + 1;
        sp70.field_8    = color;
        sp70.glyphTable = 4;
        sp70.centerMode = 0;
        sp70.field_E    = 1;
        func_8002E53C(&sp70, sp60.buf);
        textX           = arg3 + arg0->panel.field_1C.s;
        x               = textX + 2;
        y               = (arg4 + (s16)arg0->panel.field_1A.u) - 1;
        sp70.x          = arg0->panel.field_20.u + x;
        x              += 0x28;
        sp70.y          = arg0->panel.field_22.u + (y - 2);
        sp70.otIndex    = arg0->panel.field_14.s + 1;
        sp70.field_8    = 0x606060;
        sp70.glyphTable = 5;
        sp70.centerMode = 0;
        sp70.field_E    = 1;
        func_8002E53C(&sp70, D_80013BA8);
        if ((s8)save->savePoint != 0xF) {
            sp80.x          = arg0->panel.field_20.u + x;
            sp80.y          = arg0->panel.field_22.u + y;
            sp80.otIndex    = arg0->panel.field_14.s + 1;
            sp80.field_8    = 0x606060;
            sp80.glyphTable = 0;
            sp80.centerMode = 0;
            sp80.field_E    = 3;
            func_8002E53C(&sp80, Text_ItoaSigned(sp20.buf, save->playerExp));
        } else {
            sp80.x          = arg0->panel.field_20.u + x;
            sp80.y          = arg0->panel.field_22.u + y;
            sp80.otIndex    = arg0->panel.field_14.s + 1;
            sp80.field_8    = 0x606060;
            sp80.glyphTable = 0;
            sp80.centerMode = 0;
            sp80.field_E    = 3;
            func_8002E53C(&sp80, D_80013BAC);
        }
        x               = arg3 - 0x28;
        sp80.x          = arg0->panel.field_20.u + x;
        sp80.y          = arg0->panel.field_22.u + (y - 2);
        sp80.otIndex    = arg0->panel.field_14.s + 1;
        sp80.glyphTable = 5;
        sp80.field_8    = 0x606060;
        sp80.centerMode = 0;
        sp80.field_E    = 1;
        func_8002E53C(&sp80, D_80013BB0);
        if ((s8)save->savePoint != 0xF) {
            sp90.x          = arg0->panel.field_20.u + 0x1E + x;
            sp90.y          = arg0->panel.field_22.u + y;
            sp90.otIndex    = arg0->panel.field_14.s + 1;
            sp90.field_8    = 0x606060;
            sp90.glyphTable = 0;
            sp90.centerMode = 0;
            sp90.field_E    = 3;
            func_8002E53C(&sp90, Text_ItoaSigned(sp20.buf, save->playerBp));
        } else {
            sp90.x          = arg0->panel.field_20.u + 0x1E + x;
            sp90.y          = arg0->panel.field_22.u + y;
            sp90.otIndex    = arg0->panel.field_14.s + 1;
            sp90.field_8    = 0x606060;
            sp90.glyphTable = 0;
            sp90.centerMode = 0;
            sp90.field_E    = 3;
            func_8002E53C(&sp90, D_80013BAC);
        }
    } else {

        sp20.req.x          = arg0->panel.field_20.u + arg3;
        sp20.req.y          = arg0->panel.field_22.u + 5 + arg4;
        sp20.req.otIndex    = arg0->panel.field_14.s + 1;
        sp20.req.glyphTable = 4;
        sp20.req.field_8    = color;
        sp20.req.centerMode = 1;
        sp20.req.field_E    = 1;
        func_8002E53C(&sp20.req, D_80060A48);
    }
}

static u16* Mc_EncodeAsciiGlyphs(s8* arg0, u16* arg1)
{
    u16* lower;
    u16* upper;
    u16* symbol;
    s32  ch;
    s32  idx;
    u8   ch_u;

    ch_u = *arg0;
    if (*arg0 != 0) {
        lower  = Mc_GlyphsLower;
        upper  = Mc_GlyphsUpper;
        symbol = Mc_GlyphsSymbol;
        do {
            ch = (s8)ch_u;
            if (ch >= 0x61) {
                idx  = (ch - 0x61) * 2;
                idx += (s32)lower;
                goto store;
            }
            if (ch >= 0x41) {
                idx  = (ch - 0x41) * 2;
                idx += (s32)upper;
                goto store;
            }
            if (ch >= 0x20) {
                idx  = (ch - 0x20) * 2;
                idx += (s32)symbol;
            store:
                *arg1 = *(u16*)idx;
            }
            arg0++;
            ch_u = *arg0;
            arg1++;
        } while (*arg0 != 0);
    }
    *arg1 = 0;
    return arg1;
}

static void Mc_InitFileName(void)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    ptr1 = Mc_FileName;
    ptr0 = Mc_FileNameBuf;
    i    = 0;
    ch   = 0x5F;
    do {
        if (i >= 0xC) {
            *ptr0 = ch;
            *ptr1 = ch;
        }
        ptr1++;
        i++;
        ptr0++;
    } while (i < 0x14);
    *ptr0 = 0;
    *ptr1 = 0;
}

static void Mc_CopyFileName(s32 arg0)
{
    _mcCopyFileName(arg0);
}

static void Mc_WriteSaveHdrChecksum(void)
{
    s16 sum;
    u8* ptr;
    s32 limit;
    s32 i;
    s16 tmp;

    sum                           = 0;
    ptr                           = (u8*)&Mc_SaveData[0];
    ptr                          += 4;
    limit                         = 0x38;
    i                             = 0;
    Mc_SaveData[0].hdrChecksum    = 0;
    Mc_SaveData[0].hdrChecksumInv = 0xFFFF;
    do {
        i   += 1;
        tmp  = (s8)*ptr;
        sum  = sum + tmp;
        ptr += 1;
    } while (i < limit);
    Mc_SaveData[0].hdrChecksum    = sum;
    Mc_SaveData[0].hdrChecksumInv = ~sum;
    Mc_VerifySaveHdrChecksum(&Mc_SaveData[0]);
}

static s32 Mc_VerifySaveHdrChecksum(McSaveData* arg0)
{
    return _mcVerifySaveHdrChecksum(arg0);
}

/// Out-of-line form of `_mcWriteBlockChecksum`. Nothing calls it.
static void Mc_WriteBlockChecksum(u8* data, s32 size)
{
    McChecksumBlock* block;
    s16              sum;
    u32              i;

    block = (McChecksumBlock*)data;
    sum   = 0;
    data  = block->field_4;
    size -= 4;
    i     = 0;
    if (size != 0) {
        do {
            i    += 1;
            sum  += (s8)*data;
            data += 1;
        } while (i < size);
    }
    block->field_0 = sum;
    block->field_2 = ~sum;
}

void Mc_ResetSaveFlags(void)
{
    McSaveData* p;

    p               = &Mc_SaveData[0];
    p->vibration    = 0;
    p->buttonLayout = 0;
    p->musicVolume  = 0;
    p->cursorMode   = 0;
    p->soundMode    = 0;
    p->moveMode     = 0;
    CdVol_SetMixMode(1);
    Snd_ApplyVolumeTable(0);
}

static void Mc_ClearWorkBuffers(void)
{
    u8(*a)[0x6C];
    u8(*b)[0xB0];
    u8(*c)[0x24];
    u8(*d)[0xE4];
    u8(*e)[0xA4];

    a = D_800733F0;
    Mem_Set(a, 0, 0x6C);
    b = D_800734C8;
    Mem_Set(b, 0, 0xB0);
    c = D_80073628;
    Mem_Set(c, 0, 0x24);
    d = D_80073670;
    Mem_Set(d, 0, 0xE4);
    e = D_80073838;
    Mem_Set(e, 0, 0xA4);
    Mem_Set(a + 1, 0xFF, 0x6C);
    Mem_Set(b + 1, 0xFF, 0xB0);
    Mem_Set(c + 1, 0xFF, 0x24);
    Mem_Set(d + 1, 0xFF, 0xE4);
    Mem_Set(e + 1, 0xFF, 0xA4);
}

// TODO
void Mc_InitLib(void)
{
    MemCardInit(0); // 0 = No control routine
    MemCardStart();
    Mc_InitBufferSlots();
}

/// Whether a buffer's header holds the sum of its payload, as
/// `Mc_WriteBlockChecksum` stores it. Only the sum is compared, not its
/// complement. Nothing calls it.
static s32 Mc_VerifyBlockChecksum(u8* data, s32 size)
{
    McChecksumBlock* block;
    s16              sum;
    u32              i;

    block = (McChecksumBlock*)data;
    sum   = 0;
    data  = block->field_4;
    size -= 4;
    i     = 0;
    if (size != 0) {
        do {
            i    += 1;
            sum  += (s8)*data;
            data += 1;
        } while (i < size);
    }
    return ((u16)block->field_0 ^ (sum & 0xFFFF)) == 0;
}

static void func_80033C38(void)
{
}

static s32 Mc_CompareBufferHalves(void)
{
    return _mcCompareBufferHalves();
}

static void Mc_WriteSlotChecksums(void)
{
    _mcWriteSlotChecksums();
}

static void Mc_WriteFirstByteChecksum(void)
{
    _mcWriteFirstByteChecksum();
}

static s32 Mc_VerifyFirstByteChecksum(void)
{
    return _mcVerifyFirstByteChecksum();
}

static s32 Mc_VerifySlotChecksums(void)
{
    return _mcVerifySlotChecksums();
}

static void Mc_DuplicateBuffers(void)
{
    u32           i;
    u32           j;
    McBufferSlot* p;
    McBufferSlot* base;
    u8*           src;
    s32           size;
    u8*           dest;

    i    = 1;
    base = Mc_BufferSlots;
    p    = base + 1;
    do {
        src  = (u8*)p->field_0;
        size = p->field_4;
        j    = 0;
        dest = src + size;
        while (j < (u32)size) {
            j    += 1;
            *dest = *src;
            src  += 1;
            dest += 1;
        }
        i += 1;
        p += 1;
    } while (i < 9);
}

static void Mc_DrawPrompt(Task* arg0, s32 arg1)
{
    _mcDrawPrompt(arg0, arg1);
}

static void Mc_HideChildUi(Task* arg0)
{
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    child = arg0->firstChild;
    if (child != NULL) {
        obj                  = child->spawnArg2;
        flag                 = arg0->spawnArg2;
        obj->panel.field_0.w = 0;
        Ui_TeardownTree(obj, obj->owner);
        flag->panel.field_0.w = 1;
    }
}

static void Mc_WriteDataChecksum(s32 arg0, McWork* arg1)
{
    s16  sum;
    s32  count;
    u8*  src;
    s16* dst;
    s32  i;

    sum   = 0;
    count = 0x200;
    if (arg0 == 0) {
        src = Mc_DefaultChecksumSrc;
        dst = (s16*)&Mc_SaveData[0].dataChecksum;
    } else {
        src = (u8*)arg1->field_18;
        dst = (s16*)&arg1->field_A1C;
    }

    i      = 0;
    dst[0] = sum;
    dst[1] = ~sum;
    while (i < count) {
        i   += 1;
        sum += (s8)*src;
        src += 1;
    }
    dst[0] = sum;
    dst[1] = ~sum;
}

static s32 Mc_CompareSaveChecksum(McSaveData* arg0, McWork* arg1)
{
    if (arg0->cheatMode != 0) {
        return 0;
    }
    if (arg0->demoScene != 0) {
        return 0;
    }
    return arg0->dataChecksum == arg1->field_A1C;
}

static void Mc_ResetWork(Task* arg0, McWork* arg1)
{
    arg1->field_0   = 0x10;
    arg1->field_4   = 0;
    arg1->field_18  = 0;
    arg1->field_C   = 0;
    arg1->field_A18 = 0x34;
    arg1->field_A20 = 0;
    arg0->state++;
}

static void Mc_WriteSlotChecksumsEx(Task* arg0, McWork* arg1)
{
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    arg1->field_2C = 1;
    _mcWriteSlotChecksums();

    if (arg0->spawnArg1 != 0) {
        arg0->killCountdown = 2;
        arg0->state         = 0x27;
    } else {
        arg0->state = 0xE;
    }
}

static void Mc_StateAcceptMode1(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 1;
    if (MemCardAccept(arg1->field_C) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    arg1->field_4 = arg1->field_4 + 1;
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

static void Mc_StateSyncAdvance(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

static void Mc_StateDrawPromptAdvance(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0xE;
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

static void Mc_StatePromptChoiceB(Task* arg0, McWork* arg1)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;
    u8*       src;
    u8*       dst;
    s32       i;

    arg1->field_8 = 0xB;
    ret           = Mc_PromptDialogChoice(arg0, 0xB, arg1->field_0);
    if (ret != -1) {
        if (ret == 1) {
            arg0->state = 8;
        }
    } else {
        src = Mc_FileNameBuf;
        dst = Mc_FileName;
        for (i = 0; i < 0x15; i++) {
            *dst++ = *src++;
        }
        arg0->killCountdown = 0xC;
        arg0->state         = 0x27;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child       = arg0->firstChild;
                arg0->state = 2;
                if (child != NULL) {
                    obj                  = child->spawnArg2;
                    flag                 = arg0->spawnArg2;
                    obj->panel.field_0.w = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.field_0.w = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

static void Mc_StateDrawPrompt4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0xE;
    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

static void Mc_StateEnterDialog4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 0;
    arg0->state++;
    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateWriteFile(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteFile(arg1->field_C, Mc_FileName, (unsigned long*)Mc_DefaultChecksumSrc, 0,
                         0x200) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StatePromptChoiceGeneric(Task* arg0, McWork* arg1)
{
    s32 ret;

    arg1->field_8 = 7;
    ret           = Mc_PromptDialogChoice(arg0, 7, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->killCountdown = 0xC;
            arg0->state         = 0x27;
            break;
        case -1:
            arg0->state = 0x13;
            break;
    }
    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 < 0) {
        arg1->field_0 += 2;
    }
}

static void Mc_StateWriteData(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardWriteData((unsigned long*)arg1->field_18, arg1->field_1C << 7, arg1->field_20) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateClosePrompt(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;
    UiObject*     flag;
    s16           val;

    MemCardClose();
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = 0x1B;
    flag        = arg0->spawnArg2;
    if (flag != NULL) {
        val            = arg1->field_A18;
        flag->field_2E = -1;
        flag->field_2C = val;
    }
}

static void Mc_KillIfCountdown(Task* arg0, McWork* arg1)
{
    if (arg0->killCountdown != 0) {
        taskKill(arg0);
    }
}

static void Mc_StateSyncPromptFile3(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 3;
    if (Mc_PromptDialogFile(arg0, 3, arg1->field_0) != 0) {
        arg0->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->firstChild;
                if (child != NULL) {
                    obj                  = child->spawnArg2;
                    flag                 = arg0->spawnArg2;
                    obj->panel.field_0.w = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.field_0.w = syncResult;
                }
                arg0->state = 2;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StatePromptChoice9(Task* arg0, McWork* arg1)
{
    s32       ret;
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 9;
    ret           = Mc_PromptDialogSpawn(arg0, 9, arg1->field_0);
    switch (ret) {
        case 0:
            break;
        case 1:
            arg0->state = 0x16;
            break;
        case -1:
            arg0->killCountdown = 0xC;
            arg0->state         = 0x29;
            break;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    if (syncResult != -1) {
        if (syncResult == 1) {
            if (arg1->field_14 != 0) {
                child       = arg0->firstChild;
                arg0->state = 2;
                if (child != NULL) {
                    obj                  = child->spawnArg2;
                    flag                 = arg0->spawnArg2;
                    obj->panel.field_0.w = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.field_0.w = syncResult;
                }
            }
        }
    } else {
        MemCardExist(arg1->field_C);
    }
}

static void Mc_StateColdBoot(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 6;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[6];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg1->field_4 = 0xE;
    arg0->state   = arg0->state + 1;
}

static void Mc_StateSyncPrompt13(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 0x13;
    if (Mc_PromptDialogFile(arg0, 0x13, arg1->field_0) != 0) {
        arg0->state = 0x13;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            rslt = arg1->field_14;
            if (rslt == syncResult) {
                child = arg0->firstChild;
                if (child != NULL) {
                    obj                  = child->spawnArg2;
                    flag                 = arg0->spawnArg2;
                    obj->panel.field_0.w = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.field_0.w = rslt;
                }
                arg0->state = 0x14;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StateEnterPrompt0(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, arg1->field_8, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 0x13;
    }
}

static void Mc_StatePromptCountdown(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_0 -= 1;
    obj            = arg0->spawnArg2;
    idx            = arg1->field_8;
    ret            = Ui_LookupTable(obj, 1);
    obj->field_2E  = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 < -0x10) {
        arg0->killCountdown = 0;
        arg0->state         = -1;
    }
}

static void Mc_StateDrawPromptTo1F(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = 0x1F;
}

static void Mc_StateCountdownPrompt4(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_4-- <= 0) {
        arg1->field_A18 = 0x33;
        arg0->state     = 0x13;
    }
}

static void Mc_StateDrawPrompt1Advance(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[1];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

static void Mc_StateOpenSelected(Task* arg0, McWork* arg1)
{
    s32 openIdx;
    s32 openResult;

    openIdx = arg1->field_A14;
    MemCardClose();
    openResult     = MemCardOpen(arg1->field_C, arg1->field_30[openIdx].name, 1);
    arg1->field_14 = openResult;
    if (openResult == 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg0->state = 0x18;
    }
    _mcDrawPrompt(arg0, arg1->field_8);
}

static void Mc_StateReadHeader(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(arg1->field_294[arg1->field_A14], 0x200, 0x80) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateOpenNext(Task* arg0, McWork* arg1)
{
    McWork*       a1;
    Task*         a0;
    UiObject*     obj;
    s32           modeIdx;
    s32           ret;
    s32           temp_v0;
    McPromptPair* entry;
    McPromptPair* base;

    a1 = arg1;
    a0 = arg0;
    if (a1->field_14 == 0) {
        MemCardClose();
        temp_v0       = a1->field_A14 + 1;
        a1->field_A14 = temp_v0;
        if (temp_v0 < a1->field_288) {
            a0->state = 0x22;
        } else {
            a0->state = a0->state + 1;
        }
    } else {
        a0->state = 0x18;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateUiCountdown2(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg0->killCountdown -= 1;
    if (arg0->killCountdown <= 0) {
        arg0->state = 2;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateUiCountdownE(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg0->killCountdown -= 1;
    if (arg0->killCountdown <= 0) {
        arg0->state = 0xE;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateUiCountdownF(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 <= 0) {
        arg0->state = 0xF;
    }
    arg1->field_8 = 4;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[4];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateEnterPromptE(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 0xE;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0xE, 0) != 0) {
        arg0->state = 0x13;
    }
}

static void Mc_StateEnterPromptD(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 0xD;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0xD, 0) != 0) {
        arg0->state = 0x13;
    }
}

void Mc_DispatchStateTable(Task* arg0)
{
    McStateFuncTable44 sp;
    McWork*            work;
    s32                state;

    sp    = Mc_PromptStates;
    work  = &D_80071730;
    state = arg0->state;
    if (state < 0) {
        Mc_KillIfCountdown(arg0, work);
        return;
    }
    sp.funcs[state](arg0, work);
    if (work->field_4 >= 0xB5) {
        if (work->field_18 != 0) {
            memFree((void*)work->field_18);
            work->field_18 = 0;
        }
        arg0->state = 0x18;
    }
    D_80073C08 = rand();
}

static void Mc_StateInitWorkDefaults(Task* arg0, McWork* arg1)
{
    arg1->field_0                              = 0x10;
    arg1->field_8                              = 0x8;
    arg1->field_A20                            = 1;
    arg1->field_4                              = 0;
    arg1->field_18                             = 0;
    arg1->field_C                              = 0;
    gDisplayState.at100.flags.pendingPlayerPos = 0;
    arg0->state                               += 1;
}

static void Mc_StateSetOpenDefaults(Task* arg0, McWork* arg1)
{
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    arg0->state    = 7;
}

static void Mc_StateCountdownPrompt(Task* arg0, McWork* arg1)
{
    s32           status;
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    if (arg1->field_0 > 0) {
        arg1->field_0 -= 2;
    }
    if (arg1->field_0 == 0) {
        arg1->field_8 = 8;
        status        = Mc_PromptDialogChoice(arg0, 8, arg1->field_0);
        switch (status) {
            case 0:
                break;
            case 1:
                arg0->state = 7;
                break;
            case -1:
                arg0->state = 3;
                break;
        }
    } else {
        obj           = arg0->spawnArg2;
        idx           = arg1->field_8;
        ret           = Ui_LookupTable(obj, 1);
        obj->field_2E = 0;
        Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
        base  = Mc_PromptTable;
        entry = &base[idx];
        Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
        Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    }
}

static void Mc_StateCloseReturn(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    MemCardClose();
    idx           = arg1->field_8;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = 4;
    if (arg0->spawnArg2 != NULL) {
        ((UiObject*)arg0->spawnArg2)->field_2E = -1;
    }
}

static void Mc_StatePromptTimeout(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_0 -= 1;
    obj            = arg0->spawnArg2;
    idx            = arg1->field_8;
    ret            = Ui_LookupTable(obj, 1);
    obj->field_2E  = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    if (arg1->field_0 < -0x10) {
        arg0->killCountdown = 0;
        arg0->state         = arg0->state + 1;
    }
}

static void Mc_KillIfCountdownAlt(Task* arg0)
{
    if (arg0->killCountdown != 0) {
        taskKill(arg0);
    }
}

static void Mc_StateEnterPromptF(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0xF;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0xF, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 3;
    }
}

static void Mc_StateAccept(Task* arg0, McWork* arg1)
{
    arg1->field_8 = 1;
    if (MemCardAccept(arg1->field_C) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    _mcDrawPrompt(arg0, arg1->field_8);
}

static void Mc_StateSyncPrompt3(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 3;
    if (Mc_PromptDialogFile(arg0, 3, arg1->field_0) != 0) {
        arg0->state = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            if (arg1->field_14 != syncResult) {
                child = arg0->firstChild;
                if (child != NULL) {
                    obj                  = child->spawnArg2;
                    flag                 = arg0->spawnArg2;
                    obj->panel.field_0.w = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.field_0.w = syncResult;
                }
                arg0->state = 7;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StateSyncPromptA(Task* arg0, McWork* arg1)
{
    s32       syncResult;
    s32       rslt;
    Task*     child;
    UiObject* obj;
    UiObject* flag;

    arg1->field_8 = 0xA;
    if (Mc_PromptDialogFile(arg0, 0xA, arg1->field_0) != 0) {
        arg0->state = 3;
        return;
    }
    syncResult = MemCardSync(1, (long*)&arg1->field_10, (long*)&arg1->field_14);
    switch (syncResult) {
        case -1:
            MemCardExist(arg1->field_C);
            return;
        case 1:
            rslt = arg1->field_14;
            if (rslt == syncResult) {
                child = arg0->firstChild;
                if (child != NULL) {
                    obj                  = child->spawnArg2;
                    flag                 = arg0->spawnArg2;
                    obj->panel.field_0.w = 0;
                    Ui_TeardownTree(obj, obj->owner);
                    flag->panel.field_0.w = rslt;
                }
                arg0->state = 0xA;
            }
            return;
        case 0:
            return;
    }
}

static void Mc_StateDrawCurrentPrompt(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    arg1->field_4 = 4;
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

static void Mc_StateReadData(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData((unsigned long*)arg1->field_18, arg1->field_1C << 7, arg1->field_20) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateDrawPrompt1(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 = 4;
    arg1->field_8 = 1;
    obj           = arg0->spawnArg2;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[1];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
    arg0->state = arg0->state + 1;
}

static void Mc_StateGetDirentry(Task* arg0, McWork* arg1)
{
    s32           ret;
    s32           idx;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;

    arg1->field_4 -= 1;
    if (arg1->field_4 == 0) {
        arg1->field_288 = 0;
        MemCardGetDirentry(
            arg1->field_C, (char*)D_80060DC8, arg1->field_30, &arg1->field_288, 0,
            0xF);
        if (arg1->field_288 != 0) {
            arg1->field_290 = 0;
            arg1->field_A14 = 0;
            arg0->state     = arg0->state + 1;
        } else {
            arg0->state = 0xB;
        }
    }

    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateOpenDirEntry(Task* arg0, McWork* arg1)
{
    s32 idx;
    s32 openResult;

    idx = arg1->field_A14;
    MemCardClose();
    openResult     = MemCardOpen(arg1->field_C, arg1->field_30[idx].name, 1);
    arg1->field_14 = openResult;
    if (openResult == 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg0->state = 6;
    }
    _mcDrawPrompt(arg0, arg1->field_8);
}

static void Mc_StateReadSlot(Task* arg0, McWork* arg1)
{
    s32           ret;
    UiObject*     obj;
    McPromptPair* entry;
    McPromptPair* base;
    s32           idx;

    if (MemCardReadData(arg1->field_294[arg1->field_A14], 0x200, 0x80) != 0) {
        arg1->field_4 = 0;
        arg0->state   = arg0->state + 1;
    } else {
        arg1->field_4 = arg1->field_4 + 1;
    }
    obj           = arg0->spawnArg2;
    idx           = arg1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[idx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateWalkDirectory(Task* arg0, McWork* arg1)
{
    McWork*       a1;
    Task*         a0;
    UiObject*     obj;
    s32           modeIdx;
    s32           ret;
    s32           temp_v0;
    McPromptPair* entry;
    McPromptPair* base;

    a1 = arg1;
    a0 = arg0;
    if (a1->field_14 == 0) {
        MemCardClose();
        temp_v0       = a1->field_A14 + 1;
        a1->field_A14 = temp_v0;
        if (temp_v0 < a1->field_288) {
            a0->state = 0x14;
        } else {
            a0->state = a0->state + 1;
        }
    } else {
        a0->state = 0x6;
    }
    obj           = a0->spawnArg2;
    modeIdx       = a1->field_8;
    ret           = Ui_LookupTable(obj, 1);
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Mc_StrMemoryCard);
    base  = Mc_PromptTable;
    entry = &base[modeIdx];
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, -2, entry->field_0, ret, 1, 0);
    Text_DrawPrompt(obj, obj->panel.field_1C.s + 2, 0xF, entry->field_4, ret, 1, 0);
}

static void Mc_StateEnterPrompt17(Task* arg0, McWork* arg1)
{
    u8* ptr1;
    u8* ptr0;
    s32 i;
    s32 ch;

    arg1->field_8 = 0x17;
    arg1->field_4 = 0;
    if (Mc_PromptDialog(arg0, 0x17, 0) != 0) {
        ptr1 = Mc_FileName;
        ptr0 = Mc_FileNameBuf;
        i    = 0;
        ch   = 0x5F;
        do {
            if (i >= 0xC) {
                *ptr0 = ch;
                *ptr1 = ch;
            }
            ptr1++;
            i++;
            ptr0++;
        } while (i < 0x14);
        *ptr0       = 0;
        *ptr1       = 0;
        arg0->state = 3;
    }
}

void Mc_DispatchStateTable26(Task* arg0)
{
    McStateFuncTable26 sp;
    McWork*            work;

    sp   = Mc_FileSelectStates;
    work = &D_80071730;
    sp.funcs[arg0->state](arg0, work);
    if (work->field_4 >= 0xB5) {
        arg0->state = 6;
    }
}
