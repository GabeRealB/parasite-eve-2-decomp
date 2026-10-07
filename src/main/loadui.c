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
#include "gameplay/view.h"

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

static void _loadUiDrawDiskSwapMessage(void);

/// Foreground ordering-table tag shared by the prompt sprite and its texture page.
enum { LOAD_UI_DISK_SWAP_MESSAGE_OT_INDEX = -16 };

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

void cdCmdEnqueueDisplayResource(s32 fileIdHundreds, s32 fileIndex, s32 loadProfile)
{
    enum {
        CD_COMMAND_DISPLAY_FILE_GROUP           = 2,
        CD_COMMAND_DISPLAY_FILE_KEY_STACK_BYTES = 8,
        CD_COMMAND_DISPLAY_MENU_X_PAGE_OFFSET   = -8,
        CD_COMMAND_DISPLAY_MENU_Y_OFFSET        = -3,
        CD_COMMAND_DISPLAY_PREVIEW_Y_OFFSET     = -2
    };
    struct {
        u8 fileIdHundreds;   // Hundreds component, or the current room's sprite variant
        s8 loadMode;         // A CD_COMMAND_LOAD_* policy, independent of the public profile
        s8 imageXPageOffset; // Signed displacement in 64-word VRAM pages
        s8 imageYOffset;     // Signed displacement of image headers at rows 245..255
    } loadArgs;
    struct {
        u8 fileIndex;      // Stage-zero low ID component, or the mapped view index
        u8 ignoredByQueue; // This byte is not read by the enqueue API and remains untouched
        u8 fileGroup;      // Stage-zero category, or the current area
        u8 stage;          // CDF selector (0 stage-zero library, 1..5 room resources)
    }* fileKey;

    // The queue reads only key bytes 0, 2 and 3 from this eight-byte reservation.
    fileKey = SCRATCH_STACK_RESERVE_BYTES(CD_COMMAND_DISPLAY_FILE_KEY_STACK_BYTES);

    fileKey->fileGroup      = CD_COMMAND_DISPLAY_FILE_GROUP;
    fileKey->stage          = 0;
    fileKey->fileIndex      = fileIndex;
    loadArgs.fileIdHundreds = fileIdHundreds;

    switch ((u8)loadProfile) {
        case CD_COMMAND_DISPLAY_LOAD_MENU:
            loadArgs.loadMode         = CD_COMMAND_LOAD_RELOCATE_IMAGES;
            loadArgs.imageXPageOffset = CD_COMMAND_DISPLAY_MENU_X_PAGE_OFFSET;
            loadArgs.imageYOffset     = CD_COMMAND_DISPLAY_MENU_Y_OFFSET;
            break;
        case CD_COMMAND_DISPLAY_LOAD_PREVIEW:
            loadArgs.imageXPageOffset = 0;
            loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
            loadArgs.imageYOffset     = CD_COMMAND_DISPLAY_PREVIEW_Y_OFFSET;
            break;
        case CD_COMMAND_DISPLAY_LOAD_RELOCATED_PREVIEW:
            loadArgs.loadMode         = CD_COMMAND_LOAD_RELOCATE_IMAGES;
            loadArgs.imageXPageOffset = 0;
            loadArgs.imageYOffset     = CD_COMMAND_DISPLAY_PREVIEW_Y_OFFSET;
            break;
        case CD_COMMAND_DISPLAY_LOAD_DEFAULT:
            loadArgs.imageYOffset     = 0;
            loadArgs.imageXPageOffset = 0;
            loadArgs.loadMode         = CD_COMMAND_LOAD_DEFAULT;
            break;
        case CD_COMMAND_DISPLAY_LOAD_SEEK_CURRENT_VIEW:
            // Consume the pending seek even if scene audio prevents queueing it.
            if (D_800626E8 != 0) {
                fileKey->stage            = gGameSession->location.loc.stage;
                fileKey->fileGroup        = gGameSession->location.loc.area;
                fileKey->fileIndex        = viewGetMappedIndex();
                loadArgs.fileIdHundreds   = gGameSession->spriteVariant;
                loadArgs.loadMode         = CD_COMMAND_LOAD_SEEK_ONLY;
                loadArgs.imageYOffset     = 0;
                loadArgs.imageXPageOffset = 0;
                cdCmdEnqueueUnlessSceneAudioPending(CD_COMMAND_LOAD_FILE, fileKey, &loadArgs);
                D_800626E8 = 0;
            }
            SCRATCH_STACK_RELEASE_BYTES(CD_COMMAND_DISPLAY_FILE_KEY_STACK_BYTES);
            return;
    }

    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, &loadArgs);
    D_800626E8 = 1;
    SCRATCH_STACK_RELEASE_BYTES(CD_COMMAND_DISPLAY_FILE_KEY_STACK_BYTES);
}

