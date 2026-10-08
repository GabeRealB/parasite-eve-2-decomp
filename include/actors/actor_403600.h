#ifndef INCLUDE_ACTORS_ACTOR_403600_H
#define INCLUDE_ACTORS_ACTOR_403600_H

#include "common.h"
#include "types.h"

#include "main/coord.h"
#include "main/tmd_types.h"

/// Samples of its wave an `Actor403600Ripple` keeps: two for each of the
/// sixteen rings its disc is drawn in.
enum { ACTOR_403600_RIPPLE_SAMPLE_COUNT = 0x20 };

/// A ripple spreading over a disc that is drawn with the captured frame: the
/// wave's source at the centre and the samples of it still travelling outward.
///
/// Every tick the source's phase and strength are recorded as one sample at
/// `head`, which steps backward through the two sample rings. The disc is
/// drawn as twelve sectors of sixteen rings, and ring *n* shows the sample
/// taken 2*n* ticks earlier, so the wave moves out one ring every two ticks.
/// A sample lifts its ring out of the disc's plane by the sine of its phase
/// times its strength, and shifts the texels of the captured frame the ring
/// shows by an amount that grows with its strength; both fade toward the rim.
///
/// The owner sets `emitting` while the source is fed. The strength then rises,
/// and falls away once the owner clears it; a ripple whose samples all have
/// phase 0 has died out. actor_403600 allocates one zeroed for each ripple
/// task, and actor_361100 allocates its own, sets `shallow` and runs it
/// through actor_403600's tick and draw.
typedef struct {
    s16      phase[ACTOR_403600_RIPPLE_SAMPLE_COUNT];    // Wave phase of each sample, 4096 per turn; 0 in a slot that recorded no wave
    s16      strength[ACTOR_403600_RIPPLE_SAMPLE_COUNT]; // Wave strength of each sample, 0 to 0x1000
    s32      head;                                       // Slot of the newest sample; steps back one slot a tick and wraps
    s32      sourcePhase;                                // Phase the next sample takes: restarts at 0 on the tick emission begins, then advances 0x180 a sample, or 0x100 when `shallow`
    s32      sourceStrength;                             // Strength the next sample takes: rises 0x200 a tick to 0x1000 while `emitting`, falls 0x80 a tick to 0 otherwise. No sample is recorded while it is 0
    s16      wasEmitting;                                // `emitting` as the previous tick found it, which tells the tick emission begins
    s16      emitting;                                   // Written by the owner (0 the wave dies away, nonzero the source is fed)
    GfxCoord clipCoord;                                  // actor_403600 only: the disc's plane as a coordinate of its own under the disc's, turned half a turn about Z for a ripple spawned in mode 2. While the package's model drawers are pointed at it they flatten every vertex on its positive-Y side onto the plane
    s32      shallow;                                    // How far a sample lifts its ring (0 an eighth of the wave's height, 1 a 256th of it, with the slower phase advance)
    s32      clipCountdown;                              // actor_403600 only, counted down each tick. Spawn mode 1 starts it at 8, cuts the owner's model at `clipCoord` until it reaches 0 and then hides the model; mode 2 starts it at 0x1F, shows the model when it reaches 0 and cuts it for eight ticks more
} Actor403600Ripple;
STATIC_ASSERT_SIZEOF(Actor403600Ripple, 0xE8);

/// Records one ripple-source sample and advances its outward history.
///
/// Requires actor_403600 loaded and a live initialized ripple with head in 0..31.
/// Emission adds 512 strength per tick while below 4096; release subtracts 128
/// while positive. An emission edge restarts phase. Clears the new head before
/// recording nonzero strength. Phase uses 4096 units per turn and advances 384,
/// or 256 when shallow; samples retain its signed low halfword. Owns no storage
/// and retains no pointer. actor_361100 calls this while the owning overlay lives.
void actor403600TickRipple(Actor403600Ripple* state);

/// Draws a spreading ripple with the captured frame as its texture.
///
/// Requires this overlay loaded, a live ripple with head in 0..31, a live disc
/// coordinate and initialized GTE/scratch/frame-packet state. Composes discCoord;
/// sixteen radial samples form fifteen drawable bands in each of twelve sectors.
/// Consumes 192 complete POLY_FT4-sized records from D_actor_403600_8016069C,
/// including each sector's terminal record. The caller supplies that writable
/// capacity, a captured frame at VRAM (448,256), and a valid depth ordering table.
/// Ring distances and lifts use game units; UV displacement uses screen pixels
/// scaled by the sample strength. Queues a new frame capture beyond the greatest
/// drawn depth. Retains packet storage until GPU consumption; no ripple pointer
/// is retained. actor_361100 calls this export while actor_403600 remains loaded.
void actor403600DrawRipple(const Actor403600Ripple* ripple, GfxCoord* discCoord);

