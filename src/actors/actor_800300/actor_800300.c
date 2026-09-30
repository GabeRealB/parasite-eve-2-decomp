#include "actors/actor_800300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachments.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/item_pickup.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// 0x18-byte scratch stack block `func_actor_800300_80162064` takes for the
/// ground-quad heading it copies into the three `GameActor.field_88` records.
/// `func_800EA1A8` also fills the block as a `VECTOR3` from `coord->workm.t`.
typedef struct {
    /* 0x00 */ byte    pad_0[0x10];
    /* 0x10 */ SVECTOR vec;
} Actor800300VecScratch;
STATIC_ASSERT_SIZEOF(Actor800300VecScratch, 0x18);

/// View of `GameActor.field_973` as the unsigned byte its rotation
/// multiply sign-extends.
typedef struct {
    byte pad[0x973];
    u8   field_973;
} Actor800300DirByte;

extern s32 D_8017A99C;
// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        s32  (*call0)(Task*);
        s32  (*call1)(Task*, s32, AnimationPlayRequest*);
        s32  (*call2)(Task*, s32, GpCopyArg*);
        s32  (*call3)(Task*, s32, GpCountArg*);
        s32  (*call4)(Task*, s32, ActorTransform*);
        s32  (*transform)(Task*, s32, ActorTransform*, s32);
        s32  (*call5)(Task*, s32, ActorTransform*, GpOverrideArg*);
        s32  (*call6)(Task*, s32, s32);
        s32  (*call7)(Task*, s32, s32, s32);
        s32  (*call9)(Task*, s32, GfxCoord*);
        void (*call8)(Task*, s32, GpMoveArg*);
    } handler;
} Actor800300MessageEntry;
STATIC_ASSERT_SIZEOF(Actor800300MessageEntry, 8);

extern Actor800300MessageEntry D_actor_800300_80168880[26];
extern GpImgRec**              D_actor_800300_80168950[];
extern GpImgRec**              D_actor_800300_80168960[];

static void func_actor_800300_801623F8(Task* arg0);
static void func_actor_800300_801625A8(Task* task);
static void func_actor_800300_80162658(Task* arg0);
static void func_actor_800300_801628D0(Task* arg0);
static void func_actor_800300_80162A98(Task* arg0);
static void func_actor_800300_80162C2C(Task* arg0);
static void func_actor_800300_80162C98(Task* arg0);
static void func_actor_800300_80162D74(Task* arg0);
static void func_actor_800300_80162EEC(Task* arg0);
static void func_actor_800300_80162F24(Task* arg0);
static void func_actor_800300_80162F98(Task* arg0);
static void func_actor_800300_80163048(Task* arg0);
static void func_actor_800300_80163074(Task* arg0);

extern GpImgRec* D_actor_800300_80169A20[2];
extern GpImgRec* D_actor_800300_80169A28[4];
extern GpImgRec* D_actor_800300_80169A38[4];
extern GpImgRec* D_actor_800300_80169A48[6];
extern GpImgRec* D_actor_800300_80169A60[2];
extern GpImgRec* D_actor_800300_80169A68[2];

extern AnimationSet D_actor_800300_80169C5C;
extern AnimationSet D_actor_800300_8016A038;
extern AnimationSet D_actor_800300_8016A438;
extern AnimationSet D_actor_800300_8016AFB4;
extern AnimationSet D_actor_800300_8016B298;
extern AnimationSet D_actor_800300_8016B63C;
extern AnimationSet D_actor_800300_8016BB30;
extern AnimationSet D_actor_800300_8016C2F4;
extern AnimationSet D_actor_800300_8016C818;
extern AnimationSet D_actor_800300_8016CB70;

TmdBone D_actor_800300_801630A0[19] = {
#include "assets/actor_800300_model_06A3C_skeleton.inc"
};

u32 D_actor_800300_8016334C[19] = {
#include "assets/actor_800300_model_06A3C_partVerts.inc"
};

SVECTOR D_actor_800300_80163398[365] = {
#include "assets/actor_800300_model_06A3C_verts.inc"
};

SVECTOR D_actor_800300_80163F00[386] = {
#include "assets/actor_800300_model_06A3C_normals.inc"
};

u32 D_actor_800300_80164B10[3923] = {
#include "assets/actor_800300_model_06A3C_stream.inc"
};

TmdSource D_actor_800300_8016885C = {
    0,
    21760,
    5992,
    19,
    D_actor_800300_8016334C,
    D_actor_800300_80163398,
    D_actor_800300_80163F00,
    D_actor_800300_801630A0,
    D_actor_800300_80164B10,
};

Actor800300MessageEntry D_actor_800300_80168880[26] = {
    { 1000, { .call1 = func_8010C4F0 } },
    { 1002, { .call1 = func_8010C4F0 } },
    { 1003, { .call1 = func_8010C4F0 } },
    { 1004, { .call1 = func_8010C4F0 } },
    { 1001, { .call4 = func_80104D68 } },
    { 1005, { .call7 = func_8010583C } },
    { 1006, { .transform = func_8010C688 } },
    { 1007, { .call1 = func_8010C4F0 } },
    { 1008, { .call0 = func_80105828 } },
    { 1009, { .call0 = func_8010C30C } },
    { 1010, { .call5 = func_8010C6C8 } },
    { 1011, { .call6 = func_80104684 } },
    { 1012, { .call1 = func_8010C648 } },
    { 1013, { .call9 = func_80105A60 } },
    { 1014, { .call3 = func_801052B8 } },
    { 1015, { .call2 = Gp_CopyAllyAnim } },
    { 1016, { .call1 = func_8010C4F0 } },
    { 1017, { .call7 = Gp_HurtAlly } },
    { 1018, { .call1 = func_8010C4F0 } },
    { 1019, { .call5 = func_8010C708 } },
    { 1020, { .call1 = func_8010C4F0 } },
    { 1021, { .call6 = func_801058BC } },
    { 1022, { .call8 = Gp_MoveActorByKeep } },
    { 1023, { .call0 = func_8010C30C } },
    { 1024, { .call0 = func_8010C30C } },
    { 1025, { .call6 = func_80105AB0 } },
};