s32 LoadUi_PollDiskSwap(void)
{
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (D_8007A394) {
        case 0:
            D_8007A393 = fsGetRequiredStageDisc();
            if (D_8007A393 == 0) {
                break;
            }
            sndEvtRequestMidiStop(0, 8);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0x78);
            sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, 0x78);
            gDisplayState.gameMode                  = DISPLAY_GAME_MODAL;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            if (D_8007A393 == 1) {
                cdCmdEnqueueDisplayResource(1, 0x3C, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                D_8007A392 = 0;
            }
            if (D_8007A393 == 2) {
                cdCmdEnqueueDisplayResource(1, 0x3D, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                D_8007A392 = 1;
            }
            D_8007A390            = 5;
            queue->blockGamePause = 1;
            D_8007A394++;
            return 0xFF;
        case 1:
            if (cdCmdIsIdle()) {
                cdSyncStopDisc();
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                D_8007A394++;
            }
            return 0xFF;
        case 2:
            _loadUiDrawDiskSwapMessage();
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
            if (cdSyncWaitForDiscSwap() == CD_SYNC_DISC_SWAP_ERROR) {
                cdSyncWaitForCommandCompletion();
                D_8007A392 = 2;
                D_8007A390 = 0x8080;
                D_8007A394 = 1;
                return 0xFF;
            } else {
                cdSyncWaitForCommandCompletion();
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
                cdSyncWaitForCommandCompletion();
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

/// Prepends the loaded disk-prompt texture page to its foreground ordering tag.
///
/// Selects 4-bit VRAM (960, 0), with dithering and display-area drawing disabled.
/// Requires one DR_TPAGE at the word-aligned cursor and the prompt's OT tag.
static inline void _loadUiPrependDiskSwapTexturePage(void)
{
    DR_TPAGE* drawMode;

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 0, 0, getTPage(0, 0, 960, 0));
    addPrim(gGpuCurrentOt + LOAD_UI_DISK_SWAP_MESSAGE_OT_INDEX, drawMode);
}

/// Draws the selected disk-swap message from the loaded prompt texture.
///
/// The selector is 0 (disc 1), 1 (disc 2) or 2 (rejected disc/read failure).
/// Uses a raw semitransparent 4-bit sprite and the palette at VRAM (0, 255).
/// Requires a loaded prompt sheet at VRAM (960, 0), a word-aligned primitive
/// cursor with sizeof(SPRT) + sizeof(DR_TPAGE) free bytes, and foreground OT
/// tag -16. Packets borrow that storage until the current frame is drawn.
static void _loadUiDrawDiskSwapMessage(void)
{
    enum {
        LOAD_UI_DISK_SWAP_MESSAGE_DISC_1      = 0,
        LOAD_UI_DISK_SWAP_MESSAGE_DISC_2      = 1,
        LOAD_UI_DISK_SWAP_MESSAGE_REJECTED    = 2,
        LOAD_UI_DISK_SWAP_MESSAGE_ROW_HEIGHT  = 16,
        LOAD_UI_DISK_SWAP_MESSAGE_CLUT_Y      = 255,
        LOAD_UI_DISK_SWAP_MESSAGE_SPRITE_CODE = 0x67 // Raw texture with semitransparency
    };
    SPRT* sprite;
    u8    messageKind;

    sprite         = gGpuPrimCursor;
    gGpuPrimCursor = sprite + 1;
    setSprt(sprite);
    setcode(sprite, LOAD_UI_DISK_SWAP_MESSAGE_SPRITE_CODE);
    sprite->clut = GetClut(0, LOAD_UI_DISK_SWAP_MESSAGE_CLUT_Y);

    messageKind = D_8007A392;
    switch (messageKind) {
        case LOAD_UI_DISK_SWAP_MESSAGE_DISC_1:
            sprite->w  = 159;
            sprite->h  = LOAD_UI_DISK_SWAP_MESSAGE_ROW_HEIGHT;
            sprite->x0 = -80;
            sprite->u0 = 0;
            sprite->v0 = 0;
            sprite->y0 = 50;
            break;
        case LOAD_UI_DISK_SWAP_MESSAGE_DISC_2:
            sprite->v0 = LOAD_UI_DISK_SWAP_MESSAGE_ROW_HEIGHT;
            sprite->w  = 159;
            sprite->h  = LOAD_UI_DISK_SWAP_MESSAGE_ROW_HEIGHT;
            sprite->x0 = -80;
            sprite->u0 = 0;
            sprite->y0 = 50;
            break;
        case LOAD_UI_DISK_SWAP_MESSAGE_REJECTED:
            sprite->v0 = 2 * LOAD_UI_DISK_SWAP_MESSAGE_ROW_HEIGHT;
            sprite->w  = 104;
            sprite->h  = LOAD_UI_DISK_SWAP_MESSAGE_ROW_HEIGHT;
            sprite->x0 = -50;
            sprite->u0 = 0;
            sprite->y0 = 50;
            break;
    }

    // Prepending the page after the sprite makes it execute before the sprite.
    addPrim(gGpuCurrentOt + LOAD_UI_DISK_SWAP_MESSAGE_OT_INDEX, sprite);
    _loadUiPrependDiskSwapTexturePage();
}

void midiApplyMusicVolume(u16 volumeOverride)
{
    enum {
        SOUND_OUTPUT_SAVED_STEREO   = 0,
        MIDI_MUSIC_SAVED_VOLUME_OFF = 3,
        MIDI_MUSIC_ALL_SEQUENCES    = 0
    };
    _SndMusicVolumeTable volumeTable;
    u8                   savedVolume;

    /// Applies the saved music level, changing the mute gate before queuing that gain.
    ///
    /// `table` must be a side-effect-free `_SndMusicVolumeTable` expression
    /// for saved options 0..3. It is evaluated again after the gate change
    /// when no room song is selected.
    /// Uses the live save, room song and music-level cache, the local selectors
    /// and the local `savedVolume` temporary. Expands to a compound statement.
#define MIDI_APPLY_SAVED_MUSIC_VOLUME(table)                                                                                              \
    {                                                                                                                                     \
        savedVolume = (table).volumes[(u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume];                                          \
        D_8007A396  = savedVolume;                                                                                                        \
        /* Change the gate before the selected gain; unmuting queues the cached gain. */                                                  \
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume == MIDI_MUSIC_SAVED_VOLUME_OFF) {                                        \
            midiMuteMusic();                                                                                                              \
        } else {                                                                                                                          \
            midiUnmuteMusic();                                                                                                            \
        }                                                                                                                                 \
        if (gStageRoomSong != MIDI_MUSIC_ALL_SEQUENCES) {                                                                                 \
            sndEvtRequestMidiVolume(gStageRoomSong, (u8)D_8007A396);                                                                      \
        } else {                                                                                                                          \
            sndEvtRequestMidiVolume(MIDI_MUSIC_ALL_SEQUENCES, (table).volumes[(u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume]); \
        }                                                                                                                                 \
    }

    volumeTable = D_80013F18;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode == SOUND_OUTPUT_SAVED_STEREO) {
        sndOutputSetStereo(SOUND_OUTPUT_STEREO);
    } else {
        sndOutputSetStereo(SOUND_OUTPUT_MONO);
    }
    if (volumeOverride != MIDI_MUSIC_VOLUME_SAVED) {
        D_8007A396 = volumeOverride;
        if (gStageRoomSong != MIDI_MUSIC_ALL_SEQUENCES) {
            sndEvtRequestMidiVolume(gStageRoomSong, (u8)D_8007A396);
        } else {
            sndEvtRequestMidiVolume(MIDI_MUSIC_ALL_SEQUENCES, (u8)D_8007A396);
        }
    } else
        MIDI_APPLY_SAVED_MUSIC_VOLUME(volumeTable);
#undef MIDI_APPLY_SAVED_MUSIC_VOLUME
}

/// Music volume for each of the four volume settings, loudest first.
static const _SndMusicVolumeTable D_80013F18 = { { 100, 64, 32, 0 } };
