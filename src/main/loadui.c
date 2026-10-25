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
#include "sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/gamemain.h"

#include "gameplay/effect_tasks.h"
#include "gameplay/view.h"

/// Music volume for each music-volume setting, copied to the stack as a whole.
///
/// Wrapping the bytes in a struct makes the copy one unaligned `lwl`/`lwr`
/// word transfer rather than four byte moves.
typedef struct {
    u8 volumes[4]; // Indexed by the saved music-volume setting (0 loudest .. 3 off)
} _SndMusicVolumeTable;

/// Disk-swap delay in poll calls, with bit 15 requesting the required-disc message.
///
/// Bits 0..14 count down calls in the delay phase; retries start at 128 and
/// then restore the normal message for five calls. Dismissal uses five calls.
static u16 _gLoadUiDiskSwapCountdown;

/// Disk-swap message selector (0 disc 1, 1 disc 2, 2 rejected disc/read failure).
static u8 _gLoadUiDiskSwapMessageKind;

/// Required GAME_MAIN_DISC_* value retained throughout the pending disk swap.
///
/// Zero means no missing stage disc; 1 and 2 select the required main disc.
static u8 _gLoadUiDiskSwapRequiredDisc;

/* Define BSS before API headers to preserve first-declaration order. */

u8 gLoadUiDiskSwapState;

u16 gMidiMusicVolume;

#include "main/loadui.h"
#include "main/sound.h"

static const _SndMusicVolumeTable _gMidiSavedMusicVolumes;

static void _loadUiDrawDiskSwapMessage(void);

/// Foreground ordering-table tag shared by the prompt sprite and its texture page.
enum { LOAD_UI_DISK_SWAP_MESSAGE_OT_INDEX = -16 };

/// Disk-swap message selectors shared by polling, retry and drawing.
enum {
    LOAD_UI_DISK_SWAP_MESSAGE_DISC_1   = 0,
    LOAD_UI_DISK_SWAP_MESSAGE_DISC_2   = 1,
    LOAD_UI_DISK_SWAP_MESSAGE_REJECTED = 2,
};

/// Packed poll countdown: low 15 bits count calls; bit 15 restores the normal prompt.
enum {
    LOAD_UI_DISK_SWAP_DELAY_POLLS     = 5,
    LOAD_UI_DISK_SWAP_REJECTION_POLLS = 128,
    LOAD_UI_DISK_SWAP_COUNTDOWN_MASK  = 0x7FFF,
    LOAD_UI_DISK_SWAP_RESTORE_PROMPT  = 0x8000,
};

u8       gCdCmdViewSeekPending = CD_COMMAND_VIEW_SEEK_CLEAR;
TaskDesc gTaskBank8Descs[6]    = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, taskKill },
    { { { TASK_BODY_TMD, TASK_PRIORITY_EFFECT } }, effectBurstModelPartTask },
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
    CdCmdFileLoadArgs loadArgs;
    CdCmdFileKey*     fileKey;

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
            if (gCdCmdViewSeekPending != 0) {
                fileKey->stage            = gGameSession->location.stage;
                fileKey->fileGroup        = gGameSession->location.area;
                fileKey->fileIndex        = viewGetMappedIndex();
                loadArgs.fileIdHundreds   = gGameSession->spriteVariant;
                loadArgs.loadMode         = CD_COMMAND_LOAD_SEEK_ONLY;
                loadArgs.imageYOffset     = 0;
                loadArgs.imageXPageOffset = 0;
                cdCmdEnqueueUnlessSceneAudioPending(CD_COMMAND_LOAD_FILE, fileKey, &loadArgs);
                gCdCmdViewSeekPending = CD_COMMAND_VIEW_SEEK_CLEAR;
            }
            SCRATCH_STACK_RELEASE_BYTES(CD_COMMAND_DISPLAY_FILE_KEY_STACK_BYTES);
            return;
    }

    cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKey, &loadArgs);
    gCdCmdViewSeekPending = CD_COMMAND_VIEW_SEEK_PENDING;
    SCRATCH_STACK_RELEASE_BYTES(CD_COMMAND_DISPLAY_FILE_KEY_STACK_BYTES);
}

