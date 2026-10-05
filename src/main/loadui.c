#include "main/fs.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libetc.h>

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "fs.h"
#include "main/fs_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "gameplay/effect_tasks.h"
#include "gameplay/loading.h"

/// Music volume for each music-volume setting, copied to the stack as a whole.
///
/// Wrapping the bytes in a struct makes the copy one unaligned `lwl`/`lwr`
/// word transfer rather than four byte moves.
typedef struct {
    u8 volumes[4]; // Indexed by the saved music-volume setting (0 loudest .. 3 off)
} _SndMusicVolumeTable;

/* Define BSS before API headers to preserve first-declaration order. */
static u16 D_8007A390;

static u8 D_8007A392;

static u8 D_8007A393;

u8 D_8007A394;

s16 D_8007A396;

#include "main/loadui.h"

/// Music volume for each of the four volume settings, loudest first.
static const _SndMusicVolumeTable D_80013F18;

static void Prim_DrawLoadingSprt(void);

static const _SndMusicVolumeTable D_80013F18;

u8       D_800626E8    = 0;
TaskDesc D_800626EC[6] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_TMD, 0x70 } }, Gp_EffAttachTask37 },
};

void CdCmd_EnqueueLoadFile(s32 arg0, s32 arg1, s32 arg2)
{
    s8  param2[4];
    u8* param1;

    param1 = SCRATCH_STACK_RESERVE_BYTES(8);

    param1[2] = 2;
    param1[3] = 0;
    param1[0] = arg1;
    param2[0] = arg0;

    switch ((u8)arg2) {
        case 0:
            param2[1] = 3;
            param2[2] = -8;
            param2[3] = -3;
            break;
        case 1:
            param2[2] = 0;
            param2[1] = 0;
            param2[3] = -2;
            break;
        case 2:
            param2[1] = 3;
            param2[2] = 0;
            param2[3] = -2;
            break;
        case 3:
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            break;
        case 4:
            if (D_800626E8 != 0) {
                param1[3] = gGameSession->location.loc.stage;
                param1[2] = gGameSession->location.loc.area;
                param1[0] = viewGetMappedIndex();
                param2[0] = gGameSession->spriteVariant;
                param2[1] = 1;
                param2[3] = 0;
                param2[2] = 0;
                cdCmdEnqueueUnlessSceneAudioPending(CD_COMMAND_LOAD_FILE, param1, param2);
                D_800626E8 = 0;
            }
            SCRATCH_STACK_RELEASE_BYTES(8);
            return;
    }

    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
    D_800626E8 = 1;
    SCRATCH_STACK_RELEASE_BYTES(8);
}

