#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/areas.h"
#include "main/display.h"
#include "display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "fs_types.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "text.h"

/* Define BSS before API headers to preserve first-declaration order. */
s16 Fs_BootLoadSlot;

u16 Fs_BootLoadPhase;

u16 D5B498_8006AC9C;

static u8 D_8006AC9E;

static u8 D_8006AC9F;

static s16 D_8006ACA0;

static s16 D_8006ACA2;

static s16 D_8006ACA4;

static s16 D_8006ACA6;

static s16 D_8006ACA8;

void* Fs_BootTimSecondary;

void* Fs_BootTimPrimary;

// Fade/clear color; written as halfword, often re-read as byte for TILE RGB.
static s16 D_8006ACB4;

BootLoadDestination Fs_LoadParams;

/// Unreferenced.
static s32 D_8006ACBC;

s16 D5B498_8006ACC0;

#include "fs.h"

/// September 3,  1999  7:07PM\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistEvening[];

static TextStream BootCaption_MistEvening;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MistEveningSecondary;

/// September 3,  1999  7:45PM\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_AkropolisEvening[];

static TextStream BootCaption_AkropolisEvening;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_AkropolisEveningSecondary;

/// September 4,  1999  01:02PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertAfternoon[];

static TextStream BootCaption_DesertAfternoon;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertAfternoonSecondary;

/// September 5,  1999  01:95AM\nMesa on the outskirts of \nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldMesaNight[];

static TextStream BootCaption_DryfieldMesaNight;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DryfieldMesaNightSecondary;

/// September 5,  1999  5:26PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseEvening[];

static TextStream BootCaption_WhiteHouseEvening;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseEveningSecondary;

/// September 5,  1999  2:41PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertReturn[];

static TextStream BootCaption_DesertReturn;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertReturnSecondary;

/// September 6,  1999  8:94PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseAftermath[];

static TextStream BootCaption_WhiteHouseAftermath;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseAftermathSecondary;

/// September 0, 1990  6:05PM\nNature Museum,  New York
static u8 BootCaptionText_MuseumFlashback[];

static TextStream BootCaption_MuseumFlashback;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MuseumFlashbackSecondary;

/// September 3,  1999  \nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistArrival[];

static TextStream BootCaption_MistArrival;

/// September 3,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistUnused[];

/// Retained unused caption variant.
static TextStream BootCaption_MistUnused;

/// September 3,  1999\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_Akropolis[];

static TextStream BootCaption_Akropolis;

/// September 4,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistDeparture[];

static TextStream BootCaption_MistDeparture;

/// September 4,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_Dryfield[];

static TextStream BootCaption_Dryfield;

/// September 5,  1999\nMine shaft, Mojave Desert
static u8 BootCaptionText_Mine[];

static TextStream BootCaption_Mine;

/// September 5,  1999\nDwelling level, Shelter
static u8 BootCaptionText_ShelterDwelling[];

static TextStream BootCaption_ShelterDwelling;

/// September 5,  1999\nWaste level, Shelter
static u8 BootCaptionText_ShelterWaste[];

static TextStream BootCaption_ShelterWaste;

/// September 5,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldReturn[];

static TextStream BootCaption_DryfieldReturn;

/// September 5,  1999\nLaboratory level, Shelter
static u8 BootCaptionText_ShelterLaboratory[];

static TextStream BootCaption_ShelterLaboratory;

/// September 5,  1999\nPod shaft, Shelter
static u8 BootCaptionText_ShelterPod[];

static TextStream BootCaption_ShelterPod;

/// September 5,  1999\nExperiment level, Shelter
static u8 BootCaptionText_ShelterExperiment[];

static TextStream BootCaption_ShelterExperiment;

/// September 5,  1999\nHeliport, Shelter
static u8 BootCaptionText_ShelterHeliport[];

static TextStream BootCaption_ShelterHeliport;

static s32 _fadeBootImageToBlack(s16 levelStep);

static void _fadeBeginBootImageReveal(void);

static s32 _fadeBootImageFromBlack(s32 levelStep);

static void _gameFlowSelectAcropolisLoadScreenAssets(u8* imageFileIndex);