/// Restarts the stopped-drive prompt with a timed rejection message.
///
/// Requires an active disk-swap prompt and the recorded required-disc number.
/// Retains its loaded texture and restarts the CD-idle/drive-stop phase; the
/// following delay phase shows rejection for 128 polls. The countdown's high
/// bit then restores the required-disc message for five polls before probing
/// again. Pause blocking and modal presentation remain the caller's responsibility.
static inline void _loadUiRestartRejectedDiskSwapPrompt(void)
{
    _gLoadUiDiskSwapMessageKind = LOAD_UI_DISK_SWAP_MESSAGE_REJECTED;
    _gLoadUiDiskSwapCountdown   = LOAD_UI_DISK_SWAP_RESTORE_PROMPT | LOAD_UI_DISK_SWAP_REJECTION_POLLS;
    gLoadUiDiskSwapState        = LOAD_UI_DISK_SWAP_WAIT_PROMPT_LOAD;
}

u8 loadUiPollDiskSwap(void)
{
    enum {
        LOAD_UI_DISK_SWAP_RESOURCE_HUNDREDS   = 1,
        LOAD_UI_DISK_SWAP_DISC_1_FILE_INDEX   = 0x3C,
        LOAD_UI_DISK_SWAP_DISC_2_FILE_INDEX   = 0x3D,
        LOAD_UI_DISK_SWAP_MIDI_ALL_SEQUENCES  = 0,
        LOAD_UI_DISK_SWAP_MIDI_FADE_TICKS     = 8,
        LOAD_UI_DISK_SWAP_SCRIPT_STOP_CONTROL = 0x78,
    };
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (gLoadUiDiskSwapState) {
        case LOAD_UI_DISK_SWAP_CHECK_REQUIRED_DISC:
            _gLoadUiDiskSwapRequiredDisc = fsGetRequiredStageDisc();
            if (_gLoadUiDiskSwapRequiredDisc == GAME_MAIN_DISC_UNKNOWN) {
                break;
            }
            // Silence the loaded scene before borrowing its presentation for the prompt.
            sndEvtRequestMidiStop(LOAD_UI_DISK_SWAP_MIDI_ALL_SEQUENCES, LOAD_UI_DISK_SWAP_MIDI_FADE_TICKS);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, LOAD_UI_DISK_SWAP_SCRIPT_STOP_CONTROL);
            sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, LOAD_UI_DISK_SWAP_SCRIPT_STOP_CONTROL);
            gDisplayState.gameMode                  = DISPLAY_GAME_MODAL;
            gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
            if (_gLoadUiDiskSwapRequiredDisc == GAME_MAIN_DISC_1) {
                cdCmdEnqueueDisplayResource(LOAD_UI_DISK_SWAP_RESOURCE_HUNDREDS, LOAD_UI_DISK_SWAP_DISC_1_FILE_INDEX, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                _gLoadUiDiskSwapMessageKind = LOAD_UI_DISK_SWAP_MESSAGE_DISC_1;
            }
            if (_gLoadUiDiskSwapRequiredDisc == GAME_MAIN_DISC_2) {
                cdCmdEnqueueDisplayResource(LOAD_UI_DISK_SWAP_RESOURCE_HUNDREDS, LOAD_UI_DISK_SWAP_DISC_2_FILE_INDEX, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                _gLoadUiDiskSwapMessageKind = LOAD_UI_DISK_SWAP_MESSAGE_DISC_2;
            }
            _gLoadUiDiskSwapCountdown = LOAD_UI_DISK_SWAP_DELAY_POLLS;
            queue->blockGamePause     = 1;
            gLoadUiDiskSwapState++;
            return LOAD_UI_DISK_SWAP_PENDING;
        case LOAD_UI_DISK_SWAP_WAIT_PROMPT_LOAD:
            if (cdCmdIsIdle()) {
                cdSyncStopDisc();
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
                gLoadUiDiskSwapState++;
            }
            return LOAD_UI_DISK_SWAP_PENDING;
        case LOAD_UI_DISK_SWAP_DELAY_BEFORE_PROBE:
            // Countdown units are calls in this state; bit 15 requests the normal prompt again.
            _loadUiDrawDiskSwapMessage();
            _gLoadUiDiskSwapCountdown--;
            if ((_gLoadUiDiskSwapCountdown & LOAD_UI_DISK_SWAP_COUNTDOWN_MASK) == 0) {
                if (_gLoadUiDiskSwapCountdown & LOAD_UI_DISK_SWAP_RESTORE_PROMPT) {
                    if (_gLoadUiDiskSwapRequiredDisc == GAME_MAIN_DISC_1) {
                        _gLoadUiDiskSwapMessageKind = LOAD_UI_DISK_SWAP_MESSAGE_DISC_1;
                    } else if (_gLoadUiDiskSwapRequiredDisc == GAME_MAIN_DISC_2) {
                        _gLoadUiDiskSwapMessageKind = LOAD_UI_DISK_SWAP_MESSAGE_DISC_2;
                    }
                    _gLoadUiDiskSwapCountdown = LOAD_UI_DISK_SWAP_DELAY_POLLS;
                    return LOAD_UI_DISK_SWAP_PENDING;
                } else {
                    gLoadUiDiskSwapState++;
                }
            }
            return LOAD_UI_DISK_SWAP_PENDING;
        case LOAD_UI_DISK_SWAP_PROBE_DISC:
            // The drive probe blocks through a tray cycle; its result does not identify the game disc.
            if (cdSyncWaitForDiscSwap() == CD_SYNC_DISC_SWAP_ERROR) {
                cdSyncWaitForCommandCompletion();
                _loadUiRestartRejectedDiskSwapPrompt();
                return LOAD_UI_DISK_SWAP_PENDING;
            } else {
                cdSyncWaitForCommandCompletion();
                gLoadUiDiskSwapState++;
                return LOAD_UI_DISK_SWAP_PENDING;
            }
        case LOAD_UI_DISK_SWAP_VALIDATE_DISC:
            // Directory discovery identifies the disc and starts an asynchronous HED read.
            fsScanIsoDirectory(0);
            if (gGameMainPersistentState.discNumber != GAME_MAIN_DISC_UNKNOWN) {
                while (gFsCdOperationStatus != FILE_SYSTEM_CD_OPERATION_COMPLETE) {
                    if (gFsCdOperationStatus == FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST) {
                        return LOAD_UI_DISK_SWAP_PENDING;
                    }
                    VSync(0);
                }
                cdSyncWaitForCommandCompletion();
            }
            if (gGameMainPersistentState.discNumber != _gLoadUiDiskSwapRequiredDisc) {
                _loadUiRestartRejectedDiskSwapPrompt();
                return LOAD_UI_DISK_SWAP_PENDING;
            } else {
                _gLoadUiDiskSwapCountdown = LOAD_UI_DISK_SWAP_DELAY_POLLS;
                gLoadUiDiskSwapState++;
                return LOAD_UI_DISK_SWAP_PENDING;
            }
        case LOAD_UI_DISK_SWAP_DISMISS_PROMPT:
            _gLoadUiDiskSwapCountdown--;
            if (_gLoadUiDiskSwapCountdown == 0) {
                gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
                gDisplayState.gameMode                  = DISPLAY_GAME_ACTIVE;
                queue->blockGamePause                   = 0;
                break;
            }
            return LOAD_UI_DISK_SWAP_PENDING;
    }
    return LOAD_UI_DISK_SWAP_COMPLETE;
}