extern u8* D_actor_403600_8016069C;

/// Projects, lights and draws GT3 packets with the package's bottom screen fade.
///
/// Resident stream resolution selects this actor-overlay export for 0x8038;
/// actor_403600 must be loaded while the stored callback is used. `elements`
/// follows the three-word command header. `workspace->elemCount` is 0..65535
/// and `elemStride` is the stride in u32 words, at least three. The first six
/// u16 values are vertex offsets 0..2 then normal offsets 0..2, in bytes;
/// low three bits are discarded. Each resulting offset must address a complete
/// eight-byte SVECTOR in its borrowed array. Texture words have already been
/// copied into the selected buffer half's packets by construction.
///
/// The GTE holds the part transform, projection and lighting state. `primWrite`
/// addresses one complete POLY_GT3 per element in the half's second region.
/// Rejects failed projections and nonpositive NCLIP(0,1,2). For accepted packets,
/// nonzero `obj->shading.screenFadeDistance` is a pixel distance d: corner 0
/// below b = 360 - d gives fade amount 2*(y0-b). RGB lighting uses weight
/// 128 minus that amount; amounts above 128 write black. All packet Y values
/// then subtract 2*(y0-b), reflecting corner 0 about b without changing shape.
/// Zero d disables both effects. Y stores retain signed 16-bit truncation.
/// Draws with semi-transparent GPU code 0x36 regardless of `objectFlags`.
///
/// Every element consumes its 40-byte packet, even when rejected. Advances
/// `primWrite`, decrements `elemCount` to -1 (also for an empty record), and
/// returns `elements + initialCount * elemStride`. Accepted packets prepend
/// to `ot` at (((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023 using
/// AVSZ3; the display supplies shifts 0..3. The OT base includes the object's
/// displacement, so each resulting bucket must fit the backing table.
/// Stream, geometry, packet and OT bounds are unchecked. All storage is borrowed;
/// keep packets and the OT alive through GPU consumption. Retains no pointer.
u32* actor403600DrawStreamGt3BottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Fades and reflects pre-transformed GT3 packets at the bottom screen boundary.
///
/// Selected for 0x8039. Uses the lifetime, pixel fade, return, counter and OT
/// contracts of `actor403600DrawStreamGt3BottomFade`, but consumes 40-byte slots
/// from `preXformWrite` in the first buffer region. Earlier records must have
/// filled XY and RGB; this callback does no projection or lighting. Each element
/// has at least two words, whose first three u16 values are depth-cache byte
/// offsets; low two bits are discarded. Each resulting offset must be in
/// 0..4092 in the draw pass's 1024-entry `szTable`. Positive NCLIP(0,1,2) and
/// three depths without `TMD_VERTEX_DEPTH_INVALID` are required. RGB channels
/// multiply by the shared 0..128 fade weight and shift right by 7; repeated
/// calls therefore fade the packet's existing colours again. Always draws
/// semi-transparently, ignores `objectFlags`, and advances only `preXformWrite`.
u32* actor403600DrawStreamGt3PreXformBottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and fades GT4 packets at the bottom screen boundary.
///
/// Selected for 0x8078. Uses `actor403600DrawStreamGt3BottomFade`'s contracts
/// with four corners, 52-byte POLY_GT4 slots, AVSZ4 depth and GPU code 0x3E.
/// Each element has at least four words: four vertex byte offsets, then four
/// normal byte offsets. Both projection stages must succeed. Keeps positive
/// NCLIP(0,1,2), or otherwise negative NCLIP(1,2,3). All four colours share
/// corner 0's fade weight and all four Y values share its displacement.
u32* actor403600DrawStreamGt4BottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Fades and reflects pre-transformed GT4 packets at the bottom screen boundary.
///
/// Selected for 0x8079. Uses `actor403600DrawStreamGt3PreXformBottomFade`'s
/// contracts with 52-byte POLY_GT4 slots, four depth-cache byte offsets in two
/// words, AVSZ4 and GPU code 0x3E. Keeps positive NCLIP(0,1,2), or otherwise
/// negative NCLIP(1,2,3), and requires all four cached depths to be valid.
/// The retained colour sequence leaves corner 1 unchanged, writes its faded
/// RGB to corner 3, and then fades corner 3 again; corners 0 and 2 fade once.
/// This sequence also runs with full weight when d is nonzero but y0 <= 360-d.
u32* actor403600DrawStreamGt4PreXformBottomFade(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Draws GT3 packets with bottom-side colour fading and upward top displacement.
///
/// Selected for 0x10038. Uses `actor403600DrawStreamGt3BottomFade`'s record,
/// lighting, culling, cursor and lifetime contracts, including its bottom-side
/// RGB weight. With nonzero pixel distance d and y0 < t = d - 360, adds
/// 2*(y0-t) to every packet Y, moving the whole packet farther upward.
/// Uses modulated texture code 0x34 normally and semi-transparent code 0x36
/// when displaced past the top threshold. `objectFlags` is ignored; clearing
/// the raw-texture bit retains the blend bit set by that displacement.
u32* actor403600DrawStreamGt3TopDisplace(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Draws the semi-transparent entry of the top-displaced GT3 screen fade.
///
/// Selected for 0x1003A. Has `actor403600DrawStreamGt3TopDisplace`'s complete
/// contract, but always sets GPU code 0x36, including undisplaced packets.
u32* actor403600DrawStreamGt3TopDisplaceSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Draws GT4 packets with bottom-side colour fading and upward top displacement.
///
/// Selected for 0x10078. Uses `actor403600DrawStreamGt4BottomFade`'s record,
/// lighting, culling, cursor and lifetime contracts, but displaces every Y by
/// the top-side rule of `actor403600DrawStreamGt3TopDisplace`. Uses modulated
/// texture code 0x3C normally and semi-transparent code 0x3E for displaced
/// packets. `objectFlags` is ignored.
u32* actor403600DrawStreamGt4TopDisplace(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Draws GT3 packets after flattening their positive-Y side onto the ripple plane.
///
/// Selected for 0x20038. With an active ripple plane, uses the record, geometry,
/// packet, OT and lifetime contracts of `actor403600DrawStreamGt3BottomFade`,
/// but no screen fade. Converts each vertex from the current GTE part transform
/// into the composed ripple coordinate, clamps local Y to at most zero, then
/// projects through that coordinate. Normals remain in the original lighting
/// frame with RGB weight (128,128,128); keeps positive NCLIP(0,1,2) and always
/// draws opaque code 0x34, ignoring `objectFlags`. Saves and restores GTE
/// rotation and translation using one nested scratch-stack block; lighting
/// and projection result registers are overwritten. The plane coordinate and
/// its ancestors must be composed and remain live throughout the callback.
///
/// With no active plane, delegates to `tmdDrawStreamGt3`, including that
/// handler's object-flag, counter and depth-shift behavior.
u32* actor403600DrawStreamGt3PlaneClamp(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Draws GT4 packets after flattening their positive-Y side onto the ripple plane.
///
/// Selected for 0x20078. Uses `actor403600DrawStreamGt3PlaneClamp`'s active-plane
/// and lifetime contracts with four vertex and four normal byte offsets in at
/// least four words, 52-byte POLY_GT4 slots, AVSZ4 depth and opaque code 0x3C.
/// Keeps positive NCLIP(0,1,2), or otherwise negative NCLIP(1,2,3), and rejects
/// either failed projection stage. Without a plane, delegates to
/// `tmdDrawStreamGt4` with the original `objectFlags`.
u32* actor403600DrawStreamGt4PlaneClamp(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects plane-clamped vertices and scatters their XY and lit RGB into packets.
///
/// Selected for 0x200C8. With an active ripple plane, each element has at least
/// two words packing four u16 byte offsets: vertex, normal, XY destination and
/// RGB destination. Discards the geometry offsets' low three bits; each must
/// name a complete eight-byte SVECTOR in its borrowed array. The vertex index
/// must also fit the draw pass's 1024-entry `szTable`. Both destinations are
/// aligned four-byte words relative to `preXformWrite`, within the selected
/// buffer half's first region. Writes XY before RGB even when they coincide;
/// the RGB word includes a zero command byte. Bounds are unchecked.
///
/// Uses `actor403600DrawStreamGt3PlaneClamp`'s coordinate and GTE save/restore
/// contract. Consecutive equal vertex offsets reuse XY and cached screen Z,
/// including failed projections marked with `TMD_VERTEX_DEPTH_INVALID`.
/// Each element still lights its own original-frame normal with (128,128,128).
/// Advances only the returned stream cursor; packet-region cursors are bases
/// and remain unchanged. Nonempty active-plane records leave `elemCount` -1;
/// an empty one returns immediately with count zero and reserves no scratch.
/// Allocates no packet and links nothing. Ignores `objectFlags` while active;
/// without a plane, delegates to `tmdXformStreamVerts`. Borrows every pointer
/// only for the call; keep destination packets live through GPU consumption.
u32* actor403600XformStreamVertsPlaneClamp(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

#endif // INCLUDE_ACTORS_ACTOR_403600_H
