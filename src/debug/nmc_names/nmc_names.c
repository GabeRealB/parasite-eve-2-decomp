#include "debug/nmc_names.h"

#include "types.h"

// Keep the encoded labels in serialized table order in the package's .data section.

/// Empty NUL-terminated label for enemy text id 0x500.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesEmptyName[] __attribute__((section(".data"))) = "";
/// NUL-terminated Shift-JIS "ネズミ" label for enemy text id 0x501.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesRatName[] __attribute__((section(".data"))) = "\x83\x6C\x83\x59\x83\x7E";
/// NUL-terminated Shift-JIS "ガ" label for enemy text id 0x502.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesMothName[] __attribute__((section(".data"))) = "\x83\x4B";
/// NUL-terminated Shift-JIS "ミトコンバス" label for enemy text id 0x503.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesMitokonbasuName[] __attribute__((section(".data"))) = "\x83\x7E\x83\x67\x83\x52\x83\x93\x83\x6F\x83\x58";
/// NUL-terminated Shift-JIS "コウモリ" label for enemy text id 0x504.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesBatName[] __attribute__((section(".data"))) = "\x83\x52\x83\x45\x83\x82\x83\x8A";
/// NUL-terminated Shift-JIS "サソリ" label for enemy text id 0x505.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesScorpionName[] __attribute__((section(".data"))) = "\x83\x54\x83\x5C\x83\x8A";
/// NUL-terminated Shift-JIS "こうかく" label for enemy text id 0x506.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesKoukakuName[] __attribute__((section(".data"))) = "\x82\xB1\x82\xA4\x82\xA9\x82\xAD";
/// NUL-terminated Shift-JIS "カイコ" label for enemy text id 0x507.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesSilkwormName[] __attribute__((section(".data"))) = "\x83\x4A\x83\x43\x83\x52";
/// NUL-terminated Shift-JIS "スライム（あお）" label for enemy text id 0x508.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesBlueSlimeName[] __attribute__((section(".data"))) = "\x83\x58\x83\x89\x83\x43\x83\x80\x81\x69\x82\xA0\x82\xA8\x81\x6A";
/// NUL-terminated Shift-JIS "スライム（あか）" label for enemy text id 0x509.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesRedSlimeName[] __attribute__((section(".data"))) = "\x83\x58\x83\x89\x83\x43\x83\x80\x81\x69\x82\xA0\x82\xA9\x81\x6A";
/// NUL-terminated Shift-JIS "チキンマンＡ" label for enemy text id 0x50A.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesChickenManAName[] __attribute__((section(".data"))) = "\x83\x60\x83\x4C\x83\x93\x83\x7D\x83\x93\x82\x60";
/// NUL-terminated Shift-JIS "チキンマンＢ" label for enemy text id 0x50B.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesChickenManBName[] __attribute__((section(".data"))) = "\x83\x60\x83\x4C\x83\x93\x83\x7D\x83\x93\x82\x61";
/// NUL-terminated Shift-JIS "チキンウォーカー" label for enemy text id 0x50C.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesChickenWalkerName[] __attribute__((section(".data"))) = "\x83\x60\x83\x4C\x83\x93\x83\x45\x83\x48\x81\x5B\x83\x4A\x81\x5B";
/// NUL-terminated Shift-JIS "チキンライダー" label for enemy text id 0x50D.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesChickenRiderName[] __attribute__((section(".data"))) = "\x83\x60\x83\x4C\x83\x93\x83\x89\x83\x43\x83\x5F\x81\x5B";
/// NUL-terminated Shift-JIS "チキンホッパー" label for enemy text id 0x50E.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesChickenHopperName[] __attribute__((section(".data"))) = "\x83\x60\x83\x4C\x83\x93\x83\x7A\x83\x62\x83\x70\x81\x5B";
/// NUL-terminated Shift-JIS "コーンヘッド" label for enemy text id 0x50F.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesConeHeadName[] __attribute__((section(".data"))) = "\x83\x52\x81\x5B\x83\x93\x83\x77\x83\x62\x83\x68";
/// NUL-terminated Shift-JIS "タートルヘッド" label for enemy text id 0x510.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesTurtleHeadName[] __attribute__((section(".data"))) = "\x83\x5E\x81\x5B\x83\x67\x83\x8B\x83\x77\x83\x62\x83\x68";
/// NUL-terminated Shift-JIS "パラサイトベイビー" label for enemy text id 0x511.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesParasiteBabyName[] __attribute__((section(".data"))) = "\x83\x70\x83\x89\x83\x54\x83\x43\x83\x67\x83\x78\x83\x43\x83\x72\x81\x5B";
/// NUL-terminated Shift-JIS "ブラッドサッカー" label for enemy text id 0x512.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesBloodsuckerName[] __attribute__((section(".data"))) = "\x83\x75\x83\x89\x83\x62\x83\x68\x83\x54\x83\x62\x83\x4A\x81\x5B";
/// NUL-terminated Shift-JIS "デザートランナー" label for enemy text id 0x513.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesDesertRunnerName[] __attribute__((section(".data"))) = "\x83\x66\x83\x55\x81\x5B\x83\x67\x83\x89\x83\x93\x83\x69\x81\x5B";
/// NUL-terminated Shift-JIS "ステルスパニッシャー" label for enemy text id 0x514.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesStealthPunisherName[] __attribute__((section(".data"))) = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x70\x83\x6A\x83\x62\x83\x56\x83\x83\x81\x5B";
/// NUL-terminated Shift-JIS "ドッペルパニッシャー" label for enemy text id 0x515.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesDoppelPunisherName[] __attribute__((section(".data"))) = "\x83\x68\x83\x62\x83\x79\x83\x8B\x83\x70\x83\x6A\x83\x62\x83\x56\x83\x83\x81\x5B";
/// NUL-terminated Shift-JIS "ナイトシンガー" label for enemy text id 0x516.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesNightSingerName[] __attribute__((section(".data"))) = "\x83\x69\x83\x43\x83\x67\x83\x56\x83\x93\x83\x4B\x81\x5B";
/// NUL-terminated Shift-JIS "パラサイトフライヤー" label for enemy text id 0x517.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesParasiteFlyerName[] __attribute__((section(".data"))) = "\x83\x70\x83\x89\x83\x54\x83\x43\x83\x67\x83\x74\x83\x89\x83\x43\x83\x84\x81\x5B";
/// NUL-terminated Shift-JIS "ストーカー" label for enemy text id 0x518.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesStalkerName[] __attribute__((section(".data"))) = "\x83\x58\x83\x67\x81\x5B\x83\x4A\x81\x5B";
/// NUL-terminated Shift-JIS "シードリフター" label for enemy text id 0x519.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesSeaDrifterName[] __attribute__((section(".data"))) = "\x83\x56\x81\x5B\x83\x68\x83\x8A\x83\x74\x83\x5E\x81\x5B";
/// NUL-terminated Shift-JIS "スマートコーン" label for enemy text id 0x51A.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesSmartConeName[] __attribute__((section(".data"))) = "\x83\x58\x83\x7D\x81\x5B\x83\x67\x83\x52\x81\x5B\x83\x93";
/// NUL-terminated Shift-JIS "コーンウォーカー" label for enemy text id 0x51B.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesConeWalkerName[] __attribute__((section(".data"))) = "\x83\x52\x81\x5B\x83\x93\x83\x45\x83\x48\x81\x5B\x83\x4A\x81\x5B";
/// NUL-terminated Shift-JIS "ハーフデザート" label for enemy text id 0x51C.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesHalfDesertName[] __attribute__((section(".data"))) = "\x83\x6E\x81\x5B\x83\x74\x83\x66\x83\x55\x81\x5B\x83\x67";
/// NUL-terminated Shift-JIS "パラサイトだま" label for enemy text id 0x51D.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesParasiteDamaName[] __attribute__((section(".data"))) = "\x83\x70\x83\x89\x83\x54\x83\x43\x83\x67\x82\xBE\x82\xDC";
/// NUL-terminated Shift-JIS "ステルスヘッド" label for enemy text id 0x51E.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesStealthHeadName[] __attribute__((section(".data"))) = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x77\x83\x62\x83\x68";
/// NUL-terminated Shift-JIS "レーザーほうだい１" label for enemy text id 0x51F.
///
/// Borrowed text; readable only while the `nmc_names` overlay is loaded.
static const char _gNmcNamesLaserTurret1Name[] __attribute__((section(".data"))) = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x50";
/* レーザーほうだい２ */
static char D_nmc_names_801D61FC[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x51";
/* レーザーほうだい３ */
static char D_nmc_names_801D6210[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x52";
/* レーザーほうだい４ */
static char D_nmc_names_801D6224[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x53";
/* レーザーほうだいマシンガン */
static char D_nmc_names_801D6238[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x83\x7D\x83\x56\x83\x93\x83\x4B\x83\x93";
/* ノーマルゴーレム（ブレード） */
static char D_nmc_names_801D6254[] = "\x83\x6D\x81\x5B\x83\x7D\x83\x8B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x75\x83\x8C\x81\x5B\x83\x68\x81\x6A";
/* ノーマルゴーレム（グレネード） */
static char D_nmc_names_801D6274[] = "\x83\x6D\x81\x5B\x83\x7D\x83\x8B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x4F\x83\x8C\x83\x6C\x81\x5B\x83\x68\x81\x6A";
/* ヘビーゴーレム（ブレード） */
static char D_nmc_names_801D6294[] = "\x83\x77\x83\x72\x81\x5B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x75\x83\x8C\x81\x5B\x83\x68\x81\x6A";
/* ヘビーゴーレム（グレネード） */
static char D_nmc_names_801D62B0[] = "\x83\x77\x83\x72\x81\x5B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x4F\x83\x8C\x83\x6C\x81\x5B\x83\x68\x81\x6A";
/* ステルスゴーレム */
static char D_nmc_names_801D62D0[] = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x53\x81\x5B\x83\x8C\x83\x80";
/* ステルスゴーレムホワイト */
static char D_nmc_names_801D62E4[] = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x53\x81\x5B\x83\x8C\x83\x80\x83\x7A\x83\x8F\x83\x43\x83\x67";
/* メガネおんな */
static char D_nmc_names_801D6300[] = "\x83\x81\x83\x4B\x83\x6C\x82\xA8\x82\xF1\x82\xC8";
/* ギュンター１ */
static char D_nmc_names_801D6310[] = "\x83\x4D\x83\x85\x83\x93\x83\x5E\x81\x5B\x82\x50";
/* ギュンター２ */
static char D_nmc_names_801D6320[] = "\x83\x4D\x83\x85\x83\x93\x83\x5E\x81\x5B\x82\x51";
/* スノーマフラー */
static char D_nmc_names_801D6330[] = "\x83\x58\x83\x6D\x81\x5B\x83\x7D\x83\x74\x83\x89\x81\x5B";
/* アンデッドスノーマフラー */
static char D_nmc_names_801D6340[] = "\x83\x41\x83\x93\x83\x66\x83\x62\x83\x68\x83\x58\x83\x6D\x81\x5B\x83\x7D\x83\x74\x83\x89\x81\x5B";
/* ギガントサピエンス */
static char D_nmc_names_801D635C[] = "\x83\x4D\x83\x4B\x83\x93\x83\x67\x83\x54\x83\x73\x83\x47\x83\x93\x83\x58";
/* ガービジイーター１ */
static char D_nmc_names_801D6370[] = "\x83\x4B\x81\x5B\x83\x72\x83\x57\x83\x43\x81\x5B\x83\x5E\x81\x5B\x82\x50";
/* ガービジイーター２ */
static char D_nmc_names_801D6384[] = "\x83\x4B\x81\x5B\x83\x72\x83\x57\x83\x43\x81\x5B\x83\x5E\x81\x5B\x82\x51";
/* ぞうふくそうちＡ */
static char D_nmc_names_801D6398[] = "\x82\xBC\x82\xA4\x82\xD3\x82\xAD\x82\xBB\x82\xA4\x82\xBF\x82\x60";
/* ぞうふくそうちＢ */
static char D_nmc_names_801D63AC[] = "\x82\xBC\x82\xA4\x82\xD3\x82\xAD\x82\xBB\x82\xA4\x82\xBF\x82\x61";
/* ジャイアントストーカー */
static char D_nmc_names_801D63C0[] = "\x83\x57\x83\x83\x83\x43\x83\x41\x83\x93\x83\x67\x83\x58\x83\x67\x81\x5B\x83\x4A\x81\x5B";
/* ブラフマンＡ */
static char D_nmc_names_801D63D8[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x60";
/* ブラフマンＢ */
static char D_nmc_names_801D63E8[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x61";
/* ブラフマンＣ */
static char D_nmc_names_801D63F8[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x62";
/* ブラフマンＤ */
static char D_nmc_names_801D6408[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x63";
/* クリアブラフマンＡ */
static char D_nmc_names_801D6418[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x60";
/* クリアブラフマンＢ */
static char D_nmc_names_801D642C[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x61";
/* クリアブラフマンＣ */
static char D_nmc_names_801D6440[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x62";
/* クリアブラフマンＤ */
static char D_nmc_names_801D6454[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x63";
/* サボテンダー */
static char D_nmc_names_801D6468[] = "\x83\x54\x83\x7B\x83\x65\x83\x93\x83\x5F\x81\x5B";
/* チョコボ */
static char D_nmc_names_801D6478[] = "\x83\x60\x83\x87\x83\x52\x83\x7B";

const char* Gp_ItemTextHi[63] = {
    _gNmcNamesEmptyName,
    _gNmcNamesRatName,
    _gNmcNamesMothName,
    _gNmcNamesMitokonbasuName,
    _gNmcNamesBatName,
    _gNmcNamesScorpionName,
    _gNmcNamesKoukakuName,
    _gNmcNamesSilkwormName,
    _gNmcNamesBlueSlimeName,
    _gNmcNamesRedSlimeName,
    _gNmcNamesChickenManAName,
    _gNmcNamesChickenManBName,
    _gNmcNamesChickenWalkerName,
    _gNmcNamesChickenRiderName,
    _gNmcNamesChickenHopperName,
    _gNmcNamesConeHeadName,
    _gNmcNamesTurtleHeadName,
    _gNmcNamesParasiteBabyName,
    _gNmcNamesBloodsuckerName,
    _gNmcNamesDesertRunnerName,
    _gNmcNamesStealthPunisherName,
    _gNmcNamesDoppelPunisherName,
    _gNmcNamesNightSingerName,
    _gNmcNamesParasiteFlyerName,
    _gNmcNamesStalkerName,
    _gNmcNamesSeaDrifterName,
    _gNmcNamesSmartConeName,
    _gNmcNamesConeWalkerName,
    _gNmcNamesHalfDesertName,
    _gNmcNamesParasiteDamaName,
    _gNmcNamesStealthHeadName,
    _gNmcNamesLaserTurret1Name,
    D_nmc_names_801D61FC,
    D_nmc_names_801D6210,
    D_nmc_names_801D6224,
    D_nmc_names_801D6238,
    D_nmc_names_801D6254,
    D_nmc_names_801D6274,
    D_nmc_names_801D6294,
    D_nmc_names_801D62B0,
    D_nmc_names_801D62D0,
    D_nmc_names_801D62E4,
    D_nmc_names_801D6300,
    D_nmc_names_801D6310,
    D_nmc_names_801D6320,
    D_nmc_names_801D6330,
    D_nmc_names_801D6340,
    D_nmc_names_801D635C,
    D_nmc_names_801D6370,
    D_nmc_names_801D6384,
    D_nmc_names_801D6398,
    D_nmc_names_801D63AC,
    D_nmc_names_801D63C0,
    D_nmc_names_801D63D8,
    D_nmc_names_801D63E8,
    D_nmc_names_801D63F8,
    D_nmc_names_801D6408,
    D_nmc_names_801D6418,
    D_nmc_names_801D642C,
    D_nmc_names_801D6440,
    D_nmc_names_801D6454,
    D_nmc_names_801D6468,
    D_nmc_names_801D6478,
};

/// The values 1 to 62 in order, then 0xFF. Nothing decompiled reads it.
static u8 D_nmc_names_801D6580[63] = {
    0x01,
    0x02,
    0x03,
    0x04,
    0x05,
    0x06,
    0x07,
    0x08,
    0x09,
    0x0A,
    0x0B,
    0x0C,
    0x0D,
    0x0E,
    0x0F,
    0x10,
    0x11,
    0x12,
    0x13,
    0x14,
    0x15,
    0x16,
    0x17,
    0x18,
    0x19,
    0x1A,
    0x1B,
    0x1C,
    0x1D,
    0x1E,
    0x1F,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x28,
    0x29,
    0x2A,
    0x2B,
    0x2C,
    0x2D,
    0x2E,
    0x2F,
    0x30,
    0x31,
    0x32,
    0x33,
    0x34,
    0x35,
    0x36,
    0x37,
    0x38,
    0x39,
    0x3A,
    0x3B,
    0x3C,
    0x3D,
    0x3E,
    0xFF,
};