/// Prepends the loaded disk-prompt texture page to its foreground ordering tag.
///
/// Selects 4-bit VRAM (960, 0), average semitransparency, with dithering and
/// display-area drawing disabled. Requires a word-aligned primitive cursor
/// with sizeof(DR_TPAGE) free bytes and foreground OT tag -16. Advances the
/// cursor by one complete packet; the packet stays live until GPU use finishes.
/// Prepend after the prompt sprite so this command executes before that sprite.
static inline void _loadUiPrependDiskSwapTexturePage(void)
{
    enum {
        LOAD_UI_DISK_SWAP_TEXTURE_DEPTH_4BIT = 0,
        LOAD_UI_DISK_SWAP_TEXTURE_PAGE_X     = 960,
        LOAD_UI_DISK_SWAP_TEXTURE_PAGE_Y     = 0
    };
    DR_TPAGE* drawMode;

    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, false,
                 getTPage(LOAD_UI_DISK_SWAP_TEXTURE_DEPTH_4BIT, GPU_BLEND_AVERAGE,
                          LOAD_UI_DISK_SWAP_TEXTURE_PAGE_X, LOAD_UI_DISK_SWAP_TEXTURE_PAGE_Y));
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

    messageKind = _gLoadUiDiskSwapMessageKind;
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
        MIDI_MUSIC_ALL_SEQUENCES = 0
    };
    _SndMusicVolumeTable volumeTable;

    /// Applies the saved music level, changing the mute gate before queuing that gain.
    ///
    /// `table` must be a side-effect-free `_SndMusicVolumeTable` expression
    /// for saved options 0..3. It is evaluated again after the gate change
    /// when no room song is selected.
    /// Updates the live music-level cache from gains 100/64/32/0, then queues
    /// the selected room song's gain, or sequence zero for all songs. Sound
    /// request allocation can fail; the cache and mute gate still change.
    /// Uses the live save, room song, cache, local enum selectors and sound APIs;
    /// captures no caller temporaries. Arguments cannot name `_midiSavedVolume`.
    /// Expands to a compound statement, defined only around this saved-volume path.