static void _gameFlowSelectDryfieldLoadScreenAssets(u8* imageFileIndex);

static void _gameFlowSelectDryfieldNightLoadScreenAssets(u8* imageFileIndex);

static void _gameFlowSelectMineShelterLoadScreenAssets(u8* imageFileIndex);

/// Story-chapter keys used by the loading-screen selectors.
enum {
    GAME_FLOW_LOAD_CHAPTER_MIST_ARRIVAL = 0,
    GAME_FLOW_LOAD_CHAPTER_3            = 3,
    GAME_FLOW_LOAD_CHAPTER_4            = 4,
    GAME_FLOW_LOAD_CHAPTER_5            = 5,
    GAME_FLOW_LOAD_CHAPTER_6            = 6,
};

/// Category-zero file indices paired with each loading-screen caption context.
enum {
    GAME_FLOW_LOAD_FILE_CATEGORY                          = 0,
    GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION                 = 3,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_MOTEL_ROOM_6            = 4,
    GAME_FLOW_LOAD_IMAGE_ACROPOLIS_SQUARE                 = 5,
    GAME_FLOW_LOAD_IMAGE_ACROPOLIS_OTHER                  = 6,
    GAME_FLOW_LOAD_IMAGE_MIST_ARRIVAL                     = 7,
    GAME_FLOW_LOAD_IMAGE_MIST_DEPARTURE                   = 8,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_MAIN_STREET             = 10,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_TRAILER_COACH           = 11,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_3   = 12,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_3 = 13,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_3   = 14,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_4   = 15,
    GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_4            = 16,
    GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_4  = 17,
    GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_4    = 18,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_4   = 19,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_4 = 20,
    GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_4     = 21,
    GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_4            = 22,
    GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_5            = 23,
    GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_5  = 24,
    GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_5    = 25,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_5   = 26,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_5 = 27,
    GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_5     = 28,
    GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_5            = 29,
    GAME_FLOW_LOAD_IMAGE_SHELTER_NURSERY                  = 30,
    GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_5        = 31,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_5   = 32,
    GAME_FLOW_LOAD_IMAGE_SHELTER_HELIPORT                 = 35,
    GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_6  = 36,
    GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_6     = 37,
    GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_6        = 38,
    GAME_FLOW_LOAD_IMAGE_DRYFIELD_OTHER                   = 39,
    GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_4        = 40,
};

/// September 3,  1999  7:07PM\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistEvening[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xBB,
    0xBE,
    0xB4,
    0xBB,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MistEvening = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_MistEvening, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MistEveningSecondary = { 0 };
/// September 3,  1999  7:45PM\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_AkropolisEvening[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xBB,
    0xBE,
    0xB8,
    0xB9,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x00,
    0x24,
    0x2B,
    0x28,
    0x29,
    0x28,
    0x25,
    0x22,
    0x2C,
    0x45,
    0x13,
    0x28,
    0x30,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_AkropolisEvening = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_AkropolisEvening, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_AkropolisEveningSecondary = { 0 };
/// September 4,  1999  01:02PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertAfternoon[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB4,
    0xB5,
    0xBE,
    0xB4,
    0xB6,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x2F,
    0x1A,
    0x1D,
    0x1A,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DesertAfternoon = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_DesertAfternoon, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertAfternoonSecondary = { 0 };
/// September 5,  1999  01:95AM\nMesa on the outskirts of \nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldMesaNight[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB4,
    0xB5,
    0xBE,
    0xBD,
    0xB9,
    0x80,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x1E,
    0x2C,
    0x1A,
    0x45,
    0x28,
    0x27,
    0x45,
    0x2D,
    0x21,
    0x1E,
    0x45,
    0x28,
    0x2E,
    0x2D,
    0x2C,
    0x24,
    0x22,
    0x2B,
    0x2D,
    0x2C,
    0x45,
    0x28,
    0x1F,
    0x45,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DryfieldMesaNight = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_DryfieldMesaNight, Caption_Glyphs, 13, 150, 216, 40 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DryfieldMesaNightSecondary = { 0 };