GpImgRec** D_actor_800300_80168950[4] = {
    D_actor_800300_80169A20,
    D_actor_800300_80169A28,
    D_actor_800300_80169A38,
    D_actor_800300_80169A48,
};

GpImgRec** D_actor_800300_80168960[2] = {
    D_actor_800300_80169A68,
    D_actor_800300_80169A60,
};

u_long D_actor_800300_80168968[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD6F,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x6858AD87,
    0x81696969,
    0xE66F917F,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x36697FAD,
    0x80573636,
    0x879DAD98,
    0x59494A4B,
    0x53789D4B,
    0x9F9F9F56,
    0x7D569F9F,
    0xED9DD6AD,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x53416F9D,
    0x9F9F9F56,
    0x7D539F9F,
    0x4A4CBF6F,
    0x6FE64959,
    0xAEAED25E,
    0xAEAEAEAE,
    0x9F5CCFD2,
    0xBABF878A,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0xCF9F87C2,
    0xAEAED2D2,
    0xAEAEAEAE,
    0x878ACFD2,
    0x49494A5B,
    0xAED29F87,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x875CAEAE,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0xCFEDBFE2,
    0xAEAEAEBB,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x49878ACF,
    0xD29F5949,
    0x107D2AE,
    0x1010201,
    0xAEAE0101,
    0xE6ED5CBB,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0xAEB7D29F,
    0x1010102,
    0x7010102,
    0xCFAEAED2,
    0x5C49498A,
    0x5E5CD2CF,
    0x2040504,
    0x7070202,
    0xCFBBD207,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0xD29F9DC0,
    0x110707D2,
    0x1020205,
    0x5E5E0405,
    0x599FD2BB,
    0xCF5C5B4A,
    0x9B11537D,
    0x2FA076A,
    0x5E040404,
    0xC59D5C5E,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x4565C9F,
    0xAE9B9B05,
    0x5110702,
    0xCF5C9856,
    0x87494A6F,
    0x587DCF8A,
    0x4116A11,
    0x5110702,
    0x8A535304,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0x568A9DC5,
    0x6A040546,
    0x4010205,
    0x58460511,
    0x86AD5C5C,
    0x5CA6894A,
    0x56A485E,
    0x6A050505,
    0x5811059B,
    0xCA9D7053,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0x5648566F,
    0x56A1104,
    0x11050505,
    0x78CF5348,
    0x894A8791,
    0x53CF8A8C,
    0x9B6A6A46,
    0x56A9B9B,
    0xA6985853,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0x11054658,
    0x6A6A9B6A,
    0x5E469B11,
    0x87A97D5E,
    0x768F894A,
    0x53535E5E,
    0x6A6A6A6A,
    0x50585305,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x58988CA6,
    0x6A115653,
    0x53539B6A,
    0x7E985E5E,
    0xCA4A9D8F,
    0x467F7EAB,
    0x46467D53,
    0x517F5846,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x46464646,
    0x7F53537D,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

GpImgRec D_actor_800300_80168D50[2] = {
    { 0, 0, { 0, 0, 25, 20 }, D_actor_800300_80168968 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_80168D70[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD87,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x687F8787,
    0x81686868,
    0xE66F917F,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x81817FAD,
    0x98818181,
    0x879DAD6F,
    0x59494A4B,
    0x91789D4B,
    0x69686868,
    0x7F816969,
    0xED9DD691,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x817F6F9D,
    0x81696969,
    0x7D988181,
    0x4A4CBF6F,
    0x6FE64959,
    0x69687F5E,
    0x69696969,
    0xD67F8169,
    0xBABF87D6,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0x7F6F6FC2,
    0x69696981,
    0x81816969,
    0x878A7D98,
    0x49494A5B,
    0xD6D69F87,
    0xBBED8787,
    0xEDBBBBBB,
    0x87D68787,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0x6F87BFE2,
    0xBBED876F,
    0xD2BBBBBB,
    0xD6875BCF,
    0x49878AD6,
    0x9F9F5949,
    0xAEAED25C,
    0xAEAEAEAE,
    0x5CBBAEAE,
    0xE6ED87ED,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0xAEAE9F9F,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x875BBBAE,
    0x5C49498A,
    0xAEAED2CF,
    0x2020202,
    0x2020202,
    0xED5CAEAE,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0xD25C9DC0,
    0x20202D2,
    0x2020202,
    0xAEAE0202,
    0x599FD2BB,
    0xD25C5B4A,
    0x20702AE,
    0x2FA0202,
    0x5C070707,
    0xC59D5CBB,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x4565CD2,
    0x1070707,
    0x2020202,
    0xCFAEAE5E,
    0x87494A6F,
    0x5C02CF8A,
    0x2070704,
    0x5050702,
    0x8A5E5304,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0x568A9DC5,
    0x11040546,
    0x4010205,
    0x5E460404,
    0x86AD5CAE,
    0x5CA6894A,
    0x5045ED2,
    0x6A05056A,
    0x5811056A,
    0xCA9D7053,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0x5648566F,
    0x56A1104,
    0x4040505,
    0x78CF5348,
    0x894A8791,
    0x535E8A8C,
    0x11050446,
    0x5116A6A,
    0xA6985853,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0x4054658,
    0x46A9B6A,
    0x5E460504,
    0x87A97D5E,
    0x768F894A,
    0x53535E8A,
    0x5050504,
    0x50585305,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x58988CA6,
    0x5115653,
    0x53040405,
    0x7E985E5E,
    0xCA4A9D8F,
    0x467F7EAB,
    0x53535353,
    0x517F5846,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x53464646,
    0x7F535353,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

GpImgRec D_actor_800300_80169158[2] = {
    { 0, 0, { 0, 0, 25, 20 }, D_actor_800300_80168D70 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_80169178[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD87,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x91918787,
    0x91919191,
    0xE66F9191,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x767676AD,
    0x76767676,
    0x879DAD6F,
    0x59494A4B,
    0x91789D4B,
    0x7F7F9191,
    0x917F7F7F,
    0xED9DD691,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x76766F9D,
    0x7F7F7F7F,
    0x7D76767F,
    0x4A4CBF6F,
    0x78E64959,
    0x7F7F9191,
    0x7F7F7F7F,
    0xD6917F7F,
    0xBABF87D6,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0x76916FC2,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x878A7D76,
    0x49494A5B,
    0x7F91D687,
    0x6969697F,
    0x81696969,
    0x87D6917F,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0x9187BFE2,
    0x69817F91,
    0x69696969,
    0x767F7F69,
    0x49878AD6,
    0xD69F5949,
    0x69817F91,
    0x69696969,
    0x91816969,
    0xE6EDD6D6,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0x81919187,
    0x69696969,
    0x69696969,
    0x8776767F,
    0x5C49498A,
    0x7F91D69F,
    0x69696981,
    0x69696969,
    0xD6919181,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0x919D9DC0,
    0x69698191,
    0x69696969,
    0x767F6969,
    0x59878776,
    0xD65C5B4A,
    0x817F7FD6,
    0x81818181,
    0x917F7F81,
    0xC59DD691,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x7F919191,
    0x8181817F,
    0x7F818181,
    0x8787767F,
    0x87494A87,
    0xD2D2ED8A,
    0xD6D6D6ED,
    0xEDD6D6D6,
    0xEDAEAEED,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0xAEED9DC5,
    0xD6EDEDAE,
    0xD6D6D6D6,
    0xD2EDD6D6,
    0x86ADEDD2,
    0x9FA6894A,
    0xAEAEAE5C,
    0xAEAEAEAE,
    0xD2AEAEAE,
    0xCA9D70ED,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0xAED2ED6F,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x785CAEAE,
    0x894A8791,
    0xED8A8A8C,
    0xAEAED25C,
    0xD2AEAEAE,
    0xA698EDED,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0xD2EDED8C,
    0xAEAEAEAE,
    0xED5CD2AE,
    0x87A97D76,
    0x768F894A,
    0xA1767676,
    0xEDEDEDED,
    0x5076A1ED,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x8C8C8CA6,
    0xEDEDA18C,
    0xA1EDEDED,
    0x7E987676,
    0xCA4A9D8F,
    0x7F7F7EAB,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

GpImgRec D_actor_800300_80169560[2] = {
    { 0, 0, { 0, 0, 25, 20 }, D_actor_800300_80169178 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_80169580[140] = {
    0x42524545,
    0x79606042,
    0x41AA9074,
    0x6F8A9D6F,
    0x6079908C,
    0x45525242,
    0x35353545,
    0x42425245,
    0x42424242,
    0x64424242,
    0x52635061,
    0x52525245,
    0x45455252,
    0x45454545,
    0x42424242,
    0x42424242,
    0x45524242,
    0x35353535,
    0x57353535,
    0x45525252,
    0x52525245,
    0x42424242,
    0x42424242,
    0x45455242,
    0x35353535,
    0x57353529,
    0x52525252,
    0x52525252,
    0x42424242,
    0x42424242,
    0x52986052,
    0x57292929,
    0x45457E7F,
    0x52525252,
    0x64525252,
    0x42424264,
    0x735F4242,
    0x87B88689,
    0x8770ADAD,
    0x709D5ABA,
    0x4242607B,
    0x60606464,
    0x60426460,
    0x595B707A,
    0x6C6C6D6D,
    0x6D6C6D6D,
    0x596E6E6E,
    0x607589BA,
    0x79796064,
    0x98606464,
    0x788A8A6F,
    0x8A787878,
    0x8AD5D5D5,
    0x8A8A8A8A,
    0x9187EDED,
    0x79606079,
    0x9A626464,
    0x4552657E,
    0x8083557,
    0x29080808,
    0x67553529,
    0x767D987F,
    0x6060605F,
    0x60606464,
    0x65656464,
    0x29354565,
    0x29290829,
    0x617E5735,
    0x7A7A9898,
    0x6060605F,
    0x60606460,
    0x76927A5F,
    0x657E7BB1,
    0x65656565,
    0xA6AA759A,
    0x609691A6,
    0x60796060,
    0x60646460,
    0x91767A5F,
    0xB1DB9191,
    0x91DBACB1,
    0xAAAAA6AA,
    0x607A8F8C,
    0x60606060,
    0x60646060,
    0x91737762,
    0xD4AAAAAA,
    0xAA8C8CD4,
    0x73918CAA,
    0x60629575,
    0x79796060,
    0x60606079,
    0x76777960,
    0xAC737373,
    0x92927292,
    0x7A959292,
    0x60606062,
    0x93747979,
    0x60797979,
    0x63646464,
    0x65667E62,
    0x64646465,
    0x64656464,
    0x60646464,
    0x93937479,
    0x60797493,
    0x45524264,
    0x29293535,
    0x35353529,
    0x45573535,
    0x60644252,
    0x8D937479,
    0x79749390,
    0x35456460,
    0x9080808,
    0x8090909,
    0x35290808,
    0x64645245,
    0x8D937479,
    0x7493908E,
    0x35526079,
    0x8080829,
    0x9090909,
    0x29080808,
    0x60645257,
    0x908D7479,
    0x8E8FA98C,
    0x646074AF,
    0x29353545,
    0x8080829,
    0x35080808,
    0x79606545,
    0x8BD99093,
    0xA4D4D4CE,
    0x96B08FA9,
    0x52656462,
    0x35354545,
    0x52573535,
    0x90937964,
    0xA2D3D7D9,
};

GpImgRec D_actor_800300_801697B0[2] = {
    { 0, 0, { 0, 0, 14, 20 }, D_actor_800300_80169580 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

u_long D_actor_800300_801697D0[140] = {
    0x42524545,
    0x79606042,
    0x41AA9074,
    0x6F8A9D6F,
    0x6079908C,
    0x45525242,
    0x35353545,
    0x42425245,
    0x42424242,
    0x64424242,
    0x52635061,
    0x52525245,
    0x45455252,
    0x45454545,
    0x42424242,
    0x42424242,
    0x45524242,
    0x35353535,
    0x57353535,
    0x45525252,
    0x52525245,
    0x42424242,
    0x42424242,
    0x7F7F6042,
    0x7F7F7F7F,
    0x57607F7F,
    0x52525252,
    0x52525252,
    0x42424242,
    0x42424242,
    0x6D6D5A89,
    0x6D6D6D6D,
    0x899D5A6D,
    0x52525289,
    0x64525252,
    0x42424264,
    0x6D604242,
    0x40401DD,
    0x4040404,
    0xDD010404,
    0x42646DDD,
    0x60606464,
    0x60426460,
    0xDD6D6065,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0x646DDDF6,
    0x79796064,
    0x65606464,
    0xF6DD6D60,
    0xF7F7F6F6,
    0xF7F7F7F7,
    0xF6F7F7F7,
    0x6DDDF6F6,
    0x79606079,
    0x65626464,
    0xF6F6DD6D,
    0xDDF7F7F6,
    0xDDDDDDDD,
    0xF7F7DDDD,
    0xDDF6F6F6,
    0x6060606D,
    0x60606464,
    0xF6F6DD6D,
    0xDDDDF6F6,
    0x6B6B6BDE,
    0xF6DDDDDE,
    0xDDF6F6F6,
    0x6060606D,
    0x60606460,
    0xF6F6DD6D,
    0x6BDDF6F6,
    0x6B6B6B6B,
    0xF6DD6B6B,
    0xDDF6F6F6,
    0x6077646D,
    0x45776460,
    0xF6F6DD6D,
    0xDEDDF6F6,
    0x6B6B6B6B,
    0xF6DDDE6B,
    0xDDF6F6F6,
    0x6077646D,
    0x45776060,
    0xF6DD6DD3,
    0xDDF6F6F6,
    0xDEDEDEDE,
    0xF6F6DDDE,
    0x6DDDF6F6,
    0x797764D3,
    0x45776079,
    0xDDD3D335,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xD3D3DDF6,
    0x93776435,
    0x64777779,
    0x77353545,
    0x5A8989D3,
    0x5A5A5A5A,
    0xD389895A,
    0x35356477,
    0x93776445,
    0x77777793,
    0x35456464,
    0x64353535,
    0x64646464,
    0x35356464,
    0x45453535,
    0x8D937764,
    0x77777790,
    0x64647777,
    0x9083545,
    0x8090909,
    0x35290808,
    0x77776464,
    0x8D937477,
    0x7777908E,
    0x77777777,
    0x77777777,
    0x77777777,
    0x77777777,
    0x60777777,
    0x908D7479,
    0x8E8FA98C,
    0x646074AF,
    0x52525252,
    0x52525252,
    0x52525252,
    0x79606552,
    0x8BD99093,
    0xA4D4D4CE,
    0x96B08FA9,
    0x52656462,
    0x52525245,
    0x79525252,
    0x90937979,
    0xA2D3D7D9,
};

GpImgRec D_actor_800300_80169A00[2] = {
    { 0, 0, { 0, 0, 14, 20 }, D_actor_800300_801697D0 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

GpImgRec* D_actor_800300_80169A20[2] = {
    D_actor_800300_80168D50,
    NULL,
};

GpImgRec* D_actor_800300_80169A28[4] = {
    D_actor_800300_80168D50,
    D_actor_800300_80169158,
    D_actor_800300_80169560,
    NULL,
};

GpImgRec* D_actor_800300_80169A38[4] = {
    D_actor_800300_80169560,
    D_actor_800300_80169158,
    D_actor_800300_80168D50,
    NULL,
};

GpImgRec* D_actor_800300_80169A48[6] = {
    D_actor_800300_80168D50,
    D_actor_800300_80169158,
    D_actor_800300_80169560,
    D_actor_800300_80169158,
    D_actor_800300_80168D50,
    NULL,
};

GpImgRec* D_actor_800300_80169A60[2] = {
    D_actor_800300_801697B0,
    NULL,
};

GpImgRec* D_actor_800300_80169A68[2] = {
    D_actor_800300_80169A00,
    NULL,
};

AnimationPackedPose D_actor_800300_80169A70[2] = {
#include "assets/actor_800300_animation_07E3C_bank1.inc"
};

AnimationPackedRotation D_actor_800300_80169A88[23] = {
#include "assets/actor_800300_animation_07E3C_bank4.inc"
};

AnimationRecord D_actor_800300_80169AE4[84] = {
#include "assets/actor_800300_animation_07E3C_records.inc"
};

u16 D_actor_800300_80169C34[20] = {
#include "assets/actor_800300_animation_07E3C_indices.inc"
};

AnimationSet D_actor_800300_80169C5C = {
    D_actor_800300_80169AE4,
    D_actor_800300_80169C34,
    { NULL, D_actor_800300_80169A70, NULL, NULL, D_actor_800300_80169A88, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_80169C84[6] = {
#include "assets/actor_800300_animation_08218_bank1.inc"
};

AnimationPackedRotation D_actor_800300_80169CCC[73] = {
#include "assets/actor_800300_animation_08218_bank4.inc"
};

AnimationRecord D_actor_800300_80169DF0[136] = {
#include "assets/actor_800300_animation_08218_records.inc"
};

u16 D_actor_800300_8016A010[20] = {
#include "assets/actor_800300_animation_08218_indices.inc"
};

AnimationSet D_actor_800300_8016A038 = {
    D_actor_800300_80169DF0,
    D_actor_800300_8016A010,
    { NULL, D_actor_800300_80169C84, NULL, NULL, D_actor_800300_80169CCC, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016A060[5] = {
#include "assets/actor_800300_animation_08618_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016A09C[76] = {
#include "assets/actor_800300_animation_08618_bank4.inc"
};

AnimationRecord D_actor_800300_8016A1CC[145] = {
#include "assets/actor_800300_animation_08618_records.inc"
};

u16 D_actor_800300_8016A410[20] = {
#include "assets/actor_800300_animation_08618_indices.inc"
};

AnimationSet D_actor_800300_8016A438 = {
    D_actor_800300_8016A1CC,
    D_actor_800300_8016A410,
    { NULL, D_actor_800300_8016A060, NULL, NULL, D_actor_800300_8016A09C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016A460[14] = {
#include "assets/actor_800300_animation_09194_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016A508[301] = {
#include "assets/actor_800300_animation_09194_bank4.inc"
};

AnimationRecord D_actor_800300_8016A9BC[372] = {
#include "assets/actor_800300_animation_09194_records.inc"
};

u16 D_actor_800300_8016AF8C[20] = {
#include "assets/actor_800300_animation_09194_indices.inc"
};

AnimationSet D_actor_800300_8016AFB4 = {
    D_actor_800300_8016A9BC,
    D_actor_800300_8016AF8C,
    { NULL, D_actor_800300_8016A460, NULL, NULL, D_actor_800300_8016A508, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016AFDC[2] = {
#include "assets/actor_800300_animation_09478_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016AFF4[58] = {
#include "assets/actor_800300_animation_09478_bank4.inc"
};

AnimationRecord D_actor_800300_8016B0DC[101] = {
#include "assets/actor_800300_animation_09478_records.inc"
};

u16 D_actor_800300_8016B270[20] = {
#include "assets/actor_800300_animation_09478_indices.inc"
};

AnimationSet D_actor_800300_8016B298 = {
    D_actor_800300_8016B0DC,
    D_actor_800300_8016B270,
    { NULL, D_actor_800300_8016AFDC, NULL, NULL, D_actor_800300_8016AFF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016B2C0[2] = {
#include "assets/actor_800300_animation_0981C_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016B2D8[83] = {
#include "assets/actor_800300_animation_0981C_bank4.inc"
};

AnimationRecord D_actor_800300_8016B424[124] = {
#include "assets/actor_800300_animation_0981C_records.inc"
};

u16 D_actor_800300_8016B614[20] = {
#include "assets/actor_800300_animation_0981C_indices.inc"
};

AnimationSet D_actor_800300_8016B63C = {
    D_actor_800300_8016B424,
    D_actor_800300_8016B614,
    { NULL, D_actor_800300_8016B2C0, NULL, NULL, D_actor_800300_8016B2D8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016B664[4] = {
#include "assets/actor_800300_animation_09D10_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016B694[76] = {
#include "assets/actor_800300_animation_09D10_bank4.inc"
};

AnimationRecord D_actor_800300_8016B7C4[209] = {
#include "assets/actor_800300_animation_09D10_records.inc"
};

u16 D_actor_800300_8016BB08[20] = {
#include "assets/actor_800300_animation_09D10_indices.inc"
};

AnimationSet D_actor_800300_8016BB30 = {
    D_actor_800300_8016B7C4,
    D_actor_800300_8016BB08,
    { NULL, D_actor_800300_8016B664, NULL, NULL, D_actor_800300_8016B694, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016BB58[18] = {
#include "assets/actor_800300_animation_0A4D4_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016BC30[184] = {
#include "assets/actor_800300_animation_0A4D4_bank4.inc"
};

AnimationRecord D_actor_800300_8016BF10[239] = {
#include "assets/actor_800300_animation_0A4D4_records.inc"
};

u16 D_actor_800300_8016C2CC[20] = {
#include "assets/actor_800300_animation_0A4D4_indices.inc"
};

AnimationSet D_actor_800300_8016C2F4 = {
    D_actor_800300_8016BF10,
    D_actor_800300_8016C2CC,
    { NULL, D_actor_800300_8016BB58, NULL, NULL, D_actor_800300_8016BC30, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016C31C[5] = {
#include "assets/actor_800300_animation_0A9F8_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016C358[101] = {
#include "assets/actor_800300_animation_0A9F8_bank4.inc"
};

AnimationRecord D_actor_800300_8016C4EC[193] = {
#include "assets/actor_800300_animation_0A9F8_records.inc"
};

u16 D_actor_800300_8016C7F0[20] = {
#include "assets/actor_800300_animation_0A9F8_indices.inc"
};

AnimationSet D_actor_800300_8016C818 = {
    D_actor_800300_8016C4EC,
    D_actor_800300_8016C7F0,
    { NULL, D_actor_800300_8016C31C, NULL, NULL, D_actor_800300_8016C358, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_800300_8016C840[4] = {
#include "assets/actor_800300_animation_0AD50_bank1.inc"
};

AnimationPackedRotation D_actor_800300_8016C870[41] = {
#include "assets/actor_800300_animation_0AD50_bank4.inc"
};

AnimationRecord D_actor_800300_8016C914[141] = {
#include "assets/actor_800300_animation_0AD50_records.inc"
};

u16 D_actor_800300_8016CB48[20] = {
#include "assets/actor_800300_animation_0AD50_indices.inc"
};

AnimationSet D_actor_800300_8016CB70 = {
    D_actor_800300_8016C914,
    D_actor_800300_8016CB48,
    { NULL, D_actor_800300_8016C840, NULL, NULL, D_actor_800300_8016C870, NULL, NULL, NULL },
};

AnimationSet* D_actor_800300_8016CB98[79] = {
    NULL,
    &D_actor_800300_80169C5C,
    &D_actor_800300_8016A038,
    &D_actor_800300_8016A438,
    &D_actor_800300_8016AFB4,
    &D_actor_800300_8016B298,
    &D_actor_800300_8016B63C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_8016C818,
    &D_actor_800300_8016CB70,
    &D_actor_800300_8016C2F4,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_8016BB30,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    &D_actor_800300_80169C5C,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static void func_actor_800300_80161E80(Task* arg0);
static void func_actor_800300_80162064(Task* arg0);
static void func_actor_800300_8016259C(Task* arg0);

static void func_actor_800300_80161E80(Task* arg0)
{
    GameActor*             actor;
    TmdObject*             extra;
    GfxCoord*              coord;
    GfxCoord*              next;
    GfxCoord**             addr;
    GpActorD4*             d4;
    WorldCollisionBody*    obj;
    WorldCollisionContact* recs;
    McSaveData*            save;
    s32                    packed;
    s8                     fcc;

    actor = arg0->work;
    extra = arg0->extra.tmd;
    d4    = actor->field_910;
    addr  = &extra->coords;
    coord = *addr;
    arg0->state++;
    arg0->msgTable      = D_actor_800300_80168880;
    arg0->exitCallback  = &func_actor_800300_801625A8;
    actor->field_938    = 0x13;
    Gp_ActorSlots[1]    = arg0;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
    func_8010BFCC(arg0);
    actor->field_985 = 0x10;
    Gp_AnimResetChildSlots(arg0, actor->field_93C);
    recs                        = actor->field_17C;
    obj                         = (WorldCollisionBody*)actor->field_AC;
    actor->field_10             = coord->coord.t[0];
    actor->field_14             = coord->coord.t[1];
    actor->field_18             = coord->coord.t[2];
    obj->context.motion         = &actor->field_88[0];
    obj->coord                  = coord;
    actor->field_88[0].contacts = recs;
    save                        = &Mc_SaveData[0];
    obj->pos.vx                 = 0;
    obj->pos.vy                 = -0x12C;
    obj->pos.vz                 = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0x12C;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        packed      = 0x10000;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    Gp_InitRec18Table(actor->field_88[0].contacts, ARRAY_SIZE(actor->field_17C), 0);
    obj->flags                 |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    obj                         = (WorldCollisionBody*)actor->field_CC;
    next                        = arg0->extra.tmd->coords;
    obj->context.motion         = &actor->field_88[1];
    obj->coord                  = next + 4;
    actor->field_88[1].contacts = recs;
    obj->pos.vx                 = 0;
    obj->pos.vy                 = 0;
    obj->pos.vz                 = 0;
    {
        s32 temp;

        temp        = save->state.characterId;
        obj->radius = 0xC8;
        obj->flags  = WORLD_COLLISION_BODY_MOTION_SPHERE;
        obj->key    = temp | packed;
        Gp_LinkObj(0, obj);
    }
    obj->flags      |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    actor->field_984 = 7;
    func_8010BF7C(arg0, 0x3C, 0x7F);
    fcc             = -0x6A;
    d4->repeatCount = fcc;
}

static void func_actor_800300_80162064(Task* arg0)
{
    void**                 scratch;
    u8*                    head;
    Actor800300VecScratch* sc;
    GameActor*             actor;
    TmdObject*             obj;
    TmdObject*             extra;
    GfxCoord*              coord;
    WorldCollisionBody*    objs[2];
    s32                    dy;
    s32                    i;
    s8                     bits;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    obj                            = arg0->extra.tmd;
    SCRATCH_HEAD_AT(scratch, void) = head - 0x18;
    extra                          = obj;
    sc                             = (Actor800300VecScratch*)(head - 0x18);
    actor                          = arg0->work;
    coord                          = extra->coords;
    if (actor->field_954 != 2 &&
        (dy = coord->coord.t[1], dy = dy - actor->field_14, dy = ABS(dy), dy >= 0x200)) {
        coord->coord.t[0] = actor->field_10;
        coord->coord.t[1] = actor->field_14;
        coord->coord.t[2] = actor->field_18;
    } else {
        if (actor->field_984 & 1) {
            actor->field_992 = func_801011D0(coord, actor->field_88[0].contacts, ARRAY_SIZE(actor->field_17C), &actor->field_930);
            if ((s8)actor->field_992 == 2) {
                coord->coord.t[0] = actor->field_10;
                coord->coord.t[1] = actor->field_14;
                coord->coord.t[2] = actor->field_18;
            }
        } else {
            actor->field_992 = 0;
        }
        actor->field_10 = coord->coord.t[0];
        actor->field_14 = coord->coord.t[1];
        actor->field_18 = coord->coord.t[2];
    }
    objs[0] = (WorldCollisionBody*)actor->field_AC;
    objs[1] = (WorldCollisionBody*)actor->field_CC;
    for (i = 0; i < 2; i++) {
        bits = actor->field_983;
        if ((bits >> i) & 1) {
            actor->field_984 |= 1 << i;
            objs[i]->flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        } else if (bits & (8 << i)) {
            actor->field_984 &= ~(1 << i);
            objs[i]->flags   &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
        }
    }
    actor->field_983 = 0;
    if (D_80115768 == 0) {
        func_actor_800300_80162C2C(arg0);
    }
    func_actor_800300_801623F8(arg0);
    Gp_ClearRec18Occupied(actor->field_17C);
    if (actor->field_984 & 1) {
        coord->coord.t[1] = actor->field_14 + 0x10;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    if ((s8)actor->field_986 != 0) {
        sc->vec.vx = (u16)actor->field_30.vx;
        sc->vec.vy = (u16)actor->field_30.vy;
        sc->vec.vz = (u16)actor->field_30.vz;
    } else {
        sc->vec.vx = (u16)coord->workm.m[0][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
        sc->vec.vy = (u16)coord->workm.m[1][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
        sc->vec.vz = (u16)coord->workm.m[2][2] * (s8)((volatile Actor800300DirByte*)actor)->field_973;
    }
    actor->field_88[0].motionDirection.vx = sc->vec.vx;
    actor->field_88[0].motionDirection.vy = sc->vec.vy;
    actor->field_88[0].motionDirection.vz = sc->vec.vz;
    actor->field_88[1].motionDirection.vx = sc->vec.vx;
    actor->field_88[1].motionDirection.vy = sc->vec.vy;
    actor->field_88[1].motionDirection.vz = sc->vec.vz;
    actor->field_88[2].motionDirection.vx = sc->vec.vx;
    actor->field_88[2].motionDirection.vy = sc->vec.vy;
    actor->field_88[2].motionDirection.vz = sc->vec.vz;
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&coord->workm), (VECTOR3*)sc) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)sc, 0x200, Gp_State1C->groundShadowShade);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

static void func_actor_800300_801623F8(Task* arg0)
{
    void**      scratch;
    u8*         head;
    u8*         temp;
    RECT*       rect;
    GameActor*  actor;
    GpImgRec*** table;
    s32         idx;
    u32         row;
    GpImgRec*   img;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    actor                          = arg0->work;
    temp                           = head - 8;
    SCRATCH_HEAD_AT(scratch, void) = temp;
    rect                           = (RECT*)temp;

    if ((s8)actor->field_987 != 0) {
        actor->field_988--;
        if ((s8)actor->field_988 <= 0) {
            table = D_actor_800300_80168950;
            idx   = (s8)actor->field_987 - 1;
            img   = table[idx][(s8)actor->field_989];
            if (img != NULL) {
                ((RECT*)head)[-1].x = 0;
                rect->y             = 0x40;
                rect->w             = 0x19;
                rect->h             = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_988 = 4;
                actor->field_989++;
            } else {
                actor->field_987 = 0;
            }
        }
    }

    if ((s8)actor->field_98A != 0) {
        actor->field_98B--;
        if ((s8)actor->field_98B <= 0) {
            table = D_actor_800300_80168960;
            idx   = (row = (s8)actor->field_98A - 1);
            img   = table[row][(s8)actor->field_98C];
            if (img != NULL) {
                rect->x = 0xC;
                rect->y = 0x60;
                rect->w = 0xE;
                rect->h = 0x14;
                Gp_LoadActorImage(arg0, img, rect);
                actor->field_98B = 8;
                actor->field_98C++;
            } else {
                actor->field_98A = 0;
            }
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_actor_800300_8016259C(Task* arg0)
{
    arg0->state = 3;
}

/// Teardown of the actor's main task, run both as its exit callback and as
/// the last entry of its state table: clears the second `Gp_ActorSlots` slot,
/// unlinks the two collision objects the set-up state linked, and kills the
/// task.
static void func_actor_800300_801625A8(Task* task)
{
    GameActor* actor;

    actor            = (GameActor*)task->work;
    Gp_ActorSlots[1] = NULL;
    Gp_UnlinkObj((WorldCollisionBody*)actor->field_AC);
    Gp_UnlinkObj((WorldCollisionBody*)actor->field_CC);
    taskKill(task);
}

/// State handlers of the actor's main task, indexed by its state: set-up, the
/// per-frame update, a step that only advances to the last state, and the
/// teardown.
static const TaskFuncTable4 D_actor_800300_80161E24 = { {
    func_actor_800300_80161E80,
    func_actor_800300_80162064,
    func_actor_800300_8016259C,
    func_actor_800300_801625A8,
} };

/// Per-frame entry point of the actor's main task: runs the handler its state
/// selects. The table is a local, so it is copied from `.rodata` onto the
/// stack on every call.
void func_actor_800300_801625F4(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_800300_80161E24;
    states.funcs[task->state](task);
}

/// Handlers `func_actor_800300_80162C2C` runs, indexed by `field_954`.
static const TaskFuncTable3 D_actor_800300_80161E34 = { {
    func_actor_800300_80162658,
    func_actor_800300_80162F24,
    func_actor_800300_80162F98,
} };

/// Behaviours `func_actor_800300_80162658` runs, indexed by `field_956`.
static const TaskFuncTable9 D_actor_800300_80161E40 = { {
    func_actor_800300_80162C98,
    func_actor_800300_801628D0,
    func_actor_800300_80162A98,
    func_actor_800300_80162C98,
    func_actor_800300_80162C98,
    func_actor_800300_80162EEC,
    func_actor_800300_80162C98,
    func_actor_800300_80162D74,
    func_actor_800300_80162C98,
} };

static void func_actor_800300_80162658(Task* arg0)
{
    TaskFuncTable9 sp;
    GameActor*     actor;
    GpActorD4*     d4;
    GfxCoord*      obj;
    s8             cc;
    s32            pan;
    s32            depth;
    s32            anim;
    s32            sound;

    sp    = D_actor_800300_80161E40;
    actor = arg0->work;
    d4    = actor->field_910;
    obj   = arg0->extra.tmd->coords;
    if (d4->decisionTimer > 0) {
        d4->decisionTimer = (u16)d4->decisionTimer - 1;
    }
    if (D_8017A99C >= 0x30C) {
        if (actor->field_956 != 5) {
            actor->field_942++;
            if ((s16)actor->field_942 >= (s8)d4->repeatCount) {
                actor->field_942 = 0;
                d4->actionCount++;
                cc              = (u8)d4->repeatCount - 7;
                d4->repeatCount = cc;
                if (cc < 0x5A) {
                    d4->repeatCount = 0x3C;
                }
                actor->field_95C = 7;
                actor->field_956 = 5;
                actor->field_958 = 0;
                actor->field_95A = 0;
                actor->field_95E = 0;
                actor->field_97E = 1;
                Gp_PlayObjSfx(obj, (rand() & 1) + 0x55170005, 0);
                if (Gp_HurtAlly(arg0, 0, 0x40010, 0) != 0) {
                    return;
                }
                anim = 0x10;
                if ((s8)d4->actionCount >= 5) {
                    anim = 0x11;
                }
                Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 3);
            }
        }
    }
    sp.funcs[actor->field_956](arg0);
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, &actor->field_17C[0]);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan   = (s8)Gp_GetObjPan(obj);
            depth = (s8)gpGetObjDepth(obj);
            sound = 7;
            if ((u16)actor->field_96C == 1) {
                sound = 6;
            }
            SndEvt_EnqueueType6(sound, pan, depth);
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
    if (Mc_SaveData[0].state.companionHp <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

static void func_actor_800300_801628D0(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR3*   vec;
    s32        dist;
    s32        angle;
    s32        arg;

    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    actor  = arg0->work;
    switch (actor->field_95E) {
        case 0:
            actor->field_934 = 0;
            if (func_8010BC70(coord) >= 0xE00) {
                arg              = 4;
                actor->field_95E = 2;
                actor->field_958 = 3;
            } else {
            resume:
                if (actor->field_95E != 3) {
                    actor->field_95E = 1;
                }
                actor->field_958 = 7;
                arg              = 2;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
        case 2:
        case 3:
            actor->field_973 = 1;
            dist             = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            if (actor->field_95E == 3) {
                break;
            }
            actor->field_934++;
            if (actor->field_934 == 0xB4) {
                actor->field_95E = 3;
                goto resume;
            }
            if (actor->field_93E > 0) {
                actor->field_93E = (u16)actor->field_93E - 1;
            } else {
                angle = rand() & 0x3FF;
                if ((0x800 - angle) < dist) {
                    goto in_range;
                }
                if (actor->field_95E == 2) {
                    goto reset;
                }
            in_range:
                if (dist < angle + 0xC00) {
                    break;
                }
                if (actor->field_95E != 1) {
                    break;
                }
            reset:
                actor->field_95E = 0;
                actor->field_93E = 0x3C;
            }
            break;
    }
    vec = MATRIX_TRANS(&target->coord);
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
    func_80105ED4(arg0);
}

static void func_actor_800300_80162A98(Task* arg0)
{
    u8*              head;
    VECTOR3*         vec;
    GameActor*       actor;
    WorldTargetNode* node;
    TmdObject*       extra;
    GfxCoord*        src;
    s32              val;
    s32              arg;
    s32              flag;

    actor                    = arg0->work;
    extra                    = (gameGetPtrSlot(3))->extra.tmd;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    vec                      = (VECTOR3*)(head - 0x10);
    node                     = actor->field_90C;
    src                      = extra->coords;
    if (node != NULL) {
        if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(node, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = src->coord.t[0];
        vec->vy                       = src->coord.t[1];
        vec->vz                       = src->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            if (func_8010BCF4(arg0, vec) < 0) {
                actor->field_93E = -1;
                arg              = 5;
            } else {
                actor->field_93E = 1;
                arg              = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, arg, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_975 = (u8)actor->field_93E;
            val              = func_8010BCF4(arg0, vec);
            if (val < 0) {
                val = -val;
            }
            if ((val < 0x81) || (actor->field_95E == 2)) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&src->coord));
    func_80105ED4(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800300_80162C2C(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable3 sp;

    sp               = D_actor_800300_80161E34;
    actor            = arg0->work;
    actor->field_973 = 0;
    actor->field_975 = 0;
    sp.funcs[actor->field_954](arg0);
    actor->field_986 = 0;
}

static void func_actor_800300_80162C98(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  target;
    s32        val;

    actor  = arg0->work;
    coord  = arg0->extra.tmd->coords;
    target = (gameGetPtrSlot(3))->extra.tmd->coords;
    if (((GameActor*)arg0->work)->field_910->decisionTimer <= 0) {
        func_8010BF7C(arg0, 0x14, 0x3F);
        if ((u32)(func_8010BC70(coord) - 0x581) < 0x87F) {
            func_actor_800300_80163048(arg0);
        }
        val = func_8010BCF4(arg0, MATRIX_TRANS(&target->coord));
        if (val < 0) {
            val = -val;
        }
        if (val >= 0x200) {
            actor->field_90C = NULL;
            func_actor_800300_80163074(arg0);
        }
    }
    func_8010BE5C(arg0, MATRIX_TRANS(&target->coord));
    func_80105ED4(arg0);
}

static void func_actor_800300_80162D74(Task* arg0)
{
    GameActor*       actor;
    GfxCoord*        coord;
    GfxCoord*        target;
    WorldTargetNode* lock;
    u8*              head;
    VECTOR3*         vec;
    u16              state;

    coord                    = arg0->extra.tmd->coords;
    target                   = (gameGetPtrSlot(3))->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    vec                      = (VECTOR3*)(head - 0x10);
    actor                    = arg0->work;
    lock                     = actor->field_90C;
    if (lock != NULL) {
        if (!(lock->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            Gp_GetLockPos(lock, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->field_95E;
    switch (state) {
        case 0:
            actor->field_95E = 1;
            actor->field_934 = 0;
            actor->field_958 = 3;
            Gp_AnimPlayChildSlotsEx(arg0, 0xC, 0, 5);
            /* fallthrough */
        case 1:
            actor->field_973 = 1;
            if (func_8010BC70(coord) < 0x601) {
                Gp_ResetActorMove(arg0, 0);
            }
            break;
    }
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
    func_80105ED4(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

static void func_actor_800300_80162EEC(Task* arg0)
{
    if (((GameActor*)arg0->work)->field_95E == 1) {
        Gp_ResetActorMove(arg0, 0);
    }
}

static void func_actor_800300_80162F24(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    s32        flag;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->field_95E) {
        case 0:
            flag               = 1;
            actor->field_95E   = flag;
            coord->coord.t[1] += 0xC0;
        case 1:
            Gp_AnimTickChildSlots(arg0);
            Gp_TurnPlayer(arg0);
            Gp_StepPlayerMove(arg0);
            break;
    }
}

/// Handlers `func_actor_800300_80162F98` runs, indexed by `field_956`: the
/// gameplay module's own mode-2 player states.
static const TaskFuncTable7 D_actor_800300_80161E64 = { {
    Gp_PlayerMode2State0,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State2,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State4,
    Gp_PlayerMode2State1,
    Gp_PlayerMode2State6,
} };

static void func_actor_800300_80162F98(Task* arg0)
{
    GameActor*     actor;
    TaskFuncTable7 sp;

    sp    = D_actor_800300_80161E64;
    actor = arg0->work;
    sp.funcs[(u16)actor->field_956](arg0);
    Gp_TurnPlayer(arg0);
    if (Mc_SaveData[0].state.companionHp <= 0) {
        Gp_StopPlayerAnim(arg0, 0);
    }
}

/// Switches the actor's update into its approach behaviour (entry 1 of the
/// behaviour table), restarting the behaviour's step and counters and setting
/// the approach timer to 60 frames.
static void func_actor_800300_80163048(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 1;
    actor->field_95A = 1;
    actor->field_954 = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0x3C;
}

/// Switches the actor's update into its turn-to-face behaviour (entry 2 of
/// the behaviour table), restarting the behaviour's step and counters.
static void func_actor_800300_80163074(Task* arg0)
{
    GameActor* actor;

    actor            = arg0->work;
    actor->field_956 = 2;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 1;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
}