#define MIDI_APPLY_SAVED_MUSIC_VOLUME(table)                                                                                              \
    {                                                                                                                                     \
        u8 _midiSavedVolume;                                                                                                              \
        _midiSavedVolume = (table).volumes[(u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume];                                     \
        gMidiMusicVolume = _midiSavedVolume;                                                                                              \
        /* Change the gate before the selected gain; unmuting queues the cached gain. */                                                  \
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume == MEMORY_CARD_OPTION_MUSIC_OFF) {                                       \
            midiMuteMusic();                                                                                                              \
        } else {                                                                                                                          \
            midiUnmuteMusic();                                                                                                            \
        }                                                                                                                                 \
        if (gStageRoomMusicSequence != MIDI_MUSIC_ALL_SEQUENCES) {                                                                        \
            sndEvtRequestMidiVolume(gStageRoomMusicSequence, (u8)gMidiMusicVolume);                                                       \
        } else {                                                                                                                          \
            sndEvtRequestMidiVolume(MIDI_MUSIC_ALL_SEQUENCES, (table).volumes[(u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.musicVolume]); \
        }                                                                                                                                 \
    }

    volumeTable = _gMidiSavedMusicVolumes;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.soundMode == MEMORY_CARD_OPTION_SOUND_STEREO) {
        sndOutputSetStereo(SOUND_OUTPUT_STEREO);
    } else {
        sndOutputSetStereo(SOUND_OUTPUT_MONO);
    }
    if (volumeOverride != MIDI_MUSIC_VOLUME_SAVED) {
        gMidiMusicVolume = volumeOverride;
        if (gStageRoomMusicSequence != MIDI_MUSIC_ALL_SEQUENCES) {
            sndEvtRequestMidiVolume(gStageRoomMusicSequence, (u8)gMidiMusicVolume);
        } else {
            sndEvtRequestMidiVolume(MIDI_MUSIC_ALL_SEQUENCES, (u8)gMidiMusicVolume);
        }
    } else
        MIDI_APPLY_SAVED_MUSIC_VOLUME(volumeTable);
#undef MIDI_APPLY_SAVED_MUSIC_VOLUME
}

/// MIDI gain for each saved music-volume option (0 loudest, 1 medium, 2 low, 3 off).
///
/// The four bytes are copied together into the music-volume setter's local table.
static const _SndMusicVolumeTable _gMidiSavedMusicVolumes = { { 100, 64, 32, 0 } };