/// September 5,  1999  5:26PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseEvening[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB9,
    0xBE,
    0xB6,
    0xBA,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x21,
    0x22,
    0x2D,
    0x1E,
    0x45,
    0x07,
    0x28,
    0x2E,
    0x2C,
    0x1E,
    0x41,
    0x45,
    0x45,
    0x16,
    0x1A,
    0x2C,
    0x21,
    0x22,
    0x27,
    0x20,
    0x2D,
    0x28,
    0x27,
    0x45,
    0x03,
    0x42,
    0x02,
    0x42,
    TEXT_STREAM_END,
};
static TextStream BootCaption_WhiteHouseEvening = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_WhiteHouseEvening, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseEveningSecondary = { 0 };
/// September 5,  1999  2:41PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertReturn[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB6,
    0xBE,
    0xB8,
    0xB5,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x2F,
    0x1A,
    0x1D,
    0x1A,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DesertReturn = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_DesertReturn, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertReturnSecondary = { 0 };
/// September 6,  1999  8:94PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseAftermath[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x3A,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xBC,
    0xBE,
    0xBD,
    0xB8,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x21,
    0x22,
    0x2D,
    0x1E,
    0x45,
    0x07,
    0x28,
    0x2E,
    0x2C,
    0x1E,
    0x41,
    0x45,
    0x45,
    0x16,
    0x1A,
    0x2C,
    0x21,
    0x22,
    0x27,
    0x20,
    0x2D,
    0x28,
    0x27,
    0x45,
    0x03,
    0x42,
    0x02,
    0x42,
    TEXT_STREAM_END,
};
static TextStream BootCaption_WhiteHouseAftermath = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_WhiteHouseAftermath, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseAftermathSecondary = { 0 };
/// September 0, 1990  6:05PM\nNature Museum,  New York
static u8 BootCaptionText_MuseumFlashback[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x34,
    0x41,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x34,
    0x45,
    0x45,
    0x3A,
    0x3E,
    0x34,
    0x39,
    0x0F,
    0x0C,
    TEXT_STREAM_LINE_BREAK,
    0x0D,
    0x1A,
    0x2D,
    0x2E,
    0x2B,
    0x1E,
    0x45,
    0x0C,
    0x2E,
    0x2C,
    0x1E,
    0x2E,
    0x26,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x30,
    0x45,
    0x18,
    0x28,
    0x2B,
    0x24,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MuseumFlashback = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_MuseumFlashback, Caption_Glyphs, 13, 300, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MuseumFlashbackSecondary = { 0 };
/// September 3,  1999  \nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistArrival[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MistArrival = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_MistArrival, Caption_Glyphs, 13, 107, 216, 29 };
/// September 3,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistUnused[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Retained unused caption variant.
static TextStream BootCaption_MistUnused = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_MistUnused, Caption_Glyphs, 13, 300, 216, 29 };
/// September 3,  1999\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_Akropolis[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x00,
    0x24,
    0x2B,
    0x28,
    0x29,
    0x28,
    0x25,
    0x22,
    0x2C,
    0x45,
    0x13,
    0x28,
    0x30,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_Akropolis = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_Akropolis, Caption_Glyphs, 13, 300, 216, 29 };
