#include "enemy_names.h"

#include <core/pe2.h>

/* Each name in Shift-JIS, by its text id, in the table's order: also their order in the package's data. */

// 0x500, empty
static char _gEmptyName[] = "";
// 0x501 ネズミ
static char _gRatName[] = "\x83\x6C\x83\x59\x83\x7E";
// 0x502 ガ
static char _gMothName[] = "\x83\x4B";
// 0x503 ミトコンバス
static char _gMitokonbasuName[] = "\x83\x7E\x83\x67\x83\x52\x83\x93\x83\x6F\x83\x58";
// 0x504 コウモリ
static char _gBatName[] = "\x83\x52\x83\x45\x83\x82\x83\x8A";
// 0x505 サソリ
static char _gScorpionName[] = "\x83\x54\x83\x5C\x83\x8A";
// 0x506 こうかく
static char _gKoukakuName[] = "\x82\xB1\x82\xA4\x82\xA9\x82\xAD";
// 0x507 カイコ
static char _gSilkwormName[] = "\x83\x4A\x83\x43\x83\x52";
// 0x508 スライム（あお）
static char _gBlueSlimeName[] = "\x83\x58\x83\x89\x83\x43\x83\x80\x81\x69\x82\xA0\x82\xA8\x81\x6A";
// 0x509 スライム（あか）
static char _gRedSlimeName[] = "\x83\x58\x83\x89\x83\x43\x83\x80\x81\x69\x82\xA0\x82\xA9\x81\x6A";
// 0x50A チキンマンＡ
static char _gChickenManAName[] = "\x83\x60\x83\x4C\x83\x93\x83\x7D\x83\x93\x82\x60";
// 0x50B チキンマンＢ
static char _gChickenManBName[] = "\x83\x60\x83\x4C\x83\x93\x83\x7D\x83\x93\x82\x61";
// 0x50C チキンウォーカー
static char _gChickenWalkerName[] = "\x83\x60\x83\x4C\x83\x93\x83\x45\x83\x48\x81\x5B\x83\x4A\x81\x5B";
// 0x50D チキンライダー
static char _gChickenRiderName[] = "\x83\x60\x83\x4C\x83\x93\x83\x89\x83\x43\x83\x5F\x81\x5B";
// 0x50E チキンホッパー
static char _gChickenHopperName[] = "\x83\x60\x83\x4C\x83\x93\x83\x7A\x83\x62\x83\x70\x81\x5B";
// 0x50F コーンヘッド
static char _gConeHeadName[] = "\x83\x52\x81\x5B\x83\x93\x83\x77\x83\x62\x83\x68";
// 0x510 タートルヘッド
static char _gTurtleHeadName[] = "\x83\x5E\x81\x5B\x83\x67\x83\x8B\x83\x77\x83\x62\x83\x68";
// 0x511 パラサイトベイビー
static char _gParasiteBabyName[] = "\x83\x70\x83\x89\x83\x54\x83\x43\x83\x67\x83\x78\x83\x43\x83\x72\x81\x5B";
// 0x512 ブラッドサッカー
static char _gBloodsuckerName[] = "\x83\x75\x83\x89\x83\x62\x83\x68\x83\x54\x83\x62\x83\x4A\x81\x5B";
// 0x513 デザートランナー
static char _gDesertRunnerName[] = "\x83\x66\x83\x55\x81\x5B\x83\x67\x83\x89\x83\x93\x83\x69\x81\x5B";
// 0x514 ステルスパニッシャー
static char _gStealthPunisherName[] = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x70\x83\x6A\x83\x62\x83\x56\x83\x83\x81\x5B";
// 0x515 ドッペルパニッシャー
static char _gDoppelPunisherName[] = "\x83\x68\x83\x62\x83\x79\x83\x8B\x83\x70\x83\x6A\x83\x62\x83\x56\x83\x83\x81\x5B";
// 0x516 ナイトシンガー
static char _gNightSingerName[] = "\x83\x69\x83\x43\x83\x67\x83\x56\x83\x93\x83\x4B\x81\x5B";
// 0x517 パラサイトフライヤー
static char _gParasiteFlyerName[] = "\x83\x70\x83\x89\x83\x54\x83\x43\x83\x67\x83\x74\x83\x89\x83\x43\x83\x84\x81\x5B";
// 0x518 ストーカー
static char _gStalkerName[] = "\x83\x58\x83\x67\x81\x5B\x83\x4A\x81\x5B";
// 0x519 シードリフター
static char _gSeaDrifterName[] = "\x83\x56\x81\x5B\x83\x68\x83\x8A\x83\x74\x83\x5E\x81\x5B";
// 0x51A スマートコーン
static char _gSmartConeName[] = "\x83\x58\x83\x7D\x81\x5B\x83\x67\x83\x52\x81\x5B\x83\x93";
// 0x51B コーンウォーカー
static char _gConeWalkerName[] = "\x83\x52\x81\x5B\x83\x93\x83\x45\x83\x48\x81\x5B\x83\x4A\x81\x5B";
// 0x51C ハーフデザート
static char _gHalfDesertName[] = "\x83\x6E\x81\x5B\x83\x74\x83\x66\x83\x55\x81\x5B\x83\x67";
// 0x51D パラサイトだま
static char _gParasiteDamaName[] = "\x83\x70\x83\x89\x83\x54\x83\x43\x83\x67\x82\xBE\x82\xDC";
// 0x51E ステルスヘッド
static char _gStealthHeadName[] = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x77\x83\x62\x83\x68";
// 0x51F レーザーほうだい１
static char _gLaserTurret1Name[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x50";
// 0x520 レーザーほうだい２
static char _gLaserTurret2Name[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x51";
// 0x521 レーザーほうだい３
static char _gLaserTurret3Name[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x52";
// 0x522 レーザーほうだい４
static char _gLaserTurret4Name[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x82\x53";
// 0x523 レーザーほうだいマシンガン
static char _gLaserTurretMachineGunName[] = "\x83\x8C\x81\x5B\x83\x55\x81\x5B\x82\xD9\x82\xA4\x82\xBE\x82\xA2\x83\x7D\x83\x56\x83\x93\x83\x4B\x83\x93";
// 0x524 ノーマルゴーレム（ブレード）
static char _gNormalGolemBladeName[] = "\x83\x6D\x81\x5B\x83\x7D\x83\x8B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x75\x83\x8C\x81\x5B\x83\x68\x81\x6A";
// 0x525 ノーマルゴーレム（グレネード）
static char _gNormalGolemGrenadeName[] = "\x83\x6D\x81\x5B\x83\x7D\x83\x8B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x4F\x83\x8C\x83\x6C\x81\x5B\x83\x68\x81\x6A";
// 0x526 ヘビーゴーレム（ブレード）
static char _gHeavyGolemBladeName[] = "\x83\x77\x83\x72\x81\x5B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x75\x83\x8C\x81\x5B\x83\x68\x81\x6A";
// 0x527 ヘビーゴーレム（グレネード）
static char _gHeavyGolemGrenadeName[] = "\x83\x77\x83\x72\x81\x5B\x83\x53\x81\x5B\x83\x8C\x83\x80\x81\x69\x83\x4F\x83\x8C\x83\x6C\x81\x5B\x83\x68\x81\x6A";
// 0x528 ステルスゴーレム
static char _gStealthGolemName[] = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x53\x81\x5B\x83\x8C\x83\x80";
// 0x529 ステルスゴーレムホワイト
static char _gWhiteStealthGolemName[] = "\x83\x58\x83\x65\x83\x8B\x83\x58\x83\x53\x81\x5B\x83\x8C\x83\x80\x83\x7A\x83\x8F\x83\x43\x83\x67";
// 0x52A メガネおんな
static char _gGlassesWomanName[] = "\x83\x81\x83\x4B\x83\x6C\x82\xA8\x82\xF1\x82\xC8";
// 0x52B ギュンター１
static char _gGunther1Name[] = "\x83\x4D\x83\x85\x83\x93\x83\x5E\x81\x5B\x82\x50";
// 0x52C ギュンター２
static char _gGunther2Name[] = "\x83\x4D\x83\x85\x83\x93\x83\x5E\x81\x5B\x82\x51";
// 0x52D スノーマフラー
static char _gSnowMufflerName[] = "\x83\x58\x83\x6D\x81\x5B\x83\x7D\x83\x74\x83\x89\x81\x5B";
// 0x52E アンデッドスノーマフラー
static char _gUndeadSnowMufflerName[] = "\x83\x41\x83\x93\x83\x66\x83\x62\x83\x68\x83\x58\x83\x6D\x81\x5B\x83\x7D\x83\x74\x83\x89\x81\x5B";
// 0x52F ギガントサピエンス
static char _gGigantSapiensName[] = "\x83\x4D\x83\x4B\x83\x93\x83\x67\x83\x54\x83\x73\x83\x47\x83\x93\x83\x58";
// 0x530 ガービジイーター１
static char _gGarbageEater1Name[] = "\x83\x4B\x81\x5B\x83\x72\x83\x57\x83\x43\x81\x5B\x83\x5E\x81\x5B\x82\x50";
// 0x531 ガービジイーター２
static char _gGarbageEater2Name[] = "\x83\x4B\x81\x5B\x83\x72\x83\x57\x83\x43\x81\x5B\x83\x5E\x81\x5B\x82\x51";
// 0x532 ぞうふくそうちＡ
static char _gAmplifierAName[] = "\x82\xBC\x82\xA4\x82\xD3\x82\xAD\x82\xBB\x82\xA4\x82\xBF\x82\x60";
// 0x533 ぞうふくそうちＢ
static char _gAmplifierBName[] = "\x82\xBC\x82\xA4\x82\xD3\x82\xAD\x82\xBB\x82\xA4\x82\xBF\x82\x61";
// 0x534 ジャイアントストーカー
static char _gGiantStalkerName[] = "\x83\x57\x83\x83\x83\x43\x83\x41\x83\x93\x83\x67\x83\x58\x83\x67\x81\x5B\x83\x4A\x81\x5B";
// 0x535 ブラフマンＡ
static char _gBrahmanAName[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x60";
// 0x536 ブラフマンＢ
static char _gBrahmanBName[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x61";
// 0x537 ブラフマンＣ
static char _gBrahmanCName[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x62";
// 0x538 ブラフマンＤ
static char _gBrahmanDName[] = "\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x63";
// 0x539 クリアブラフマンＡ
static char _gClearBrahmanAName[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x60";
// 0x53A クリアブラフマンＢ
static char _gClearBrahmanBName[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x61";
// 0x53B クリアブラフマンＣ
static char _gClearBrahmanCName[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x62";
// 0x53C クリアブラフマンＤ
static char _gClearBrahmanDName[] = "\x83\x4E\x83\x8A\x83\x41\x83\x75\x83\x89\x83\x74\x83\x7D\x83\x93\x82\x63";
// 0x53D サボテンダー
static char _gSabotenderName[] = "\x83\x54\x83\x7B\x83\x65\x83\x93\x83\x5F\x81\x5B";
// 0x53E チョコボ
static char _gChocoboName[] = "\x83\x60\x83\x87\x83\x52\x83\x7B";

char* gEnemyNames[] = {
    _gEmptyName,
    _gRatName,
    _gMothName,
    _gMitokonbasuName,
    _gBatName,
    _gScorpionName,
    _gKoukakuName,
    _gSilkwormName,
    _gBlueSlimeName,
    _gRedSlimeName,
    _gChickenManAName,
    _gChickenManBName,
    _gChickenWalkerName,
    _gChickenRiderName,
    _gChickenHopperName,
    _gConeHeadName,
    _gTurtleHeadName,
    _gParasiteBabyName,
    _gBloodsuckerName,
    _gDesertRunnerName,
    _gStealthPunisherName,
    _gDoppelPunisherName,
    _gNightSingerName,
    _gParasiteFlyerName,
    _gStalkerName,
    _gSeaDrifterName,
    _gSmartConeName,
    _gConeWalkerName,
    _gHalfDesertName,
    _gParasiteDamaName,
    _gStealthHeadName,
    _gLaserTurret1Name,
    _gLaserTurret2Name,
    _gLaserTurret3Name,
    _gLaserTurret4Name,
    _gLaserTurretMachineGunName,
    _gNormalGolemBladeName,
    _gNormalGolemGrenadeName,
    _gHeavyGolemBladeName,
    _gHeavyGolemGrenadeName,
    _gStealthGolemName,
    _gWhiteStealthGolemName,
    _gGlassesWomanName,
    _gGunther1Name,
    _gGunther2Name,
    _gSnowMufflerName,
    _gUndeadSnowMufflerName,
    _gGigantSapiensName,
    _gGarbageEater1Name,
    _gGarbageEater2Name,
    _gAmplifierAName,
    _gAmplifierBName,
    _gGiantStalkerName,
    _gBrahmanAName,
    _gBrahmanBName,
    _gBrahmanCName,
    _gBrahmanDName,
    _gClearBrahmanAName,
    _gClearBrahmanBName,
    _gClearBrahmanCName,
    _gClearBrahmanDName,
    _gSabotenderName,
    _gChocoboName,
};

/// The table's slots 1..62 in order, then 0xFF: an order to list the names in, which nothing reads.
static u8 _gListOrder[] = {
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