s32 LoadUi_PollDiskSwap(void)
{
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (D_8007A394) {
        case 0:
            D_8007A393 = Fs_GetStageDiskKind();
            if (D_8007A393 == 0) {
                break;
            }
            SndEvt_EnqueueType2(0, 8);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0x78);
            sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, 0x78);
            gDisplayState.gameMode                  = DISPLAY_GAME_MODAL;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            if (D_8007A393 == 1) {
                CdCmd_EnqueueLoadFile(1, 0x3C, 3);
                D_8007A392 = 0;
            }
            if (D_8007A393 == 2) {
                CdCmd_EnqueueLoadFile(1, 0x3D, 3);
                D_8007A392 = 1;
            }
            D_8007A390            = 5;
            queue->blockGamePause = 1;
            D_8007A394++;
            return 0xFF;
        case 1:
            if (CdCmd_IsIdle()) {
                Fs_StopCd();
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                D_8007A394++;
            }
            return 0xFF;
        case 2:
            Prim_DrawLoadingSprt();
            D_8007A390--;
            if ((D_8007A390 & 0x7FFF) == 0) {
                if (D_8007A390 & 0x8000) {
                    if (D_8007A393 == 1) {
                        D_8007A392 = 0;
                    } else if (D_8007A393 == 2) {
                        D_8007A392 = 1;
                    }
                    D_8007A390 = 5;
                    return 0xFF;
                } else {
                    D_8007A394++;
                }
            }
            return 0xFF;
        case 3:
            if ((u8)Fs_WaitDiskSwap() == 0xFF) {
                Fs_ClearDiskError();
                D_8007A392 = 2;
                D_8007A390 = 0x8080;
                D_8007A394 = 1;
                return 0xFF;
            } else {
                Fs_ClearDiskError();
                D_8007A394++;
                return 0xFF;
            }
        case 4:
            Fs_ScanIsoDirectory(0);
            if (Wip_SysFlags.discNumber != GAME_MAIN_DISC_UNKNOWN) {
                while (Fs_CdOpStatus != 0xFF) {
                    if (Fs_CdOpStatus == 0x80) {
                        return 0xFF;
                    }
                    VSync(0);
                }
                Fs_ClearDiskError();
            }
            if (Wip_SysFlags.discNumber != D_8007A393) {
                D_8007A392 = 2;
                D_8007A390 = 0x8080;
                D_8007A394 = 1;
                return 0xFF;
            } else {
                D_8007A390 = 5;
                D_8007A394++;
                return 0xFF;
            }
        case 5:
            D_8007A390--;
            if (D_8007A390 == 0) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
                gDisplayState.gameMode                  = DISPLAY_GAME_ACTIVE;
                queue->blockGamePause                   = 0;
                break;
            }
            return 0xFF;
    }
    return 0;
}

static void Prim_DrawLoadingSprt(void)
{
    SPRT*     p;
    DR_TPAGE* dr;
    u8        mode;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setlen(p, 4);
    setcode(p, 0x67);
    p->clut = GetClut(0, 0xFF);

    mode = D_8007A392;
    switch (mode) {
        case 0:
            p->w  = 0x9F;
            p->h  = 0x10;
            p->x0 = -0x50;
            p->u0 = 0;
            p->v0 = 0;
            p->y0 = 0x32;
            break;
        case 1:
            p->v0 = 0x10;
            p->w  = 0x9F;
            p->h  = 0x10;
            p->x0 = -0x50;
            p->u0 = 0;
            p->y0 = 0x32;
            break;
        case 2:
            p->v0 = 0x20;
            p->w  = 0x68;
            p->h  = 0x10;
            p->x0 = -0x32;
            p->u0 = 0;
            p->y0 = 0x32;
            break;
    }

    addPrim(gGpuCurrentOt - 0x10, p);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 0, 0, 0xF);
    addPrim(gGpuCurrentOt - 0x10, dr);
}

void Snd_ApplyVolumeTable(s32 arg0)
{
    _SndMusicVolumeTable sp10;
    u8                   temp;

    sp10 = D_80013F18;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode == 0) {
        sndOutputSetStereo(SOUND_OUTPUT_STEREO);
    } else {
        sndOutputSetStereo(SOUND_OUTPUT_MONO);
    }
    if ((arg0 & 0xFFFF) != 0) {
        D_8007A396 = arg0;
        if (gStageRoomSong != 0) {
            SndEvt_EnqueueType5(gStageRoomSong, (u8)D_8007A396);
        } else {
            SndEvt_EnqueueType5(0, (u8)D_8007A396);
        }
    } else {
        temp       = sp10.volumes[(u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume];
        D_8007A396 = temp;
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume == 3) {
            midiMuteMusic();
        } else {
            SndEvt_FlushType5Pending();
        }
        if (gStageRoomSong != 0) {
            SndEvt_EnqueueType5(gStageRoomSong, (u8)D_8007A396);
        } else {
            SndEvt_EnqueueType5(0, sp10.volumes[(u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume]);
        }
    }
}

/// Music volume for each of the four volume settings, loudest first.
static const _SndMusicVolumeTable D_80013F18 = { { 100, 64, 32, 0 } };