/// September 4,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistDeparture[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MistDeparture = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_MistDeparture, Caption_Glyphs, 13, 300, 216, 29 };
/// September 4,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_Dryfield[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_Dryfield = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_Dryfield, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nMine shaft, Mojave Desert
static u8 BootCaptionText_Mine[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x22,
    0x27,
    0x1E,
    0x45,
    0x2C,
    0x21,
    0x1A,
    0x1F,
    0x2D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_Mine = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_Mine, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nDwelling level, Shelter
static u8 BootCaptionText_ShelterDwelling[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x30,
    0x1E,
    0x25,
    0x25,
    0x22,
    0x27,
    0x20,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterDwelling = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterDwelling, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nWaste level, Shelter
static u8 BootCaptionText_ShelterWaste[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x1A,
    0x2C,
    0x2D,
    0x1E,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterWaste = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterWaste, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldReturn[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DryfieldReturn = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_DryfieldReturn, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nLaboratory level, Shelter
static u8 BootCaptionText_ShelterLaboratory[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0B,
    0x1A,
    0x1B,
    0x28,
    0x2B,
    0x1A,
    0x2D,
    0x28,
    0x2B,
    0x32,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterLaboratory = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterLaboratory, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nPod shaft, Shelter
static u8 BootCaptionText_ShelterPod[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0F,
    0x28,
    0x1D,
    0x45,
    0x2C,
    0x21,
    0x1A,
    0x1F,
    0x2D,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterPod = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterPod, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nExperiment level, Shelter
static u8 BootCaptionText_ShelterExperiment[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x04,
    0x31,
    0x29,
    0x1E,
    0x2B,
    0x22,
    0x26,
    0x1E,
    0x27,
    0x2D,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterExperiment = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterExperiment, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nHeliport, Shelter
static u8 BootCaptionText_ShelterHeliport[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x07,
    0x1E,
    0x25,
    0x22,
    0x29,
    0x28,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterHeliport = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterHeliport, Caption_Glyphs, 13, 300, 216, 29 };

/// Boot-image mask colour range and its screen/ordering-table placement.
enum {
    FADE_BOOT_IMAGE_MAX_COLOR         = 255,
    FADE_BOOT_IMAGE_CLEAR_LEVEL       = 256,
    FADE_BOOT_IMAGE_OT_INDEX          = -16,
    FADE_BOOT_IMAGE_SECOND_BUFFER_ROW = 272,
};

/// Queues a grayscale subtractive mask over the 320x240 loading image.
///
/// `maskColor` is the amount subtracted from each RGB channel (0..255).
/// Requires word-aligned space for one TILE and one DR_TPAGE in the system
/// primitive arena, and a current OT with its reserved prefix. Packet storage
/// must survive GPU completion; this advances the arena cursor without a bound check.
static inline void _fadeDrawBootImageMask(u8 maskColor)
{
    TILE*     tile;
    DR_TPAGE* pagePacket;

    tile              = (TILE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(tile + 1);
    setTile(tile);
    setSemiTrans(tile, true);
    tile->r0 = maskColor;
    tile->g0 = maskColor;
    tile->b0 = maskColor;
    tile->x0 = -FILE_SYSTEM_IMAGE_WIDTH / 2;
    tile->y0 = -FILE_SYSTEM_IMAGE_HEIGHT / 2;
    tile->w  = FILE_SYSTEM_IMAGE_WIDTH;
    tile->h  = FILE_SYSTEM_IMAGE_HEIGHT;
    addPrim(gGpuCurrentOt + FADE_BOOT_IMAGE_OT_INDEX, tile);

    pagePacket        = (DR_TPAGE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(pagePacket + 1);
    // Head insertion makes the blend-mode command execute before the tile.
    setDrawTPage(pagePacket, false, true, getTPage(0, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + FADE_BOOT_IMAGE_OT_INDEX, pagePacket);
}

/// Draws and strengthens the boot-image mask, then clears both buffers at completion.
///
/// Returns 1 once the signed mask level is above 256, otherwise adds `levelStep`
/// and returns 0. The caller starts at zero and uses a positive step that stays
/// in s16 range. Drawing uses the pre-step low byte, including its wrap at 256;
/// completion clears both 320x240 buffers and disables image-strip presentation.
static s32 _fadeBootImageToBlack(s16 levelStep)
{
    u8   maskColor;
    RECT clearRect;

    maskColor = D_8006ACB4;
    _fadeDrawBootImageMask(maskColor);

    if (D_8006ACB4 > FADE_BOOT_IMAGE_CLEAR_LEVEL) {
        clearRect.y = 0;
        clearRect.x = 0;
        clearRect.w = FILE_SYSTEM_IMAGE_WIDTH;
        clearRect.h = FILE_SYSTEM_IMAGE_HEIGHT;
        ClearImage(&clearRect, 0, 0, 0);
        clearRect.y = FADE_BOOT_IMAGE_SECOND_BUFFER_ROW;
        ClearImage(&clearRect, 0, 0, 0);
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
        return 1;
    }
    D_8006ACB4 += levelStep;
    return 0;
}

/// Enables the boot-image display and initializes a full-strength subtractive mask.
///
/// Keeps the view and existing framebuffer contents; the white mask colour makes
/// the displayed image black. Requires primitive-arena and ordering-table space.
static void _fadeBeginBootImageReveal(void)
{
    u8 maskColor;

    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
    SetDispMask(1);

    D_8006ACB4 = FADE_BOOT_IMAGE_MAX_COLOR;
    maskColor  = *(volatile u8*)&D_8006ACB4;
    _fadeDrawBootImageMask(maskColor);
}

/// Draws the boot-image mask and reduces its signed level to reveal the image.
///
/// Enables image strips and returns 1 when the s16 result of subtracting
/// `levelStep` is negative. Starts at 255; the caller uses a positive step without
/// signed overflow. The current low byte is drawn before the level changes.
static s32 _fadeBootImageFromBlack(s32 levelStep)
{
    u8  maskColor;
    s16 nextLevel;

    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    maskColor                               = *(volatile u8*)&D_8006ACB4;
    _fadeDrawBootImageMask(maskColor);

    nextLevel  = D_8006ACB4 - levelStep;
    D_8006ACB4 = nextLevel;
    return nextLevel < 0;
}

/// Selects the MIST/Acropolis loading caption and category-zero image file.
///
/// Reads the retained destination area and distinguishes MIST arrival from
/// departure by story chapter. Writes one byte to `imageFileIndex`; the caller
/// clears the secondary caption before selection, and all captions remain resident.
static void _gameFlowSelectAcropolisLoadScreenAssets(u8* imageFileIndex)
{
    switch (Fs_LoadParams.area) {
        case GAME_AREA_ACROPOLIS_SQUARE:
            Fs_BootTimPrimary = &BootCaption_Akropolis;
            *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_ACROPOLIS_SQUARE;
            break;
        case GAME_AREA_ACROPOLIS_PLAZA:
            Fs_BootTimPrimary   = &BootCaption_AkropolisEvening;
            Fs_BootTimSecondary = &BootCaption_AkropolisEveningSecondary;
            *imageFileIndex     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            break;
        case GAME_AREA_ACROPOLIS_EAST_ELEVATOR_HALL ... GAME_AREA_ACROPOLIS_CAFETERIA:
        case GAME_AREA_ACROPOLIS_SECURITY_ROOM ... GAME_AREA_MIST_R18:
        default:
            Fs_BootTimPrimary = &BootCaption_Akropolis;
            *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_ACROPOLIS_OTHER;
            break;
        case GAME_AREA_MIST_PARKING:
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == GAME_FLOW_LOAD_CHAPTER_MIST_ARRIVAL) {
                Fs_BootTimPrimary = &BootCaption_MistArrival;
                *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_MIST_ARRIVAL;
            } else {
                Fs_BootTimPrimary = &BootCaption_MistDeparture;
                *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_MIST_DEPARTURE;
            }
            break;
        case GAME_AREA_MIST_SHOOTING_GALLERY:
            Fs_BootTimPrimary   = &BootCaption_MistEvening;
            Fs_BootTimSecondary = &BootCaption_MistEveningSecondary;
            *imageFileIndex     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            break;
    }
}

/// Selects the daytime Dryfield loading caption and category-zero image file.
///
/// Reads the retained destination area and uses the alternate-caption selector
/// at the gas station. Writes one byte to `imageFileIndex`; the caller clears
/// the secondary caption first, and all selected captions remain resident.
static void _gameFlowSelectDryfieldLoadScreenAssets(u8* imageFileIndex)
{
    switch (Fs_LoadParams.area) {
        case GAME_AREA_DRYFIELD_GAS_STATION:
            if (D5B498_8006ACC0 == GAME_FLOW_LOAD_CAPTION_NORMAL) {
                Fs_BootTimPrimary = &BootCaption_Dryfield;
                *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_MAIN_STREET;
            } else {
                Fs_BootTimPrimary   = &BootCaption_DesertAfternoon;
                Fs_BootTimSecondary = &BootCaption_DesertAfternoonSecondary;
                *imageFileIndex     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            }
            break;
        case GAME_AREA_DRYFIELD_MAIN_STREET:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_MAIN_STREET;
            break;
        case GAME_AREA_DRYFIELD_TRAILER_COACH:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_TRAILER_COACH;
            break;
        case GAME_AREA_DRYFIELD_MOTEL_ROOM_6:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_MOTEL_ROOM_6;
            break;
        case GAME_AREA_DRYFIELD_GENERAL_STORE ... GAME_AREA_DRYFIELD_JUNK_YARD:
        case GAME_AREA_DRYFIELD_MOTEL_ROOM_5 ... GAME_AREA_DRYFIELD_MOTEL_BALCONY:
        case GAME_AREA_DRYFIELD_MOTEL_LOFT ... GAME_AREA_DRYFIELD_UNDERPASS:
        default:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_OTHER;
            break;
    }
}

/// Selects the nighttime Dryfield loading caption and category-zero image file.
///
/// Lobby, trailer and other-area images differ in story chapters 3, 4 and 5.
/// Chapters other than 4 and 5 use the chapter-3 image. Writes one byte to
/// `imageFileIndex` and leaves the caller-cleared secondary caption untouched.
static void _gameFlowSelectDryfieldNightLoadScreenAssets(u8* imageFileIndex)
{
    u16 storyChapter;

    storyChapter = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
    switch (Fs_LoadParams.area) {
        case GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_3:
                default:
                    Fs_BootTimPrimary = &BootCaption_Dryfield;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_3;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_4:
                    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_LOBBY_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_3:
                default:
                    Fs_BootTimPrimary = &BootCaption_Dryfield;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_3;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_4:
                    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_TRAILER_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_DRYFIELD_NIGHT_MOTEL_ROOM_5:
        default:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_3:
                default:
                    Fs_BootTimPrimary = &BootCaption_Dryfield;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_3;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_4:
                    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_DRYFIELD_NIGHT_OTHER_CHAPTER_5;
                    break;
            }
            break;
    }
}

/// Selects the Mine/Shelter loading caption and category-zero image file.
///
/// Uses the retained area, story chapter and alternate-caption selector. Chapter
/// 5 selects later images; chapter 6 has distinct sterilization, parking and lab
/// images. Other chapters use chapter-4 images. Writes one byte to
/// `imageFileIndex`; the caller clears the secondary caption before selection.
static void _gameFlowSelectMineShelterLoadScreenAssets(u8* imageFileIndex)
{
    u16 storyChapter;

    storyChapter = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
    switch (Fs_LoadParams.area) {
        case GAME_AREA_MINE_MESA:
            Fs_BootTimPrimary   = &BootCaption_DryfieldMesaNight;
            Fs_BootTimSecondary = &BootCaption_DryfieldMesaNightSecondary;
            *imageFileIndex     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            break;
        case GAME_AREA_MINE_REFUGE:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    Fs_BootTimPrimary = &BootCaption_Mine;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_Mine;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_MINE_REFUGE_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_B1_STERILIZATION_ROOM:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_5;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_6:
                    Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_STERILIZATION_CHAPTER_6;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_5;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_6:
                    Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_PARKING_CHAPTER_6;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_R36:
            if (D5B498_8006ACC0 == GAME_FLOW_LOAD_CAPTION_NORMAL) {
                Fs_BootTimPrimary   = &BootCaption_WhiteHouseAftermath;
                Fs_BootTimSecondary = &BootCaption_WhiteHouseAftermathSecondary;
                *imageFileIndex     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            } else {
                Fs_BootTimPrimary   = &BootCaption_MuseumFlashback;
                Fs_BootTimSecondary = &BootCaption_MuseumFlashbackSecondary;
                *imageFileIndex     = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
            }
            break;
        case GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    Fs_BootTimPrimary = &BootCaption_ShelterWaste;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_ShelterWaste;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_INCINERATOR_CHAPTER_5;
                    break;
            }
            break;
        case GAME_AREA_SHELTER_B2_LABORATORY:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    Fs_BootTimPrimary = &BootCaption_ShelterLaboratory;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_ShelterLaboratory;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_5;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_6:
                    Fs_BootTimPrimary = &BootCaption_ShelterLaboratory;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_LABORATORY_CHAPTER_6;
                    break;
            }
            break;
        case GAME_AREA_MINE_CAVERN ... GAME_AREA_MINE_GORGE:
        case GAME_AREA_MINE_FORKED_TUNNEL ... GAME_AREA_SHELTER_B1_MAIN_CORRIDOR:
        case GAME_AREA_SHELTER_B1_POD_ACCESS_TUNNEL ... GAME_AREA_SHELTER_B1_ACCESS_TUNNEL:
        case GAME_AREA_SHELTER_B1_GOLEM_FREEZER_1 ... GAME_AREA_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY:
        case GAME_AREA_SHELTER_B2_BREEDING_ROOM ... GAME_AREA_SHELTER_B2_POD_ACCESS_TUNNEL:
        case GAME_AREA_SHELTER_R37 ... GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR:
        case GAME_AREA_SHELTER_B3_ELEVATOR_HALL ... GAME_AREA_SHELTER_R47:
        default:
            switch (storyChapter) {
                case GAME_FLOW_LOAD_CHAPTER_4:
                default:
                    Fs_BootTimPrimary = &BootCaption_ShelterPod;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_4;
                    break;
                case GAME_FLOW_LOAD_CHAPTER_5:
                    Fs_BootTimPrimary = &BootCaption_ShelterPod;
                    *imageFileIndex   = GAME_FLOW_LOAD_IMAGE_SHELTER_POD_CHAPTER_5;
                    break;
            }
            break;
    }
}

/// Clears both loading-screen framebuffers and blanks the display.
///
/// `clearRect` supplies writable scratch, left describing the second framebuffer.
/// Requires both 320x240 VRAM regions to be available for reuse. The queued clears
/// precede the incoming image; decoded-image strip presentation is disabled.
static inline void _gameFlowPrepareLoadScreenDisplay(RECT* clearRect)
{
    clearRect->y = 0;
    clearRect->x = 0;
    clearRect->w = FILE_SYSTEM_IMAGE_WIDTH;
    clearRect->h = FILE_SYSTEM_IMAGE_HEIGHT;
    ClearImage(clearRect, 0, 0, 0);
    clearRect->y = FADE_BOOT_IMAGE_SECOND_BUFFER_ROW;
    ClearImage(clearRect, 0, 0, 0);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
    SetDispMask(false);
}

void gameFlowStartLoadScreenImage(void)
{
    struct {
        u8 fileIndex; // Low component of the stage-zero file ID
        u8 unused;    // Byte 1 is not read by cdCmdEnqueue
        u8 fileGroup; // Stage-zero file category (0)
        u8 stage;     // CDF selector (0, the global library)
    } imageKey;
    struct {
        u8 fileIdHundreds;   // Hundreds component of the file ID (0)
        s8 loadMode;         // CD_COMMAND_LOAD_DEFAULT
        s8 imageXPageOffset; // Horizontal image shift in 64-word VRAM pages (0)
        s8 imageYOffset;     // Vertical image shift in VRAM rows (0)
    } loadOptions;

    RECT clearRect;

    // Discard the old image before selecting the next screen's resident captions.
    memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
    _gameFlowPrepareLoadScreenDisplay(&clearRect);
    imageKey.stage      = GAME_STAGE_NONE;
    imageKey.fileGroup  = GAME_FLOW_LOAD_FILE_CATEGORY;
    Fs_BootTimSecondary = NULL;
    switch (Fs_LoadParams.stage) {
        case GAME_STAGE_ACROPOLIS:
            _gameFlowSelectAcropolisLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_DRYFIELD:
            _gameFlowSelectDryfieldLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_DRYFIELD_NIGHT:
            _gameFlowSelectDryfieldNightLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_MINE_SHELTER:
            _gameFlowSelectMineShelterLoadScreenAssets(&imageKey.fileIndex);
            break;
        case GAME_STAGE_SHELTER_NEO_ARK:
        default:
            // Retain the story-flag read even though this stage does not branch on it.
            gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
            switch (Fs_LoadParams.area) {
                case GAME_AREA_NEO_ARK_SHRINE:
                default:
                    if (D5B498_8006ACC0 == GAME_FLOW_LOAD_CAPTION_NORMAL) {
                        Fs_BootTimPrimary  = &BootCaption_ShelterHeliport;
                        imageKey.fileIndex = GAME_FLOW_LOAD_IMAGE_SHELTER_HELIPORT;
                    } else {
                        Fs_BootTimPrimary   = &BootCaption_DesertReturn;
                        Fs_BootTimSecondary = &BootCaption_DesertReturnSecondary;
                        imageKey.fileIndex  = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
                    }
                    break;
                case GAME_AREA_SHELTER_B6_NURSERY:
                    Fs_BootTimPrimary  = &BootCaption_ShelterExperiment;
                    imageKey.fileIndex = GAME_FLOW_LOAD_IMAGE_SHELTER_NURSERY;
                    break;
                case GAME_AREA_NEO_ARK_R26:
                    Fs_BootTimPrimary   = &BootCaption_WhiteHouseEvening;
                    Fs_BootTimSecondary = &BootCaption_WhiteHouseEveningSecondary;
                    imageKey.fileIndex  = GAME_FLOW_LOAD_IMAGE_DETAILED_CAPTION;
                    break;
            }
            break;
    }
    // The queue copies the key and all four option bytes before these locals expire.
    loadOptions.fileIdHundreds   = 0;
    loadOptions.loadMode         = CD_COMMAND_LOAD_DEFAULT;
    loadOptions.imageXPageOffset = 0;
    loadOptions.imageYOffset     = 0;
    Fs_BootLoadSlot              = cdCmdEnqueue(CD_COMMAND_LOAD_FILE, &imageKey, &loadOptions);
}

void Fs_BootImageMachine(void* arg0, void* arg1)
{
    CdCmdQueue* queue;
    void*       secondary;
    s32         ret;
    s32         temp;

    queue     = &gCdCmdQueue;
    secondary = NULL;
    switch ((s16)D5B498_8006AC9C) {
        case 0:
            D_8006ACA0 = 0;
            D_8006AC9F = 0;
            D_8006AC9E = 0;
            D_8006ACA4 = 0;
            D_8006ACA2 = 0;
            D_8006ACA8 = 0;
            D_8006ACA6 = 0;
            _fadeBeginBootImageReveal();
            D5B498_8006AC9C++;
            /* fallthrough */
        case 1:
            if ((_fadeBootImageFromBlack(0x10) & 0xFFFF) != 0) {
                D5B498_8006AC9C++;
            }
            return;
        case 2:
            if (D_8006ACA6 < 0) {
                D_8006ACA2 = 1;
            }
            temp = textDrawStream(arg0, &D_8006AC9E, &D_8006ACA2, 0);
            if (D_8006ACA6 >= 0) {
                D_8006ACA6 = temp;
            }
            if (D_8006ACA0 >= 0x3D) {
                if (D_8006ACA8 < 0) {
                    D_8006ACA4 = 1;
                }
                if (secondary != NULL) {
                    ret = textDrawStream(secondary, &D_8006AC9F, &D_8006ACA4, 0);
                } else {
                    D_8006ACA8 = -1;
                }
                if (D_8006ACA8 >= 0) {
                    D_8006ACA8 = ret;
                }
            } else {
                D_8006ACA0++;
            }
            if (D_8006ACA8 < 0 && D_8006ACA6 < 0) {
                D_8006ACA0 = 0;
                D5B498_8006AC9C++;
            }
            return;
        case 3:
            if (D_8006ACA0 >= 0x3C) {
                if (queue->holdBootImage == 0) {
                    D_8006ACB4 = 0;
                    D5B498_8006AC9C++;
                }
            } else {
                D_8006ACA0++;
            }
            break;
        case 4:
            if ((_fadeBootImageToBlack(0x10) & 0xFFFF) != 0) {
                Fs_BootLoadPhase           = 0;
                gCdCmdQueue.bootLoadActive = 0;
            }
            break;
        default:
            return;
    }
    D_8006ACA4 = 1;
    D_8006ACA2 = 1;
    textDrawStream(arg0, &D_8006AC9E, &D_8006ACA2, 0);
    if (secondary != NULL) {
        textDrawStream(secondary, &D_8006AC9F, &D_8006ACA4, 0);
    }
}
